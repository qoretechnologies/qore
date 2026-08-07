#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

class FtpServer {
    public {
        #! Certificate and key used for FTPS (AUTH TLS) and for PROT P data connections
        /** self-signed; the same fixture certificate the socket and HTTP tests use
        */
        const TestCert = "-----BEGIN PRIVATE KEY-----
MIIJQwIBADANBgkqhkiG9w0BAQEFAASCCS0wggkpAgEAAoICAQDmlWxmDH8RxGqt
Fl7rRvA73hzbMOpsppnYuOXfZtAg+jO2/HPtCre7ssjLbk6h1fUnSdsyTu5hAFYL
Jmib3i6p97yox8PHr97XB9PuE3TfQ7ecptHYoZC63H5HVuXkq3S13TRmN6NvgOsH
QOtshleyybc+n/t7WXjKKgFUS0v//28q/JKENUop5hBmVMWiSS1CGxpvDUgUk8Vt
gnbQthrB6FRdRmAQ8McfMfYDxDPKgJ8lZWJf2dKFOBki2pralJ5ZsV00UsXYbXmF
R1Ehf5ozTK3kr/bBuPTxRtATYGlIuLQCKMsRH/UaPmli2VcDZHDsZYeHfG1YBwnq
Jy1zdelLryTvKGqeGCjm9MslWtWMzBVPxvG31qV3NO+9RV1cBVDRU8lF3Avjqzel
LB14K8TcloimsZG1IwjCp9rcejuQEP3BhKujnlTgWaI4YliEfQih0Dgk1oHyjINg
fGLvlbYyxVKDcA+1Qvq6f8aOOegq2MpQH3sGFOWT1iJPhmB9d91MFWbNSzGb4LO4
dFmGGPXmzKU2y1y5KycqTU2BdmqKOjnP+eC3RYUEAN0kXxyUnXsnc+D42R00+IY+
wBu1/wlPLsxxxNWuTHHZy1nxWP98G7uZs/OrTG50GOclyjC/sAgGveYQHKmha2+Q
lwB4RnmZeBhL3US1GVMFPjPqJg0ruQIDAQABAoICAGiZ7Fy22oYS1lnCxjhxK9Xc
wrZkkFPNmAkKh606lLwBBKjECUrFC+hoCFLQrXcY+mdiR7+gst7qKh5QxC/BMOYF
7l2akaPqkmp1sJxV0zTdIVQ9EQD2+hLqp5ZxQ0TiysUdngGox8lzcCsQNjw7DVmb
zDbqmEiyo+sB9yxoJAfztliHz2eOTSCIr3PcjPM7wv40EV/DZKmBB5TQFFpF4gmI
/Yx6umW09H+gSHwY6223v3pVbbURuUCZ3D1Fce4qcVM69I8h5YJy1yew3IsXubn/
1PrjIEttPiCeARJpMBm/QFmia8xO9P98xI4FDkYJsHhs7nh155yBROwl7aZtHPFO
AnDHHr3bymHAxvIfOuiv3RJNxEWsFE+i4axM6s+FEIPNv4Jbp6bP+ctNqeJxK0NJ
KBg+HRzjQa2rZVD8U1BDp+zdR5P35Ce8JwZRx3LtP4Fljiqloum9DTTQ6NXEtEmk
xxOM1szVCGiH4SNoUicxKTyNhZT8ktaU98ZP1rN4TI45s4F8vH0g/PJ3jWs56HCz
CR3qrwfOQg7XiBFUnyRoa1hv07AsTY3JZsxQpHxhRV6Sctztpfqotxck6eLkNX5M
PS+Ff65cnFecWG5mCtACgdLAWmfWGtJey7KLFQD9KK+iqaNPSuCSUyeZd/NReWTu
D3uydFsSuv0twZmvHbJpAoIBAQD21XnrxZMmXrsg55Xhz7kYt+VuQM/WrqiLqfRs
gT8Qu53HIAr0dWYmDpex5jTgEWfwPeHeOKNL4rIqQgpVykbVHsum2wxltdKxt+wq
rCYaC/208W1X/b3ra8VfIhlIbQDD5sDExA3lbvrqkjrjhANV2VU96PhDNOFBJM7M
DfnpO5gZe9Ts+bHylWqFXcKLKhoGQTbwevOoEJbhXxJn9OgXvCdrmfAYUQedxmBV
yN+gMLSxkCtOarokMdMRBuvq22fYZ2Tfflf8Se+zE+ll89UF/QbPri7IEimKo71s
FtsQXdCInKLpj5QfgIoANalkL/G9jqIsYMqJure0fjzMFiN3AoIBAQDvJXb65YMh
QCS9Vvjm+WTE7t49syH3BrSPZPZ6u97SGtEXSLx4bASGIwOYypN4f0C9aKu7WLMM
mY/niT/LtsU4s1VeD4rX50RZaY/OOiGfZZEruggB1dvefxv3OxiyA9LP79kGCI+e
QRQ5jbZMO/aSzH1hXw5nPqDqC4KgIvzxrFduwROt9HUr+lmspYc/2o27IRXHp4YO
bgkDv+2LLLmlZRNjjGe79K2kMvFeYLwn3WtWjB6iWhh4dFD9hFpccCap8QNMKU4L
UJt41cYbXkO4yIjH8vVkOB8kqNTtHRznFf7LYT6OKxXYbiyF4mmKFuZD5OfCJVzV
ooiIbbZwNBZPAoIBAEy1dSlngl6HTCJitjqyAnUDGYMVE0JFj02wKKzb0cyScesj
MyRbo74qBMydnUsRPbr0kFAuS9lWf0YQtgOJVFABPQsCwiyNwAfI/9UXINQxa8wi
x3dz/OS/3lu9Thwq+/Tlrny37RWjUsk5XqG8Mgko/Iu77gN82JAVL0tNAKCSC6fH
yxlGVB3uf2cpBz+WevX8qGU94KEXXrx1flPeUY6mw511/p6f9/z5XJDsV0sCN/WC
t2KsWmLLNSvyebjILNKc90VZj2BxXdPKWVt81nQA+W/rUtCMjGpwjnqb88J3VUpP
W3W4UDEmEj0UcCsZl0fwnsep4OfCpjqqjJxkOOsCggEBANmMOVowI0PCOCnNzos/
YD4OFjoGlOVFQkgXzmkEMsTrBD6sOldJ3Cux+nmp/tFk4Z3sdBzvqaHSqCg2oMRO
tcbIzOXOOb4CchjsgJdYJnH8UT139ATHaUZUVzpmliaKd+IVDFYCv7N0/FMG6Ukc
W0hboctMT6v4OyXt5pyKd5PZ9YeZOK/MbDLfgWgfBJQ/2yLaDBgd+0NxMd4pYIx0
2UTuV979QPiosrVmalAyq7eQIy8ONGf4kb4yAeM93X2twF5kE13bTVXZtDUOMzN+
YxjbxCIwkK7kpuAQffJdw4csaSbWhbOK2kqSnwoEV4H5JuuodnQUzQjC3HVPaOFV
YN0CggEBAJFHixUC5JNQcDl8AsBuj709yqCQQB+JERVm6VtN26GgkEzuh39owcpb
LWQ2vSl6HqB/77SF/PeAXyGcLupxnsAz8XBt6Oi9Gtw3IV7Bh88OUMat1h6l4yTd
3yqkiU3qOhtp2a44SSTCulFhUUAsfRN77npSrjxP2GhAj93LJsMs6y729tXJCMM+
um91nJDkMgHEUNUc7tcMdUfof6WDvBklmRiAnR3OTpCYKHneAxbdft36YkOYUhiQ
OCcIkE7cUllbNX7KIhRfIuG4ETlpOQzDxQIdzj0ZeO8B9d1ULMRnB8VccqdVUE0l
WkEP9QjmvU4tYxLvhP6peBr7PhLJ66A=
-----END PRIVATE KEY-----
-----BEGIN CERTIFICATE-----
MIIFhTCCA22gAwIBAgIUWg3AaxJ1kKrhwH7TTcGh3Z0yPEwwDQYJKoZIhvcNAQEL
BQAwUTELMAkGA1UEBhMCQ1oxEzARBgNVBAgMClNvbWUtU3RhdGUxDzANBgNVBAcM
BlByYWd1ZTENMAsGA1UECgwEUW9yZTENMAsGA1UEAwwEUW9yZTAgFw0yMDAxMzEw
NjEyMzVaGA8yMTIwMDEwNzA2MTIzNVowUTELMAkGA1UEBhMCQ1oxEzARBgNVBAgM
ClNvbWUtU3RhdGUxDzANBgNVBAcMBlByYWd1ZTENMAsGA1UECgwEUW9yZTENMAsG
A1UEAwwEUW9yZTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAOaVbGYM
fxHEaq0WXutG8DveHNsw6mymmdi45d9m0CD6M7b8c+0Kt7uyyMtuTqHV9SdJ2zJO
7mEAVgsmaJveLqn3vKjHw8ev3tcH0+4TdN9Dt5ym0dihkLrcfkdW5eSrdLXdNGY3
o2+A6wdA62yGV7LJtz6f+3tZeMoqAVRLS///byr8koQ1SinmEGZUxaJJLUIbGm8N
SBSTxW2CdtC2GsHoVF1GYBDwxx8x9gPEM8qAnyVlYl/Z0oU4GSLamtqUnlmxXTRS
xdhteYVHUSF/mjNMreSv9sG49PFG0BNgaUi4tAIoyxEf9Ro+aWLZVwNkcOxlh4d8
bVgHCeonLXN16UuvJO8oap4YKOb0yyVa1YzMFU/G8bfWpXc0771FXVwFUNFTyUXc
C+OrN6UsHXgrxNyWiKaxkbUjCMKn2tx6O5AQ/cGEq6OeVOBZojhiWIR9CKHQOCTW
gfKMg2B8Yu+VtjLFUoNwD7VC+rp/xo456CrYylAfewYU5ZPWIk+GYH133UwVZs1L
MZvgs7h0WYYY9ebMpTbLXLkrJypNTYF2aoo6Oc/54LdFhQQA3SRfHJSdeydz4PjZ
HTT4hj7AG7X/CU8uzHHE1a5McdnLWfFY/3wbu5mz86tMbnQY5yXKML+wCAa95hAc
qaFrb5CXAHhGeZl4GEvdRLUZUwU+M+omDSu5AgMBAAGjUzBRMB0GA1UdDgQWBBTS
O+Koh2q4QJgVnFud2cW0n2hxjzAfBgNVHSMEGDAWgBTSO+Koh2q4QJgVnFud2cW0
n2hxjzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQC0n27EM59I
5fa6vuSvSFpD0yV0YLsPD6P6Qr+WnosgZD1E7Vs84OT1p/QmHHMbzFzAlzRN050C
FvUNZNYeGGmj+sPrXhc15FI7GKYY5HrF75YQAPfZ0p4kB6VZQA/Y+5t/7dM7tUUQ
KuIP4KKpSdFrnEH2LQ9xO8LwBRum/2yhOU00GuO/M2wQFmDlWnDE4OphpJCW5zHd
+vDWcvnhFr30IHyBNMxau5ad8vnceHX1WmwQW95zeGQ8sQNxtCjRa/05KnjgnPtj
Dk9FCsB2Iw35Ebfk6gO9K78cKxAYGcWyKaHfN0Okt0uyLZAd88ogAspM2DB0VbGe
DnunO/56PjlwRb2oFVltRGioo1HnLJCtUOAaFc1KOlQ/4xqWlSoQ9a1oUtjRrZjP
H960K/rqBYhDQi/TrdpIRSQheS7zIR+quqcSJPSqxn4r/TEVx/8APm0FSF8p0T56
Ps1yVuAUWSVAJ0JivVlJTnG6aQ1h1xHCDnvW5PSG0bL0sqeSYZ4Gt55LWaqWToo9
qibte1W+dyK5IZEpk3f081N6GegIv+ulzcdcedp4oRCtaOUBziqSlZqXXkoiFyQ4
P1CvlJwL79QntIvjfC5nRMHwMy2wE6Eqt57sFlUnvD25CbbX/TNDSNqXhXQbYaCZ
I45SPdad6fpziP1EtT5rfrC0ZH7cMU8edg==
-----END CERTIFICATE-----";
    }

