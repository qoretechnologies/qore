# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TableauDataProvider module definition

/*  TableauDatasourceDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau datasource record type
public class TableauDatasourceDataType inherits HashDataType {
    private {
        #! Field definitions for Tableau datasources
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The datasource ID",
                "desc": "The unique identifier of the datasource",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The datasource name",
                "desc": "The name of the datasource",
            },
            "contentUrl": {
                "type": StringOrNothingType,
                "display_name": "Content URL",
                "short_desc": "The content URL",
                "desc": "The content URL of the datasource",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The datasource type",
                "desc": "The type of the datasource",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the datasource was created",
                "desc": "When the datasource was created",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the datasource was last updated",
                "desc": "When the datasource was last updated",
            },
            "isCertified": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Certified",
                "short_desc": "Whether the datasource is certified",
                "desc": "Whether the datasource is certified",
            },
            "certificationNote": {
                "type": StringOrNothingType,
                "display_name": "Certification Note",
                "short_desc": "Certification note",
                "desc": "Note about the datasource certification",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Datasource description",
                "desc": "Description of the datasource",
            },
            "encryptExtracts": {
                "type": StringOrNothingType,
                "display_name": "Encrypt Extracts",
                "short_desc": "Whether extracts are encrypted",
                "desc": "Whether extracts are encrypted",
            },
            "hasExtracts": {
                "type": SoftBoolOrNothingType,
                "display_name": "Has Extracts",
                "short_desc": "Whether the datasource has extracts",
                "desc": "Whether the datasource has extracts",
            },
            "useRemoteQueryAgent": {
                "type": SoftBoolOrNothingType,
                "display_name": "Use Remote Query Agent",
                "short_desc": "Whether to use remote query agent",
                "desc": "Whether to use remote query agent",
            },
            "webpageUrl": {
                "type": StringOrNothingType,
                "display_name": "Webpage URL",
                "short_desc": "The web URL for the datasource",
                "desc": "The web URL for the datasource",
            },
            "project": {
                "type": AutoHashOrNothingType,
                "display_name": "Project",
                "short_desc": "The project containing this datasource",
                "desc": "The project containing this datasource",
            },
            "owner": {
                "type": AutoHashOrNothingType,
                "display_name": "Owner",
                "short_desc": "The owner of the datasource",
                "desc": "The owner of the datasource",
            },
            "tags": {
                "type": AutoHashOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags associated with the datasource",
                "desc": "Tags associated with the datasource",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace TableauDataProvider
