# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Base class for WooCommerce data providers
public class WooCommerceDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for WooCommerce data providers
        const BaseConstructorOptions = {
            "woocommercerestclient": <DataProviderOptionInfo>{
                "display_name": "WooCommerce REST Client",
                "short_desc": "The WooCommerceRestClient object",
                "type": AbstractDataProviderType::get(new Type("WooCommerceRestClient::WooCommerceRestClient")),
                "desc": "The WooCommerceRestClient object to use for API calls",
            },
            "woocommercerestclient_options": <DataProviderOptionInfo>{
                "display_name": "WooCommerce REST Client Options",
                "short_desc": "Options for creating a WooCommerceRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a WooCommerceRestClient object if one is not provided directly",
            },
            "store_url": <DataProviderOptionInfo>{
                "display_name": "Store URL",
                "short_desc": "WooCommerce store URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce store URL (e.g., `https://mystore.com`)",
                "required": True,
                "preselected": True,
            },
            "consumer_key": <DataProviderOptionInfo>{
                "display_name": "Consumer Key",
                "short_desc": "WooCommerce consumer key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce consumer key for authentication (used as Basic Auth username)",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
            "consumer_secret": <DataProviderOptionInfo>{
                "display_name": "Consumer Secret",
                "short_desc": "WooCommerce consumer secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce consumer secret for authentication (used as Basic Auth password)",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        WooCommerceRestClient::WooCommerceRestClient rest;

        #! The connection object, if applicable
        *WooCommerceRestClient::WooCommerceRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.woocommercerestclient) {
            rest = copts.woocommercerestclient;
        } else {
            hash<auto> client_opts = copts.woocommercerestclient_options ?? {};
            if (copts.store_url) {
                client_opts.store_url = copts.store_url;
            }
            if (copts.consumer_key) {
                client_opts.consumer_key = copts.consumer_key;
            }
            if (copts.consumer_secret) {
                client_opts.consumer_secret = copts.consumer_secret;
            }
            rest = new WooCommerceRestClient::WooCommerceRestClient(client_opts);
        }
    }

    #! Returns the REST client
    WooCommerceRestClient::WooCommerceRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *WooCommerceRestClient::WooCommerceRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the WooCommerce API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        return response.body;
    }

    #! Makes a GET request returning both body and pagination headers
    /** @param path the API path
        @param params optional query parameters
        @return a hash with \c body (response data), \c total (total items), and \c total_pages

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    hash<auto> doGetPaginated(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> info;
        hash<auto> response = rest.get(url, NOTHING, \info);
        checkResponse(response);
        return {
            "body": response.body,
            "total": info."response-headers"."x-wp-total" ? info."response-headers"."x-wp-total".toInt() : 0,
            "total_pages": info."response-headers"."x-wp-totalpages" ? info."response-headers"."x-wp-totalpages".toInt() : 0,
        };
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the WooCommerce API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a PUT request to the WooCommerce API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        hash<auto> response = rest.put(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a DELETE request to the WooCommerce API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.del(url);
        checkResponse(response);
        return response.body;
    }

    #! Checks the WooCommerce API response for errors
    /** @param response the API response hash

        @throw WOOCOMMERCE-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.message) {
                err_msg = response.body.message;
            } else if (response.body.code) {
                err_msg = sprintf("WooCommerce API error: %s", response.body.code);
            } else {
                err_msg = sprintf("WooCommerce API error (HTTP %d)", response.status_code);
            }
            throw "WOOCOMMERCE-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.store_url) {
            rv.store_url = copts.store_url;
        }
        if (copts.consumer_key) {
            rv.consumer_key = copts.consumer_key;
        }
        if (copts.consumer_secret) {
            rv.consumer_secret = copts.consumer_secret;
        }
        foreach string key in (keys copts.woocommercerestclient_options) {
            rv{key} = copts.woocommercerestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "order_statuses": True,
            "product_types": True,
            "product_statuses": True,
            "stock_statuses": True,
            "discount_types": True,
            "webhook_statuses": True,
            "categories": True,
            "tags": True,
            "customers": True,
            "products": True,
            "shipping_zones": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "order_statuses": return OrderStatusAllowedValues;
            case "product_types": return ProductTypeAllowedValues;
            case "product_statuses": return ProductStatusAllowedValues;
            case "stock_statuses": return StockStatusAllowedValues;
            case "discount_types": return DiscountTypeAllowedValues;
            case "webhook_statuses": return WebhookStatusAllowedValues;
            case "categories": return getReferenceCategories();
            case "tags": return getReferenceTags();
            case "customers": return getReferenceCustomers();
            case "products": return getReferenceProducts();
            case "shipping_zones": return getReferenceShippingZones();
        }
    }

    #! Returns available product categories for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCategories() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("products/categories", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> cat in (response) {
                    rv += <AllowedValueInfo>{
                        "display_name": cat.name ?? sprintf("Category %d", cat.id),
                        "short_desc": cat.slug ?? "",
                        "desc": cat.name ?? sprintf("Category %d", cat.id),
                        "value": cat.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceCategories: error fetching categories: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available product tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("products/tags", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> tag in (response) {
                    rv += <AllowedValueInfo>{
                        "display_name": tag.name ?? sprintf("Tag %d", tag.id),
                        "short_desc": tag.slug ?? "",
                        "desc": tag.name ?? sprintf("Tag %d", tag.id),
                        "value": tag.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceTags: error fetching tags: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("customers", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> customer in (response) {
                    string display_name;
                    if (customer.first_name || customer.last_name) {
                        display_name = trim(sprintf("%s %s", customer.first_name ?? "", customer.last_name ?? ""));
                    } else {
                        display_name = customer.email ?? sprintf("Customer %d", customer.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer.email ?? "",
                        "desc": sprintf("%s (%s)", display_name, customer.email ?? "no email"),
                        "value": customer.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceCustomers: error fetching customers: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("products", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> product in (response) {
                    string display_name = product.name ?? sprintf("Product %d", product.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": product.sku ?? "",
                        "desc": sprintf("%s (%s)", display_name, product.sku ?? "no SKU"),
                        "value": product.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceProducts: error fetching products: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available shipping zones for reference data
    private *list<hash<AllowedValueInfo>> getReferenceShippingZones() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("shipping/zones");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> zone in (response) {
                    rv += <AllowedValueInfo>{
                        "display_name": zone.name ?? sprintf("Zone %d", zone.id),
                        "short_desc": "",
                        "desc": zone.name ?? sprintf("Zone %d", zone.id),
                        "value": zone.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceShippingZones: error fetching zones: %s", ex.desc);
        }
        return rv;
    }
}

} # namespace WooCommerceDataProvider
