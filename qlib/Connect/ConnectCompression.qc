# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectCompression.qc Connect protocol per-message compression

/*  ConnectCompression.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace Connect {

#! Supported compression encoding: gzip
public const CONNECT_ENCODING_GZIP = "gzip";

#! Supported compression encoding: deflate (zlib format per RFC 2616)
public const CONNECT_ENCODING_DEFLATE = "deflate";

#! Per-message compression utilities for Connect streaming
/** Provides compression and decompression for individual Connect message payloads
    using the gzip or deflate algorithms.

    Per the Connect protocol specification, streaming messages use the envelope's
    compressed flag (bit 0, @ref CONNECT_FLAG_COMPRESSED) to indicate per-message
    compression.  The compression algorithm is negotiated via the
    @ref ConnectContentEncodingHeader and @ref ConnectAcceptEncodingHeader headers.

    Supported encodings:
    - \c gzip — RFC 1952 (gzip format)
    - \c deflate — RFC 1950 (zlib format, per HTTP specification)
*/
public class ConnectCompression {
    #! Compress a binary payload
    /** @param data the uncompressed payload
        @param encoding the compression encoding (\c "gzip" or \c "deflate")
        @return the compressed binary data

        @throw CONNECT-COMPRESSION-ERROR if the encoding is not supported
    */
    static binary compress(binary data, string encoding) {
        switch (encoding) {
            case CONNECT_ENCODING_GZIP:
                return gzip(data);
            case CONNECT_ENCODING_DEFLATE:
                return Qore::compress(data);
            default:
                throw "CONNECT-COMPRESSION-ERROR",
                    sprintf("unsupported compression encoding: %y", encoding);
        }
    }

    #! Decompress a binary payload
    /** @param data the compressed payload
        @param encoding the compression encoding (\c "gzip" or \c "deflate")
        @return the decompressed binary data

        @throw CONNECT-COMPRESSION-ERROR if the encoding is not supported or
        decompression fails
    */
    static binary decompress(binary data, string encoding) {
        switch (encoding) {
            case CONNECT_ENCODING_GZIP:
                return gunzip_to_binary(data);
            case CONNECT_ENCODING_DEFLATE:
                return uncompress_to_binary(data);
            default:
                throw "CONNECT-COMPRESSION-ERROR",
                    sprintf("unsupported compression encoding: %y", encoding);
        }
    }

    #! Returns the list of supported compression encodings
    /** @return (\c "gzip", \c "deflate")
    */
    static list<string> supportedEncodings() {
        return (CONNECT_ENCODING_GZIP, CONNECT_ENCODING_DEFLATE);
    }

    #! Check if a compression encoding is supported
    /** @param encoding the encoding name to check
        @return True if the encoding is supported
    */
    static bool isSupported(string encoding) {
        return encoding == CONNECT_ENCODING_GZIP || encoding == CONNECT_ENCODING_DEFLATE;
    }

    #! Returns the supported encodings as a comma-separated string
    /** Suitable for use as the value of the @ref ConnectAcceptEncodingHeader header.

        @return \c "gzip,deflate"
    */
    static string acceptEncodingValue() {
        return "gzip,deflate";
    }

    #! Parse an accept-encoding header and return the best supported encoding
    /** Selects the first encoding from the comma-separated list that is supported.

        @param accept_encoding the accept-encoding header value (comma-separated,
        may include quality parameters like \c "gzip;q=1.0")
        @return the first supported encoding, or NOTHING if none are supported
    */
    static *string negotiateEncoding(*string accept_encoding) {
        if (!accept_encoding.val()) {
            return NOTHING;
        }
        list<string> parts = accept_encoding.split(",");
        for (int i = 0; i < parts.size(); ++i) {
            string enc = trim(parts[i]);
            # Strip quality parameters (e.g., "gzip;q=1.0")
            int semi = enc.find(";");
            if (semi >= 0) {
                enc = trim(enc.substr(0, semi));
            }
            if (ConnectCompression::isSupported(enc)) {
                return enc;
            }
        }
        return NOTHING;
    }
}

} # namespace Connect
