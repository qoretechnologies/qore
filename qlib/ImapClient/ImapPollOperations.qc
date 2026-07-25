# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapPollOperations.qc IMAP non-blocking poll operations

/*  ImapPollOperations.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ImapClient {
#! SASL authentication setup derived from a connection's configuration
/** @since %ImapClient 1.0
*/
public hashdecl ImapAuthInfo {
    #! @ref True if any authentication should be attempted
    bool do_auth = False;

    #! @ref True if a bearer-token mechanism (\c XOAUTH2 or \c OAUTHBEARER) is in use
    bool use_bearer = False;

    #! The mechanism that will be used, or an empty string if no authentication is configured
    string mechanism = "";

    #! The base64 SASL initial response for the bearer mechanism, or an empty string
    string bearer_ir = "";

    #! The base64 SASL initial response for \c XOAUTH2, or an empty string
    string xoauth2_ir = "";

    #! The base64 SASL initial response for \c OAUTHBEARER, or an empty string
    string oauthbearer_ir = "";

    #! The base64 SASL initial response for \c PLAIN, or an empty string
    string plain_ir = "";

    #! The \c LOGIN command arguments, or an empty string
    string login_args = "";
}

#! Poll operation that connects to an IMAP server and authenticates
/** Expresses the whole IMAP session setup — connect, greeting, \c CAPABILITY, optional \c STARTTLS with
    a mandatory capability re-read, SASL authentication, a post-authentication capability re-read, and an
    optional \c NOOP — as a single declarative pipeline, so the connection can be established entirely on
    the async I/O thread.

    Mechanism selection is driven by the capability list the server advertises, never by the hostname.
    The list is re-read after \c STARTTLS because servers commonly advertise \c AUTH mechanisms only on a
    secure connection, and again after login because
    <a href="https://datatracker.ietf.org/doc/html/rfc3501#section-6.2.1">RFC 3501 §6.2.1</a> allows the
    server to return an updated list on successful authentication.

    @since %ImapClient 1.0
*/
public class ImapPingPollOperation inherits Qore::PollPipeline {
    #! @cond nodoc
    private {
        #! the tag used for each command in the pipeline
        hash<string, string> tags;
    }
    #! @endcond

    #! Creates the object with a new socket
    /** @param imap the ImapClient whose connection info drives the pipeline
        @param send_noop if True, send a \c NOOP after authentication

        @throw IMAP-POLL-ERROR the server returned an error response
    */
    constructor(ImapClient imap, bool send_noop = True) : PollPipeline(new Socket()) {
        setup(imap, send_noop);
    }

    #! Creates the object on an existing socket
    /** @param imap the ImapClient whose connection info drives the pipeline
        @param sock the socket to connect and leave open after the pipeline completes
        @param send_noop if True, send a \c NOOP after authentication

        @throw IMAP-POLL-ERROR the server returned an error response
    */
    constructor(ImapClient imap, Socket sock, bool send_noop = True) : PollPipeline(sock) {
        setup(imap, send_noop);
    }

