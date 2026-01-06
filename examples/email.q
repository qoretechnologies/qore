#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# @file email.q example program using the SmtpClient module

/*  email.q Copyright 2012 - 2023 Qore Technologies, s.r.o.

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
    * 1.0: simple example program using the SmtpClient user module to send emails from the command line
*/

%modern

%exec-class mail

%requires SmtpClient >= 1.0
%requires Mime >= 1.0
%requires ConnectionProvider

class mail {
    private {
        const opts = (
            "help": "h,help",
            "attach": "a,attach=s@",
            "bcc": "B,bcc=s@",
            "body": "b,body=s",
            "cc": "c,cc=s@",
            "from": "f,from=s",
            "ssl": "S,ssl",
            "subject": "s,subject=s",
            "to": "t,to=s@",
            "user": "u,user=s",
            "pass": "p,pass=s",
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
        if (ARGV.empty()) {
            stderr.print("missing SMTP server address\n");
            usage();
        }

        # ensure the required options are set
        checkOpt("from");
        checkOpt("body");
        checkOpt("subject");

        # make sure we have at least 1 target address
        if (opt.to.empty() && opt.cc.empty() && opt.bcc.empty()) {
            stderr.printf("no target address; use %s -h for option information\n", get_script_name());
            exit(1);
        }

        # if we have either user or pass, make sure we have both
        if ((!opt.user.empty() || !opt.pass.empty()) && (opt.pass.empty() || opt.user.empty())) {
            stderr.printf("missing one of the username or password; if one is supplied then both must be supplied opt: %y\n", opt);
            exit(1);
        }

        server = shift ARGV;

        {
            string orig_server = server;
            server = get_connection_url(server);
            if (server != orig_server)
                printf("using connection: %y url: %y\n", orig_server, server);
        }

        try {
            SmtpClient smtp(server, \log(), \log());
            if (opt.ssl)
                smtp.tls(True);
            if (opt.user.val())
                smtp.setUserPass(opt.user, opt.pass);

            Message msg(opt.from, opt.subject);

            # set message body
            msg.setBody(opt.body);

            # add To: addresses
            foreach string addr in (opt.to)
                msg.addTO(addr);

            # add CC: addresses
            foreach string addr in (opt.cc)
                msg.addCC(addr);

            # add BCC: addresses
            foreach string addr in (opt.bcc)
                msg.addBCC(addr);

            # add attachments
            foreach string fn in (opt.attach) {
                ReadOnlyFile f(fn);
                msg.attach(basename(fn), get_mime_type_from_ext(fn), f.readBinary(-1));
            }

            smtp.sendMessage(msg);
        }
        catch (hash ex) {
            printf("%s: %s\n", ex.err, ex.desc);
            exit(2);
        }
    }

    checkOpt(string o) {
        if (opt{o}.empty()) {
            stderr.printf("missing %y option; use %s -h for option information\n", o, get_script_name());
            exit(1);
        }
    }

    static usage() {
        printf("usage: %s [options] <SMTP>\n"
               "  where <SMTP> is a server address with an optional port (ex: stmp.com:25)\n"
               "  --from, --body, --subject and one of --to, --cc, or --bcc is required\n"
               "options:\n"
               " -a,--attach=ARG   a file to attach\n"
               " -B,--bcc=ARG      an email address for the BCC: line\n"
               " -b,--body=ARG     the message body (required)\n"
               " -c,--cc=ARG       an email address for the CC: line\n"
               " -f,--from=ARG     an email address for the sender\n"
               " -h,--help         this help text\n"
               " -p,--pass=ARG     password for authenticated connections\n"
               " -s,--subject=ARG  the subject of the message (required)\n"
               " -S,--ssl          use a TLS/SSL connection to the SMTP server\n"
               " -t,--to=ARG       an email address for the To: line\n"
               " -u,--user=ARG     username for authenticated connections\n",
               get_script_name());
        exit(1);
    }

    static log(string str) {
        stdout.printf("%y: %s\n", now_us(), str);
    }
}
