# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaDataProvider module definition

/** RestSchemaDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaDataProvider module
public namespace RestSchemaDataProvider {
#! The REST schema data provider class
public class RestSchemaDataProvider inherits RestSchemaDataProviderCommon {
    public {
        #! current URI path
        string uri_path = "/";

        #! The value to returns as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "RestSchemaDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": NOTHING,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "transaction_management": False,
            "supports_schema": True,
            "schema_type": "rest",
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "lax_parsing": <DataProviderOptionInfo>{
                "display_name": "Lax Parsing",
                "short_desc": "Sets lax parsing for REST schemas",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then common errors in REST schemas will be ignored; some schemas with "
                    "errors will be able to be used; this option is not recommended; schemas should be corrected "
                    "instead of setting this option",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("AbstractRestClient"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The REST client object to use for API calls; accepts either a "
                    "`RestClient` (synchronous HTTP/1.x transport) or a `RestClientIo` (async "
                    "transport supporting HTTP/1.x, HTTP/2, and HTTP/3 with connection multiplexing "
                    "on HTTP/2 and HTTP/3) — both implement `AbstractRestClient`. `RestClientIo` is "
                    "the recommended transport for new code and is the only transport that supports "
                    "HTTP/2 and HTTP/3. If a `url` option is also provided, a sibling client is built "
                    "via `AbstractRestClient::copyWithUrl()` so the supplied object is not mutated",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the `RestClientIo` constructor; only used if a `RestClientIo` object "
                    "is created for a call",
            },
            "schema": <DataProviderOptionInfo>{
                "display_name": "Schema",
                "short_desc": "The REST schema file or object",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromFile: True,
                    }),
                    AbstractDataProviderType::get(new Type("AbstractRestSchemaValidator")),
                ),
                "desc": "The REST schema file or object (Swagger 2.0 or OpenAPI 3.0)",
                "required_groups": ("schema",),
            },
            "schema_location": <DataProviderOptionInfo>{
                "display_name": "Schema Location",
                "short_desc": "The REST schema source URL",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromLocation: True,
                    }),
                ),
                "desc": "The REST schema source URL",
                "required_groups": ("schema",),
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The URL to the REST server; overrides any URL in the schema or in any REST client "
                    "object passed as an option. When both `restclient` and `url` are provided, a sibling "
                    "client is built via `AbstractRestClient::copyWithUrl()` — the supplied client is not "
                    "mutated",
            },
        };

        #! Hash of HTTP Methods
        const HttpMethods = {
            "GET": True,
            "PUT": True,
            "PATCH": True,
            "POST": True,
            "DELETE": True,
        };
    }

    private {
        #! hash of valid paths
        hash<auto> path_tree;
    }

    #! Private constructor; used when traversing the tree
    private constructor(hash<auto> path_tree, string uri_path, AbstractRestSchemaValidator schema,
            *AbstractRestClient rest) : RestSchemaDataProviderCommon(rest) {
        self.path_tree = path_tree;
        self.uri_path = uri_path;
        self.schema = schema;

        display_name = uri_path.split("/").last();
    }

    #! Creates the object from the arguments
    constructor(AbstractRestSchemaValidator schema, *AbstractRestClient rest)
            : RestSchemaDataProviderCommon(rest) {
        setSchema(schema);
        setupTree();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (!copts.schema.val() && !copts.schema_location.val()) {
            throw "CONSTRUCTOR-ERROR", "No 'schema' or 'schema_location' option given";
        }
        if (exists copts.schema) {
            switch (copts.schema.typeCode()) {
                case NT_STRING:
                    # determine if the string is schema content or a file/URL path
                    if (copts.schema.size() > 1024
                        || copts.schema =~ /^swagger:/
                        || copts.schema =~ /^openapi:/
                        || copts.schema =~ /^{/
                        || copts.schema =~ /^swagger":/
                        || copts.schema =~ /^"openapi":/
                    ) {
                        setSchema(loadSchemaFromString(copts.schema, copts.lax_parsing));
                    } else {
                        setSchema(loadSchemaFromUrl(copts.schema, copts.lax_parsing));
                    }
                    break;
                case NT_OBJECT:
                    setSchema(copts.schema);
                    break;
                default:
                    throw "CONSTRUCTOR-ERROR", sprintf("Unsupported schema type: %y", copts.schema.type());
            }
        } else {
            setSchema(loadSchemaFromUrl(copts.schema_location, copts.lax_parsing));
        }

        if (copts.restclient) {
            checkRestClient(copts.restclient);
            rest = copts.restclient;
        }

        if (!rest) {
            if (copts.url || copts.restclient_options) {
                hash<auto> opts;
                opts += copts.restclient_options + {
                    "validator": schema,
                };
                if (copts.url) {
                    opts.url = copts.url;
                }
                rest = new RestClientIo(opts);
            }
        } else if (copts.url) {
            # RestClientIo is immutable by URL — instead of mutating the
            # caller-supplied client, build a sibling pointed at the new URL.
            # The sibling inherits config, token, and the connection manager
            # via RestClientIo::copyWithUrl().
            rest = rest.copyWithUrl(copts.url);
        }

        setupTree();
    }

    #! Returns the data provider name
    string getName() {
        return display_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        string url;
        if (rest) {
            url = rest.getUrl();
        } else {
            url = schema.getTargetUrl();
        }
        string type = isSwagger() ? "Swagger/OpenAPI 2.0" : "OpenAPI 3.0";
        return sprintf("%s data provider for URL `%s` and URI path `%s`", type, url, uri_path);
    }

    #! Loads a schema from a string
    private static AbstractRestSchemaValidator loadSchemaFromString(string schema_str, *bool lax_parsing) {
        int parse_flags = lax_parsing ? Swagger::LM_ALL : 0;
        # try to detect if it's OpenAPI 3.0 or Swagger 2.0
        if (schema_str =~ /^openapi\s*[:\"\']?\s*[:\"\']?3/ || schema_str =~ /"openapi"\s*:\s*"3/) {
            return OpenApi3Loader::fromString(schema_str, NOTHING, {"parse_flags": parse_flags});
        }
        return SwaggerLoader::fromString(schema_str, NOTHING, {"parse_flags": parse_flags});
    }

    #! Loads a schema from a URL
    private static AbstractRestSchemaValidator loadSchemaFromUrl(string url, *bool lax_parsing) {
        int parse_flags = lax_parsing ? Swagger::LM_ALL : 0;
        # first load the content to detect the schema type
        string content;
        if (url =~ /^[a-z]+:\/\//i) {
            HTTPClient client({"url": url});
            content = client.get("");
        } else {
            content = ReadOnlyFile::readTextFile(url);
        }
        # detect schema type
        if (content =~ /^openapi\s*[:\"\']?\s*[:\"\']?3/ || content =~ /"openapi"\s*:\s*"3/) {
            return OpenApi3Loader::fromString(content, NOTHING, {"parse_flags": parse_flags});
        }
        return SwaggerLoader::fromString(content, NOTHING, {"parse_flags": parse_flags});
    }

    #! Sets the schema and display name
    private setSchema(AbstractRestSchemaValidator schema) {
        self.schema = schema;
        if (schema instanceof SwaggerSchema) {
            display_name = cast<SwaggerSchema>(schema).info.title;
        } else if (schema instanceof OpenApi3Schema) {
            display_name = cast<OpenApi3Schema>(schema).info.title;
        } else {
            display_name = "REST Schema";
        }
        display_name =~ s/\//-/g;
    }

    #! Checks the REST client
    private checkRestClient(AbstractRestClient rest) {
        # RestClientIo always has a validator installed (defaults to
        # NullRestSchemaValidator); treat the null validator as "no schema"
        AbstractRestSchemaValidator validator = rest.getValidator();
        if (validator instanceof NullRestSchemaValidator) {
            rest.setValidator(schema);
        } else if (validator.getHash() != schema.getHash()) {
            throw "CONSTRUCTOR-ERROR", sprintf("REST client provided in %s::constructor() is configured with "
                "a different REST validator schema than %y required by this data provider", self.className(),
                getName());
        }
    }

    private setupTree() {
        hash<string, list<string>> paths = schema.getPathOperationHash();
        foreach hash<auto> i in (paths.pairIterator()) {
            # ignore paths with no operations
            if (!i.value) {
                continue;
            }
            code do_ref = sub (reference<auto> ref, list<string> path_elements, int offset = 0) {
                if (!path_elements) {
                    return;
                }
                *string path_element = path_elements[offset];
                # check for wildcards
                if (path_element =~ /^{.*}$/) {
                    path_element = "*";
                }
                if (!exists ref{path_element}) {
                    ref{path_element} = {};
                }
                if (++offset == path_elements.size()) {
                    return;
                }
                do_ref(\ref{path_element}, path_elements, offset);
            };

            do_ref(\path_tree, i.key.split("/"));
        }
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        *list<string> rv = keys path_tree;

        # now try to add operations
        if (isSwagger()) {
            *Swagger::PathItemObject pio = cast<SwaggerSchema>(schema).paths.tryMatch(uri_path);
            if (pio) {
                rv += map $1.upr(), pio.getMethods();
            }
        } else {
            *OpenApi3::PathItemObject pio = cast<OpenApi3Schema>(schema).paths.tryMatch(uri_path);
            if (pio) {
                rv += map $1.upr(), pio.getMethods();
            }
        }

        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (exists path_tree{name}) {
            return getChildIntern(name);
        }

        # try to match an operation
        if (isSwagger()) {
            *Swagger::PathItemObject pio;
            SwaggerSchema swagger = cast<SwaggerSchema>(schema);
            if (swagger.paths) {
                pio = swagger.paths.tryMatch(uri_path);
            }
            if (pio) {
                try {
                    return new RestSchemaRequestDataProvider(schema, rest, uri_path, pio,
                        pio.getOperation(name.lwr(), uri_path));
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "INVALID-METHOD") {
                        rethrow ex.err, sprintf("%s (while resolving %y %y)", ex.desc, name, uri_path);
                    }
                }
            }
        } else {
            *OpenApi3::PathItemObject pio;
            OpenApi3Schema openapi3 = cast<OpenApi3Schema>(schema);
            if (openapi3.paths) {
                pio = openapi3.paths.tryMatch(uri_path);
            }
            if (pio) {
                try {
                    return new RestSchemaRequestDataProvider(schema, rest, uri_path, pio,
                        pio.getOperation(name.lwr(), uri_path));
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "INVALID-METHOD") {
                        rethrow ex.err, sprintf("%s (while resolving %y %y)", ex.desc, name, uri_path);
                    }
                }
            }
        }

        if (exists path_tree."*") {
            return getChildIntern("*", name);
        }
    }

    #! Returns the child provider for the given path component
    private AbstractDataProvider getChildIntern(string name, *string real_name) {
        string new_uri_path = uri_path;
        if (new_uri_path !~ /\/$/) {
            new_uri_path += "/";
        }
        new_uri_path += real_name ?? name;
        return new RestSchemaDataProvider(path_tree{name}, new_uri_path, schema, rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the schema supporting this data provider
    private object getSchemaObjectImpl() {
        return schema;
    }
}
} # end public namespace RestSchemaDataProvider