    private {
        Socket s();
        Counter cnt();
        bool quit;
        int verbose;
        hash broken;
        bool get_empty;

        TmpDir t;

        string pwd = "/";

        bool direct;

        #! if True, EPSV is refused so clients must fall back to PASV
        bool no_epsv;

        const PollInterval = 250ms;
    }

    private bool isSocketDisconnectError(hash<ExceptionInfo> ex, string io_err) {
        if (ex.err == "SOCKET-NOT-OPEN" || ex.err == "SOCKET-CLOSED") {
            return True;
        }
        if (ex.err != io_err) {
            return False;
        }
        return ex.arg == ECONNRESET || ex.arg == ECONNABORTED || ex.arg == EPIPE;
    }

    constructor(int port, int verbose = 0, string address = "localhost") {
        self.verbose = verbose;
        s.bindINET(address, port.toString(), True);
        if (s.listen()) {
            throw "FTPSERVER-ERROR", strerror();
        }
        if (verbose > 1) {
            printf("FTP server bound and listening on %s:%d\n", address, s.getSocketInfo().port);
        }
        cnt.inc();
        background listener();
    }

    setFs() {
        t = new TmpDir();
    }

    setDirect() {
        direct = True;
    }

    #! Refuse EPSV, so that clients preferring it must fall back to PASV
    /** Real servers that predate RFC 2428 answer EPSV with a 5xx; this makes the fallback reachable.
    */
    setNoEpsv(bool v = True) {
        no_epsv = v;
    }

