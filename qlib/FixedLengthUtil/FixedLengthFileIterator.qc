# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FixedLengthFileIterator.qc Qore user module for working with files with fixed lines length

/*  FixedLengthFileIterator Copyright 2015 - 2024 Qore Technologies, s.r.o.

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

#! The FixedLengthUtil namespace contains all the definitions in the FixedLengthUtil module
public namespace FixedLengthUtil {
#! Structured line iterator for fixed-length line files allowing efficient "pipelined" processing.
/**
    @par Example:
    @code{.py}
#!/usr/bin/env qore

%modern

%requires FixedLengthUtil

hash<auto> specs = {
    "type1": {
        "col1": {"length": 5, "type": "int"},
        "col2": {"length": 2, "type": "string"},
    },
    "type2": {
        "col3": {"length": 1, "type": "string"},
        "col4": {"length": 3, "type": "string"},
        "col5": {"length": 8, "type": "date", "format": "DDMMYYYY", "timezone": "Europe/Prague"},
    },
};

hash<auto> global_options = {
    "encoding"    : "UTF-8",
    "eol"         : "\n",
    "ignore_empty": True,
    "timezone"    : "Europe/Prague", # used if not overridden in a date field specification
};

FixedLengthFileIterator i(file, specs, global_options);
# output all records
map printf("%y\n", $1), i;
    @endcode

    @see
    - @ref FixedLengthUtil::FixedLengthIterator "FixedLengthIterator" for a stream-based class providing the same functionality as this class in a more generic way
*/
public class FixedLengthFileIterator inherits FixedLengthUtil::FixedLengthAbstractIterator {
    private {
        #! the path of the file being iterated
        string m_file_path;
    }

    #! Instantiates the FixedLengthFileIterator object
    /**
        @param path File path to read
        @param spec Fixed-length line specification; see @ref fixedlengthspec for more information
        @param opts Global options; see @ref fixedlengthglobals for more information
    */
    constructor(string path, hash<auto> spec, *hash<auto> opts)
        : FixedLengthAbstractIterator(new InputStreamLineIterator(new FileInputStream(path), opts.encoding, opts.eol),
            spec, opts) {
        m_file_path = path;
        # do not convert every string to the same encoding
        m_opts.input_encoding = remove m_opts.encoding;
    }

    #! Returns the character encoding for the file
    string getEncoding() {
        return m_opts.input_encoding ?? get_default_encoding();
    }

    #! Returns the file path/name used to open the file
    string getFileName() {
        return m_file_path;
    }

    #! Returns a @ref Qore::StatInfo "StatInfo" hash of the underlying file
    hash<Qore::StatInfo> hstat() {
        return Qore::hstat(m_file_path);
    }

    #! Returns a @ref stat_list "stat list" of the underlying file
    list<auto> stat() {
        return Qore::stat(m_file_path);
    }
}
}
