# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationUserDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation user data type
public class ShipStationUserDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationUser", {
        "userId": {
            "display_name": "User ID",
            "short_desc": "Unique identifier for the user",
            "type": SoftStringType,
            "desc": "The unique identifier for the user (GUID format)",
        },
        "userName": {
            "display_name": "Username",
            "short_desc": "User's login name",
            "type": SoftStringOrNothingType,
            "desc": "The user's login username",
        },
        "name": {
            "display_name": "Name",
            "short_desc": "User's display name",
            "type": SoftStringOrNothingType,
            "desc": "The user's full display name",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
