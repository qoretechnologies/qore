# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

#! Base class for Zoho Inventory data providers
public class ZohoInventoryDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Organization ID field for request types - added to all DPAT_API request types
        const OrganizationIdField = {
            "organization_id": {
                "type": SoftStringOrNothingType,
                "display_name": "Organization",
                "short_desc": "Zoho Inventory organization ID",
                "desc": "The Zoho Inventory organization ID for this operation",
            },
        };

        #! Base constructor options for Zoho Inventory data providers
        const BaseConstructorOptions = {
            "zohoinventoryrestclient": <DataProviderOptionInfo>{
                "display_name": "Zoho Inventory REST Client",
                "short_desc": "The ZohoInventoryRestClient object",
                "type": AbstractDataProviderType::get(new Type("ZohoInventoryRestClient::ZohoInventoryRestClient")),
                "desc": "The ZohoInventoryRestClient object to use for API calls",
            },
            "zohoinventoryrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Zoho Inventory REST Client Options",
                "short_desc": "Options for creating a ZohoInventoryRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ZohoInventoryRestClient object if one is not provided directly",
            },
            "subdomain": <DataProviderOptionInfo>{
                "display_name": "Sub Domain",
                "short_desc": "Zoho subdomain",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Zoho subdomain for your account (zoho or zohocloud)",
                "default_value": ZohoInventoryRestClient::ZohoInventoryRestClient::DefaultSubdomain,
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
                "default_value": ZohoInventoryRestClient::ZohoInventoryRestClient::DefaultDomain,
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
        ZohoInventoryRestClient::ZohoInventoryRestClient rest;

        #! The connection object, if applicable
        *ZohoInventoryRestClient::ZohoInventoryRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ZohoInventoryRestClient::ZohoInventoryRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.zohoinventoryrestclient) {
            rest = copts.zohoinventoryrestclient;
        } else {
            hash<auto> client_opts = copts.zohoinventoryrestclient_options ?? {};
            if (copts.subdomain) {
                client_opts.subdomain = copts.subdomain;
            }
            if (copts.domain) {
                client_opts.domain = copts.domain;
            }
            rest = new ZohoInventoryRestClient::ZohoInventoryRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ZohoInventoryRestClient::ZohoInventoryRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ZohoInventoryRestClient::ZohoInventoryRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Extracts organization_id from a request hash, sets it on the REST client, and returns the cleaned request
    /** Used by all DPAT_API providers to handle organization_id from action options
    */
    private hash<auto> setOrganizationIdFromReq(hash<auto> req) {
        *string org_id = remove req.organization_id;
        if (org_id) {
            rest.setOrganizationId(org_id);
        }
        return req;
    }

    #! Makes a GET request to the Zoho Inventory API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Zoho Inventory API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Zoho Inventory API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Zoho Inventory API
    /** @param path the API path
        @return the response body

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
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
                        throw "ZOHOINVENTORY-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Zoho Inventory API response for errors
    /** @param response the API response hash

        @throw ZOHOINVENTORY-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Zoho Inventory API returns code and message in the body
        if (response.body.code.val() && response.body.code != 0) {
            string err_msg = response.body.message ?? "Unknown Zoho Inventory API error";
            throw "ZOHOINVENTORY-API-ERROR", err_msg, response.body;
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
        foreach string key in (keys copts.zohoinventoryrestclient_options) {
            rv{key} = copts.zohoinventoryrestclient_options{key};
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
            "item_groups": True,
            "salesorders": True,
            "purchaseorders": True,
            "packages": True,
            "invoices": True,
            "shipmentorders": True,
            "inventory_adjustments": True,
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
            case "item_groups": return getReferenceItemGroups(org_id);
            case "salesorders": return getReferenceSalesOrders(org_id);
            case "purchaseorders": return getReferencePurchaseOrders(org_id);
            case "packages": return getReferencePackages(org_id);
            case "invoices": return getReferenceInvoices(org_id);
            case "shipmentorders": return getReferenceShipmentOrders(org_id);
            case "inventory_adjustments": return getReferenceInventoryAdjustments(org_id);
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
                    string type_label;
                    switch (contact.contact_type) {
                        case "customer": type_label = "Customer"; break;
                        case "vendor": type_label = "Vendor"; break;
                        default: type_label = "Contact"; break;
                    }
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
                    if (item.sku.val()) {
                        display_name += sprintf(" (%s)", item.sku);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": item.sku ?? "",
                        "desc": sprintf("%s (SKU: %s, ID: %s)", item.name ?? "Item", item.sku ?? "N/A",
                            item.item_id),
                        "value": item.item_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available item groups for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceItemGroups(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/itemgroups", params);
            if (response.itemgroups) {
                foreach hash<auto> group in (response.itemgroups) {
                    string display_name = group.group_name ?? sprintf("Item Group %s", group.group_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": group.group_id,
                        "desc": sprintf("%s (ID: %s)", display_name, group.group_id),
                        "value": group.group_id,
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
                    string display_name = order.salesorder_number
                        ?? sprintf("Sales Order %s", order.salesorder_id);
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
                    string display_name = order.purchaseorder_number
                        ?? sprintf("Purchase Order %s", order.purchaseorder_id);
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

    #! Returns available packages for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferencePackages(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/packages", params);
            if (response.packages) {
                foreach hash<auto> pkg in (response.packages) {
                    string display_name = pkg.package_number ?? sprintf("Package %s", pkg.package_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": pkg.package_id,
                        "desc": sprintf("%s (ID: %s)", display_name, pkg.package_id),
                        "value": pkg.package_id,
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
                    string display_name = invoice.invoice_number
                        ?? sprintf("Invoice %s", invoice.invoice_id);
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

    #! Returns available shipment orders for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceShipmentOrders(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/shipmentorders", params);
            if (response.shipment_orders) {
                foreach hash<auto> shipment in (response.shipment_orders) {
                    string display_name = shipment.shipment_number
                        ?? sprintf("Shipment %s", shipment.shipment_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": shipment.shipment_id,
                        "desc": sprintf("%s (ID: %s)", display_name, shipment.shipment_id),
                        "value": shipment.shipment_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available inventory adjustments for reference data
    /** @param org_id optional organization ID from action options
    */
    private *list<hash<AllowedValueInfo>> getReferenceInventoryAdjustments(*string org_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (org_id) {
                params.organization_id = org_id;
            }
            hash<auto> response = doGet("/inventoryadjustments", params);
            if (response.inventory_adjustments) {
                foreach hash<auto> adj in (response.inventory_adjustments) {
                    string display_name = adj.reference_number
                        ?? sprintf("Adjustment %s", adj.inventory_adjustment_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": adj.inventory_adjustment_id,
                        "desc": sprintf("%s (ID: %s)", display_name, adj.inventory_adjustment_id),
                        "value": adj.inventory_adjustment_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
}

} # namespace ZohoInventoryDataProvider
