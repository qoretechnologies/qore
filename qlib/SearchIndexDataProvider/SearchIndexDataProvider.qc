# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SearchIndexDataProvider classes

/** SearchIndexDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains public definitions in the SearchIndexDataProvider module
public namespace SearchIndexDataProvider {
#! Constructor option definitions for REST-backed search-index providers
/** @param backend_name user-facing backend name used in descriptions
    @return constructor options for a REST-backed search-index provider
*/
public hash<auto> sub getRestConstructorOptions(string backend_name) {
    return {
        "restclient": <DataProviderOptionInfo>{
            "display_name": "REST Client",
            "short_desc": sprintf("The REST client for accessing the %s server", backend_name),
            "type": AbstractDataProviderType::get(new Type("AbstractRestClient")),
            "desc": sprintf("The REST client for accessing the %s server; must be an `AbstractRestClient` subclass. "
                "Use `RestClientIo` for async, multiplexed HTTP/1.1/2/3 or `RestClient` for sync HTTP/1.x.",
                backend_name),
        },
        "url": <DataProviderOptionInfo>{
            "display_name": "URL",
            "short_desc": sprintf("The URL to the %s server", backend_name),
            "type": AbstractDataProviderTypeMap."string",
            "desc": sprintf("The URL to the %s REST server; overrides any URL in a REST client object passed as "
                "an option", backend_name),
        },
        "restclient_options": <DataProviderOptionInfo>{
            "display_name": "REST Client Options",
            "short_desc": "Options to any REST client created",
            "type": AbstractDataProviderTypeMap."*hash",
            "desc": "Options to the `RestClientIo` constructor; only used if a `RestClientIo` object is created "
                "for the request",
        },
    };
}

#! Common REST-backed data provider base class
/** Concrete data-provider modules can inherit this class to share REST client storage and logger propagation. */
public class SearchIndexDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The REST client; any @ref RestClient::AbstractRestClient "AbstractRestClient" subclass
        RestClient::AbstractRestClient rest;
    }

    #! Creates the base object without a REST client
    constructor() {
    }

    #! Creates the base object from a REST connection
    /** @param rest REST client to use for backend requests */
    constructor(RestClient::AbstractRestClient rest) {
        self.rest = rest;
    }

    #! Accepts a LoggerInterface object for logging or clears it
    /** @param logger logger to use, or @ref NOTHING to clear the logger */
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }
}

#! Common REST client helper methods for search-index providers
public class SearchIndexRestUtil {
    #! Creates or adapts a REST client from constructor options
    /** @param options constructor options with `restclient`, `url`, or `restclient_options`
        @param backend_name user-facing backend name used in validation errors
        @return REST client for backend requests
        @throw CONSTRUCTOR-ERROR no REST connection information is provided
    */
    static public RestClient::AbstractRestClient getRestConnection(*hash<auto> options, string backend_name) {
        RestClient::AbstractRestClient rest;
        if (options.restclient) {
            rest = options.restclient;
            if (options.url) {
                rest = rest.copyWithUrl(options.url);
            }
        } else {
            if (!options.url && !options.restclient_options) {
                throw "CONSTRUCTOR-ERROR", sprintf("no 'restclient', 'url', or 'restclient_options' option passed; "
                    "cannot create REST client to %s instance without a URL", backend_name);
            }

            hash<auto> opts;
            opts += options.restclient_options;
            if (options.url) {
                opts.url = options.url;
            }
            rest = new RestClientIo(opts);
        }
        return rest;
    }

    #! Moves selected request fields into URI query parameters
    /** @param uri base URI
        @param req request hash reference; consumed query fields are removed
        @param query_args list of query argument names; a `_query` suffix is stripped from emitted parameter names
        @return URI with encoded query parameters appended
    */
    static public string moveQueryOptions(string uri, reference<hash<auto>> req, list<string> query_args) {
        bool q = uri.find("?") >= 0;
        foreach string arg in (query_args) {
            if (!exists req{arg}) {
                continue;
            }
            uri += q ? "&" : "?";
            q = True;

            string query_arg = arg;
            if (query_arg =~ /_query$/) {
                query_arg =~ s/_query$//;
            }
            uri += sprintf("%s=%s", query_arg, encode_url(string(remove req{arg}), True));
        }
        return uri;
    }
}

#! Boolean string type for REST query parameters
public class SearchIndexSoftBoolStringType inherits Qore::Reflection::Type {
    #! Creates the object
    constructor() : Type("*softstring") {
    }