    *string getLocalDir() {
        if (t) {
            return t.path;
        }
    }

    shutdown() {
        quit = True;
        cnt.waitForZero();
    }

    private listener() {
        on_exit cnt.dec();

        while (!quit) {
            *Socket ns = s.accept(PollInterval);
            if (ns) {
                cnt.inc();
                background connectionThread(ns);
            }
        }
    }

    private ftpSend(Socket ns, int code, string msg) {
        string str = sprintf("%d %s\r\n", code, vsprintf(msg, argv));
        if (verbose > 2)
            printf("FTP > %s", str);
        try {
            ns.send(str);
        } catch (hash<ExceptionInfo> ex) {
            # Client disconnected before we could reply — swallow the same
            # errors ftpGetCommand() treats as clean shutdown.  Prevents
            # an unhandled background-thread exception when the test races
            # the client-side close against a pending server reply.
            if (isSocketDisconnectError(ex, "SOCKET-SEND-ERROR")) {
                return;
            }
            rethrow;
        }
    }

    private string ftpGetCommand(Socket ns) {
        string str;
        while (True) {
            if (quit)
                return "";
            try {
                string c = ns.recv(1, PollInterval);
                if (c == "")
                    return "";
                str += c;
                if (c == "\n")
                    break;
            } catch (hash<ExceptionInfo> ex) {
                if (isSocketDisconnectError(ex, "SOCKET-RECV-ERROR"))
                    return "";
                if (ex.err != "SOCKET-TIMEOUT")
                    rethrow;
            }
        }
        trim str;
        if (verbose > 2)
            printf("FTP < %s\n", str);
        return str;
    }

