# -*- mode: qore; indent-tabs-mode: nil -*-

/*  FtpClientPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! @file FtpClientPollOperation.qc Composite FTP client poll operation

public namespace FtpClientIo {

#! High-level FTP operation types
public const FTP_OP_LOGIN = "login";
public const FTP_OP_GET = "get";
public const FTP_OP_PUT = "put";
public const FTP_OP_LIST = "list";
public const FTP_OP_COMMAND = "command";
public const FTP_OP_CWD = "cwd";

#! FTP transfer result
public hashdecl FtpTransferResult {
    #! FTP response code from the completion response (226 = success)
    int code;
    #! FTP response message
    string message;
    #! Received data (for get/list operations)
    *binary data;
}

#! FTP command result
public hashdecl FtpCommandResult {
    #! FTP response code
    int code;
    #! FTP response message
    string message;
}

/** @brief Composite FTP client poll operation orchestrator

    This class coordinates FtpControlPollOperation and FtpDataPollOperation
    to implement high-level FTP operations (get, put, list).

    Usage pattern:
    1. Create the operation
    2. Call submitGet()/submitPut()/submitList() to queue a transfer
    3. Use with AsyncIoController via DelegatingPollOperation

    @par Example
    @code{.py}
    FtpClientPollOperation op("ftp.example.com", 21);
    # ... submit to controller, wait for login ...
    op.submitGet("/path/to/file");
    # ... submit to controller, wait for transfer ...
    binary data = op.getTransferResult().data;
    @endcode

    @since %Qore 3.0
*/
public class FtpClientPollOperation {
    private {
        #! Control channel poll operation
        FtpControlPollOperation ctrl;

        #! Current data channel poll operation (created per-transfer)
        *FtpDataPollOperation data_op;

        #! Connection parameters
        string host;
        int port;
        bool secure;
        bool secure_data;

        #! Current high-level operation state
        string op_type = FTP_OP_LOGIN;
        int op_phase = 0;
        string op_remote_path;
        bool op_long_list;
        *binary op_send_data;

        #! Data mode (epsv preferred, falls back to pasv)
        string data_mode = FTP_MODE_EPSV;
        #! Parsed data host/port from EPSV/PASV response
        string data_host;
        int data_port;

        #! Transfer result
        *hash<FtpTransferResult> transfer_result;

        #! Current active poll operation (ctrl or data)
        AbstractPollOperation current_poll_op;

        #! True while the data connection is being established
        /** The milestone this phase waits for is the data connection being up, which is not a goal
            @ref Qore::FtpDataPollOperation "FtpDataPollOperation" ever reports on its own: it is only "done" once the entire transfer
            has been read, and that cannot happen until the transfer command has been sent.
        */
        bool awaiting_data_connect = False;

        #! Initial directory requested by a connection provider
        *string initial_path;
    }

    public {
        #! @ref Qore::FtpDataPollOperation "FtpDataPollOperation" states in which the data connection is not yet usable
        const DataConnectStates = {
            "data-connecting": True,
            "data-ssl-upgrade": True,
        };

    }

    /** @brief Creates a new FTP client poll operation

        @param host FTP server hostname
        @param port FTP server port (default: 21)
        @param secure use FTPS (AUTH TLS)
        @param secure_data encrypt data channel
        @param user FTP username
        @param pass FTP password
        @param initial_path optional directory to select after login
    */
    constructor(string host, int port = 21, bool secure = False, bool secure_data = False,
            *string user, *string pass, *string initial_path) {
        self.host = host;
        self.port = port;
        self.secure = secure;
        self.secure_data = secure_data;
        self.initial_path = initial_path;
        ctrl = new FtpControlPollOperation(host: host, port: port, secure: secure, secure_data: secure_data,
            username: user, password: pass);
        current_poll_op = ctrl;
    }

    #! Returns the current poll operation to submit to the async controller
    AbstractPollOperation getCurrentPollOp() {
        return current_poll_op;
    }

    #! Returns @ref True when the operation in flight has reached the milestone its phase waits for
    /** Drivers must use this rather than @ref Qore::AbstractPollOperation::goalReached() "goalReached()" on
        the operation returned by @ref getCurrentPollOp(), and call @ref advancePhase() when it returns
        @ref True.

        For most phases the milestone *is* the operation's own goal.  While the data connection is being
        established it is not: the data operation's goal is the whole transfer, which cannot complete until
        the transfer command has been sent, and the transfer command cannot be answered until the data
        connection is up.  Waiting on \c goalReached() there deadlocks.

        @note Drivers must check this *before* acting on the @ref Qore::SocketPollInfo "SocketPollInfo"
        returned by the current operation's \c continuePoll(), not after.  The poll cycle that completes the
        data connection also starts the transfer and therefore returns a read/write descriptor to wait on;
        waiting on it blocks until data arrives, and no data arrives until the transfer command is sent.

        @return @ref True if the current phase may advance
    */
    bool currentOpReady() {
        if (awaiting_data_connect) {
            return !DataConnectStates{data_op.getState()};
        }
        return current_poll_op.goalReached();
    }

