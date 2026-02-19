# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDeleteTagFromDatasourceDataProvider.qc Tableau delete tag from datasource action data provider

/*  TableauDeleteTagFromDatasourceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a tag from a datasource
public class TableauDeleteTagFromDatasourceRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "datasource_id": {
                "type": StringType,
                "display_name": "Datasource ID",
                "short_desc": "The datasource to remove the tag from",
                "desc": "The unique identifier of the datasource to remove a tag from",
                "required": True,
            },
            "tag_name": {
                "type": StringType,
                "display_name": "Tag Name",
                "short_desc": "The tag to remove",
                "desc": "The name of the tag to remove from the datasource",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau delete tag from datasource action data provider
public class TableauDeleteTagFromDatasourceDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-tag",
            "desc": "Delete a tag from a datasource in Tableau",
            "type": "TableauDeleteTagFromDatasourceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "delete-tag";
    }

    *string getDesc() {
        return "Delete a tag from a datasource in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauDeleteTagFromDatasourceRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("datasources/" + req.datasource_id + "/tags/" + req.tag_name);
        return {};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
