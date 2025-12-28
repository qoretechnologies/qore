# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AsyncApiDataProvider module definition

/** AsyncApiDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AsyncApiDataProvider module
public namespace AsyncApiDataProvider {
#! The AsyncAPI data provider class
public class AsyncApiDataProvider inherits AsyncApiDataProviderCommon {
    public {
        #! The value to return as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "AsyncApiDataProvider",
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
            "schema_type": "asyncapi",
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": True,
            "children_can_support_messages": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "transport": <DataProviderOptionInfo>{
                "display_name": "Transport",
                "short_desc": "The transport object to use for messaging",
                "type": AbstractDataProviderType::get(new Type("AbstractAsyncApiTransport"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The transport object to use for message operations (WebSocket, MQTT, Kafka, etc.)",
            },
            "schema": <DataProviderOptionInfo>{
                "display_name": "Schema",
                "short_desc": "The AsyncAPI schema file or object",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromFile: True,
                    }),
                    AbstractDataProviderType::get(new Type("AsyncApi3Schema")),
                ),
                "desc": "The AsyncAPI schema file or object",
                "required_groups": ("schema",),
            },
            "schema_location": <DataProviderOptionInfo>{
                "display_name": "Schema Location",
                "short_desc": "The AsyncAPI schema source URL",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromLocation: True,
                    }),
                ),
                "desc": "The AsyncAPI schema source URL",
                "required_groups": ("schema",),
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The URL to the messaging server; used to create a default WebSocket transport if no "
                    "transport is provided",
            },
        };
    }

    #! Creates the object from the arguments
    /** @param schema the AsyncAPI schema
        @param transport optional transport for message operations
    */
    constructor(AsyncApi::AsyncApi3Schema schema, *AbstractAsyncApiTransport transport)
            : AsyncApiDataProviderCommon(transport) {
        setSchema(schema);
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
                    # determine if the string is a file or a location
                    if (copts.schema.size() > 1024 || copts.schema =~ /^asyncapi:/ || copts.schema =~ /^{/
                        || copts.schema =~ /^"asyncapi":/) {
                        setSchema(AsyncApi::AsyncApi3Loader::fromString(copts.schema));
                    } else {
                        setSchema(AsyncApi::AsyncApi3Loader::fromUrl(copts.schema));
                    }
                    break;
                case NT_OBJECT:
                    setSchema(copts.schema);
                    break;
                default:
                    throw "CONSTRUCTOR-ERROR", sprintf("Unsupported schema type: %y", copts.schema.type());
            }
        } else {
            setSchema(AsyncApi::AsyncApi3Loader::fromUrl(copts.schema_location));
        }

        if (copts.transport) {
            transport = copts.transport;
        } else if (copts.url) {
            # create a default WebSocket transport if URL is provided
            transport = new WebSocketTransport({"url": copts.url});
        }
    }

    #! Returns the data provider name
    string getName() {
        return display_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        string desc = sprintf("AsyncAPI data provider for %y", display_name);
        if (schema.info.description) {
            desc += sprintf("\n\n%s", schema.info.description);
        }
        return desc;
    }

    #! Sets the schema and display name
    private setSchema(AsyncApi::AsyncApi3Schema schema) {
        self.schema = schema;
        display_name = schema.info.title;
        display_name =~ s/\//-/g;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys schema.channels;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *AsyncApi::ChannelObject channel = schema.getChannel(name);
        if (channel) {
            return new AsyncApiChannelDataProvider(schema, transport, name, channel);
        }
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
}