    private connectionThread(Socket ns) {
        on_exit cnt.dec();

        #! True once PROT P has been negotiated, so data connections must be TLS too
        bool prot_p;

        # output banner
        ftpSend(ns, 220, "Qore FtpClient Test Fake FTP Server");

        # wait for user; an FTPS client secures the control channel with AUTH TLS and negotiates data
        # channel protection with PBSZ/PROT first, all before it logs in.  A plaintext client simply
        # never sends any of them, which is why they need no server-side switch.
        while (True) {
            string cmd = ftpGetCommand(ns);
            if (quit || cmd == "")
                return;
            if (cmd =~ /^AUTH /i) {
                *string arg = (cmd =~ x/^AUTH (.*)$/i)[0];
                if (arg !~ /^(TLS|TLS-C|SSL)$/i) {
                    ftpSend(ns, 504, "Auth type %y not supported.", arg);
                    continue;
                }
                # the 234 goes out in the clear; the handshake follows immediately on the same socket
                ftpSend(ns, 234, "Proceed with negotiation.");
                try {
                    ns.setCertificate(TestCert);
                    ns.setPrivateKey(TestCert);
                    ns.upgradeServerToSSL();
                } catch (hash<ExceptionInfo> ex) {
                    if (isSocketDisconnectError(ex, "SOCKET-SSL-ERROR")) {
                        return;
                    }
                    rethrow;
                }
                continue;
            }
            if (cmd =~ /^PBSZ /i) {
                # only PBSZ 0 is meaningful over TLS
                ftpSend(ns, 200, "PBSZ set to 0.");
                continue;
            }
            if (cmd =~ /^PROT /i) {
                *string arg = (cmd =~ x/^PROT (.*)$/i)[0];
                switch (arg.upr()) {
                    case "P":
                        prot_p = True;
                        ftpSend(ns, 200, "PROT now Private.");
                        break;
                    case "C":
                        prot_p = False;
                        ftpSend(ns, 200, "PROT now Clear.");
                        break;
                    default:
                        ftpSend(ns, 504, "PROT level %y not supported.", arg);
                        break;
                }
                continue;
            }
            if (cmd !~ /^USER \w+/i) {
                ftpSend(ns, 530, "Please login with USER and PASS.");
                continue;
            }
            break;
        }
        ftpSend(ns, 331, "Please specify the password.");
        # wait for pass
        while (True) {
            string cmd = ftpGetCommand(ns);
            if (quit || cmd == "")
                return;
            if (cmd !~ /^PASS \w+/i) {
                ftpSend(ns, 530, "Please login with USER and PASS.");
                continue;
            }
            break;
        }
        ftpSend(ns, 230, "Login successful.");

        Socket data();
        Counter dcnt();
        bool dataquit;

        # port data connection pending
        hash<auto> port_info;

        while (True) {
            string cmd = ftpGetCommand(ns);
            if (quit || cmd == "")
                break;

            if (cmd =~ /^EPSV$/i) {
                if (no_epsv) {
                    ftpSend(ns, 502, "Command not implemented.");
                    continue;
                }
                remove port_info;
                termWaitData(data, dcnt, \dataquit);
                doEpsv(ns, \data, dcnt, \dataquit, prot_p);
                continue;
            }

            if (cmd =~ /^PASV$/i) {
                remove port_info;
                termWaitData(data, dcnt, \dataquit);
                doPasv(ns, \data, dcnt, \dataquit, prot_p);
                continue;
            }

            if (cmd =~ /^PORT .+$/i) {
                string arg = (cmd =~ x/PORT (.+)$/i)[0];
                termWaitData(data, dcnt, \dataquit);
                doPort(ns, arg, \port_info);
                continue;
            }

            if (cmd =~ /^QUIT$/i) {
                ftpSend(ns, 221, "Goodbye.");
                break;
            }

            if (cmd =~ /^TYPE /i) {
                *string arg = (cmd =~ x/^TYPE (.*)$/i)[0];
                if (arg == "A")
                    ftpSend(ns, 200, "Switching to Text mode.");
                else if (arg != "I")
                    ftpSend(ns, 500, "Unrecognised TYPE command %y.", arg);
                else
                    ftpSend(ns, 200, "Switching to Binary mode.");
                continue;
            }

            if (cmd =~ /^STOR .+$/i) {
                string file = (cmd =~ x/^STOR (.+)$/i)[0];
                if (!data.isOpen()) {
                    if (port_info) {
                        if (broken.PORT) {
                            continue;
                        }
                        # connect to the remote socket
                        data.connectINET(port_info.ip, port_info.port, 10s);
                    } else {
                        ftpSend(ns, 425, "Use EPASV first.");
                        continue;
                    }
                }
                # Poll for data-accept completion; abort on shutdown so
                # serv.shutdown() doesn't hang when the client never
                # opens the data connection (e.g. after a failed put).
                while (dcnt.getCount() > 0) {
                    if (quit) {
                        dataquit = True;
                    }
                    dcnt.waitForZero(PollInterval);
                }
                if (quit) {
                    return;
                }
                ftpSend(ns, 150, "Opening BINARY mode data connection for %s.", file);

                if (doBroken("stor", data))
                    break;

                OutputStream f;
                if (t) {
                    f = new FileOutputStream(t.path + DirSep + file);
                } else {
                    f = new BinaryOutputStream();
                }
                # read all data from data connection until closed
                data.recvToOutputStream(f, -1);
                data.close();
                ftpSend(ns, 226, "Transfer completed.");
                continue;
            }

            if (cmd =~ /^RETR .+$/i) {
                string file = (cmd =~ x/^RETR (.+)$/i)[0];
                if (!data.isOpen()) {
                    ftpSend(ns, 425, "Use EPASV first.");
                    continue;
                }

                *string send_path = resolveLocalPath(file);
                if (!send_path && (t || direct)) {
                    # a filesystem-backed server refuses a file it cannot open *before* the transfer
                    # starts, and never sends the 150; without a local filesystem configured the server
                    # has no way to know a name is bad, so it serves canned data for any name
                    ftpSend(ns, 550, "Failed to open file %y.", file);
                    # the data channel advertised for this transfer will never carry anything: retire it
                    # here, so the next EPSV/PASV binds a fresh socket and no accept thread is left behind
                    termWaitData(data, dcnt, \dataquit);
                    if (data.isOpen()) {
                        data.close();
                    }
                    continue;
                }

                while (dcnt.getCount() > 0) {
                    if (quit) {
                        dataquit = True;
                    }
                    dcnt.waitForZero(PollInterval);
                }
                if (quit) {
                    return;
                }
                ftpSend(ns, 150, "Opening BINARY mode data connection for %s.", file);

                if (doBroken("retr", data))
                    break;

                if (send_path) {
                    data.sendFromInputStream(new FileInputStream(send_path));
                } else if (!get_empty) {
                    # write a bunch of data and then exit
                    for (int i = 0; i < 200; ++i)
                        data.send("testing abcdefghijklmnopqrstuvwxyz 0123456789\n");
                }

                data.close();
                ftpSend(ns, 226, "Transfer completed.");
                continue;
            }

            if (cmd =~ /^(LIST|NLST)/i) {
                # a real server answers both: LIST with a long listing, NLST with bare names
                bool long_list = cmd =~ /^LIST/i;
                if (!data.isOpen()) {
                    ftpSend(ns, 425, "Use EPASV first.");
                    continue;
                }
                while (dcnt.getCount() > 0) {
                    if (quit) {
                        dataquit = True;
                    }
                    dcnt.waitForZero(PollInterval);
                }
                if (quit) {
                    return;
                }
                ftpSend(ns, 150, "Here comes the directory listing.");

                if (doBroken(long_list ? "list" : "nlst", data))
                    break;

                #printf("get: empty %y\n", get_empty);
                if (!get_empty && !pwd) {
                    # write a bunch of data and then exit
                    for (int i = 0; i < 200; ++i)
                        data.send(sprintf("drwxr-xr-x    2 0        0            4096 Jul 08  2016 pub_%s\n", get_random_string(5)));
                } else if (pwd) {
                    Dir dir();
                    dir.chdir(pwd);
                    list<hash<DirStatInfo>> files = dir.listInfo(regex: ".*");
                    string result;
                    if (long_list) {
                        map result += sprintf("%s %4d %-8d %-8d %12d %s %s\r\n", $1.perm, $1.nlink, $1.uid,
                            $1.gid, $1.size, $1.mtime.format("MMM DD YYYY"), $1.name), files;
                    } else {
                        map result += $1.name + "\r\n", files;
                    }
                    data.send(result);
                }

                data.close();
                ftpSend(ns, 226, "Directory send OK.");
                continue;
            }

            if (cmd =~ /^PWD/i) {
                ftpSend(ns, 257, pwd ?? "/");
                continue;
            }

            if (cmd =~ /^CWD/i) {
                string dir = (cmd =~ x/^CWD (.+)$/i)[0];
                string path = getPath(dir);

                if (!is_dir(path)) {
                    ftpSend(ns, 550, "Failed to change directory to %y: %s", path, strerror());
                } else {
                    ftpSend(ns, 250, "Okay.");
                    pwd = dir;
                }
                continue;
            }

            if (cmd =~ /^MKD/i) {
                string dir = (cmd =~ x/^MKD (.+)$/i)[0];
                string path = getPath(dir);

                if (mkdir(path, 0755, True)) {
                    ftpSend(ns, 550, "Failed to make directory " + dir);
                } else {
                    ftpSend(ns, 250, "Okay.");
                }
                continue;
            }

            if (cmd =~ /^MDTM/i) {
                string file_name = (cmd =~ x/^MDTM (.+)$/i)[0];
                # printf("FILE_NAME: %y\n", file_name);
                string path = getPath(file_name);

                *hash<StatInfo> info = hstat(path);
                if (!info) {
                    ftpSend(ns, 550, "Failed to retrieve modified date for %s: %s", path, strerror());
                } else {
                    ftpSend(ns, 250, info.mtime.toString());
                }
                continue;
            }

            if (cmd =~ /^SIZE/i) {
                string file_name = (cmd =~ x/^SIZE (.+)$/i)[0];
                # printf("FILE_NAME: %y\n", file_name);
                string path = getPath(file_name);

                *hash<StatInfo> info = hstat(path);
                if (!info) {
                    ftpSend(ns, 550, "Failed to retrieve size for " + file_name);
                } else {
                    ftpSend(ns, 250, info.size.toString());
                }
                continue;
            }

            if (cmd =~ /^DELE/i) {
                string file_name = (cmd =~ x/^DELE (.+)$/i)[0];
                # printf("FILE_NAME: %y\n", file_name);
                string path = getPath(file_name);

                int rc = unlink(path);
                if (rc) {
                    ftpSend(ns, 550, "Failed to delete " + file_name);
                } else {
                    ftpSend(ns, 250, "Okay.");
                }
                continue;
            }

            ftpSend(ns, 500, "Unknown command: " + cmd);
        }

        # ensure any data connections terminate
        termWaitData(data, dcnt, \dataquit);
    }