    #! @cond nodoc
    private setup(ImapClient imap, bool send_noop) {
        hash<auto> info = imap.getInfo();
        goal = send_noop ? "ping" : "connect";

        hash<ImapAuthInfo> ai = ImapClient::getAuthInfo(info);
        int max_size = info.max_response_size;

        # --- configuration ---
        addSetContextBool("need_starttls", (!info.ssl && info.tls) ? True : False);
        addSetContextBool("do_auth", ai.do_auth);
        addSetContextBool("use_bearer", ai.use_bearer);

        addSetContextValue("cap_tag", "c1");
        addSetContextBinary("cap_cmd", "c1 CAPABILITY\r\n");
        addSetContextValue("stls_tag", "s1");
        addSetContextBinary("stls_cmd", "s1 STARTTLS\r\n");
        addSetContextValue("cap2_tag", "c2");
        addSetContextBinary("cap2_cmd", "c2 CAPABILITY\r\n");
        addSetContextValue("auth_tag", "a1");
        addSetContextValue("cap3_tag", "c3");
        addSetContextBinary("cap3_cmd", "c3 CAPABILITY\r\n");
        addSetContextValue("noop_tag", "n1");
        addSetContextBinary("noop_cmd", "n1 NOOP\r\n");

        # both SASL-IR and non-SASL-IR forms of every mechanism, pre-computed; which one is sent is
        # decided from the capability list at run time
        addSetContextBinary("xoauth2_ir_cmd", sprintf("a1 AUTHENTICATE XOAUTH2 %s\r\n", ai.xoauth2_ir));
        addSetContextBinary("oauthbearer_ir_cmd",
            sprintf("a1 AUTHENTICATE OAUTHBEARER %s\r\n", ai.oauthbearer_ir));
        addSetContextBinary("xoauth2_cmd", "a1 AUTHENTICATE XOAUTH2\r\n");
        addSetContextBinary("oauthbearer_cmd", "a1 AUTHENTICATE OAUTHBEARER\r\n");
        addSetContextBinary("xoauth2_payload", ai.xoauth2_ir + "\r\n");
        addSetContextBinary("oauthbearer_payload", ai.oauthbearer_ir + "\r\n");
        addSetContextBinary("plain_ir_cmd", sprintf("a1 AUTHENTICATE PLAIN %s\r\n", ai.plain_ir));
        addSetContextBinary("login_cmd", sprintf("a1 LOGIN %s\r\n", ai.login_args));

        # --- connect ---
        if (info.ssl) {
            addConnectSsl(info.target);
        } else {
            addConnect(info.target);
        }

        # --- read the server greeting (an untagged OK/PREAUTH/BYE line) ---
        addRecvUntilBytes("\r\n");
        addValidateResponsePrefix("*", "IMAP-POLL-ERROR");

        # --- CAPABILITY ---
        addSendFromContext("cap_cmd");
        addRecvImapResponse("cap_tag", "capabilities", "IMAP-POLL-ERROR", max_size);

        # --- optional STARTTLS ---
        int br_stls = addBranchOnContextKey("need_starttls", -1, -1);
        int stls_start = getStepCount();
        addSendFromContext("stls_cmd");
        addRecvImapResponse("stls_tag", "starttls_response", "IMAP-POLL-ERROR", max_size);
        addSslUpgrade();
        # RFC 3501 §6.2.1: the pre-TLS capability list must be discarded and re-read
        addSendFromContext("cap2_cmd");
        addRecvImapResponse("cap2_tag", "capabilities", "IMAP-POLL-ERROR", max_size);

        # --- both paths converge here ---
        int detect_start = getStepCount();
        setStepTarget(br_stls, stls_start, detect_start);

        # mechanism and SASL-IR selection from the advertised capability list
        addCheckContains("capabilities", "AUTH=XOAUTH2", "server_has_xoauth2");
        addCheckContains("capabilities", "SASL-IR", "server_has_sasl_ir");
        addSelectContextValue("bearer_ir_cmd", "server_has_xoauth2", "xoauth2_ir_cmd",
            "oauthbearer_ir_cmd");
        addSelectContextValue("bearer_cmd", "server_has_xoauth2", "xoauth2_cmd", "oauthbearer_cmd");
        addSelectContextValue("bearer_payload", "server_has_xoauth2", "xoauth2_payload",
            "oauthbearer_payload");

        # --- authentication ---
        int br_auth = addBranchOnContextKey("do_auth", -1, -1);
        int br_bearer = addBranchOnContextKey("use_bearer", -1, -1);

        # bearer path: SASL-IR when supported, otherwise the two-step exchange
        int bearer_start = getStepCount();
        int br_sasl_ir = addBranchOnContextKey("server_has_sasl_ir", -1, -1);

        int bearer_ir_start = getStepCount();
        addSendFromContext("bearer_ir_cmd");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);
        int br_ir_challenge = addBranchOnContextKey("imap_continuation", -1, -1);
        int goto_after_bearer_ir = addGoto(-1);

