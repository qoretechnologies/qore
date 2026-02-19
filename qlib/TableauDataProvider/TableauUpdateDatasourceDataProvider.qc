# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauUpdateDatasourceDataProvider.qc Tableau update datasource action data provider

/*  TableauUpdateDatasourceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a datasource
public class TableauUpdateDatasourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "datasource_id": {
                "type": StringType,
                "display_name": "Datasource ID",
                "short_desc": "The datasource ID to update",
                "desc": "The unique identifier of the datasource to update",
                "required": True,
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The new datasource name",
                "desc": "The new name of the datasource",
                "example_value": "Sales Data 2026",
            },
            "project_id": {
                "type": StringOrNothingType,
                "display_name": "Project ID",
                "short_desc": "The project to move the datasource to",
                "desc": "The ID of the project to move the datasource to",
                "example_value": "f1e2d3c4-b5a6-7890-abcd-ef1234567890",
            },
            "is_certified": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Certified",
                "short_desc": "Whether the datasource is certified",
                "desc": "Whether the datasource is certified",
            },
            "certification_note": {
                "type": StringOrNothingType,
                "display_name": "Certification Note",
                "short_desc": "The certification note",
                "desc": "A note about the datasource certification",
                "example_value": "Verified by the data team",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau update datasource action data provider
public class TableauUpdateDatasourceDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing datasource in Tableau",
            "type": "TableauUpdateDatasourceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Response type for action registration
        static TableauDatasourceDataType ResponseType();
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing datasource in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauUpdateDatasourceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string datasource_id = remove req.datasource_id;
        hash<auto> body;
        if (req.name) body.name = req.name;
        if (req.project_id) body.project = {"id": req.project_id};
        if (exists req.is_certified) body.isCertified = req.is_certified;
        if (req.certification_note) body.certificationNote = req.certification_note;
        return doPut("datasources/" + datasource_id, {"datasource": body}).datasource;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
