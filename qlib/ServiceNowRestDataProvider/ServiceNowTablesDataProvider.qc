# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ServiceNowRestDataProvider module definition

/** ServiceNowTablesDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ServiceNowRestDataProvider module
public namespace ServiceNowRestDataProvider {
#! The ServiceNowTablesDataProvider data provider class
public class ServiceNowTablesDataProvider inherits ServiceNowRestDataProviderBase {
    private {
        #! Flag if all access info has been queried
        bool all_access;

        #! Table access keyed by table name
        *hash<auto> access;

        #! Table access keyed by table ID
        *hash<auto> accessid;

        #! Metadata keyed by table name
        *hash<auto> meta;

        #! Superclass map; child -> parent
        hash<string, string> scmap;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from the arguments
    constructor(ServiceNowRestClient rest) : ServiceNowRestDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "tables";
    }

    #! Returns the data provider description
    string getDesc() {
        return "ServiceNow tables; children are record-based data providers for each table";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ServiceNowRestDataProvider::ProviderInfo;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllAccess();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *hash<auto> obj_access = getAccess(name);
        if (obj_access) {
            return new ServiceNowTableDataProvider(rest, name, obj_access, getMetadata(name));
        }
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("ServiceNow table %y", $1),
            "type": "ServiceNowTableDataProvider",
            "supports_read": ServiceNowRestDataProvider::ProviderInfo.supports_read,
            "supports_create": ServiceNowRestDataProvider::ProviderInfo.supports_create,
            "supports_update": ServiceNowRestDataProvider::ProviderInfo.supports_update,
            "supports_upsert": ServiceNowRestDataProvider::ProviderInfo.supports_upsert,
            "supports_delete": ServiceNowRestDataProvider::ProviderInfo.supports_delete,
            "supports_native_search": ServiceNowRestDataProvider::ProviderInfo.supports_native_search,
            "supports_bulk_read": ServiceNowRestDataProvider::ProviderInfo.supports_bulk_read,
            "supports_bulk_create": ServiceNowRestDataProvider::ProviderInfo.supports_bulk_create,
            "supports_bulk_upsert": ServiceNowRestDataProvider::ProviderInfo.supports_bulk_upsert,
            "supports_request": ServiceNowRestDataProvider::ProviderInfo.supports_request,
            "supports_children": False,
            "transaction_management": ServiceNowRestDataProvider::ProviderInfo.transaction_management,
            "has_record": ServiceNowRestDataProvider::ProviderInfo.has_record,
            "children_can_support_records": False,
        }, getChildProviderNames();
    }

    #! Returns access info for all tables
    private *hash<auto> getAllAccess() {
        if (all_access) {
            return access;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (all_access) {
            return access;
        }

        hash<auto> info;
        try {
            access = map {$1.name: $1 + parseBools($1 - ("name", "super_class", "sys_id"))}, rest.get("table/sys_db_object?"
                "sysparm_fields=name,sys_id,create_access,delete_access,read_access,update_access,super_class", NOTHING,
                \info).body.response.result;
            # sort table names
            access = access{sort(keys access)};
            all_access = True;
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }

        return access;
    }

    #! Returns access info for one table
    private *hash<auto> getAccess(string name) {
        *hash<auto> rv = access{name};
        if (rv && rv.super_class.typeCode() != NT_HASH) {
            return rv;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        rv = access{name};
        if (rv) {
            if (rv.super_class.typeCode() == NT_HASH) {
                string super_name;
                getAccessIdIntern(rv.super_class.value, \super_name);
                access{name}.super_class = super_name;
                accessid{rv.sys_id} = access{name};
                scmap{name} = super_name;
            }
            return rv;
        }

        hash<auto> info;
        try {
            rv = rest.get("table/sys_db_object?"
                "sysparm_fields=name,sys_id,create_access,delete_access,read_access,update_access,super_class&sysparm_query=name=" + name,
                NOTHING, \info).body.response.result;

            if (rv.super_class) {
                string super_class;
                getAccessIdIntern(rv.super_class.value, \super_class);
                scmap{name} = super_class;
            }
            access{name} = accessid{rv.sys_id} = rv + parseBools(rv - ("name", "sys_id", "super_class"));
        } catch (hash<ExceptionInfo> ex) {
            printf("%s\n", get_exception_string(ex));
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        return access{name};
    }

    #! Returns access info for one table; must be called with the lock held
    private *hash<auto> getAccessIdIntern(string sys_id, reference<string> name) {
        *hash<auto> rv = accessid{sys_id};
        if (rv) {
            name = rv.name;
            return rv;
        }

        hash<auto> info;
        try {
            rv = rest.get("table/sys_db_object?"
                "sysparm_fields=name,sys_id,create_access,delete_access,read_access,update_access&sysparm_query=sys_id=" + sys_id,
                NOTHING, \info).body.response.result;
            name = rv.name;
            accessid{sys_id} = access{name} = rv + parseBools(rv - ("name", "sys_id"));

            if (rv.super_class) {
                string super_name;
                getAccessIdIntern(rv.super_class.value, \super_name);
                scmap{name} = super_name;
            }
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        return accessid{sys_id};
    }

    #! Returns metadata for the given table
    private *hash<auto> getMetadata(string name) {
        *hash<auto> rv = meta{name};
        if (rv) {
            return rv;
        }

        lck.lock();
        on_exit lck.unlock();

        return getMetadataIntern(name);
    }

    #! Returns metadata for the given table; must have the lock held
    private *hash<auto> getMetadataIntern(string name) {
        # check again in the lock
        *hash<auto> rv = meta{name};
        if (rv) {
            return rv;
        }

        hash<auto> info;
        try {
            *softlist<auto> res = rest.get("table/sys_dictionary?sysparm_query=name=" + name, NOTHING,
                \info).body.response.result;
            rv = map {$1.element: $1 - "element"}, res, exists $1.element;
            meta{name} = rv;
            *string super_class = scmap{name};
            if (super_class) {
                rv += getMetadataIntern(super_class);
            }
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        return rv;
    }

    #! Returns a hash with all values parsed to boolean values
    static hash<string, bool> parseBools(hash<auto> h) {
        return map {$1.key: parse_boolean($1.value)}, h.pairIterator();
    }
}
}
