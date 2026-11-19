# -*- mode: qore; indent-tabs-mode: nil -*-

/** FileGetRequestDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FileDataProvider module
public namespace FileDataProvider {
#! Data type for FILE get file request calls
public class FileGetRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "encoding": {
                "type": StringType,
                "desc": "The string encoding to use if `text` = `True` (default: `UTF-8`)",
                "default_value": "UTF-8",
            },
            "encoding_errors": {
                "type": StringType,
                "short_desc": "What text that is not valid in its character encoding gives",
                "desc": "What text that is not valid in its character encoding gives if `text` = `True`: `raise` "
                    "(the default) raises an `INVALID-ENCODING` error, `latin1` returns the text as `ISO-8859-1`, "
                    "and `keep` returns it in the encoding without a check",
                "default_value": ENCODING_ERRORS_RAISE,
            },
            "max_len": {
                "type": IntType,
                "desc": "The maximum length of the data returned in bytes",
                "default_value": -1,
            },
            "path": {
                "type": StringType,
                "desc": "The path to the file",
            },
            "text": {
                "type": BoolType,
                "desc": "If `True` the file will be returned as a string, `False` (the default) as binary data",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
