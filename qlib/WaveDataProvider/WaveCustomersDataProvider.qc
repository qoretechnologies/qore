# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveCustomersDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave customers collection data provider
public class WaveCustomersDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Wave customers data provider",
            "type": "WaveCustomersDataProvider",
            "supports_read": True,
            "supports_create": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "create_options": CreateOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "businessId": <DataProviderOptionInfo>{
                "display_name": "Business",
                "short_desc": "The business to list customers for",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the business to list customers for (required)",
            },
            "id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Get a specific customer by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The ID of a specific customer to retrieve",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by email address",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Create options
        const CreateOptions = {};
    }

    private {
        #! Record type
        static WaveCustomerDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Wave customers collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for customers
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Get a single customer by ID
        if (search_options.id) {
            *hash<auto> customer;
            try {
                hash<auto> data = doGraphQL(GetCustomerQuery, {
                    "businessId": search_options.businessId,
                    "customerId": search_options.id,
                });
                customer = data.business.customer;
            } catch (hash<ExceptionInfo> ex) {
                # Wave API throws NOT_FOUND for non-existent customer IDs
                if (ex.err == "GRAPHQL-ERROR" && ex.arg[0].extensions.code == "NOT_FOUND") {
                    customer = NOTHING;
                } else {
                    rethrow;
                }
            }
            if (!customer) {
                return new DefaultRecordIterator(new ListHashIterator(()),
                    where_cond, search_options, recordType.getFields());
            }
            return new DefaultRecordIterator(new ListHashIterator((flattenEdges(customer),)),
                where_cond, search_options, recordType.getFields());
        }

        # Filter by email client-side (Wave has no server-side email filter)
        if (search_options.email) {
            list<hash<auto>> results = ();
            hash<auto> vars = {"businessId": search_options.businessId, "page": 1, "pageSize": 100};
            while (True) {
                hash<auto> response = rest.graphql(ListCustomersQuery, vars);
                if (response.body.errors) {
                    throw "GRAPHQL-ERROR", response.body.errors[0].message, response.body.errors;
                }
                *hash<auto> customers = response.body.data.business.customers;
                if (!customers || !customers.edges || !customers.edges.size()) {
                    break;
                }
                foreach hash<auto> edge in (customers.edges) {
                    hash<auto> customer = flattenEdges(edge.node);
                    if (customer.email && customer.email.lwr() == search_options.email.lwr()) {
                        results += customer;
                        break;  # Email addresses are unique
                    }
                }
                # Stop if we found a match (email addresses are unique)
                if (results.size()) {
                    break;
                }
                if (!customers.pageInfo || vars.page >= customers.pageInfo.totalPages) {
                    break;
                }
                vars.page++;
            }
            return new DefaultRecordIterator(new ListHashIterator(results),
                where_cond, search_options, recordType.getFields());
        }

        # Default: paginated list of all customers
        hash<auto> vars = {"businessId": search_options.businessId};
        int pageSize = search_options.limit ?? DefaultPageSize;

        return new WaveRecordIterator(rest, ListCustomersQuery, "business", "customers",
            vars, pageSize, recordType.getFields());
    }

    #! Returns the record type for create operations
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> search_options) {
        return (new WaveCustomerCreateDataType()).getFields();
    }

    #! Returns the record type for update operations
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return (new WaveCustomerUpdateDataType()).getFields();
    }

    #! Creates a new customer
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        @assert(rec.businessId.val(), "businessId is required to create a customer");
        @assert(rec.name.val(), "name is required to create a customer");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: creating customer with name=%y businessId=%y", rec.name,
            rec.businessId));
        hash<auto> result = doMutation(CustomerCreateMutation, {"input": rec}, "customerCreate", "customer");
        @debug(log(LoggerLevel::DEBUG, "createRecordImpl: created customer id=%y", result.id));
        return flattenEdges(result);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
