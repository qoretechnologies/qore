/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CompressionTransforms.h

  Qore Programming Language

  Copyright (C) 2016 - 2026 Qore Technologies, s.r.o.

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QORE_COMPRESSIONTRANSFORMS_H
#define _QORE_COMPRESSIONTRANSFORMS_H

#include "qore/Transform.h"

#include <memory>
#include <string>

class CompressionTransforms {
public:
   static constexpr const char *ALG_ZLIB = "zlib";
   static constexpr const char *ALG_GZIP = "gzip";
   static constexpr const char *ALG_BZIP2 = "bzip2";
   static constexpr const char *ALG_BROTLI = "br";
   static constexpr const char *ALG_ZSTD = "zstd";
   static constexpr const char *ALG_LZ4 = "lz4";

   static constexpr int64 LEVEL_DEFAULT = -1;

   DLLLOCAL static Transform *getCompressor(const QoreStringNode *alg, int64 level, ExceptionSink *xsink);
   //! Returns a decompressor for the given algorithm
   /** @param alg the algorithm
       @param max_size the maximum total decompressed size in bytes; 0 = no limit; the transform raises
       \c DECOMPRESSION-LIMIT-EXCEEDED as soon as its output would exceed this size
       @param xsink exception sink
   */
   DLLLOCAL static Transform *getDecompressor(const QoreStringNode *alg, size_t max_size, ExceptionSink *xsink);

   //! Returns the decompression algorithm for an HTTP content coding
   /** Maps \c "deflate" and \c "x-deflate" to \c "zlib", \c "x-gzip" to \c "gzip", and \c "x-bzip2" to
       \c "bzip2" (compared case-insensitively); any other value, including an algorithm name, is returned unchanged

       @param content_coding the HTTP content coding or algorithm name

       @return the algorithm name, or nullptr for \c "identity" or an empty value, which need no decompression
   */
   DLLLOCAL static const char* getContentCodingAlgorithm(const char* content_coding);
};

//! Decompresses a stream incrementally for a consumer that reads the decompressed data byte by byte
/** Keeps the compressed input that has not been decompressed yet and the decompressed output that has not been read
    yet, so that a stream can be read across several read operations.  The decompressed output held at any time is
    bounded by the output buffer size of the transform.
*/
class StreamDecoder {
public:
   //! Creates the decoder
   /** @param alg the decompression algorithm; see CompressionTransforms::getDecompressor()
       @param xsink raises an exception if the algorithm is unknown
   */
   DLLLOCAL StreamDecoder(const char* alg, ExceptionSink* xsink);

   //! Returns the decompression algorithm
   DLLLOCAL const std::string& getAlgorithm() const {
      return alg;
   }

   //! Adds compressed input
   DLLLOCAL void feed(const void* data, size_t len) {
      input.append(static_cast<const char*>(data), len);
   }

   //! Returns the next decompressed byte
   /** @return the next decompressed byte (0 - 255), -1 if more input is needed, or -2 if an exception was raised
   */
   DLLLOCAL int next(ExceptionSink* xsink);

   //! Returns true if decompressed output or compressed input is buffered
   DLLLOCAL bool hasData() const {
      return pos < len || !input.empty();
   }

private:
   std::string alg;
   SimpleRefHolder<Transform> transform;
   //! compressed input not consumed by the transform yet
   std::string input;
   //! decompressed output
   std::unique_ptr<char[]> buf;
   size_t buf_size = 0;
   size_t len = 0;
   size_t pos = 0;
};

#endif // _QORE_COMPRESSIONTRANSFORMS_H
