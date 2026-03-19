# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitSequenceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Kit sequence record type
public class KitSequenceDataType inherits HashDataType {
    public {
        #! Field definitions for Kit sequences
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The sequence ID",
                "desc": "The unique identifier of the sequence",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The sequence name",
                "desc": "The name of the sequence",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the sequence was created",
                "desc": "The timestamp when the sequence was created",
            },
            "hold_count": {
                "type": SoftIntOrNothingType,
                "display_name": "Hold Count",
                "short_desc": "The number of subscribers on hold",
                "desc": "The number of subscribers currently on hold in the sequence",
            },
            "subscriber_count": {
                "type": SoftIntOrNothingType,
                "display_name": "Subscriber Count",
                "short_desc": "The number of subscribers",
                "desc": "The total number of subscribers in the sequence",
            },
            "repeat": {
                "type": SoftBoolOrNothingType,
                "display_name": "Repeat",
                "short_desc": "Whether the sequence repeats",
                "desc": "Indicates whether the sequence repeats for subscribers",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
