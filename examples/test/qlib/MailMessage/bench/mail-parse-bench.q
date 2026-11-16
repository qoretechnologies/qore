#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a benchmark of the time to parse messages and to decode headers and quoted-printable text with the MailMessage and
# Mime modules (issue #6139); it checks nothing, and it is not run by run_tests.sh, since the time depends on the
# load of the machine: the tests check the work that grows with the input by counting it (see Mime::MimeScanCounter)
#
# usage: mail-parse-bench.q [-s <sizes>] [-n <repeat>] [<case>...]
#   -s <sizes>  comma-separated numbers of lines, encoded words or codes of the large inputs (default:
#               12500,25000,50000,100000); "none" for the small inputs only
#   -n <repeat> the number of times that each small input is parsed or decoded (default: 20000)
#   <case>      the large inputs to time (default: all); "small" for the small inputs only

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires MailMessage
%requires Mime

%exec-class MailParseBench

class MailParseBench {
    private {
        const Opts = {
            "sizes": "s,sizes=s",
            "repeat": "n,repeat=i",
            "help": "h,help",
        };

        # the large inputs: a message with the given number of lines (or encoded words, or codes) and the code that
        # parses or decodes it
        const Cases = {
            "folded": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com%s\r\nSubject: s\r\n\r\nbody\r\n",
                    strmul(",\r\n a@b.com", n));
            },
            "folded non-ascii": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com%s\r\nSubject: s\r\n\r\nbody\r\n", strmul(",\r\n ö", n));
            },
            "separate": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com\r\n%s\r\nbody\r\n", strmul("X-H: o\r\n", n));
            },
            "separate non-ascii": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com\r\n%s\r\nbody\r\n", strmul("X-H: ö\r\n", n));
            },
            "separate iso-8859-1": string sub (int n) {
                return convert_encoding(sprintf("From: a@x.com\r\nTo: z@y.com\r\n%s\r\nbody\r\n",
                    strmul("X-H: ö\r\n", n)), "ISO-8859-1");
            },
            "separate shift_jis": string sub (int n) {
                return convert_encoding(sprintf("From: a@x.com\r\nTo: z@y.com\r\n%s\r\nbody\r\n",
                    strmul("X-H: ポ\r\n", n)), "SHIFT_JIS");
            },
            "separate utf-16": string sub (int n) {
                return convert_encoding(sprintf("From: a@x.com\r\nTo: z@y.com\r\n%s\r\nbody\r\n",
                    strmul("X-H: ポ\r\n", n)), "UTF-16");
            },
            "encoded words folded": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com\r\nSubject: s%s\r\n\r\nbody\r\n",
                    strmul("\r\n =?UTF-8?Q?=C3=B6?=", n));
            },
            "encoded words": string sub (int n) {
                return sprintf("From: a@x.com\r\nTo: z@y.com\r\nSubject: s%s\r\n\r\nbody\r\n",
                    strmul(" =?UTF-8?Q?=C3=B6?= x", n));
            },
            "quoted-printable body": string sub (int n) {
                return "From: a@x.com\r\nTo: z@y.com\r\nContent-Transfer-Encoding: quoted-printable\r\n"
                    "Content-Type: text/plain; charset=UTF-8\r\n\r\n" + strmul("=C3=B6", n);
            },
            "multipart": string sub (int n) {
                return "From: a@x.com\r\nTo: z@y.com\r\nMIME-Version: 1.0\r\n"
                    "Content-Type: multipart/mixed; boundary=\"BB\"\r\n\r\n"
                    + strmul("--BB\r\nContent-Type: text/plain; charset=UTF-8\r\nX-A: =?UTF-8?Q?=C3=B6?=\r\n\r\n"
                        "hello ö\r\n", n / 4)
                    + "--BB--\r\n";
            },
        };

        # the small inputs
        const SmallMessage = "From: \"Jörg\" <j@x.com>\r\nTo: a@x.com,\r\n b@x.com,\r\n\tö <c@x.com>\r\nCC: d@x.com\r\n"
            "Subject: =?UTF-8?Q?a_b?=\r\n =?UTF-8?B?w7Y=?= c\r\nX-A:   ä  \r\nX-B: one\r\n \r\n two\r\n"
            "Date: Sun, 15 Nov 2026 10:00:00 +0100\r\nMessage-ID: <abc@x.com>\r\nMIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n"
            "Hello J=C3=B6rg,\r\nthis is a short message with a soft=\r\n break.\r\n";
        const PlainMessage = "From: a@x.com\r\nTo: b@y.com\r\nSubject: hello\r\nX-A: b\r\nX-B: c\r\nX-C: d\r\n"
            "X-D: e\r\n\r\nbody\r\n";
    }

    constructor() {
        GetOpt g(Opts);
        hash<auto> opts = g.parse3(\ARGV);
        if (opts.help) {
            printf("usage: %s [-s <sizes>] [-n <repeat>] [<case>...]\ncases: small, %s\n", get_script_name(),
                keys Cases);
            return;
        }
        list<string> cases = ARGV ? ARGV : ("small",) + keys Cases;
        if (inlist("small", cases)) {
            small(opts.repeat ?? 20000);
        }
        if (opts.sizes == "none") {
            return;
        }
        list<int> sizes = map $1.toInt(), (opts.sizes ?? "12500,25000,50000,100000").split(",");
        foreach string name in (cases) {
            if (name == "small") {
                continue;
            }
            if (!Cases{name}) {
                stderr.printf("unknown case %y\n", name);
                continue;
            }
            # once with a small input first, so that the code is compiled before it is timed
            new MailMessage::Message(Cases{name}(100));
            *float last;
            foreach int n in (sizes) {
                string msg = Cases{name}(n);
                date start = now_us();
                new MailMessage::Message(msg);
                float s = (now_us() - start).durationSecondsFloat();
                printf("%-22s n=%-7d %9d bytes %8.3f s%s\n", name, n, msg.size(), s,
                    exists last ? sprintf("  x%.2f", s / last) : "");
                last = s;
            }
        }
    }

    # prints the time to parse or decode small inputs (the minimum of 3 rounds)
    private small(int repeat) {
        hash<string, code> inputs = {
            "message": sub () { new MailMessage::Message(SmallMessage); },
            "plain message": sub () { new MailMessage::Message(PlainMessage); },
            "header without word": sub () { mime_decode_header("plain text without words"); },
            "header with a word": sub () { mime_decode_header("=?UTF-8?Q?=C3=B6?="); },
            "header with words": sub () { mime_decode_header("=?UTF-8?Q?a_b?= =?UTF-8?B?w7Y=?= c plain text"); },
            "quoted-printable": sub () {
                mime_decode_quoted_printable("Hello J=C3=B6rg, this is a line=\r\n", "UTF-8", True);
            },
        };
        foreach hash<auto> i in (inputs.pairIterator()) {
            *float best;
            for (int r = 0; r < 3; ++r) {
                date start = now_us();
                for (int k = 0; k < repeat; ++k) {
                    i.value();
                }
                float us = (now_us() - start).durationMicroseconds() / float(repeat);
                if (!exists best || us < best) {
                    best = us;
                }
            }
            printf("%-22s %8.2f us\n", i.key, best);
        }
    }
}
