# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyGroupDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly group record type
public class BitlyGroupDataType inherits HashDataType {
    public {
        #! Field definitions for Bitly groups
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "The group identifier",
                "desc": "The unique identifier (GUID) of the group",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The group name",
                "desc": "The name of the group",
            },
            "created": {
                "type": StringOrNothingType,
                "display_name": "Created",
                "short_desc": "When the group was created",
                "desc": "The ISO 8601 timestamp when the group was created",
            },
            "modified": {
                "type": StringOrNothingType,
                "display_name": "Modified",
                "short_desc": "When the group was last modified",
                "desc": "The ISO 8601 timestamp when the group was last modified",
            },
            "is_active": {
                "type": BoolType,
                "display_name": "Active",
                "short_desc": "Whether the group is active",
                "desc": "Whether the group is currently active",
            },
            "role": {
                "type": StringOrNothingType,
                "display_name": "Role",
                "short_desc": "User's role in the group",
                "desc": "The current user's role within the group",
            },
            "organization_guid": {
                "type": StringOrNothingType,
                "display_name": "Organization GUID",
                "short_desc": "Parent organization",
                "desc": "The GUID of the parent organization this group belongs to",
            },
            "bsds": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Branded Short Domains",
                "short_desc": "BSDs for this group",
                "desc": "A list of branded short domains available in this group",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
