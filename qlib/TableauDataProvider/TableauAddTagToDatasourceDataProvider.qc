# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauAddTagToDatasourceDataProvider.qc Tableau add tag to datasource action data provider

/*  TableauAddTagToDatasourceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for adding a tag to a datasource
public class TableauAddTagToDatasourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "datasource_id": {
                "type": StringType,
                "display_name": "Datasource ID",
                "short_desc": "The datasource to tag",
                "desc": "The unique identifier of the datasource to add a tag to",
                "required": True,
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
                "ref_data": "datasources",
            },
            "tag_name": {
                "type": StringType,
                "display_name": "Tag Name",
                "short_desc": "The tag to add",
                "desc": "The name of the tag to add to the datasource",
                "required": True,
                "example_value": "production",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau add tag to datasource action data provider
public class TableauAddTagToDatasourceDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-tag",
            "desc": "Add a tag to a datasource in Tableau",
            "type": "TableauAddTagToDatasourceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Response type for action registration
        const ResponseType = AbstractDataProviderTypeMap."hash";
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "add-tag";
    }

    *string getDesc() {
        return "Add a tag to a datasource in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauAddTagToDatasourceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("datasources/" + req.datasource_id + "/tags",
            {"tags": {"tag": ({"label": req.tag_name})}});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