    #! Returns True if the login phase is complete
    bool isLoggedIn() {
        return ctrl.isReady();
    }

    #! Returns the control poll operation
    FtpControlPollOperation getControlOp() {
        return ctrl;
    }

    #! Submit a GET (download) operation
    /** Must be called after login is complete.
        The operation will:
        1. Send TYPE I (binary mode)
        2. Send EPSV/PASV to get data port
        3. Connect data channel
        4. Send RETR with the remote path
        5. Transfer data
        6. Read 226 completion response

        @param remote_path the remote file path to download
    */
    submitGet(string remote_path) {
        if (!ctrl.isReady()) {
            throw "FTP-ERROR", "not logged in";
        }
        op_type = FTP_OP_GET;
        op_phase = 0;
        op_remote_path = remote_path;
        remove transfer_result;
        # Phase 0: TYPE I
        ctrl.submitCommand(command: "TYPE", argument: "I");
        current_poll_op = ctrl;
    }

    #! Submit a PUT (upload) operation
    /** @param remote_path the remote file name to upload
        @param data the data to upload
    */
    submitPut(string remote_path, binary data) {
        if (!ctrl.isReady()) {
            throw "FTP-ERROR", "not logged in";
        }
        op_type = FTP_OP_PUT;
        op_phase = 0;
        op_remote_path = remote_path;
        op_send_data = data;
        remove transfer_result;
        # Phase 0: TYPE I
        ctrl.submitCommand(command: "TYPE", argument: "I");
        current_poll_op = ctrl;
    }

    #! Submit a LIST (directory listing) operation
    /** @param path the directory path to list (empty for current directory)
        @param long_list if True, use LIST; otherwise use NLST
    */
    submitList(*string path, bool long_list = True) {
        if (!ctrl.isReady()) {
            throw "FTP-ERROR", "not logged in";
        }
        op_type = FTP_OP_LIST;
        op_phase = 0;
        op_remote_path = path ?? "";
        op_long_list = long_list;
        remove transfer_result;
        # Phase 0: TYPE A (ASCII for directory listings)
        ctrl.submitCommand(command: "TYPE", argument: "A");
        current_poll_op = ctrl;
    }

    #! Submit a simple FTP command
    /** @param cmd FTP command
        @param arg optional argument
    */
    submitCommand(string cmd, *string arg) {
        if (!ctrl.isReady()) {
            throw "FTP-ERROR", "not logged in";
        }
        op_type = FTP_OP_COMMAND;
        op_phase = 0;
        ctrl.submitCommand(command: cmd, argument: arg);
        current_poll_op = ctrl;
    }

