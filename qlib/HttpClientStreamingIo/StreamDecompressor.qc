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
        #! Holds the decompressed output until it is taken
        StreamDecompressorOutputStream output;

        #! Decompression transform wrapping the output buffer
        *TransformOutputStream dstream;

        #! Whether decompression is active
        bool active = False;
    }

    #! Creates a decompressor for the given HTTP Content-Encoding
    /** @param content_encoding the value of the \c Content-Encoding response header
        (e.g., \c "gzip", \c "deflate", \c "br", \c "zstd"); \c "x-gzip", \c "x-deflate", and \c "x-bzip2" are
        accepted as well; pass \c NOTHING or \c "identity" for no decompression
        @param max_size the maximum size in bytes of the data decompressed by one call; \c 0 (the default) or a
        negative value means no limit; decompression stops with a \c DECOMPRESSION-LIMIT-EXCEEDED exception as soon
        as the data would exceed it, so that a small chunk of compressed data that expands to a huge one cannot
        exhaust memory; the decompressor cannot be used after this exception

        @throw ENCODING-ERROR if the content encoding is not supported

        @since %HttpClientStreamingIo 1.0 added the \a max_size parameter
    */
    constructor(*string content_encoding, int max_size = 0) {
        output = new StreamDecompressorOutputStream(max_size);
        if (!content_encoding || content_encoding.lwr() == "identity") {
            return;
        }

        Transform decompressor;
        switch (content_encoding.lwr()) {
            case "deflate":
            case "x-deflate":
                decompressor = get_decompressor(COMPRESSION_ALG_ZLIB);
                break;
            case "gzip":
            case "x-gzip":
                decompressor = get_decompressor(COMPRESSION_ALG_GZIP);
                break;
            case "bzip2":
            case "x-bzip2":
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

        dstream = new TransformOutputStream(output, decompressor);
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

        @throw DECOMPRESSION-LIMIT-EXCEEDED the data decompressed by this call exceeds the maximum size

        @note This method is designed for text-based streaming protocols (SSE,
        JSON streams); use decompressToBinary() for binary content.
    */
    string decompressToString(binary data) {
        if (!active) {
            return data.toString();
        }
        dstream.write(data);
        return output.take().toString();
    }

    #! Decompresses binary data and returns the result as a binary value
    /** @param data compressed binary data
        @return the decompressed data (may be empty if the decompressor is buffering across chunk boundaries)

        @throw DECOMPRESSION-LIMIT-EXCEEDED the data decompressed by this call exceeds the maximum size

        @since %HttpClientStreamingIo 1.0
    */
    binary decompressToBinary(binary data) {
        if (!active) {
            return data;
        }
        dstream.write(data);
        return output.take();
    }
}

#! Holds decompressed data up to a maximum size until it is taken
/** A transform writes its output in small pieces, so the limit is enforced before more than the maximum size is
    held.
*/
class StreamDecompressorOutputStream inherits Qore::OutputStream {
    private {
        #! Buffered decompressed data
        BinaryOutputStream buf();

        #! The maximum size of the data held at once; <= 0 means no limit
        int max_size;

        #! The size of the data held
        int size = 0;
    }

    #! Creates the stream
    constructor(int max_size) {
        self.max_size = max_size;
    }

    #! Closes the stream
    close() {
    }

    #! Buffers decompressed data
    /** @throw DECOMPRESSION-LIMIT-EXCEEDED the buffered data would exceed the maximum size
    */
    write(binary data) {
        if (max_size > 0 && data.size() > max_size - size) {
            throw "DECOMPRESSION-LIMIT-EXCEEDED", sprintf("the data decompressed from the stream at once exceeds "
                "the maximum size of %d bytes", max_size);
        }
        buf.write(data);
        size += data.size();
    }

    #! Returns and clears the buffered data
    binary take() {
        size = 0;
        return buf.getData();
    }
}

} # namespace HttpClientStreamingIo
