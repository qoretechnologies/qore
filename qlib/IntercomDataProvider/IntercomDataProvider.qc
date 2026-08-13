# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the IntercomDataProvider module
public namespace IntercomDataProvider {
#! The root Intercom data provider
/** Exposes every action in @ref IntercomDataProvider::IntercomManifest "IntercomManifest" as a child
    provider, so that each registered action path resolves, plus the \c events container holding the
    polling event providers.
*/
public class IntercomDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        const AppName = "Intercom";

        #! The name of the child provider holding the event providers
        const EventsChildName = "events";

        #! The ID of the one hand-written action
        /** @see @ref IntercomDataProvider::IntercomReplyConversationDataProvider
            "IntercomReplyConversationDataProvider" for why it is not generated
        */
        const ReplyConversationAction = "replyConversation";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "intercom",
            "type": "IntercomDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The Intercom REST client to use",
                "type": AbstractDataProviderType::get(new Type("IntercomRestClient::IntercomRestClientIo")),
                "desc": "The `IntercomRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Intercom REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `IntercomRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "An Intercom access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An Intercom access token, sent as a bearer token",
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The Intercom API URL",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Intercom API URL; the default addresses the United States host, which "
                    "Intercom routes on to whichever region the workspace is hosted in. Set it to "
                    "`https://api.eu.intercom.io` or `https://api.au.intercom.io` to address a region "
                    "directly",
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Intercom REST client to use for API calls
    */
    constructor(*IntercomRestClient::IntercomRestClientIo rest)
            : RestSchemaActionSetDataProvider(IntercomSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(IntercomSchema::getActionSet(),
            IntercomDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "intercom";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Intercom data provider";
    }

    #! Returns the action IDs plus the events container
    /** Event actions name a path under \c "/events", so that container has to resolve alongside the
        schema-derived actions.
    */
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (ReplyConversationAction, EventsChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == EventsChildName) {
            return new IntercomEventsDataProvider(cast<*IntercomRestClient::IntercomRestClientIo>(rest));
        }
        if (name == ReplyConversationAction) {
            return new IntercomReplyConversationDataProvider(IntercomSchema::getActionSet(),
                cast<*IntercomRestClient::IntercomRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *IntercomRestClient::IntercomRestClientIo getRestClient(*hash<auto> options) {
        # validated here rather than through checkOptions(), which is an instance method and so is not
        # available while resolving the arguments of the base class constructor
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
        if (copts.token.val()) {
            ropts.token = copts.token;
        }
        if (copts.url.val()) {
            ropts.url = copts.url;
        }
        if (ropts) {
            return new IntercomRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
