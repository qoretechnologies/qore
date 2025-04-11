# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SwaggerDataProvider module definition

/** SwaggerDataProvider.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SwaggerDataProvider module
public namespace SwaggerDataProvider {
#! The Swagger data provider class
public class SwaggerDataProvider inherits SwaggerDataProviderCommon {
    public {
        #! current URI path
        string uri_path = "/";

        #! The value to returns as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SwaggerDataProvider",
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
            "schema_type": "swagger",
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "lax_parsing": <DataProviderOptionInfo>{
                "display_name": "Swagger Lax Parsing",
                "short_desc": "Sets lax parsing for Swagger schemas",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then common errors in Swagger schemas will be ignored; some Swagger schemas with "
                    "errors will be able to be used; this option is not recommended; it will not parse "
                    "all bad Swagger schemas; schemas should be corrected instead of setting this option",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClient"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the RestClient constructor; only used if a RestClient object is created for a "
                    "call",
            },
            "schema": <DataProviderOptionInfo>{
                "display_name": "Schema",
                "short_desc": "The Swagger schema file or object",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromFile: True,
                    }),
                    AbstractDataProviderType::get(new Type("SwaggerSchema")),
                ),
                "desc": "The Swagger schema file or object",
                "required_groups": ("schema",),
            },
            "schema_location": <DataProviderOptionInfo>{
                "display_name": "Schema Location",
                "short_desc": "The Swagger schema source URL",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromLocation: True,
                    }),
                ),
                "desc": "The Swagger schema source URL",
                "required_groups": ("schema",),
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The URL to the REST server; overrides any URL in the schema or in any RestClient object "
                    "passed as an option",
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
    private constructor(hash<auto> path_tree, string uri_path, SwaggerSchema schema, *RestClient rest)
            : SwaggerDataProviderCommon(rest) {
        self.path_tree = path_tree;
        self.uri_path = uri_path;
        self.schema = schema;

        display_name = uri_path.split("/").last();
    }

    #! Creates the object from the arguments
    constructor(SwaggerSchema schema, *RestClient rest) : SwaggerDataProviderCommon(rest) {
        setSchema(schema);
        setupTree();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.lax_parsing) {
            copts.lax_parsing = LM_ALL;
        }
        if (!copts.schema.val() && !copts.schema_location.val()) {
            throw "CONSTRUCTOR-ERROR", "No 'schema' or 'schema_location' option given";
        }
        if (exists copts.schema) {
            switch (copts.schema.typeCode()) {
                case NT_STRING:
                    # determine if the string is a file or a location
                    if (copts.schema.size() > 1024 || copts.schema =~ /^swagger:/ || copts.schema =~ /^{/
                        || copts.schema =~ /^swagger":/) {
                        setSchema(SwaggerLoader::fromString(copts.schema, NOTHING, {"parse_flags": copts.lax_parsing}));
                    } else {
                        setSchema(SwaggerLoader::fromUrl(copts.schema, NOTHING, {"parse_flags": copts.lax_parsing}));
                    }
                    break;
                case NT_OBJECT:
                    setSchema(copts.schema);
                    break;
                default:
                    throw "CONSTRUCTOR-ERROR", sprintf("Unsupported schema type: %y", copts.schema.type());
            }
        } else {
            setSchema(SwaggerLoader::fromUrl(copts.schema_location, NOTHING, {"parse_flags": copts.lax_parsing}));
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
                rest = new RestClient(opts);
            }
        } else if (copts.url) {
            rest.setURL(copts.url);
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
            url = rest.getURL();
        } else {
            string url;
            if (schema.schemes) {
                url = schema.schemes.https ? "https" : schema.schemes.firstKey();
            } else {
                url = "https";
            }
            url += "://";
            url += schema.host ?? "localhost";
            if (schema.basePath) {
                url += "/" + schema.basePath;
            }
        }
        return sprintf("Swagger/OpenAPI data provider for URL `%s` and URI path `%s`", url, uri_path);
    }

    #! Sets the schema and display name
    private setSchema(SwaggerSchema schema) {
        self.schema = schema;
        display_name = schema.info.title;
        display_name =~ s/\//-/g;
    }

    #! Checks the REST client
    private checkRestClient(RestClient rest) {
        *AbstractRestSchemaValidator validator = rest.getValidator();
        if (validator) {
            if (validator.getHash() != schema.getHash()) {
                throw "CONSTRUCTOR-ERROR", sprintf("RestClient provided in %s::constructor() is configured with a "
                    "different REST validator schema than %y required by this data provider", self.className(),
                    getName());
            }
        } else {
            rest.setValidator(validator);
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
        *PathItemObject pio = schema.paths.tryMatch(uri_path);
        if (pio) {
            rv += map $1.upr(), pio.getMethods();
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
        *PathItemObject pio;
        if (schema.paths) {
            pio = schema.paths.tryMatch(uri_path);
        }
        if (pio) {
            try {
                return new SwaggerRequestDataProvider(schema, rest, uri_path, pio,
                    pio.getOperation(name.lwr(), uri_path));
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "INVALID-METHOD") {
                    rethrow ex.err, sprintf("%s (while resolving %y %y)", ex.desc, name, uri_path);
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
        return new SwaggerDataProvider(path_tree{name}, new_uri_path, schema, rest);
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
} # end public namespace swagger