    #! Returns the coerced REST query value
    /** @param value value to coerce with @ref Qore::parse_boolean() "parse_boolean()"
        @return `"true"` or `"false"`
    */
    auto acceptsValue(auto value) {
        return parse_boolean(value) ? "true" : "false";
    }

    #! Returns the default value
    /** @return `"false"` */
    auto getDefaultValue() {
        return "false";
    }
}

#! Boolean string type instance for REST query parameters
public const SoftBoolStringType = new SearchIndexSoftBoolStringType();

#! Boolean data provider string type for REST query parameters
public const SoftBoolDataProviderStringType = AbstractDataProviderType::get(SoftBoolStringType);

#! Common NDJSON helpers for search-index bulk APIs
public class SearchIndexBulkUtil {
    #! Formats explicit bulk operation hashes as newline-delimited JSON
    /** @param operations list of operation hashes with `action` and optional document metadata
        @param error_prefix error prefix to use when validation fails
        @return NDJSON request body
        @throw *-BULK-ERROR invalid operation or missing JSON support
    */
    static public string formatOperations(list<hash<auto>> operations, string error_prefix) {
%ifdef NoJson
        throw error_prefix, "the json module >= 1.11 is required for bulk operations";
%else
        NdjsonDataProvider::NdjsonStringWriter writer();

        foreach hash<auto> op in (operations) {
            string action = op.action;
            if (action !~ /^(index|create|update|delete)$/) {
                throw error_prefix, sprintf("Invalid action type '%s'; must be one of: index, create, update, "
                    "delete", action);
            }

            if ((action == "index" || action == "create") && !op.doc) {
                throw error_prefix, sprintf("Missing 'doc' field for '%s' operation; a document is required for "
                    "index and create actions", action);
            }

            hash<auto> action_meta = {};
            if (op._index) {
                action_meta._index = op._index;
            }
            if (op._id) {
                action_meta._id = op._id;
            }
            if (op.routing) {
                action_meta.routing = op.routing;
            }
            if (op.pipeline) {
                action_meta.pipeline = op.pipeline;
            }
            if (op.if_seq_no.typeCode() != NT_NOTHING) {
                action_meta.if_seq_no = op.if_seq_no;
            }
            if (op.if_primary_term.typeCode() != NT_NOTHING) {
                action_meta.if_primary_term = op.if_primary_term;
            }

            writer.write({action: action_meta});

            if (action != "delete" && op.doc) {
                if (action == "update") {
                    hash<auto> update_body = {};
                    if (op.doc) {
                        update_body.doc = op.doc;
                    }
                    if (op.doc_as_upsert) {
                        update_body.doc_as_upsert = op.doc_as_upsert;
                    }
                    if (op.script) {
                        update_body.script = op.script;
                    }
                    if (op.upsert) {
                        update_body.upsert = op.upsert;
                    }
                    writer.write(update_body);
                } else {
                    writer.write(op.doc);
                }
            }
        }

        return writer.getString();
%endif
    }

    #! Formats column-oriented record buffers as newline-delimited JSON
    /** @param records column-oriented record buffer from @ref DataProvider::AbstractDataProviderBulkOperation
        @param index target index
        @param action bulk action, normally `index`, `create`, `update`, or `delete`
        @param error_prefix error prefix to use when JSON support is unavailable
        @return NDJSON request body
        @throw *-BULK-ERROR missing JSON support
    */
    static public string formatRecordBuffer(hash<auto> records, string index, string action, string error_prefix) {
%ifdef NoJson
        throw error_prefix, "the json module >= 1.11 is required for bulk operations";
%else
        if (!records) {
            return "";
        }

        auto first = records.firstValue();
        if (!first) {
            return "";
        }

        NdjsonDataProvider::NdjsonStringWriter writer();
        int count = first.lsize();
        for (int i = 0; i < count; ++i) {
            hash<auto> record = map {$1: records{$1}[i]}, keys records;
            *string doc_id = remove record._id;

            hash<auto> action_meta = {
                "_index": index,
            };
            if (doc_id) {
                action_meta._id = doc_id;
            }

            writer.write({action: action_meta});

            if (action != "delete") {
                if (action == "update") {
                    writer.write({"doc": record});
                } else {
                    writer.write(record);
                }
            }
        }

        return writer.getString();
%endif
    }
}
}