        # a "+" reply to a SASL-IR command means the login failed and the server is waiting for an
        # empty response before it will report the failure
        int ir_challenge_start = getStepCount();
        addSend("\r\n");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);
        int goto_after_ir_challenge = addGoto(-1);

        # two-step path for servers without SASL-IR
        int bearer_two_step = getStepCount();
        addSendFromContext("bearer_cmd");
        addRecvImapResponse("auth_tag", "auth_challenge", "IMAP-POLL-ERROR", max_size);
        addSendFromContext("bearer_payload");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);
        int br_two_step_challenge = addBranchOnContextKey("imap_continuation", -1, -1);
        int goto_after_two_step = addGoto(-1);

        int two_step_challenge_start = getStepCount();
        addSend("\r\n");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);
        int goto_after_two_step_challenge = addGoto(-1);

        # password path: AUTHENTICATE PLAIN with SASL-IR, otherwise LOGIN
        int password_start = getStepCount();
        int br_plain_ir = addBranchOnContextKey("server_has_sasl_ir", -1, -1);
        int plain_start = getStepCount();
        addSendFromContext("plain_ir_cmd");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);
        int goto_after_plain = addGoto(-1);
        int login_start = getStepCount();
        addSendFromContext("login_cmd");
        addRecvImapResponse("auth_tag", "auth_response", "IMAP-POLL-ERROR", max_size);

        # --- post-authentication capability re-read ---
        int post_auth = getStepCount();
        addSendFromContext("cap3_cmd");
        addRecvImapResponse("cap3_tag", "capabilities", "IMAP-POLL-ERROR", max_size);

        int done = getStepCount();
        if (send_noop) {
            addSendFromContext("noop_cmd");
            addRecvImapResponse("noop_tag", "noop_response", "IMAP-POLL-ERROR", max_size);
        }

        # patch every branch target now that all steps exist
        setStepTarget(br_auth, br_bearer, done);
        setStepTarget(br_bearer, bearer_start, password_start);
        setStepTarget(br_sasl_ir, bearer_ir_start, bearer_two_step);
        setStepTarget(br_ir_challenge, ir_challenge_start, post_auth);
        setStepTarget(goto_after_bearer_ir, post_auth, post_auth);
        setStepTarget(goto_after_ir_challenge, post_auth, post_auth);
        setStepTarget(br_two_step_challenge, two_step_challenge_start, post_auth);
        setStepTarget(goto_after_two_step, post_auth, post_auth);
        setStepTarget(goto_after_two_step_challenge, post_auth, post_auth);
        setStepTarget(br_plain_ir, plain_start, login_start);
        setStepTarget(goto_after_plain, post_auth, post_auth);
    }
    #! @endcond

    #! Returns the goal
    /** @return \c "ping" or \c "connect"
    */
    string getGoal() {
        return goal;
    }

    #! Returns the current state
    string getState() {
        if (goalReached()) {
            return "complete";
        }
        if (isClosed()) {
            return "error";
        }
        return PollPipeline::getState();
    }

    #! Returns the capabilities advertised by the server
    /** @return a hash of capability names (upper case) to @ref True; the list is the one read after any
        \c STARTTLS upgrade and after authentication
    */
    hash<string, bool> getCapabilities() {
        hash<auto> extra = getResult().extra ?? {};
        if (!extra.capabilities) {
            return {};
        }
        return ImapClient::parseCapabilities(extra.capabilities);
    }

    #! Returns True if authentication was performed
    bool didAuth() {
        hash<auto> extra = getResult().extra ?? {};
        return extra.do_auth ? True : False;
    }

    #! Returns True if a bearer-token mechanism was used
    bool didBearerAuth() {
        hash<auto> extra = getResult().extra ?? {};
        return (extra.do_auth && extra.use_bearer) ? True : False;
    }

    #! Returns True if STARTTLS was performed
    bool didTls() {
        hash<auto> extra = getResult().extra ?? {};
        return extra.need_starttls ? True : False;
    }
}

#! Poll operation that runs a single IMAP command on an already-authenticated socket
/** @since %ImapClient 1.0
*/
public class ImapCommandPollOperation inherits Qore::PollPipeline {
    #! @cond nodoc
    private {
        #! the command tag
        string tag;
    }
    #! @endcond

