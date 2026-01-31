# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Base class for Zoho Books data providers
public class ZohoBooksDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Zoho Books data providers
        const BaseConstructorOptions = {
            "zohobooksrestclient": <DataProviderOptionInfo>{
                "display_name": "Zoho Books REST Client",
                "short_desc": "The ZohoBooksRestClient object",
                "type": AbstractDataProviderType::get(new Type("ZohoBooksRestClient::ZohoBooksRestClient")),
                "desc": "The ZohoBooksRestClient object to use for API calls",
            },
            "zohobooksrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Zoho Books REST Client Options",
                "short_desc": "Options for creating a ZohoBooksRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ZohoBooksRestClient object if one is not provided directly",
            },
            "subdomain": <DataProviderOptionInfo>{
                "display_name": "Sub Domain",
                "short_desc": "Zoho subdomain",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Zoho subdomain for your account (zoho or zohocloud)",
                "default_value": ZohoBooksRestClient::ZohoBooksRestClient::DefaultSubdomain,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "zoho",
                        "desc": "Standard Zoho subdomain",
                    },
                    <AllowedValueInfo>{
                        "value": "zohocloud",
                        "desc": "Zoho Cloud subdomain",
                    },
                ),
            },
            "domain": <DataProviderOptionInfo>{
                "display_name": "Domain",
                "short_desc": "Zoho domain",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Zoho domain for your data center region",
                "default_value": ZohoBooksRestClient::ZohoBooksRestClient::DefaultDomain,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": ".com",
                        "desc": "United States / Global data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".eu",
                        "desc": "Europe data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".in",
                        "desc": "India data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".com.au",
                        "desc": "Australia data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".ca",
                        "desc": "Canada data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".sa",
                        "desc": "Saudi Arabia data center",
                    },
                    <AllowedValueInfo>{
                        "value": ".jp",
                        "desc": "Japan data center",
                    },
                ),
            },
        };
    }

    private {
        #! The REST client object for API calls
        ZohoBooksRestClient::ZohoBooksRestClient rest;

        #! The connection object, if applicable
        *ZohoBooksRestClient::ZohoBooksRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ZohoBooksRestClient::ZohoBooksRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.zohobooksrestclient) {
            rest = copts.zohobooksrestclient;
        } else {
            hash<auto> client_opts = copts.zohobooksrestclient_options ?? {};
            if (copts.subdomain) {
                client_opts.subdomain = copts.subdomain;
            }
            if (copts.domain) {
                client_opts.domain = copts.domain;
            }
            rest = new ZohoBooksRestClient::ZohoBooksRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ZohoBooksRestClient::ZohoBooksRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ZohoBooksRestClient::ZohoBooksRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Zoho Books API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Zoho Books API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Zoho Books API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Zoho Books API
    /** @param path the API path
        @return the response body

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> info;
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.get(path, NOTHING, \info);
                        break;
                    case "POST":
                        response = rest.post(path, body, \info);
                        break;
                    case "PUT":
                        response = rest.put(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "ZOHOBOOKS-ERROR", sprintf("unsupported HTTP method: %s", method);
                }

                # Check for rate limiting (headers are lowercase in RestClient)
                if (info."response-headers"."x-rate-limit-remaining" == "0") {
                    *string reset = info."response-headers"."x-rate-limit-reset";
                    if (reset.val()) {
                        int reset_secs = reset.toInt();
                        if (reset_secs > 0 && reset_secs <= 60) {
                            # Wait for rate limit reset (up to 60 seconds)
                            sleep(reset_secs * 1000ms);
                        }
                    }
                }

                checkResponse(response);
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Checks if a request should be retried
    /** @param ex the exception info
        @param retries reference to retry counter
        @return True if the request should be retried
    */
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        # Retry on certain socket errors
        if (RetrySet{ex.err}) {
            ++retries;
            # Exponential backoff
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            # Wait before retrying rate-limited requests
            sleep(5000ms);
            return True;
        }
        return False;
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

    #! Checks the Zoho Books API response for errors
    /** @param response the API response hash

        @throw ZOHOBOOKS-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Zoho Books API returns code and message in the body
        if (response.body.code.val() && response.body.code != 0) {
            string err_msg = response.body.message ?? "Unknown Zoho Books API error";
            throw "ZOHOBOOKS-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.subdomain) {
            rv.subdomain = copts.subdomain;
        }
        if (copts.domain) {
            rv.domain = copts.domain;
        }
        foreach string key in (keys copts.zohobooksrestclient_options) {
            rv{key} = copts.zohobooksrestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "organizations": True,
            "contacts": True,
            "customers": True,
            "vendors": True,
            "items": True,
            "currencies": True,
            "taxes": True,
            "invoices": True,
            "bills": True,
            "payments": True,
            "estimates": True,
            "salesorders": True,
            "purchaseorders": True,
            "expenses": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        # Extract organization_id from action_opts if provided
        *string org_id = action_opts.organization_id;

        switch (type) {
            case "organizations": return getReferenceOrganizations();
            case "contacts": return getReferenceContacts(NOTHING, org_id);
            case "customers": return getReferenceContacts("customer", org_id);
            case "vendors": return getReferenceContacts("vendor", org_id);
            case "items": return getReferenceItems(org_id);
            case "currencies": return getReferenceCurrencies(org_id);
            case "taxes": return getReferenceTaxes(org_id);
            case "invoices": return getReferenceInvoices(org_id);
            case "bills": return getReferenceBills(org_id);
            case "payments": return getReferencePayments(org_id);
            case "estimates": return getReferenceEstimates(org_id);
            case "salesorders": return getReferenceSalesOrders(org_id);
            case "purchaseorders": return getReferencePurchaseOrders(org_id);
            case "expenses": return getReferenceExpenses(org_id);
        }
    }

    #! Returns available organizations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrganizations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # Note: /organizations endpoint doesn't require organization_id
            hash<auto> response = rest.get("/organizations");
            if (response.body.organizations) {
                foreach hash<auto> org in (response.body.organizations) {
                    string display_name = org.name ?? sprintf("Organization %s", org.organization_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": org.organization_id,
                        "desc": sprintf("%s (ID: %s)", display_name, org.organization_id),
                        "value": org.organization_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available contacts for reference data
    /** @param contact_type optional contact type filter ("customer" or "vendor")
        @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceContacts(*string contact_type, *string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            if (contact_type) {
                params.contact_type = contact_type;
            }
            hash<auto> response = doGet("/contacts", params);
            if (response.contacts) {
                foreach hash<auto> contact in (response.contacts) {
                    string display_name = contact.contact_name ?? sprintf("Contact %s", contact.contact_id);
                    string type_label = contact.contact_type == "customer" ? "Customer" : "Vendor";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": type_label,
                        "desc": sprintf("%s (%s, ID: %s)", display_name, type_label, contact.contact_id),
                        "value": contact.contact_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available items for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceItems(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/items", params);
            if (response.items) {
                foreach hash<auto> item in (response.items) {
                    string display_name = item.name ?? sprintf("Item %s", item.item_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": item.sku ?? "",
                        "desc": sprintf("%s (SKU: %s, ID: %s)", display_name, item.sku ?? "N/A", item.item_id),
                        "value": item.item_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available currencies for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceCurrencies(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/settings/currencies", params);
            if (response.currencies) {
                foreach hash<auto> currency in (response.currencies) {
                    string display_name = sprintf("%s (%s)", currency.currency_name, currency.currency_code);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": currency.currency_code,
                        "desc": sprintf("%s - %s", currency.currency_code, currency.currency_name),
                        "value": currency.currency_code,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available taxes for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceTaxes(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/settings/taxes", params);
            if (response.taxes) {
                foreach hash<auto> tax in (response.taxes) {
                    string display_name = sprintf("%s (%s%%)", tax.tax_name, tax.tax_percentage);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("%s%%", tax.tax_percentage),
                        "desc": sprintf("%s - %s%%", tax.tax_name, tax.tax_percentage),
                        "value": tax.tax_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available invoices for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceInvoices(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/invoices", params);
            if (response.invoices) {
                foreach hash<auto> invoice in (response.invoices) {
                    string display_name = invoice.invoice_number ?? sprintf("Invoice %s", invoice.invoice_id);
                    string customer = invoice.customer_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, invoice.invoice_id),
                        "value": invoice.invoice_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available bills for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceBills(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/bills", params);
            if (response.bills) {
                foreach hash<auto> bill in (response.bills) {
                    string display_name = bill.bill_number ?? sprintf("Bill %s", bill.bill_id);
                    string vendor = bill.vendor_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": vendor,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, vendor, bill.bill_id),
                        "value": bill.bill_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available payments for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferencePayments(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/customerpayments", params);
            if (response.customerpayments) {
                foreach hash<auto> payment in (response.customerpayments) {
                    string display_name = payment.payment_number ?? sprintf("Payment %s", payment.payment_id);
                    string customer = payment.customer_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, payment.payment_id),
                        "value": payment.payment_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available estimates for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceEstimates(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/estimates", params);
            if (response.estimates) {
                foreach hash<auto> estimate in (response.estimates) {
                    string display_name = estimate.estimate_number ?? sprintf("Estimate %s", estimate.estimate_id);
                    string customer = estimate.customer_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, estimate.estimate_id),
                        "value": estimate.estimate_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available sales orders for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceSalesOrders(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/salesorders", params);
            if (response.salesorders) {
                foreach hash<auto> order in (response.salesorders) {
                    string display_name = order.salesorder_number ?? sprintf("Sales Order %s", order.salesorder_id);
                    string customer = order.customer_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, order.salesorder_id),
                        "value": order.salesorder_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available purchase orders for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferencePurchaseOrders(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/purchaseorders", params);
            if (response.purchaseorders) {
                foreach hash<auto> order in (response.purchaseorders) {
                    string display_name = order.purchaseorder_number ?? sprintf("Purchase Order %s", order.purchaseorder_id);
                    string vendor = order.vendor_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": vendor,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, vendor, order.purchaseorder_id),
                        "value": order.purchaseorder_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available expenses for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceExpenses(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/expenses", params);
            if (response.expenses) {
                foreach hash<auto> expense in (response.expenses) {
                    string display_name = expense.description ?? sprintf("Expense %s", expense.expense_id);
                    string account = expense.account_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": account,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, account, expense.expense_id),
                        "value": expense.expense_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
}

} # namespace ZohoBooksDataProvider
