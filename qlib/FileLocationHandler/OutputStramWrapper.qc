# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file OutputStreamWrapper.qc module for returning file data from a URL-like location string

/*  FileLocationHandler Copyright 2021 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FileLocationHandler module
public namespace FileLocationHandler {
#! The abstract file location handler class
public class OutputStreamWrapper inherits Qore::OutputStream {
    public {
    }

    private:internal {
        #! The output stream object
        Qore::OutputStream stream;

        #! The synchronization WaitGroup
        Qore::Thread::WaitGroup cnt;
    }

    #! Creates the output stream wrapper object
    constructor(Qore::OutputStream stream, Qore::Thread::WaitGroup cnt) {
        self.stream = stream;
        self.cnt = cnt;
    }

    #! Returns the output stream
    Qore::OutputStream getStream() {
        return stream;
    }

    #! Wait for any background I/O to complete
    waitForIo() {
        cnt.wait();
    }

    #! Flushes any buffered (unwritten) bytes, closes the output stream and releases all resources.
    /** Any methods called on a closed output stream will throw an exception.
    */
    close() {
        stream.close();
    }

    #! Writes bytes to the output stream
    /**
        @param data the data to write

        @par Example:
        @code{.py}
        os.write(<0405>);
        @endcode
    */
    write(binary data) {
        stream.write(data);
    }

    #! Redirects any method calls to the stream
    methodGate(string m) {
        call_object_method_args(stream, m, argv);
    }
}
}
