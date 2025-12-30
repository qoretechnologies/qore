/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CompressionTransforms.h

  Qore Programming Language

  Copyright (C) 2016 - 2024 Qore Technologies, s.r.o.

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

class CompressionTransforms {
public:
   static constexpr const char *ALG_ZLIB = "zlib";
   static constexpr const char *ALG_GZIP = "gzip";
   static constexpr const char *ALG_BZIP2 = "bzip2";
   static constexpr const char *ALG_BROTLI = "br";

   static constexpr int64 LEVEL_DEFAULT = -1;

   DLLLOCAL static Transform *getCompressor(const QoreStringNode *alg, int64 level, ExceptionSink *xsink);
   DLLLOCAL static Transform *getDecompressor(const QoreStringNode *alg, ExceptionSink *xsink);
};

#endif // _QORE_COMPRESSIONTRANSFORMS_H
