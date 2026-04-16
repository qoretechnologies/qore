# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyDateDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly date/occasion data type
public class InsightlyDateDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly dates
        const Fields = {
            "DATE_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Date ID",
                "short_desc": "The unique date identifier",
                "desc": "The unique identifier of the date entry",
            },
            "OCCASION_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Occasion Name",
                "short_desc": "Name of the occasion",
                "desc": "The name of the occasion (e.g., `Birthday`, `Anniversary`)",
            },
            "OCCASION_DATE": {
                "type": SoftDateOrNothingType,
                "display_name": "Occasion Date",
                "short_desc": "The date of the occasion",
                "desc": "The date of the occasion",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyDateDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
