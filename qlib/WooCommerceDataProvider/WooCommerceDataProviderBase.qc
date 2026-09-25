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
                "short_desc": "The WooCommerceRestClientIo object",
                "type": AbstractDataProviderType::get(new Type("WooCommerceRestClient::WooCommerceRestClientIo")),
                "desc": "The `WooCommerceRestClientIo` object to use for API calls",
            },
            "woocommercerestclient_options": <DataProviderOptionInfo>{
                "display_name": "WooCommerce REST Client Options",
                "short_desc": "Options for creating a WooCommerceRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a `WooCommerceRestClientIo` object if one is not provided directly",
            },
            "store_url": <DataProviderOptionInfo>{
                "display_name": "Store URL",
                "short_desc": "WooCommerce store URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce store URL (e.g., `https://mystore.com`)",
                "required": True,
            },
            "consumer_key": <DataProviderOptionInfo>{
                "display_name": "Consumer Key",
                "short_desc": "WooCommerce consumer key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce consumer key for authentication (used as Basic Auth username)",
                "sensitive": True,
                "required": True,
            },
            "consumer_secret": <DataProviderOptionInfo>{
                "display_name": "Consumer Secret",
                "short_desc": "WooCommerce consumer secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The WooCommerce consumer secret for authentication (used as Basic Auth password)",
                "sensitive": True,
                "required": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        WooCommerceRestClient::WooCommerceRestClientIo rest;

        #! The connection object, if applicable
        *WooCommerceRestClient::WooCommerceRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    /** @param rest the REST client for API calls
    */
    constructor(WooCommerceRestClient::WooCommerceRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    /** @param conn the connection; API calls are made with the REST client returned by its
        @ref ConnectionProvider::AbstractConnection::getAsync() "getAsync()" method
    */
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync();
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
            rest = new WooCommerceRestClient::WooCommerceRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    WooCommerceRestClient::WooCommerceRestClientIo getRestClient() {
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

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    auto doGet(string path, *hash<auto> params) {
        return doApiRequest("GET", getRequestPath(path, params)).body;
    }

    #! Makes a GET request returning both body and pagination headers
    /** @param path the API path
        @param params optional query parameters
        @return a hash with \c body (response data), \c total (total items), and \c total_pages

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    hash<auto> doGetPaginated(string path, *hash<auto> params) {
        hash<RestClient::RestResponseInfo> response = doApiRequest("GET", getRequestPath(path, params));
        *string total = RestClient::RestClient::getSingleHeaderValue(response.headers."x-wp-total");
        *string total_pages = RestClient::RestClient::getSingleHeaderValue(response.headers."x-wp-totalpages");
        return {
            "body": response.body,
            "total": total ? total.toInt() : 0,
            "total_pages": total_pages ? total_pages.toInt() : 0,
        };
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", encode_url(key, True), encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Returns the request path with any query parameters
    private string getRequestPath(string path, *hash<auto> params) {
        if (params) {
            string query = buildQueryString(params);
            if (query.val()) {
                path += "?" + query;
            }
        }
        return path;
    }

    #! Makes a POST request to the WooCommerce API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doApiRequest("POST", path, body).body ?? {};
    }

    #! Makes a PUT request to the WooCommerce API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doApiRequest("PUT", path, body).body ?? {};
    }

    #! Makes a DELETE request to the WooCommerce API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    hash<auto> doDelete(string path, *hash<auto> params) {
        return doApiRequest("DELETE", getRequestPath(path, params)).body ?? {};
    }

    #! Makes a request to the WooCommerce API and returns the response
    /** Transport errors are retried up to @ref MaxRetries times: errors in @ref RetrySet for all requests, and
        errors in @ref IdempotentRetrySet for requests with an idempotent method.

        @param method the HTTP method
        @param path the API path, including any query string
        @param body the request body, if any

        @return the response

        @throw WOOCOMMERCE-API-ERROR the server returned an error response; the exception argument is the response
        (a @ref RestClient::RestResponseInfo "RestResponseInfo" hash)
    */
    private hash<RestClient::RestResponseInfo> doApiRequest(string method, string path, *hash<auto> body) {
        int retries = 0;
        while (True) {
            try {
                return rest.restDoRequest(method, path, body);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.typeCode() == NT_HASH) {
                    throwApiError(ex.arg);
                }
                bool retriable = IdempotentMethods{method} ? IdempotentRetrySet{ex.err} : RetrySet{ex.err};
                if (retriable && ++retries <= MaxRetries) {
                    log(LoggerLevel::WARN, "WooCommerce %s %s: %s: %s; retrying (%d/%d)", method, path, ex.err,
                        ex.desc, retries, MaxRetries);
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Throws a \c WOOCOMMERCE-API-ERROR exception for an error response
    /** @param response the error response

        @throw WOOCOMMERCE-API-ERROR always thrown; the description is the error message of the response, and the
        argument is the response
    */
    private static throwApiError(hash<auto> response) {
        string err_msg;
        if (response.body.typeCode() == NT_HASH && response.body.message) {
            err_msg = sprintf("%s (HTTP %d)", response.body.message, response.status_code);
        } else if (response.body.typeCode() == NT_HASH && response.body.code) {
            err_msg = sprintf("WooCommerce API error: %s (HTTP %d)", response.body.code, response.status_code);
        } else {
            err_msg = sprintf("WooCommerce API error (HTTP %d)", response.status_code);
        }
        throw "WOOCOMMERCE-API-ERROR", err_msg, response;
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
            "orders": True,
            "coupons": True,
            "webhooks": True,
            "tax_rates": True,
            "variations": True,
            "order_notes": True,
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
            case "orders": return getReferenceOrders();
            case "coupons": return getReferenceCoupons();
            case "webhooks": return getReferenceWebhooks();
            case "tax_rates": return getReferenceTaxRates();
            case "variations": return getReferenceVariations(action_opts);
            case "order_notes": return getReferenceOrderNotes(action_opts);
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
                        "value": int(cat.id),
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
                        "value": int(tag.id),
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
                        "value": int(customer.id),
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
                        "value": int(product.id),
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
                        "value": int(zone.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceShippingZones: error fetching zones: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("orders", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> order in (response) {
                    string display_name = sprintf("Order #%d", order.id);
                    string status = order.status ?? "unknown";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": status,
                        "desc": sprintf("%s — status: %s, total: %s %s", display_name, status,
                            order.total ?? "0", order.currency ?? ""),
                        "value": int(order.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceOrders: error fetching orders: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available coupons for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCoupons() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("coupons", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> coupon in (response) {
                    string display_name = coupon.code ?? sprintf("Coupon %d", coupon.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": coupon.discount_type ?? "",
                        "desc": sprintf("%s — %s: %s", display_name, coupon.discount_type ?? "discount",
                            coupon.amount ?? "0"),
                        "value": int(coupon.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceCoupons: error fetching coupons: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available webhooks for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWebhooks() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("webhooks", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> webhook in (response) {
                    string display_name = webhook.name ?? sprintf("Webhook %d", webhook.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": webhook.topic ?? "",
                        "desc": sprintf("%s — topic: %s, status: %s", display_name,
                            webhook.topic ?? "unknown", webhook.status ?? "unknown"),
                        "value": int(webhook.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceWebhooks: error fetching webhooks: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available tax rates for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTaxRates() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("taxes", {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> tax in (response) {
                    string display_name = tax.name ?? sprintf("Tax Rate %d", tax.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": tax.rate ?? "",
                        "desc": sprintf("%s — rate: %s%%, country: %s", display_name,
                            tax.rate ?? "0", tax.country ?? "*"),
                        "value": int(tax.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceTaxRates: error fetching tax rates: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available product variations for reference data (cascading: requires product_id in action_opts)
    private *list<hash<AllowedValueInfo>> getReferenceVariations(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        # variations are scoped to a product; without product_id we cannot enumerate
        *softint product_id = action_opts.product_id;
        if (!product_id) {
            return rv;
        }
        try {
            auto response = doGet(sprintf("products/%d/variations", product_id), {"per_page": "100"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> variation in (response) {
                    string display_name = variation.sku ? sprintf("Variation %s", variation.sku)
                        : sprintf("Variation %d", variation.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": variation.sku ?? "",
                        "desc": sprintf("%s — price: %s", display_name, variation.price ?? "0"),
                        "value": int(variation.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceVariations: error fetching variations: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available order notes for reference data (cascading: requires order_id in action_opts)
    private *list<hash<AllowedValueInfo>> getReferenceOrderNotes(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        *softint order_id = action_opts.order_id;
        if (!order_id) {
            return rv;
        }
        try {
            auto response = doGet(sprintf("orders/%d/notes", order_id));
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> note in (response) {
                    string display_name = sprintf("Note %d", note.id);
                    string preview = note.note ?? "";
                    if (preview.length() > 40) {
                        preview = preview.substr(0, 37) + "...";
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": preview,
                        "desc": sprintf("%s — %s", display_name, preview),
                        "value": int(note.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceOrderNotes: error fetching notes: %s", ex.desc);
        }
        return rv;
    }
}

} # namespace WooCommerceDataProvider
