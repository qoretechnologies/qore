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
    }

    /** @brief Creates a new FTP client poll operation

        @param host FTP server hostname
        @param port FTP server port (default: 21)
        @param secure use FTPS (AUTH TLS)
        @param secure_data encrypt data channel
        @param user FTP username
        @param pass FTP password
    */
    constructor(string host, int port = 21, bool secure = False, bool secure_data = False,
            *string user, *string pass) {
        self.host = host;
        self.port = port;
        self.secure = secure;
        self.secure_data = secure_data;
        ctrl = new FtpControlPollOperation(host: host, port: port, secure: secure, secure_data: secure_data,
            username: user, password: pass);
        current_poll_op = ctrl;
    }

    #! Returns the current poll operation to submit to the async controller
    AbstractPollOperation getCurrentPollOp() {
        return current_poll_op;
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
        4. Send RETR <path>
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
                return True;  # Login is handled entirely by ctrl op
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
                # EPSV/PASV response → parse data port, send RETR
                if (!parseDataModeResponse()) {
                    return False;  # Fell back to PASV, need another phase
                }
                ctrl.submitCommand(command: "RETR", argument: op_remote_path);
                current_poll_op = ctrl;
                return False;
            }
            case 2: {
                # RETR 150 response → start data transfer
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 1) {
                    throw "FTP-GET-ERROR", sprintf("RETR failed: %s", resp.message);
                }
                startDataReceive();
                return False;
            }
            case 3: {
                # Data transfer complete → save data, read 226 on control channel
                *binary recv = data_op.getOutput();
                remove data_op;
                op_send_data = recv;  # stash received data temporarily
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 4: {
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
                # EPSV/PASV response → parse data port, send STOR
                if (!parseDataModeResponse()) {
                    return False;
                }
                ctrl.submitCommand(command: "STOR", argument: op_remote_path);
                current_poll_op = ctrl;
                return False;
            }
            case 2: {
                # STOR 150 response → start data transfer
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) != 1) {
                    throw "FTP-PUT-ERROR", sprintf("STOR failed: %s", resp.message);
                }
                startDataSend();
                return False;
            }
            case 3: {
                # Data send complete → read 226 on control channel
                remove data_op;
                remove op_send_data;
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 4: {
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
                # EPSV/PASV response → parse data port, send LIST/NLST
                if (!parseDataModeResponse()) {
                    return False;
                }
                ctrl.submitCommand(op_long_list ? "LIST" : "NLST",
                    op_remote_path ? op_remote_path : NOTHING);
                current_poll_op = ctrl;
                return False;
            }
            case 2: {
                # LIST 150 response → start data transfer
                hash<auto> resp = ctrl.getOutput();
                if ((resp.code / 100) == 5) {
                    # 5xx = file not found / empty
                    transfer_result = <FtpTransferResult>{
                        "code": resp.code,
                        "message": resp.message,
                    };
                    return True;
                }
                if ((resp.code / 100) != 1) {
                    throw "FTP-LIST-ERROR", sprintf("LIST failed: %s", resp.message);
                }
                startDataReceive();
                return False;
            }
            case 3: {
                # Data transfer complete → save data, read 226 on control channel
                *binary recv = data_op.getOutput();
                remove data_op;
                op_send_data = recv;  # stash received data temporarily
                ctrl.submitRecvResponse();
                current_poll_op = ctrl;
                return False;
            }
            case 4: {
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

    private startDataReceive() {
        data_op = new FtpDataPollOperation(host: data_host, port: data_port, secure_data: secure_data);
        current_poll_op = data_op;
    }

    private startDataSend() {
        data_op = new FtpDataPollOperation(data_host, data_port, op_send_data, secure_data);
        current_poll_op = data_op;
    }
}

} # namespace FtpClientIo