    #! Runs one command and reads its complete response
    /** @param sock the connected, authenticated socket
        @param tag the command tag
        @param command the command text without the tag or trailing CRLF
        @param max_size the maximum accepted response size in bytes

        @throw IMAP-SERVER-ERROR the server returned a \c NO or \c BAD completion
    */
    constructor(Socket sock, string tag, string command, int max_size = DefaultMaxResponseSize)
            : PollPipeline(sock) {
        self.tag = tag;
        goal = "command";

        addSetContextValue("tag", tag);
        addSetContextBinary("cmd", sprintf("%s %s\r\n", tag, command));
        addSendFromContext("cmd");
        addRecvImapResponse("tag", "response", "IMAP-SERVER-ERROR", max_size);
    }

    #! Returns the command tag
    string getTag() {
        return tag;
    }

    #! Returns the goal
    string getGoal() {
        return "command";
    }

    #! Returns the current state
    string getState() {
        if (goalReached()) {
            return "complete";
        }
        if (isClosed()) {
            return "error";
        }
        return PollPipeline::getState();
    }

    #! Returns the raw response as received
    binary getRawResponse() {
        hash<auto> extra = getResult().extra ?? {};
        return extra.response ?? binary();
    }

    #! Returns the parsed response
    /** @throw IMAP-PARSE-ERROR the response could not be parsed
    */
    hash<ImapResponse> getResponse() {
        return ImapResponseParser::parse(getRawResponse(), tag);
    }
}

#! Poll operation that appends a message to a mailbox
/** \c APPEND sends the message as a literal, which requires the server to grant a command continuation
    request before the payload may be sent — unless the server advertises \c LITERAL+, in which case the
    payload follows the command immediately.

    @since %ImapClient 1.0
*/
public class ImapAppendPollOperation inherits Qore::PollPipeline {
    #! @cond nodoc
    private {
        #! the command tag
        string tag;
    }
    #! @endcond

    #! Builds the append operation
    /** @param sock the connected, authenticated socket
        @param tag the command tag
        @param mailbox the destination mailbox name
        @param message the serialized message
        @param flags optional flags to set on the appended message
        @param internal_date optional server-side arrival time to record
        @param literal_plus if True, use a non-synchronizing literal (requires the \c LITERAL+ capability)
        @param max_size the maximum accepted response size in bytes

        @throw IMAP-SERVER-ERROR the server returned a \c NO or \c BAD completion
    */
    constructor(Socket sock, string tag, string mailbox, binary message, *list<string> flags,
            *date internal_date, bool literal_plus = False, int max_size = DefaultMaxResponseSize)
            : PollPipeline(sock) {
        self.tag = tag;
        goal = "append";

        string args = sprintf("%s APPEND %s", tag, ImapClient::quoteString(mailbox));
        if (flags) {
            args += sprintf(" (%s)", flags.join(" "));
        }
        if (internal_date) {
            args += sprintf(" \"%s\"", ImapClient::formatInternalDate(internal_date));
        }
        args += sprintf(" {%d%s}\r\n", message.size(), literal_plus ? "+" : "");

        addSetContextValue("tag", tag);
        addSetContextBinary("append_cmd", args);
        addSetContextBinary("payload", message + <0d0a>);

        addSendFromContext("append_cmd");
        if (!literal_plus) {
            # wait for the "+" continuation request before sending the payload
            addRecvImapResponse("tag", "continuation", "IMAP-SERVER-ERROR", max_size);
        }
        addSendFromContext("payload");
        addRecvImapResponse("tag", "response", "IMAP-SERVER-ERROR", max_size);
    }

    #! Returns the command tag
    string getTag() {
        return tag;
    }

    #! Returns the goal
    string getGoal() {
        return "append";
    }

    #! Returns the current state
    string getState() {
        if (goalReached()) {
            return "complete";
        }
        if (isClosed()) {
            return "error";
        }
        return PollPipeline::getState();
    }

    #! Returns the raw response as received
    binary getRawResponse() {
        hash<auto> extra = getResult().extra ?? {};
        return extra.response ?? binary();
    }

    #! Returns the parsed response
    hash<ImapResponse> getResponse() {
        return ImapResponseParser::parse(getRawResponse(), tag);
    }
}
}
