# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UrlDownloader.qc downloads URLs to files

/*  UrlDownloader.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace for the UrlDownloader module
public namespace UrlDownloader {
#! Downloads URLs to files or to an output stream
/** Downloads are made one at a time in the calling thread; connections to HTTP servers are kept open and reused by
    the following downloads from the same server.  A download in progress can be cancelled from another thread with
    @ref cancel().

    @par Example:
    @code{.py}
UrlDownloader dl(<UrlDownloadOptions>{"output_dir": "downloads", "clobber": False});
foreach string url in (urls) {
    hash<UrlDownloadResult> res = dl.download(url);
    printf("%s: %d bytes\n", res.file, res.size);
}
    @endcode
*/
public class UrlDownloader {
    public {
        #! the \c Accept-Encoding value sent when the \c compressed option is set
        const AcceptEncoding = "gzip, deflate, br, zstd";

        #! the file name used when a URL has no file name
        const DefaultFileName = "index.html";
    }

    private:internal {
        #! the options
        hash<UrlDownloadOptions> opts;

        #! the client certificate and key, if any
        *SSLCertificate client_cert;
        *SSLPrivateKey client_key;

        #! the connection managers by proxy URL ("" for none)
        hash<string, HttpClientConnectionManager> mgrs = {};

        #! protects the members below
        Mutex m();

        #! the ID of the thread running a download, if any
        *int tid;

        #! the stream of the download in progress, if any
        *HttpClientResponseStream stream;

        #! True once cancel() has been called
        bool cancelled = False;
    }

