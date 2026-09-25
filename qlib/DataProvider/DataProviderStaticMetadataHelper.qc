# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderStaticMetadataHelper class definition

/** DataProviderStaticMetadataHelper.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Set while static metadata is being read in the current thread
thread_local *bool _thread_static_metadata;

#! Marks the current thread as reading static metadata while the object exists
/** Static metadata, such as the metadata written to the data provider index, must not depend on a connection or
    call context.  While an object of this class exists, fields and types whose type, default value, or example
    value is produced by a dynamic callback must return their declared values instead of running the callback,
    which would otherwise run without a context and fail or return context-dependent data.

    @par Example:
    @code{.py}
{
    DataProviderStaticMetadataHelper static_metadata();
    hash<DataTypeInfo> info = type.getInfo();
}
    @endcode

    @since DataProvider 3.8
*/
public class DataProviderStaticMetadataHelper {
    public {}

    private {
        *bool old;
    }

    #! Marks the current thread as reading static metadata
    constructor() {
        old = _thread_static_metadata;
        _thread_static_metadata = True;
    }

    #! Restores the previous state
    destructor() {
        _thread_static_metadata = old;
    }

    #! Returns @ref True if the current thread is reading static metadata
    /** @return @ref True if the current thread is reading static metadata, in which case dynamic type, default
        value, and example value callbacks must not be run
    */
    static bool get() {
        return _thread_static_metadata ?? False;
    }
}
}
