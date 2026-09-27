# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UrlDownloadListener.qc receives the events of a download

/*  UrlDownloadListener.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Receives the events of a download; override the methods for the events of interest
/** The methods are called in the thread that calls UrlDownloader::download(); an exception raised by a method
    fails the download

    @par Example:
    @code{.py}
class MyListener inherits UrlDownloadListener {
    started(hash<UrlDownloadStartInfo> info) {
        printf("downloading %s\n", info.file ?? "-");
    }

    progress(int done, *int total) {
        printf("%d/%y\r", done, total);
    }
}
    @endcode
*/
public class UrlDownloadListener {
    #! Called for each redirect followed, before UrlDownloadListener::started()
    /** @param info the redirect
    */
    redirected(hash<HttpRedirectInfo> info) {
    }

    #! Called when the response has been received and data is about to be written
    /** @param info information about the download
    */
    started(hash<UrlDownloadStartInfo> info) {
    }

    #! Called when data has been received
    /** @param done the number of bytes received
        @param total the total size, if known
    */
    progress(int done, *int total) {
    }
}
}