    #! Creates the object
    /** @param opts the options for downloads

        @throw DOWNLOAD-ERROR an option is invalid
        @throw DOWNLOAD-IO-ERROR the client certificate or key cannot be read
    */
    constructor(*hash<UrlDownloadOptions> opts) {
        self.opts = opts ?? <UrlDownloadOptions>{};
        if (exists self.opts.http_version && !UrlDownloader::getProtocol(self.opts.http_version)) {
            throw "DOWNLOAD-ERROR", sprintf("unsupported HTTP version %y; expecting \"1.1\", \"2\", or \"3\"",
                self.opts.http_version);
        }
        if (self.opts.max_redirects < 0) {
            throw "DOWNLOAD-ERROR", sprintf("invalid maximum number of redirects %d", self.opts.max_redirects);
        }
        if (self.opts.client_key && !self.opts.client_cert) {
            throw "DOWNLOAD-ERROR", "a client key requires a client certificate";
        }
        if (self.opts.client_cert) {
            try {
                string cert_pem = ReadOnlyFile::readTextFile(self.opts.client_cert);
                client_cert = new SSLCertificate(cert_pem);
                client_key = new SSLPrivateKey(self.opts.client_key
                    ? ReadOnlyFile::readTextFile(self.opts.client_key)
                    : cert_pem);
            } catch (hash<ExceptionInfo> ex) {
                throw "DOWNLOAD-IO-ERROR", sprintf("cannot load the client certificate: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Closes all connections
    destructor() {
        foreach HttpClientConnectionManager mgr in (mgrs.iterator()) {
            mgr.closeAll();
        }
    }

    #! Downloads a URL
    /** A stream given with \a output is not closed

        @param url the URL to download: an \c http, \c https, \c ftp, or \c ftps URL, or the name of a connection
        of a supported type, optionally followed by a path (\c "name/path")
        @param listener receives the events of the download, if any
        @param output the stream to write the data to when the \c output option is \c "-"

        @return the result of the download

        @throw DOWNLOAD-* one of the error codes in @ref UrlDownloadErrors, with a @ref UrlDownloadResult hash as
        the exception argument, whose \c cause key has the original exception
    */
    hash<UrlDownloadResult> download(string url, *UrlDownloadListener listener, *OutputStream output) {
        hash<UrlDownloadResult> res(<UrlDownloadResult>{"url": url});
        int start = clock_getmicros();
        {
            AutoLock al(m);
            if (cancelled) {
                throw "DOWNLOAD-CANCELLED", sprintf("%s: the download was cancelled", url), res;
            }
            if (tid) {
                throw "DOWNLOAD-ERROR", "a download is already in progress", res;
            }
            tid = gettid();
        }

        # the output of the download
        DownloadTarget target();
        *hash<ExceptionInfo> err;
        try {
            downloadIntern(url, listener, output, \res, target);
            res.complete = True;
        } catch (hash<ExceptionInfo> ex) {
            err = ex;
        }
        # a cancellation is for this download only; it is cleared with cancellation deferred, as acquiring the lock
        # is a cancellation point
        defer_thread_cancel(sub () {
            AutoLock al(m);
            remove tid;
            remove stream;
            if (cancelled) {
                clear_thread_cancel();
            }
        });
        res.elapsed_us = clock_getmicros() - start;

        # the output is closed and moved to its final name, or its partial data is kept or removed
        try {
            target.finish(res.complete || target.isComplete(), keepPartialData(err), \res);
        } catch (hash<ExceptionInfo> ex) {
            if (!err) {
                res.complete = False;
                err = ex;
            }
        }
        if (err) {
            string code = get_download_error_code(err);
            if (code == "DOWNLOAD-CANCELLED") {
                err.desc = "the download was cancelled";
            }
            res.cause = err;
            throw code, sprintf("%s: %s", url, err.desc), res;
        }
        return res;
    }

    #! Cancels a download in progress, and all further downloads
    /** Can be called from any thread, for example from a signal handler; the download in progress fails with
        \c DOWNLOAD-CANCELLED, and so do all further calls to @ref download()
    */
    cancel() {
        AutoLock al(m);
        if (cancelled) {
            return;
        }
        cancelled = True;
        if (stream) {
            stream.cancel();
        }
        if (tid && tid != gettid()) {
            cancel_thread(tid, "download cancelled");
        }
    }

    #! Returns @ref True "True" if @ref cancel() has been called
    bool isCancelled() {
        return cancelled;
    }

    #! Returns the name of the file for a URL, or @ref NOTHING if it has none
    /** Only the last path component of the URL path is used, so the name can never select a file outside of the
        output directory; percent-encoded characters are decoded

        @param url the URL

        @return the file name, or @ref NOTHING if the URL path ends with a \c "/" or has no usable name
    */
    static *string getFileNameFromUrl(string url) {
        *string path = parse_url(url).path;
        # the query and fragment are not part of the name
        path =~ s/[?#].*$//s;
        if (!path || path =~ /\/$/) {
            return;
        }
        return UrlDownloader::getSafeFileName(decode_url(path));
    }

    #! Returns the last path component of a name, or @ref NOTHING if it has no usable name
    /** Directories are ignored, with both \c "/" and \c "\" separators, so the name can never select a file outside
        of a directory; control characters are not allowed

        @param name the name, for example from a URL or from a \c Content-Disposition header

        @return the file name, or @ref NOTHING if the name has no usable last component
    */
    static *string getSafeFileName(*string name) {
        if (!name) {
            return;
        }
        string fn = basename(replace(name, "\\", "/"));
        if (fn == "" || fn == "." || fn == ".." || fn =~ /[\x00-\x1f\x7f]/) {
            return;
        }
        return fn;
    }

    #! Returns the HttpClientIo protocol for an HTTP version option, or @ref NOTHING if it is not supported
    static *string getProtocol(string http_version) {
        switch (http_version) {
            case "1.1": return "h1";
            case "2": return "h2";
            case "3": return "h3";
        }
    }

    #! Makes a download
    private:internal downloadIntern(string url, *UrlDownloadListener listener, *OutputStream output,
            reference<hash<UrlDownloadResult>> res, DownloadTarget target) {
        if (url !~ /^[a-z][a-z0-9+.-]*:\/\//i) {
            url = UrlDownloader::getConnectionUrl(url);
        }
        hash<UrlInfo> info;
        try {
            info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "DOWNLOAD-URL-ERROR", ex.desc;
        }
        if (!info.host) {
            throw "DOWNLOAD-URL-ERROR", sprintf("URL %y has no host", url);
        }
        if (opts.output == "-" && !output) {
            throw "DOWNLOAD-ERROR", "no output stream given for output \"-\"";
        }
        # a named output file that exists is not downloaded at all
        if (opts.output && opts.output != "-" && !opts.clobber && is_file(opts.output)) {
            throw "DOWNLOAD-FILE-EXISTS", sprintf("%y already exists", opts.output);
        }
        switch (info.protocol.lwr()) {
            case "http":
            case "https":
                downloadHttp(url, info, listener, output, \res, target);
                break;
            case "ftp":
            case "ftps":
                downloadFtp(url, info, listener, output, \res, target);
                break;
            default:
                throw "DOWNLOAD-URL-ERROR", sprintf("unsupported URL scheme %y; expecting http, https, ftp, or ftps",
                    info.protocol);
        }
    }

    #! Makes an HTTP download
    private:internal downloadHttp(string url, hash<UrlInfo> info, *UrlDownloadListener listener,
            *OutputStream output, reference<hash<UrlDownloadResult>> res, DownloadTarget target) {
        int deadline = opts.max_time ? clock_getmillis() + opts.max_time : 0;
        hash<string, string> hdrs = {
            "User-Agent": opts.user_agent,
            "Accept": "*/*",
        };
        if (opts.compressed) {
            hdrs."Accept-Encoding" = AcceptEncoding;
        }
        if (opts.user) {
            hdrs.Authorization = "Basic " + make_base64_string(opts.user);
        } else if (exists info.username) {
            hdrs.Authorization = "Basic " + make_base64_string(sprintf("%s:%s", decode_url(info.username),
                decode_url(info.password ?? "")));
        }
        if (opts.headers) {
            hdrs += opts.headers;
        }

        HttpClientConnectionManager mgr = getManager(url);
        # the URL without user information
        string req_url = UrlDownloader::getUrlWithoutUserInfo(url, info);
        HttpClientResponseStream s = mgr.openStream(req_url, hdrs, <HttpClientOpenStreamOptions>{
            "decode": opts.compressed,
            "timeout": getWait(opts.read_timeout, deadline),
        });
        {
            AutoLock al(m);
            if (cancelled) {
                s.cancel();
                throw "DOWNLOAD-CANCELLED", "the download was cancelled";
            }
            stream = s;
        }
        res.effective_url = s.getEffectiveUrl();
        res.status_code = s.getStatusCode();
        res.status_message = s.getStatusMessage();
        res.protocol = s.getProtocol() ?? "h1";
        res.content_type = s.getHeaderValue("content-type");
        res.redirects = s.getRedirects();
        res.total = s.getContentLength();
        if (listener) {
            foreach hash<HttpRedirectInfo> r in (res.redirects) {
                listener.redirected(r);
            }
        }

        bool error_response = res.status_code >= 400;
        if (error_response && opts.fail_on_error && !opts.keep_error_body) {
            s.cancel();
            throwHttpError(res);
        }

        # the output file
        if (opts.output == "-") {
            target.setStream(output);
        } else {
            *string name = opts.output;
            if (!name) {
                if (opts.content_disposition) {
                    if (*string cd = s.getHeaderValue("content-disposition")) {
                        name = UrlDownloader::getSafeFileName(mime_get_content_disposition_filename(cd));
                    }
                }
                if (!name) {
                    name = UrlDownloader::getFileNameFromUrl(url) ?? DefaultFileName;
                }
                name = getOutputPath(name);
            }
            if (!opts.clobber && is_file(name)) {
                s.cancel();
                throw "DOWNLOAD-FILE-EXISTS", sprintf("%y already exists", name);
            }
            target.open(name, opts.part_file);
        }

        if (listener) {
            listener.started(<UrlDownloadStartInfo>{
                "url": url,
                "effective_url": res.effective_url,
                "file": target.getFileName(),
                "total": res.total,
                "status_code": res.status_code,
                "protocol": res.protocol,
                "content_type": res.content_type,
            });
        }

        while (*binary chunk = s.read(getWait(opts.read_timeout, deadline))) {
            target.write(chunk);
            res.size += chunk.size();
            res.received = s.getReceivedSize();
            if (listener) {
                listener.progress(res.received, res.total);
            }
        }
        res.received = s.getReceivedSize();
        if (exists res.total && res.received < res.total) {
            throw "DOWNLOAD-NETWORK-ERROR", sprintf("the response body ended after %d of %d bytes", res.received,
                res.total);
        }
        if (error_response && opts.fail_on_error) {
            # the error body is kept like a complete download
            target.setComplete();
            throwHttpError(res);
        }
    }

    #! Makes an FTP download
    private:internal downloadFtp(string url, hash<UrlInfo> info, *UrlDownloadListener listener,
            *OutputStream output, reference<hash<UrlDownloadResult>> res, DownloadTarget target) {
        int deadline = opts.max_time ? clock_getmillis() + opts.max_time : 0;
        string path = decode_url(info.path ?? "");
        if (!path || path =~ /\/$/) {
            throw "DOWNLOAD-URL-ERROR", sprintf("URL %y does not name a file", url);
        }
        FtpClient fc(UrlDownloader::getUrlWithoutUserInfo(url, info));
        if (opts.user) {
            (*string user, *string pass) = (opts.user =~ x/^([^:]*):?(.*)$/s);
            fc.setUserName(user ?? "");
            fc.setPassword(pass ?? "");
        } else if (exists info.username) {
            fc.setUserName(decode_url(info.username));
            fc.setPassword(decode_url(info.password ?? ""));
        }
        fc.setTimeout(getWait(min(opts.connect_timeout, opts.read_timeout), deadline));
        try {
            fc.connect();
        } catch (hash<ExceptionInfo> ex) {
            # 530: not logged in (RFC 959)
            if (ex.desc =~ /\b530\b/) {
                throw "DOWNLOAD-AUTH-ERROR", ex.desc;
            }
            rethrow;
        }
        on_exit {
            try {
                fc.disconnect();
            } catch () {
                # the connection is closed in any case
            }
        }
        fc.setTimeout(getWait(opts.read_timeout, deadline));
        res.effective_url = res.url;
        res.protocol = info.protocol.lwr();
        try {
            res.total = fc.size(path);
        } catch (hash<ExceptionInfo> ex) {
            # the SIZE command is optional
        }

        if (opts.output == "-") {
            target.setStream(output);
        } else {
            string name = opts.output ?? getOutputPath(UrlDownloader::getSafeFileName(path) ?? DefaultFileName);
            if (!opts.clobber && is_file(name)) {
                throw "DOWNLOAD-FILE-EXISTS", sprintf("%y already exists", name);
            }
            target.open(name, opts.part_file);
        }
        res.status_code = 226;
        if (listener) {
            listener.started(<UrlDownloadStartInfo>{
                "url": url,
                "effective_url": res.url,
                "file": target.getFileName(),
                "total": res.total,
                "status_code": 226,
                "protocol": res.protocol,
            });
        }
        DownloadProgressOutputStream os(target, res.total, listener);
        on_exit {
            res.size = res.received = os.getSize();
        }
        try {
            fc.get(path, os);
        } catch (hash<ExceptionInfo> ex) {
            # an error reply to the transfer command, such as 550 for a file that does not exist
            if (ex.err == "FTP-GET-ERROR" && ex.desc =~ /RETR failed: [45][0-9][0-9]/) {
                res.status_code = (ex.desc =~ x/RETR failed: ([45][0-9][0-9])/)[0].toInt();
                throw "DOWNLOAD-SERVER-ERROR", ex.desc;
            }
            rethrow;
        }
        res.size = res.received = os.getSize();
        if (exists res.total && res.received < res.total) {
            throw "DOWNLOAD-NETWORK-ERROR", sprintf("the transfer ended after %d of %d bytes", res.received,
                res.total);
        }
    }

    #! Returns the path of a file with a name from a URL or from a server in the output directory
    /** The output directory is created with any missing parent directories if it does not exist
    */
    private:internal string getOutputPath(string name) {
        if (!opts.output_dir) {
            return name;
        }
        if (!is_dir(opts.output_dir) && mkdir(opts.output_dir, 0777, True)) {
            throw "DOWNLOAD-IO-ERROR", sprintf("cannot create the output directory %y: %s", opts.output_dir,
                strerror());
        }
        return opts.output_dir + (opts.output_dir =~ /\/$/ ? "" : DirSep) + name;
    }

    #! Throws the error for an HTTP error response
    private:internal static throwHttpError(hash<UrlDownloadResult> res) {
        string code = (res.status_code == 401 || res.status_code == 407)
            ? "DOWNLOAD-AUTH-ERROR"
            : "DOWNLOAD-SERVER-ERROR";
        throw code, sprintf("the server returned HTTP %d%s", res.status_code,
            res.status_message ? " " + res.status_message : "");
    }

    #! Returns True if the partial data of a failed download is kept, so it can be resumed
    private:internal bool keepPartialData(*hash<ExceptionInfo> err) {
        if (!err) {
            return True;
        }
        string code = get_download_error_code(err);
        if (code == "DOWNLOAD-SERVER-ERROR" || code == "DOWNLOAD-AUTH-ERROR") {
            return False;
        }
        return code == "DOWNLOAD-NETWORK-ERROR" || code == "DOWNLOAD-TIMEOUT" || code == "DOWNLOAD-CANCELLED"
            || code == "DOWNLOAD-PROTOCOL-ERROR";
    }

    #! Returns the time to wait for data, limited by the deadline of the download
    private:internal timeout getWait(timeout wait, int deadline) {
        if (!deadline) {
            return wait;
        }
        int left = deadline - clock_getmillis();
        if (left <= 0) {
            throw "DOWNLOAD-TIMEOUT", sprintf("the maximum time of %d ms was exceeded", opts.max_time);
        }
        return min(wait, left);
    }

    #! Returns the connection manager for a URL
    private:internal HttpClientConnectionManager getManager(string url) {
        string proxy = opts.proxy ?? (opts.env_proxy ? get_proxy_url_from_environment(url) : NOTHING) ?? "";
        if (*HttpClientConnectionManager mgr = mgrs{proxy}) {
            return mgr;
        }
        hash<HttpClientConnectionManagerOptions> mopts({
            "connect_timeout": opts.connect_timeout,
            "request_timeout": opts.read_timeout,
            "max_redirects": opts.max_redirects,
            "protocol": exists opts.http_version ? UrlDownloader::getProtocol(opts.http_version) : "auto",
            "ssl_ca_file": opts.ca_file,
            "ssl_ca_path": opts.ca_path,
            "client_cert": client_cert,
            "client_pk": client_key,
            "enable_cookies": True,
        });
        if (opts.verify) {
            mopts.ssl_verify_mode = SSL_VERIFY_PEER;
        } else {
            mopts.ssl_verify_mode = SSL_VERIFY_NONE;
            mopts.accept_all_certs = True;
        }
        if (proxy) {
            mopts.proxy_url = proxy;
        }
        return mgrs{proxy} = new HttpClientConnectionManager(mopts);
    }

    #! Returns the URL for a connection name, optionally followed by a path
    private:internal static string getConnectionUrl(string name) {
        (*string conn_name, *string path) = (name =~ x/^([^\/]+)(\/.*)?$/);
        if (!conn_name) {
            throw "DOWNLOAD-URL-ERROR", sprintf("%y is not a URL or a connection name", name);
        }
        AbstractConnection conn;
        try {
            conn = get_connection(conn_name);
        } catch (hash<ExceptionInfo> ex) {
            throw "DOWNLOAD-URL-ERROR", sprintf("%y is not a URL, and no connection with this name can be found: "
                "%s", name, ex.desc);
        }
        string url = conn.getRealUrl(conn.url, conn.getConnectionSchemeInfo(), "");
        if (path) {
            url =~ s/\/+$//;
            url += path;
        }
        return url;
    }

    #! Returns a URL without its user information
    private:internal static string getUrlWithoutUserInfo(string url, hash<UrlInfo> info) {
        if (!exists info.username && !exists info.password) {
            return url;
        }
        return regex_subst(url, "^([^:]+://)[^@/]*@", "$1");
    }
}

#! The output of a download: a file, written through a .part file if enabled, or an output stream
class DownloadTarget {
    private:internal {
        #! the final file name
        *string file;
        #! the file written
        *string write_file;
        #! the output
        *OutputStream out;
        #! True if the output is a file
        bool is_file = False;
        #! True if the data is complete although the download failed
        bool complete = False;
    }

    #! Marks the data as complete, so it is kept under the final name also if the download fails
    setComplete() {
        complete = True;
    }

    #! Returns True if the data has been marked as complete with setComplete()
    bool isComplete() {
        return complete;
    }

    #! Writes the data to the given stream
    setStream(OutputStream out) {
        self.out = out;
    }

    #! Opens the file for the data
    /** @param file the final file name
        @param part_file True to write to a .part file that is renamed when the download is complete
    */
    open(string file, bool part_file) {
        self.file = file;
        write_file = part_file ? file + ".part" : file;
        try {
            out = new FileOutputStream(write_file);
        } catch (hash<ExceptionInfo> ex) {
            throw "DOWNLOAD-IO-ERROR", ex.desc;
        }
        is_file = True;
    }

    #! Returns the final file name, or NOTHING if the data is written to a stream
    *string getFileName() {
        return file;
    }

    #! Writes data
    write(binary data) {
        try {
            out.write(data);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED") {
                rethrow;
            }
            throw "DOWNLOAD-IO-ERROR", sprintf("cannot write to %y: %s", write_file ?? "the output", ex.desc);
        }
    }

    #! Closes the output and moves a complete file to its final name, or keeps or removes an incomplete file
    /** @param complete True if the download is complete
        @param keep True to keep the data of an incomplete download
        @param res the result, which is updated with the file names
    */
    finish(bool complete, bool keep, reference<hash<UrlDownloadResult>> res) {
        if (!out) {
            return;
        }
        # a stream given by the caller is not closed
        if (!is_file) {
            return;
        }
        try {
            out.close();
        } catch (hash<ExceptionInfo> ex) {
            if (complete) {
                removeFile();
                throw "DOWNLOAD-IO-ERROR", sprintf("cannot write to %y: %s", write_file, ex.desc);
            }
        }
        if (complete) {
            if (write_file != file) {
                try {
                    rename(write_file, file);
                } catch (hash<ExceptionInfo> ex) {
                    removeFile();
                    throw "DOWNLOAD-IO-ERROR", sprintf("cannot rename %y to %y: %s", write_file, file, ex.desc);
                }
            }
            res.file = file;
            return;
        }
        if (keep && hstat(write_file).size > 0) {
            if (write_file != file) {
                res.part_file = write_file;
            } else {
                res.file = file;
            }
            return;
        }
        removeFile();
    }

    #! Removes the file written
    private:internal removeFile() {
        try {
            unlink(write_file);
        } catch () {
            # there is nothing left to do if the file cannot be removed
        }
    }
}

#! Writes the data of an FTP transfer to a download target and reports the progress
/** FtpClient writes a stream implemented in Qore in the thread that calls FtpClient::get()
*/
class DownloadProgressOutputStream inherits OutputStream {
    private:internal {
        #! the target of the data
        DownloadTarget target;
        #! the total size, if known
        *int total;
        #! receives the progress, if any
        *UrlDownloadListener listener;
        #! the number of bytes written
        int size = 0;
    }

    constructor(DownloadTarget target, *int total, *UrlDownloadListener listener) {
        self.target = target;
        self.total = total;
        self.listener = listener;
    }

    #! Writes data
    write(binary data) {
        target.write(data);
        size += data.size();
        if (listener) {
            listener.progress(size, total);
        }
    }

    #! The target is closed by the downloader
    close() {
    }

    #! Returns the number of bytes written
    int getSize() {
        return size;
    }
}
}