    #! Returns the local path a transfer command should read, or nothing if there is no such file
    /** Checks the configured temporary directory first, then the process working directory in direct
        mode, mirroring the order the transfer handlers used before they shared this lookup.
    */
    private *string resolveLocalPath(string file) {
        if (t) {
            string path = t.path + DirSep + file;
            if (hstat(path)) {
                return path;
            }
        }
        if (direct && hstat(file)) {
            return file;
        }
    }

    #! Returns the target path
    private string getPath(string p) {
        if (absolute_path(p)) {
            return t ? t.path + DirSep + p : p;
        }
        return (t ? t.path : pwd) + DirSep + p;
    }

    private bool doBroken(string cmd, Socket data) {
        if (broken{cmd}) {
            usleep(broken{cmd});
            data.close();
            return True;
        }
        return False;
    }

    private termWaitData(Socket data, Counter dcnt, reference dataquit) {
        if (data.isOpen()) {
            dataquit = True;
            dcnt.waitForZero();
            dataquit = False;
        }
    }

    private doEpsv(Socket ns, reference data, Counter dcnt, reference dataquit, bool secure_data = False) {
        # get the local address and family from the control connection to bind the data socket
        # to the same address family; this prevents a mismatch where the control connection is
        # IPv6 but the data socket is IPv4-only, which would cause the client to connect to the
        # wrong service if another process happens to be listening on the same port over IPv6
        hash<auto> ctrl_info = ns.getSocketInfo();
        string data_host = ctrl_info.address;
        int data_family = ctrl_info.family;
        # Let the kernel select an unused ephemeral port.  Guessing from a broad
        # range can select a Kubernetes NodePort: bind() succeeds locally, but
        # the client's loopback connection is then redirected by OUTPUT DNAT.
        data.bindINET(data_host, 0, True, data_family);
        int dport = data.getSocketInfo().port;
        data.listen();
        ftpSend(ns, 229, "Entering Extended Passive Mode (|||%d|)", dport);

        dcnt.inc();
        background doDataConnection(\data, dcnt, \dataquit, secure_data);
    }

