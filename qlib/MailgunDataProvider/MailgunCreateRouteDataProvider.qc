# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunCreateRouteDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Request type for creating a route
public class MailgunCreateRouteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "Route priority",
                "desc": "Priority of the route (lower number = higher priority, default: 0)",
                "default_value": 0,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Route description",
                "desc": "A description of what this route does",
                "example_value": "Forward support emails to webhook",
            },
            "expression": {
                "type": StringType,
                "display_name": "Expression",
                "short_desc": "Matching expression",
                "desc": "The expression used to match incoming messages. Examples:\n"
                    "- match_recipient('.*@example.com')\n"
                    "- match_header('subject', '.*support.*')\n"
                    "- catch_all()",
                "example_value": "match_recipient('support@example.com')",
                "required": True,
            },
            "action": {
                "type": new SoftListDataType(StringType),
                "display_name": "Actions",
                "short_desc": "Route actions",
                "desc": "Actions to execute when the expression matches. Examples:\n"
                    "- forward('http://example.com/webhook')\n"
                    "- forward('user@example.com')\n"
                    "- store(notify='http://example.com/callback')\n"
                    "- stop()",
                "example_value": ("forward('http://example.com/webhook')", "stop()"),
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a route
public class MailgunCreateRouteDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-route",
            "desc": "Create a Mailgun route for inbound email processing",
            "type": "MailgunCreateRouteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunCreateRouteRequestDataType RequestType();

        #! Response type
        static MailgunRouteDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) : MailgunDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-route";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a Mailgun route for inbound email processing";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!req.expression.val()) {
            throw "MAILGUN-ERROR", "expression is required";
        }
        if (!req.action) {
            throw "MAILGUN-ERROR", "action is required";
        }

        hash<auto> body = {
            "expression": req.expression,
        };

        if (exists req.priority) {
            body.priority = req.priority;
        }
        if (req.description) {
            body.description = req.description;
        }

        # Handle actions - Mailgun expects multiple 'action' parameters
        # For form-encoded, we send each action separately
        if (req.action.typeCode() == NT_LIST) {
            body.action = req.action;
        } else {
            body.action = (req.action,);
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating route with body: %y", body));

        hash<auto> response = doPostFormEncoded(ApiV3 + "/routes", body);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: response=%y", response));

        # Return the route data from response
        return response.route ?? response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace MailgunDataProvider
