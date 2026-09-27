# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file StreamDecompressor.qc Shared HTTP content decompression utility

/** StreamDecompressor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpClientStreamingIo {

#! Incremental HTTP content decompression for streaming responses
/** See @ref HttpClientIo::HttpBodyDecompressor "HttpBodyDecompressor", which this class is an alias for.

    @par Example
    @code{.py}
StreamDecompressor dec("gzip");
while (*binary chunk = readNextChunk()) {
    string text = dec.decompressToString(chunk);
    processText(text);
}
    @endcode
*/
public class StreamDecompressor inherits HttpClientIo::HttpBodyDecompressor {
    #! Creates a decompressor for the given HTTP Content-Encoding
    /** @param content_encoding the value of the \c Content-Encoding response header (e.g., \c "gzip",
        \c "deflate", \c "br", \c "zstd"); \c "x-gzip", \c "x-deflate", and \c "x-bzip2" are accepted as well;
        pass \c NOTHING or \c "identity" for no decompression
        @param max_size the maximum size in bytes of the data decompressed by one call; \c 0 (the default) or a
        negative value means no limit

        @throw ENCODING-ERROR if the content encoding is not supported

        @since %HttpClientStreamingIo 1.0 added the \a max_size parameter
    */
    constructor(*string content_encoding, int max_size = 0) : HttpClientIo::HttpBodyDecompressor(content_encoding,
            max_size) {
    }
}
} # namespace HttpClientStreamingIo
