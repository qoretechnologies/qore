# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedDataProviderBase class definition

/** MemcachedDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

public namespace MemcachedDataProvider {
#! Base class for all Memcached data providers
public class MemcachedDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! The memcached client
        MemcachedClient memcached;
    }

    #! Creates the object from a MemcachedClient
    constructor(MemcachedClient memcached) {
        self.memcached = memcached;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *MemcachedClient mc = remove options.memcached;
        if (mc) {
            memcached = mc;
        }
    }
}
}
