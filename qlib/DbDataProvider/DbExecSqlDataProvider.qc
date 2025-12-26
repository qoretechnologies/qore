# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbExecSqlDataProvider class definition

/** DbExecSqlDataProvider.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DbDataProvider module
public namespace DbDataProvider {
#! DB exec data provider
public class DbExecSqlDataProvider inherits DbDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "exec-sql",
            "type": "DbExecSqlDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = ExecSqlRequestType;

        #! Response type
        const ResponseType = ExecSqlResponseType;
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts) : DbDataProviderBase(ds, opts) {
    }

    #! creates the object
    constructor(AbstractDatabase db, *LoggerInterface logger) : DbDataProviderBase(db) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : DbDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return "exec-sql";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Data provider for database `%s` for executing SQL", db.getDatasourceDesc());
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        AbstractDatasource ds = db.getDatasource();
        on_error ds.rollback();
        on_success ds.commit();
        return {
            "result": ds.vexec(req.sql, req.args),
        };
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

public class ExecSqlRequestType inherits HashDataType {
    public {
        const Fields = {
            "sql": {
                "display_name": "SQL",
                "short_desc": "The SQL statement to execute including any placeholders for bind arguments as '%v'; "
                    "the statement is automatically committed",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The SQL statement to execute including any placeholders for bind arguments as '%v'; "
                    "the statement is automatically committed",
            },
            "args": {
                "display_name": "Arguments",
                "short_desc": "The arguments for the SQL statement",
                "type": new SoftListDataType("ExecSqlArgs", AbstractDataProviderTypeMap."auto", True),
                "desc": "Any bind arguments for the SQL statement",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const ExecSqlRequestType = new ExecSqlRequestType();

public class ExecSqlResponseType inherits HashDataType {
    public {
        const Fields = {
            "result": {
                "display_name": "Result",
                "short_desc": "The result of the SQL statement",
                "type": AbstractDataProviderTypeMap."auto",
                "desc": "The result of the SQL statement",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const ExecSqlResponseType = new ExecSqlResponseType();
}
