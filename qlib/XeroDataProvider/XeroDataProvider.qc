# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the XeroDataProvider module
public namespace XeroDataProvider {
#! The root Xero data provider
/** @section xerodataprovider_composing One Root, Several Action Sets

    @ref RestSchemaActions::RestSchemaActionSetDataProvider "RestSchemaActionSetDataProvider" exposes
    exactly one action set, and Xero has two — one per document, because a set binds one schema and the
    request URI takes its prefix from that schema's base path.  So this root composes them: it merges the
    action IDs of every set into one list of children and resolves each name against the set that exports
    it, alongside the hand-written employee actions and the polling event providers.

    Action IDs are unique across the whole application, which is what makes the merge unambiguous.  A
    collision would mean one action shadowing another, so it is reported rather than resolved by order.

    Xero is the first application to need this.  If a second one appears, this class and the per-set
    \c registerActions() / \c validateActionPaths() bookkeeping belong in \c RestSchemaActions rather than
    written twice.
*/
public class XeroDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The application name
        /** The name the superseded TypeScript application registered, which is what existing workflows
            reference.
        */
        const AppName = "Xero";

        #! The name of the child provider holding the polling event providers
        const EventsChildName = "events";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "xero",
            "type": "XeroDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The Xero REST client to use",
                "type": AbstractDataProviderType::get(new Type("XeroRestClient::XeroRestClientIo")),
                "desc": "The `XeroRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Xero REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `XeroRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "An OAuth2 access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An OAuth2 access token, sent as a bearer token",
                "sensitive": True,
            },
            "tenant_id": <DataProviderOptionInfo>{
                "display_name": "Tenant ID",
                "short_desc": "The Xero organisation to work in",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The ID of the Xero organisation to work in, sent as `Xero-Tenant-Id`",
            },
            "payroll_region": <DataProviderOptionInfo>{
                "display_name": "Payroll Region",
                "short_desc": "Which regional payroll API the employee actions use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Which regional payroll API the employee actions use: `au`, `nz` or `uk`",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *XeroRestClient::XeroRestClientIo rest;

        #! The action sets, in registration order
        list<RestSchemaActionSet> action_sets;

        #! The set exporting each action, keyed by action ID
        hash<string, RestSchemaActionSet> action_owner;
    }

    #! Creates the object from a REST client
    /** @param rest the Xero REST client to use for API calls
    */
    constructor(*XeroRestClient::XeroRestClientIo rest) {
        self.rest = rest;
        setupActionSets();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        rest = XeroDataProvider::getRestClient(options);
        setupActionSets();
    }

    #! Returns the data provider name
    string getName() {
        return "xero";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Xero data provider";
    }

    #! Returns the action IDs of every set, plus the employee actions and the events container
    private *list<string> getChildProviderNamesImpl() {
        return (keys action_owner) + XeroEmployeesDataProvider::ActionNames + (EventsChildName,);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == EventsChildName) {
            return new XeroEventsDataProvider(rest);
        }
        if (inlist(name, XeroEmployeesDataProvider::ActionNames)) {
            return new XeroEmployeesDataProvider(rest, name);
        }
        *RestSchemaActionSet aset = action_owner{name};
        if (!aset) {
            return;
        }
        return new RestSchemaActionDataProvider(aset, aset.getOperationEx(name), rest);
    }

    #! Builds the map from action ID to the set that exports it
    /** @throw XERO-ACTION-ERROR two sets export the same action ID, so one would shadow the other
    */
    private setupActionSets() {
        action_sets = XeroSchema::getActionSets();
        action_owner = {};
        foreach RestSchemaActionSet aset in (action_sets) {
            foreach string action in (aset.getActionNames()) {
                if (exists action_owner{action}) {
                    throw "XERO-ACTION-ERROR", sprintf("action %y is exported by more than one action set; "
                        "an action ID has to be unique across the application or one would shadow the "
                        "other", action);
                }
                action_owner{action} = aset;
            }
        }
        foreach string action in (XeroEmployeesDataProvider::ActionNames) {
            if (exists action_owner{action}) {
                throw "XERO-ACTION-ERROR", sprintf("hand-written action %y is also generated from a "
                    "schema; one would shadow the other", action);
            }
        }
    }

    #! Returns a REST client from the constructor options
    private static *XeroRestClient::XeroRestClientIo getRestClient(*hash<auto> options) {
        # validated here rather than through checkOptions(), which is an instance method and so is not
        # available while the object is being constructed
        list<string> unknown = (map $1, keys options, !exists ConstructorOptions{$1}) ?? ();
        if (unknown) {
            throw "CONSTRUCTOR-ERROR", sprintf("unknown option(s) %y; known options: %y", unknown,
                keys ConstructorOptions);
        }
        *hash<auto> copts = options;
        if (copts.restclient) {
            return copts.restclient;
        }
        hash<auto!> ropts = copts.restclient_options ?? {};
        foreach string name in (("token", "tenant_id", "payroll_region")) {
            if (copts{name}.val()) {
                ropts{name} = copts{name};
            }
        }
        if (ropts) {
            return new XeroRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
