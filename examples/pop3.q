#!/usr/bin/env qore

# @file pop3.q example program using the Pop3Client module

/*  pop3.q Copyright 2012 - 2023 David Nichols

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

/*  Version History
    * 2012-06-23 v1.0: simple example program using the Pop3Client user module to retrieve emails from the command line from a POP3 server
*/

%modern

%exec-class pop3

%requires Pop3Client >= 1.0
%requires MailMessage >= 1.0
%requires Mime >= 1.0
%requires Util
%requires ConnectionProvider

class pop3 {
    private {
        # command-line option hash
        const opts = (
            "del": "d,delete",
            "help": "h,help",
            "user": "u,user=s",
            "pass": "p,pass=s",
            "noquit": "N,no-quit",
            "svr": "s,server=s",
            "ssl": "S,ssl",
            "starttls": "T,starttls",
            "verbose": "v,verbose",
            );

        # command-line options
        hash opt;

        # SMTP server
        string server;
    }

    public {}

    constructor() {
        GetOpt g(opts);
        opt = g.parse3(\ARGV);
        if (opt.help)
            usage();
        if (opt.svr.empty()) {
            stderr.print("missing POP3 server address\n");
            usage();
        }

        string url;
        {
            url = get_connection_url(opt.svr);
            if (url != opt.svr)
                printf("using connection: %y url: %y\n", opt.svr, url);
            else {
                if (opt.user.empty()) {
                    stderr.print("missing user argument\n");
                    usage();
                }
                if (opt.pass.empty()) {
                    stderr.print("missing password argument\n");
                    usage();
                }
                url = sprintf("pop3%s://%s:%s@%s", opt.ssl ? "s" : "", opt.user, opt.pass, opt.svr);
            }
        }

        try {
            Pop3Client pop3(url, \log(), opt.verbose ? \log() : NOTHING);
            if (opt.noquit)
                pop3.noquit(True);
            if (opt.starttls)
                pop3.starttls(True);
            *hash h = pop3.getMail();
            foreach string k in (keys h) {
                Message msg = h{k}.msg;
                # save message body with headers
                string body = msg.getHeaderString("\n", False);
                body += "\n";
                *data mb = msg.getBody();
                if (mb.typeCode() == NT_BINARY) {
                    saveFile(k, "headers", body);
                    saveFile(k, "body", mb);
                }
                else {
                    if (mb.empty())
                        body += "<empty message body>\n";
                    else
                        body += mb;
                    saveFile(k, "headers+body", body);
                }

                # save attachments
                foreach Attachment att in (msg.getAttachments()) {
                    saveFile(k, att.getName(), att.getData());
                }

                # save other parts as alternative message body representations
                int c = 0;
                foreach Part p in (msg.getParts()) {
                    saveFile(k, sprintf("part-%d-%s", ++c, p.getTransferEncoding()), p.getData());
                }
            }
            if (opt.del) {
                foreach string k in (keys h)
                    pop3.del(k);
                log("%d message%s removed from the server", h.size(), h.size() == 1 ? "" : "s");
            }
        }
        catch (hash ex) {
            if (opt.verbose)
                printf("%s\n", get_exception_string(ex));
            else
                printf("%s: %s: %s\n", get_ex_pos(ex), ex.err, ex.desc);
        }
    }

    log(string msg) {
        printf("%y: %s\n", now_us(), vsprintf(msg, argv));
    }

    saveFile(string msgid, string tag, *data data) {
        if (data.empty())
            return;

        # create a new directory for each message, if it doesn't already exist
        if (!is_dir(msgid))
            mkdir(msgid);

        int cnt = 0;
        string fn = sprintf("%s/%s", msgid, tag);

        while (exists stat(fn)) {
            fn = sprintf("%s/%s.%d", msgid, tag, ++cnt);
        }

        File f();
        f.open2(fn, O_CREAT|O_WRONLY|O_TRUNC);
        f.write(data);
        log("saved file for msgid %y as %y (%d bytes)", msgid, fn, data.size());
    }

    static usage() {
        printf("usage: %s [options]\n"
               "  -u, -p, and -s are required unless -s gives a connection name\n"
               "options:\n"
               " -d,--delete       remove messages on the server (none are deleted by default)\n"
               " -h,--help         this help text\n"
               " -N,--no-quit      do not send a QUIT message (ex: keeps gmail from marking\n"
               "                   messages as viewed)\n"
               " -p,--pass=ARG     password for the POP3 mailbox\n"
               " -s,--server=ARG   the POP3 server for the connection; include a port number if\n"
               "                   not connecting from the default port (%d) like:\n"
               "                   ex: \"-s=pop3.com:1110\"\n"
               " -S,--ssl          use a TLS/SSL connection to the POP3 server\n"
               " -T,--starttls     issue the STARTTLS command after an unencrypted connection\n"
               " -u,--user=ARG     username for the POP3 mailbox\n"
               " -v,--verbose      show detailed technical information\n",
               get_script_name(), Pop3Client::POP3Port);
        exit(1);
    }
}
