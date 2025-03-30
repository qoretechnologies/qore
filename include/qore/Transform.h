/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  Transform.h

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

#ifndef _QORE_TRANSFORM_H
#define _QORE_TRANSFORM_H

#include "qore/AbstractPrivateData.h"

/**
 * @brief Interface for private data of transformations.
 */
class Transform : public AbstractPrivateData {
public:
   /**
    * @brief Applies the transformation.
    * @param src pointer to source data or nullptr for flushing when no more input data is available
    * @param srcLen the number of bytes in the src buffer
    * @param dst pointer to destination buffer
    * @param dstLen the number of bytes that can be written to dst
    * @param xsink the exception sink
    * @return a pair (rc, wc) where rc is the number of bytes read from src and wc is the number of bytes written to dst
    */
   virtual std::pair<int64, int64> apply(const void* src, int64 srcLen, void* dst, int64 dstLen,
         ExceptionSink* xsink) = 0;

   //! returns the default output buffer size for the object
   DLLLOCAL virtual size_t outputBufferSize();

   //! returns the default input buffer size for the object
   DLLLOCAL virtual size_t inputBufferSize();

protected:
   /**
    * @brief Constructor.
    */
   Transform() = default;

private:
   Transform(const Transform &) = delete;
   Transform(Transform &&) = delete;
   Transform &operator=(const Transform &) = delete;
   Transform &operator=(Transform &&) = delete;
};

#endif // _QORE_TRANSFORM_H
