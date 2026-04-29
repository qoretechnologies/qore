# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Base class for Segment data providers
public class SegmentDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options for Segment data providers
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClientIo::RestClientIo"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The REST client object to use for Segment API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options for creating a REST client object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Segment Public API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Segment Public API key for Bearer token authentication",
                "sensitive": True,
            },
            "region": <DataProviderOptionInfo>{
                "display_name": "Region",
                "short_desc": "Segment data center region",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Segment data center region (`us` or `eu`)",
                "default_value": "us",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "us", "display_name": "US", "short_desc": "United States",
                        "desc": "US data center (api.segmentapis.com)"},
                    <AllowedValueInfo>{"value": "eu", "display_name": "EU", "short_desc": "European Union",
                        "desc": "EU data center (eu1.api.segmentapis.com)"},
                ),
            },
        };

        #! I/O errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }

        if (!rest) {
            if (!copts.apikey) {
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Segment API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "apikey": copts.apikey,
            };
            if (copts.region) {
                opts.region = copts.region;
            }
            rest = new SegmentRestClient::SegmentRestClientIo(opts);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Makes a REST call with I/O retry support
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                return rest.restDoRequest(method, path, body, hdr);
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err}) {
                    if (++retries == MaxIoRetries) {
                        rethrow;
                    }
                    continue;
                }
                rethrow ex.err, sprintf("%s (HTTP response code: %y)", ex.desc, ex.arg.status_code);
            }
        }
    }

    #! Makes a GET request to the Segment API
    auto doGet(string path) {
        hash<auto> response = doRestCommand("GET", path, NOTHING);
        return response.body;
    }

    #! Makes a POST request to the Segment API
    auto doPost(string path, auto body) {
        hash<auto> response = doRestCommand("POST", path, body);
        return response.body;
    }

    #! Makes a PATCH request to the Segment API
    auto doPatch(string path, auto body) {
        hash<auto> response = doRestCommand("PATCH", path, body);
        return response.body;
    }

    #! Makes a DELETE request to the Segment API
    auto doDelete(string path, auto body) {
        hash<auto> response = doRestCommand("DELETE", path, body);
        return response.body;
    }

    #! Makes a paginated GET request, collecting all pages
    /** @param path the API path (e.g., "sources")
        @param count optional page size (default: DefaultPageSize, max: MaxPageSize)
        @return the list of all records across all pages
    */
    list<auto> doPaginatedGet(string path, *int count) {
        int page_size = count ?? DefaultPageSize;
        if (page_size > MaxPageSize) {
            page_size = MaxPageSize;
        }
        list<auto> results = ();
        *string cursor;
        while (True) {
            string sep = (path =~ /\?/) ? "&" : "?";
            string url = sprintf("%s%spagination.count=%d", path, sep, page_size);
            if (cursor) {
                url += sprintf("&pagination.cursor=%s", encode_url(cursor, True));
            }
            auto response = doGet(url);
            if (response.data.typeCode() == NT_HASH) {
                # Segment wraps results in data.{resource_name} or just data
                # Find the list in the response
                auto items = getListFromResponse(response.data);
                if (items) {
                    results += items;
                }
                # Check for next page cursor
                cursor = response.data.pagination.next;
                if (!cursor) {
                    break;
                }
            } else {
                break;
            }
        }
        return results;
    }

    #! Extracts the list of records from a Segment API response
    /** Segment responses wrap lists in data.{resource_name}, e.g., data.sources, data.destinations
    */
    private *list<auto> getListFromResponse(hash<auto> data) {
        # Try common response keys - Segment uses the plural resource name
        foreach string key in (keys data) {
            if (key == "pagination") {
                continue;
            }
            if (data{key}.typeCode() == NT_LIST) {
                return data{key};
            }
        }
    }

    #! Builds a URL query string from a hash of parameters
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

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "sources": True,
            "destinations": True,
            "warehouses": True,
            "tracking_plans": True,
            "catalog_sources": True,
            "catalog_destinations": True,
            "catalog_warehouses": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "sources": return getReferenceSources();
            case "destinations": return getReferenceDestinations();
            case "warehouses": return getReferenceWarehouses();
            case "tracking_plans": return getReferenceTrackingPlans();
            case "catalog_sources": return getReferenceCatalog("catalog/sources");
            case "catalog_destinations": return getReferenceCatalog("catalog/destinations");
            case "catalog_warehouses": return getReferenceCatalog("catalog/warehouses");
        }
    }

    #! Returns catalog entries (sources, destinations, or warehouses) as reference data
    /** The catalog records use slug-based IDs of the form `catalog/<kind>/<slug>`, which is the
        value the create endpoints accept as `metadataId`.
    */
    private *list<hash<AllowedValueInfo>> getReferenceCatalog(string path) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> records = doPaginatedGet(path);
            foreach hash<auto> rec in (records) {
                rv += <AllowedValueInfo>{
                    "display_name": rec.name ?? rec.slug ?? sprintf("Catalog entry %s", rec.id),
                    "short_desc": rec.slug ?? "",
                    "value": string(rec.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors fetching reference data
        }
        return rv;
    }

    #! Returns available sources for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSources() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> records = doPaginatedGet("sources");
            foreach hash<auto> rec in (records) {
                rv += <AllowedValueInfo>{
                    "display_name": rec.name ?? rec.slug ?? sprintf("Source %s", rec.id),
                    "short_desc": rec.slug ?? "",
                    "value": string(rec.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors fetching reference data
        }
        return rv;
    }

    #! Returns available destinations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceDestinations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> records = doPaginatedGet("destinations");
            foreach hash<auto> rec in (records) {
                rv += <AllowedValueInfo>{
                    "display_name": rec.name ?? sprintf("Destination %s", rec.id),
                    "short_desc": rec.metadata.name ?? "",
                    "value": string(rec.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors fetching reference data
        }
        return rv;
    }

    #! Returns available warehouses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWarehouses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> records = doPaginatedGet("warehouses");
            foreach hash<auto> rec in (records) {
                rv += <AllowedValueInfo>{
                    "display_name": rec.metadata.name ?? sprintf("Warehouse %s", rec.id),
                    "short_desc": rec.metadata.slug ?? "",
                    "value": string(rec.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors fetching reference data
        }
        return rv;
    }

    #! Returns available tracking plans for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTrackingPlans() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> records = doPaginatedGet("tracking-plans");
            foreach hash<auto> rec in (records) {
                rv += <AllowedValueInfo>{
                    "display_name": rec.name ?? sprintf("Tracking Plan %s", rec.id),
                    "short_desc": rec.type ?? "",
                    "value": string(rec.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors fetching reference data
        }
        return rv;
    }
}

} # namespace SegmentDataProvider
