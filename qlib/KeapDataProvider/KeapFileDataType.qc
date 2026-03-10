# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapFileDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap file record type
public class KeapFileDataType inherits HashDataType {
    private {
        #! Field definitions for Keap files
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The file ID",
                "desc": "The unique identifier of the file",
            },
            "file_name": {
                "type": StringOrNothingType,
                "display_name": "File Name",
                "short_desc": "The file name",
                "desc": "The name of the file",
                "example_value": "proposal.pdf",
            },
            "file_size": {
                "type": IntOrNothingType,
                "display_name": "File Size",
                "short_desc": "The file size in bytes",
                "desc": "The size of the file in bytes",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The ID of the contact this file is associated with",
            },
            "is_public": {
                "type": BoolOrNothingType,
                "display_name": "Is Public",
                "short_desc": "Whether the file is public",
                "desc": "Indicates whether the file is publicly accessible (`True` or `False`)",
            },
            "file_box_type": {
                "type": StringOrNothingType,
                "display_name": "File Box Type",
                "short_desc": "The file box type",
                "desc": "The type of file box the file belongs to",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Application"},
                    <AllowedValueInfo>{"value": "Image"},
                    <AllowedValueInfo>{"value": "Fax"},
                    <AllowedValueInfo>{"value": "Attachment"},
                    <AllowedValueInfo>{"value": "Ticket"},
                    <AllowedValueInfo>{"value": "Contact"},
                    <AllowedValueInfo>{"value": "DigitalProduct"},
                    <AllowedValueInfo>{"value": "Import"},
                    <AllowedValueInfo>{"value": "Hidden"},
                    <AllowedValueInfo>{"value": "Webform"},
                    <AllowedValueInfo>{"value": "StyledCart"},
                    <AllowedValueInfo>{"value": "Logo"},
                    <AllowedValueInfo>{"value": "ThemeImage"},
                ),
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the file was created",
                "desc": "The timestamp when the file was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the file was last updated",
                "desc": "The timestamp when the file was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