    #! Called by the orchestrator after each poll operation completes
    /** Advances the multi-step operation to the next phase.

        @return True if the high-level operation is complete; False if more phases remain
    */
    bool advancePhase() {
        switch (op_type) {
            case FTP_OP_LOGIN:
                if (initial_path.val()) {
                    op_type = FTP_OP_CWD;
                    ctrl.submitCommand(command: "CWD", argument: initial_path);
                    current_poll_op = ctrl;
                    return False;
                }
                return True;  # Login is handled entirely by ctrl op
            case FTP_OP_CWD: {
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 2) {
                    throw "FTP-CWD-ERROR", sprintf("CWD failed: %s", resp.message);
                }
                remove initial_path;
                return True;
            }
            case FTP_OP_COMMAND:
                return True;  # Simple command done
            case FTP_OP_GET:
                return advanceGetPhase();
            case FTP_OP_PUT:
                return advancePutPhase();
            case FTP_OP_LIST:
                return advanceListPhase();
        }
        return True;
    }

    #! Returns the result of the last transfer operation
    *hash<FtpTransferResult> getTransferResult() {
        return transfer_result;
    }

    #! Returns the last command result from the control channel
    *hash<FtpCommandResult> getCommandResult() {
        hash<auto> output = ctrl.getOutput();
        if (!output) {
            return NOTHING;
        }
        return <FtpCommandResult>{
            "code": output.code,
            "message": output.message,
        };
    }

    # ========================================
    # Private: GET phase advancement
    # ========================================
    private bool advanceGetPhase() {
        switch (op_phase++) {
            case 0: {
                # TYPE I response received → send EPSV/PASV
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 2) {
                    throw "FTP-ERROR", sprintf("TYPE command failed: %s", resp.message);
                }
                sendDataModeCommand();
                return False;
            }
            case 1: {
                # EPSV/PASV response → establish the data connection *before* sending RETR
                if (!parseDataModeResponse()) {
                    return False;  # Fell back to PASV, need another phase
                }
                # NOTE: the data connection is established before the transfer command is sent, which is
                # the order a real FTP client uses.  A server may not answer RETR at all until the data
                # connection it just advertised has been accepted -- it has nothing to say until then --
                # so sending RETR first and connecting afterwards deadlocks: the client waits for a
                # response the server will not send until the client connects.
                #
                # Creating the operation is not enough: FtpDataPollOperation issues its connect on the
                # first continuePoll(), not in its constructor, so it has to become the operation being
                # driven until the connection is up
                startDataReceive();
                awaiting_data_connect = True;
                return False;
            }
            case 2: {
                # data connection up → send RETR on the control channel
                awaiting_data_connect = False;
                ctrl.submitCommand(command: "RETR", argument: op_remote_path);
                current_poll_op = ctrl;
                return False;
            }
            case 3: {
                # RETR 150 response → read the data connection established above
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 1) {
                    # the server refused the transfer (550 for a file it cannot open, for example); the data
                    # connection opened for it will never be used, so tear it down rather than leaking it
                    discardDataOp();
                    throw "FTP-GET-ERROR", sprintf("RETR failed: %s", resp.message);
                }
                current_poll_op = data_op;
                return False;
            }
            case 4: {
                # Data transfer complete → save data, read 226 on control channel
                *binary recv = data_op.getOutput();
                remove data_op;
                op_send_data = recv;  # stash received data temporarily
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 5: {
                # 226 completion response received
                hash<auto> resp = ctrl.getOutput();
                transfer_result = <FtpTransferResult>{
                    "code": resp.code,
                    "message": resp.message,
                    "data": op_send_data,
                };
                remove op_send_data;
                return True;
            }
        }
        return True;
    }

    # ========================================
    # Private: PUT phase advancement
    # ========================================
    private bool advancePutPhase() {
        switch (op_phase++) {
            case 0: {
                # TYPE I response → send EPSV/PASV
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 2) {
                    throw "FTP-ERROR", sprintf("TYPE command failed: %s", resp.message);
                }
                sendDataModeCommand();
                return False;
            }
            case 1: {
                # EPSV/PASV response → establish the data connection *before* sending STOR
                /* see the note in advanceGetPhase(): the server has nothing to say about STOR until the
                   data connection it just advertised has been accepted, and the connection is not opened
                   until the data operation is driven */
                if (!parseDataModeResponse()) {
                    return False;
                }
                startDataSend();
                awaiting_data_connect = True;
                return False;
            }
            case 2: {
                # data connection up → send STOR on the control channel
                awaiting_data_connect = False;
                ctrl.submitCommand(command: "STOR", argument: op_remote_path);
                current_poll_op = ctrl;
                return False;
            }
            case 3: {
                # STOR 150 response → send the payload on the data connection established above
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 1) {
                    discardDataOp();
                    throw "FTP-PUT-ERROR", sprintf("STOR failed: %s", resp.message);
                }
                current_poll_op = data_op;
                return False;
            }
            case 4: {
                # Data send complete → read 226 on control channel
                remove data_op;
                remove op_send_data;
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 5: {
                # 226 completion response received
                hash<auto> resp = ctrl.getOutput();
                transfer_result = <FtpTransferResult>{
                    "code": resp.code,
                    "message": resp.message,
                };
                return True;
            }
        }
        return True;
    }

    # ========================================
    # Private: LIST phase advancement
    # ========================================
    private bool advanceListPhase() {
        switch (op_phase++) {
            case 0: {
                # TYPE A response → send EPSV/PASV
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 2) {
                    throw "FTP-ERROR", sprintf("TYPE command failed: %s", resp.message);
                }
                sendDataModeCommand();
                return False;
            }
            case 1: {
                # EPSV/PASV response → establish the data connection *before* sending LIST/NLST
                /* see the note in advanceGetPhase(): the data connection is opened *and driven* before the
                   command, because it is not connected until it is polled */
                if (!parseDataModeResponse()) {
                    return False;
                }
                startDataReceive();
                awaiting_data_connect = True;
                return False;
            }
            case 2: {
                # data connection up → send LIST/NLST on the control channel
                awaiting_data_connect = False;
                ctrl.submitCommand(op_long_list ? "LIST" : "NLST",
                    op_remote_path ? op_remote_path : NOTHING);
                current_poll_op = ctrl;
                return False;
            }
            case 3: {
                # LIST 150 response → read the data connection established above
                hash<auto> resp = ctrl.getOutput();
                if (resp.code == 550) {
                    # the directory does not exist or has nothing to list: a complete answer, not an error.
                    # Only 550 means that; any other 5xx (500 for a command the server does not implement,
                    # for instance) is a real failure and must not be reported as an empty listing.
                    discardDataOp();
                    transfer_result = <FtpTransferResult>{
                        "code": resp.code,
                        "message": resp.message,
                    };
                    return True;
                }
                if ((resp.code / 100) != 1) {
                    discardDataOp();
                    throw "FTP-LIST-ERROR", sprintf("LIST failed: %s", resp.message);
                }
                current_poll_op = data_op;
                return False;
            }
            case 4: {
                # Data transfer complete → save data, read 226 on control channel
                *binary recv = data_op.getOutput();
                remove data_op;
                op_send_data = recv;  # stash received data temporarily
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 5: {
                # 226 completion response received
                hash<auto> resp = ctrl.getOutput();
                transfer_result = <FtpTransferResult>{
                    "code": resp.code,
                    "message": resp.message,
                    "data": op_send_data,
                };
                remove op_send_data;
                return True;
            }
        }
        return True;
    }

    # ========================================
    # Private: data mode helpers
    # ========================================
    private sendDataModeCommand() {
        if (data_mode == FTP_MODE_EPSV) {
            ctrl.submitCommand(command: "EPSV");
        } else {
            ctrl.submitCommand(command: "PASV");
        }
        current_poll_op = ctrl;
    }

    #! Parse EPSV/PASV response and extract data host:port
    /** @return True if parsed successfully; False if falling back to PASV (need another phase)
    */
    private bool parseDataModeResponse() {
        hash<auto> resp = ctrl.getOutput();
        if (data_mode == FTP_MODE_EPSV) {
            if ((resp.code / 100) != 2) {
                # EPSV failed — fall back to PASV
                data_mode = FTP_MODE_PASV;
                ctrl.submitCommand(command: "PASV");
                current_poll_op = ctrl;
                --op_phase;  # Stay at the same phase for the fallback
                return False;
            }
            # Parse EPSV response: "229 ... (|||<port>|)"
            *list<*string> m = (resp.message =~ x/\|\|\|(\d+)\|/);
            if (!m) {
                throw "FTP-RESPONSE-ERROR", sprintf("cannot parse EPSV response: %s", resp.message);
            }
            data_host = host;
            data_port = m[0].toInt();
        } else {
            if ((resp.code / 100) != 2) {
                throw "FTP-CONNECT-ERROR", sprintf("PASV failed: %s", resp.message);
            }
            # Parse PASV response: "227 ... (h1,h2,h3,h4,p1,p2)"
            *list<*string> m = (resp.message =~ x/\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\)/);
            if (!m) {
                throw "FTP-RESPONSE-ERROR", sprintf("cannot parse PASV response: %s", resp.message);
            }
            data_host = sprintf("%s.%s.%s.%s", m[0], m[1], m[2], m[3]);
            data_port = (m[4].toInt() << 8) + m[5].toInt();
        }
        return True;
    }

    #! Opens the data connection without making it the operation being polled
    /** The connect is started by the constructor, so the server can accept it -- and therefore answer the
        transfer command -- while the control channel is still the operation being driven.
    */
    private createDataReceiveOp() {
        data_op = new FtpDataPollOperation(host: data_host, port: data_port, secure_data: secure_data);
    }

    private startDataReceive() {
        createDataReceiveOp();
        current_poll_op = data_op;
    }

    private startDataSend() {
        data_op = new FtpDataPollOperation(data_host, data_port, op_send_data, secure_data);
        current_poll_op = data_op;
    }

    #! Tears down a data connection that was opened for a transfer the server then refused
    /** The data connection is opened before the transfer command is sent, so a command the server rejects
        leaves a connected socket behind with nothing to carry.  It must be closed rather than left to the
        garbage collector, or the server's data-accept thread waits for a transfer that never comes.
    */
    private discardDataOp() {
        if (data_op) {
            data_op.abort();
            remove data_op;
        }
        current_poll_op = ctrl;
    }
}

} # namespace FtpClientIo
