# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PineconeDataProvider class definitions

/** PineconeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace PineconeDataProvider {
#! The root of the Pinecone application
/** Every action is generated from the vendored Pinecone API descriptions, so the root is the union of
    the three action sets; see @ref PineconeDataProvider::PineconeManifest "PineconeManifest" for what is
    exported and why, and @ref PineconeDataProvider::PineconeSchema "PineconeSchema" for the pinned
    documents' provenance.

    @section pineconedataprovider_router One Router for the Whole Root

    24 of the 52 actions are sent to the selected index's own host rather than to the connection's, and
    resolving that host costs a control-plane call.  A
    @ref RestSchemaActions::RestSchemaHostRouter "RestSchemaHostRouter" is created here and shared by every child, so the resolution is paid once per
    index for as long as this provider lives and all 24 actions on one index share a single client.

    The router holds the parent client, which is what keeps two connections to Pinecone apart: two
    projects can both have an index called \c products, on different hosts, and a cache keyed by index
    name alone would send one project's request to the other project's host with the first project's
    credentials.

    @since %PineconeDataProvider 1.0
*/
public class PineconeDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "pinecone",
            "type": "PineconeDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_records": True,
        };

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "The Pinecone API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Pinecone API key, sent as the `Api-Key` header (NOT "
                    "`Authorization: Bearer`).",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Control-Plane URL",
                "short_desc": "The Pinecone control-plane URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Pinecone control plane.  Data-plane requests are sent to the "
                    "selected index's own host, which is resolved through this one.",
                "default_value": PineconeRestClient::PineconeRestClientBase::DefaultControlPlaneUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *RestClientIo::RestClientIo rest;

        #! The action sets, in registration order
        list<RestSchemaActions::RestSchemaActionSet> action_sets;

        #! The set exporting each action, keyed by action ID
        hash<string, RestSchemaActions::RestSchemaActionSet> action_owner;

        #! The host router shared by every action under this root
        *RestSchemaActions::RestSchemaHostRouter router;
    }

    #! Creates the object from a REST client
    constructor(*RestClientIo::RestClientIo rest) {
        self.rest = rest;
        setupActionSets();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        rest = PineconeDataProvider::getRestClient(options);
        setupActionSets();
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Pinecone vector database data provider";
    }

    #! Returns the host router shared by this root's actions
    *RestSchemaActions::RestSchemaHostRouter getHostRouter() {
        return router;
    }

    #! Returns the action IDs of every set
    private *list<string> getChildProviderNamesImpl() {
        return ("tables",) + keys action_owner;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "tables") {
            return new RestSchemaActions::RestSchemaTablesDataProvider("tables", action_sets,
                PineconeRecordTables::Tables, rest, router);
        }
        *RestSchemaActions::RestSchemaActionSet aset = action_owner{name};
        if (!aset) {
            return;
        }
        return new RestSchemaActions::RestSchemaActionDataProvider(aset, aset.getOperationEx(name), rest,
            router);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Builds the map from action ID to the set that exports it
    /** @throw PINECONE-ACTION-ERROR two sets export the same action ID, so one would shadow the other
    */
    private setupActionSets() {
        action_sets = PineconeSchema::getActionSets();
        action_owner = {};
        foreach RestSchemaActions::RestSchemaActionSet aset in (action_sets) {
            foreach string action in (aset.getActionNames()) {
                if (exists action_owner{action}) {
                    throw "PINECONE-ACTION-ERROR", sprintf("action %y is exported by more than one "
                        "action set; an action ID has to be unique across the application or one would "
                        "shadow the other", action);
                }
                action_owner{action} = aset;
            }
        }
        # one router for the whole root, so a resolved host is shared by every action that needs it
        if (rest) {
            router = new RestSchemaActions::RestSchemaHostRouter(rest);
        }
    }

    #! Returns a REST client from the constructor options
    private static *RestClientIo::RestClientIo getRestClient(*hash<auto> options) {
        # validated here rather than through checkOptions(), which is an instance method and so is not
        # available while the object is being constructed
        list<string> unknown = (map $1, keys options, !exists ConstructorOptions{$1}) ?? ();
        if (unknown) {
            throw "CONSTRUCTOR-ERROR", sprintf("unknown option(s) %y; known options: %y", unknown,
                keys ConstructorOptions);
        }
        if (options.restclient) {
            return options.restclient;
        }
        if (!options.apikey.val()) {
            throw "CONSTRUCTOR-ERROR", "missing required 'apikey' option for Pinecone API communication";
        }
        return new PineconeRestClient::PineconeRestClientIo({
            "url": options.url ?? PineconeRestClient::PineconeRestClientBase::DefaultControlPlaneUrl,
            "apikey": options.apikey,
        });
    }
}

#! The Pinecone data provider factory
public class PineconeDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("PineconeDataProvider");

        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "pinecone",
            "desc": "Pinecone vector database data provider factory",
            "children_can_support_apis": True,
            "children_can_support_records": True,
        };
    }

    private hash<DataProviderFactoryInfo> getInfoImpl() {
        return FactoryInfo;
    }

    private hash<DataProviderInfo> getProviderInfoImpl() {
        return PineconeDataProvider::ProviderInfo;
    }

    private Class getClassImpl() {
        return cls;
    }
}
}
