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
/** Wraps a \c TransformOutputStream to decompress data chunks incrementally
    as they arrive from a streaming HTTP response.  Supports all standard
    HTTP content encodings: deflate, gzip, bzip2, brotli (br), zstd, and lz4.

    @par Example
    @code{.py}
StreamDecompressor dec("gzip");
while (*binary chunk = readNextChunk()) {
    string text = dec.decompressToString(chunk);
    processText(text);
}
    @endcode
*/
public class StreamDecompressor {
    private {
        #! Output buffer for incremental decompression
        StringOutputStream string_ostream();

        #! Decompression transform wrapping the output buffer
        *TransformOutputStream dstream;

        #! Whether decompression is active
        bool active = False;
    }

    #! Creates a decompressor for the given HTTP Content-Encoding
    /** @param content_encoding the value of the \c Content-Encoding response header
        (e.g., \c "gzip", \c "deflate", \c "br", \c "zstd"); pass \c NOTHING or
        \c "identity" for no decompression

        @throw ENCODING-ERROR if the content encoding is not supported
    */
    constructor(*string content_encoding) {
        if (!content_encoding || content_encoding == "identity") {
            return;
        }

        Transform decompressor;
        switch (content_encoding) {
            case "deflate":
                decompressor = get_decompressor(COMPRESSION_ALG_ZLIB);
                break;
            case "gzip":
                decompressor = get_decompressor(COMPRESSION_ALG_GZIP);
                break;
            case "bzip2":
                decompressor = get_decompressor(COMPRESSION_ALG_BZIP2);
                break;
            case "br":
                decompressor = get_decompressor(COMPRESSION_ALG_BROTLI);
                break;
            case "zstd":
                decompressor = get_decompressor(COMPRESSION_ALG_ZSTD);
                break;
            case "lz4":
                decompressor = get_decompressor(COMPRESSION_ALG_LZ4);
                break;
            default:
                throw "ENCODING-ERROR",
                    sprintf("unsupported Content-Encoding: %y", content_encoding);
        }

        dstream = new TransformOutputStream(string_ostream, decompressor);
        active = True;
    }

    #! Returns True if decompression is active
    bool isActive() {
        return active;
    }

    #! Decompresses binary data and returns the result as a string
    /** Writes the compressed data to the transform stream and returns all
        decompressed output accumulated so far.  Safe to call multiple times
        for incremental decompression of text-based content (SSE, JSON, etc.).

        @param data compressed binary data
        @return the decompressed string (may be empty if the decompressor
        is buffering across chunk boundaries)

        @note This method is designed for text-based streaming protocols (SSE,
        JSON streams).  For binary protocols, use a separate decompression
        approach.
    */
    string decompressToString(binary data) {
        if (!active) {
            return data.toString();
        }
        dstream.write(data);
        return string_ostream.getData();
    }
}

} # namespace HttpClientStreamingIo
