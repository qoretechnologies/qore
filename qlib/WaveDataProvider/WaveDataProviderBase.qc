# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Base class for Wave data providers
public class WaveDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Wave data providers
        const BaseConstructorOptions = {
            "waverestclient": <DataProviderOptionInfo>{
                "display_name": "Wave REST Client",
                "short_desc": "The WaveRestClient object",
                "type": AbstractDataProviderType::get(new Type("WaveRestClient::WaveRestClient")),
                "desc": "The WaveRestClient object to use for API calls",
            },
            "waverestclient_options": <DataProviderOptionInfo>{
                "display_name": "Wave REST Client Options",
                "short_desc": "Options for creating a WaveRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a WaveRestClient object if one is not provided directly",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Token",
                "short_desc": "Wave API token",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Wave API token (OAuth2 access token) for authentication",
                "sensitive": True,
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client ID for authentication",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client secret for authentication",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        WaveRestClient::WaveRestClient rest;

        #! The connection object, if applicable
        *WaveRestClient::WaveRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.waverestclient) {
            rest = copts.waverestclient;
        } else {
            hash<auto> client_opts = copts.waverestclient_options ?? {};
            if (copts.token) {
                client_opts.token = copts.token;
            }
            if (copts.oauth2_client_id) {
                client_opts.oauth2_client_id = copts.oauth2_client_id;
            }
            if (copts.oauth2_client_secret) {
                client_opts.oauth2_client_secret = copts.oauth2_client_secret;
            }
            rest = new WaveRestClient::WaveRestClient(client_opts);
        }
    }

    #! Returns the REST client
    WaveRestClient::WaveRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *WaveRestClient::WaveRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Executes a GraphQL query
    /** @param query the GraphQL query string
        @param variables optional variables for the query
        @return the response data

        @throw GRAPHQL-ERROR if the response contains errors
    */
    hash<auto> doGraphQL(string query, *hash<auto> variables) {
        @assert(query.val(), "GraphQL query cannot be empty");
        @assert(rest, "REST client must be initialized before making GraphQL calls");
        @debug(log(LoggerLevel::DEBUG, "doGraphQL: executing query with variables: %y", variables));
        hash<auto> response = rest.graphql(query, variables);
        @assert(response.body, "GraphQL response body cannot be null");
        if (response.body.errors) {
            @debug(log(LoggerLevel::DEBUG, "doGraphQL: GraphQL error: %y", response.body.errors));
            string err_msg = response.body.errors[0].message ?? "Unknown GraphQL error";
            throw "GRAPHQL-ERROR", err_msg, response.body.errors;
        }
        @debug(log(LoggerLevel::DEBUG, "doGraphQL: success, response keys: %y", keys response.body.data));
        return response.body.data;
    }

    #! Executes a GraphQL mutation and handles the Wave mutation response pattern
    /** @param mutation the GraphQL mutation string
        @param variables the variables for the mutation
        @param mutationKey the key in the response data for the mutation result
        @param entityKey optional key in the mutation result for the entity data

        @return the entity data if entityKey is provided, otherwise a success indicator

        @throw WAVE-MUTATION-ERROR if the mutation did not succeed
    */
    hash<auto> doMutation(string mutation, hash<auto> variables, string mutationKey, *string entityKey) {
        hash<auto> data = doGraphQL(mutation, variables);
        hash<auto> result = data{mutationKey};
        if (!result.didSucceed) {
            string err_msg = "Mutation failed";
            if (result.inputErrors && result.inputErrors.size() > 0) {
                err_msg = result.inputErrors[0].message;
            }
            throw "WAVE-MUTATION-ERROR", err_msg, result.inputErrors;
        }
        if (entityKey) {
            return result{entityKey};
        }
        return {"didSucceed": True};
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.token) {
            rv.token = copts.token;
        }
        if (copts.oauth2_client_id) {
            rv.oauth2_client_id = copts.oauth2_client_id;
        }
        if (copts.oauth2_client_secret) {
            rv.oauth2_client_secret = copts.oauth2_client_secret;
        }
        foreach string key in (keys copts.waverestclient_options) {
            rv{key} = copts.waverestclient_options{key};
        }
        return rv;
    }

    #! Flattens GraphQL edge/node fields to plain arrays
    /** Wave GraphQL uses Relay-style connections that return arrays as { edges: [{ node: {...} }] }.
        This method converts them to plain arrays for cleaner output.

        @param data the data hash to process
        @return the processed data with flattened edge fields
    */
    static hash<auto> flattenEdges(hash<auto> data) {
        hash<auto> rv = {};
        foreach hash<auto> i in (data.pairIterator()) {
            if (i.value.typeCode() == NT_HASH) {
                if (i.value.hasKey("edges") && i.value.edges.typeCode() == NT_LIST) {
                    rv{i.key} = map WaveDataProviderBase::flattenEdges($1.node ?? $1), i.value.edges;
                } else {
                    rv{i.key} = WaveDataProviderBase::flattenEdges(i.value);
                }
            } else if (i.value.typeCode() == NT_LIST) {
                rv{i.key} = map $1.typeCode() == NT_HASH
                    ? WaveDataProviderBase::flattenEdges($1)
                    : $1, i.value;
            } else {
                rv{i.key} = i.value;
            }
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        \c getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "businesses": True,
            "customers": True,
            "accounts": True,
            "products": True,
            "taxes": True,
            "invoices": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "businesses": return getReferenceBusinesses();
            case "customers": return getReferenceCustomers(action_opts.businessId);
            case "accounts": return getReferenceAccounts(action_opts.businessId);
            case "products": return getReferenceProducts(action_opts.businessId);
            case "taxes": return getReferenceTaxes(action_opts.businessId);
            case "invoices": return getReferenceInvoices(action_opts.businessId);
        }
    }

    #! Returns available businesses
    private *list<hash<AllowedValueInfo>> getReferenceBusinesses() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListBusinessesQuery);
        foreach hash<auto> edge in (data.businesses.edges) {
            hash<auto> biz = edge.node;
            string desc = sprintf("Business %s", biz.name);
            rv += <AllowedValueInfo>{
                "display_name": biz.name,
                "short_desc": desc,
                "desc": desc,
                "value": biz.id,
            };
        }
        return rv;
    }

    #! Returns available customers for a business
    private *list<hash<AllowedValueInfo>> getReferenceCustomers(*string businessId) {
        if (!businessId) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListCustomersQuery, {"businessId": businessId, "pageSize": 100});
        foreach hash<auto> edge in (data.business.customers.edges) {
            hash<auto> customer = edge.node;
            string desc = customer.email ?? customer.name;
            rv += <AllowedValueInfo>{
                "display_name": customer.name,
                "short_desc": desc,
                "desc": desc,
                "value": customer.id,
            };
        }
        return rv;
    }

    #! Returns available accounts for a business
    private *list<hash<AllowedValueInfo>> getReferenceAccounts(*string businessId) {
        if (!businessId) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListAccountsQuery, {"businessId": businessId, "pageSize": 100});
        foreach hash<auto> edge in (data.business.accounts.edges) {
            hash<auto> account = edge.node;
            string desc = account.description ?? account.name;
            rv += <AllowedValueInfo>{
                "display_name": sprintf("%s (%s)", account.name, account.displayId),
                "short_desc": desc,
                "desc": desc,
                "value": account.id,
            };
        }
        return rv;
    }

    #! Returns available products for a business
    private *list<hash<AllowedValueInfo>> getReferenceProducts(*string businessId) {
        if (!businessId) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListProductsQuery, {"businessId": businessId, "pageSize": 100});
        foreach hash<auto> edge in (data.business.products.edges) {
            hash<auto> product = edge.node;
            string desc = product.description ?? product.name;
            rv += <AllowedValueInfo>{
                "display_name": product.name,
                "short_desc": desc,
                "desc": desc,
                "value": product.id,
            };
        }
        return rv;
    }

    #! Returns available sales taxes for a business
    private *list<hash<AllowedValueInfo>> getReferenceTaxes(*string businessId) {
        if (!businessId) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListSalesTaxesQuery, {"businessId": businessId});
        foreach hash<auto> edge in (data.business.salesTaxes.edges) {
            hash<auto> tax = edge.node;
            string desc = sprintf("%s (%s%%)", tax.name, tax.rate);
            rv += <AllowedValueInfo>{
                "display_name": tax.name,
                "short_desc": desc,
                "desc": desc,
                "value": tax.id,
            };
        }
        return rv;
    }

    #! Returns available invoices for a business
    private *list<hash<AllowedValueInfo>> getReferenceInvoices(*string businessId) {
        if (!businessId) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListInvoicesQuery, {"businessId": businessId, "pageSize": 100});
        foreach hash<auto> edge in (data.business.invoices.edges) {
            hash<auto> invoice = edge.node;
            string display = sprintf("#%s: %s", invoice.invoiceNumber, invoice.customer.name);
            string desc = sprintf("Invoice #%s - %s (%s)", invoice.invoiceNumber,
                invoice.customer.name, invoice.status);
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": desc,
                "desc": desc,
                "value": invoice.id,
            };
        }
        return rv;
    }
}

} # namespace WaveDataProvider