    private doPasv(Socket ns, reference data, Counter dcnt, reference dataquit, bool secure_data = False) {
        # bind on random port; NOTE: PASV only supports IPv4, so we can't bind on "localhost",
        # which might resolve to an IPv6 address
        data.bindINET("127.0.0.1", 0, True);
        data.listen();
        int dport = data.getSocketInfo().port;
        {
            int dport_high = dport >> 8;
            int dport_low = (dport & 0xff);
            ftpSend(ns, 227, "Entering Passive Mode (127,0,0,1,%d,%d)", dport_high, dport_low);
        }

        dcnt.inc();
        background doDataConnection(\data, dcnt, \dataquit, secure_data);
    }

    private doPort(Socket ns, string arg, reference<hash> port_info) {
        # NOTE: PORT only supports IPv4; EPRT (RFC 2428: https://tools.ietf.org/html/rfc2428)
        # adds support for IPv6
        list<string> l = arg.split(",");
        port_info = {
            "ip": l[0..3].join("."),
            "port": (l[4] << 8) + l[5].toInt(),
        };

        #printf("arg: %y ip: %y port: %y\n", arg, ip, port);

        # send reply immediately
        ftpSend(ns, 200, "PORT command successful");
    }

    private doDataConnection(reference data, Counter dcnt, reference dataquit, bool secure_data = False) {
        on_exit dcnt.dec();

        while (!dataquit) {
            *Socket ns = data.accept(PollInterval);
            if (ns) {
                if (secure_data) {
                    # PROT P was negotiated: the client upgrades the data connection as soon as it
                    # connects, so the server has to meet it there or both sides wait for each other
                    try {
                        ns.setCertificate(TestCert);
                        ns.setPrivateKey(TestCert);
                        ns.upgradeServerToSSL();
                    } catch (hash<ExceptionInfo> ex) {
                        # a client that gave up on the transfer leaves a half-open data connection;
                        # treat it as no connection rather than killing the connection thread
                        if (isSocketDisconnectError(ex, "SOCKET-SSL-ERROR")) {
                            return;
                        }
                        rethrow;
                    }
                }
                data = ns;
                return;
            }
        }
    }

    int getPort() {
        return s.getSocketInfo().port;
    }

    setBroken(string cmd, timeout v) {
        broken{cmd} = v;
    }

    clearBroken() {
        delete broken;
    }

    setGetEmpty(bool v = True) {
        get_empty = v;
    }

    setPwd(string pwd) {
        self.pwd = pwd;
    }
}
