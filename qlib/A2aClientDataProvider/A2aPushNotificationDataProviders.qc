# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2A push notification data providers

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {
#! A2A push notifications container data provider
public class A2aPushNotificationsDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "push-notifications",
            "desc": "A2A push notification configuration data provider",
            "type": "A2aPushNotificationsDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "push-notifications";
    }

    private *list<string> getChildProviderNamesImpl() {
        return ("set", "get", "list", "delete");
    }

    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "set":
                return new A2aPushNotificationSetDataProvider(a2a);
            case "get":
                return new A2aPushNotificationGetDataProvider(a2a);
            case "list":
                return new A2aPushNotificationListDataProvider(a2a);
            case "delete":
                return new A2aPushNotificationDeleteDataProvider(a2a);
        }
        return NOTHING;
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! A2A push notification set request data type
public class A2aPushNotificationSetRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The task ID to set push notification config for",
                "example_value": "task-123",
            },
            "url": {
                "type": StringType,
                "display_name": "Webhook URL",
                "short_desc": "Push notification webhook URL",
                "desc": "The URL to receive push notifications for task events",
                "example_value": "https://example.com/webhook",
            },
            "token": {
                "type": StringOrNothingType,
                "display_name": "Token",
                "short_desc": "Verification token",
                "desc": "Optional verification token for the webhook",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const A2aPushNotificationSetRequestDataType = new A2aPushNotificationSetRequestDataType();

#! A2A push notification task ID request data type
public class A2aPushNotificationTaskRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The task ID to manage push notification config for",
                "example_value": "task-123",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const A2aPushNotificationTaskRequestDataType = new A2aPushNotificationTaskRequestDataType();

#! A2A push notification delete request data type
public class A2aPushNotificationDeleteRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The task ID to delete push notification config for",
                "example_value": "task-123",
            },
            "pushNotificationConfigId": {
                "type": StringType,
                "display_name": "Config ID",
                "short_desc": "Push notification config ID",
                "desc": "The ID of the push notification config to delete",
                "example_value": "config-1",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public const A2aPushNotificationDeleteRequestDataType = new A2aPushNotificationDeleteRequestDataType();

#! A2A push notification result data type
public class A2aPushNotificationResultDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The task ID associated with this result",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const A2aPushNotificationResultDataType = new A2aPushNotificationResultDataType();

#! Set push notification config data provider
public class A2aPushNotificationSetDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "set",
            "desc": "Sets push notification configuration for a task",
            "type": "A2aPushNotificationSetDataProvider",
            "supports_request": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "set";
    }

    *string getDesc() {
        return "Sets push notification configuration for a task";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aPushNotificationSetRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aPushNotificationResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @assert(req.url);
        @debug("Setting push notification config for task %y", req.id);
        hash<auto> config = {"url": req.url};
        if (req.token) {
            config.token = req.token;
        }
        return a2a.setPushNotificationConfig(req.id, config);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Get push notification config data provider
public class A2aPushNotificationGetDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Gets push notification configuration for a task",
            "type": "A2aPushNotificationGetDataProvider",
            "supports_request": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "get";
    }

    *string getDesc() {
        return "Gets push notification configuration for a task";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aPushNotificationTaskRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aPushNotificationResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @debug("Getting push notification config for task %y", req.id);
        return a2a.getPushNotificationConfig(req.id);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! List push notification configs data provider
public class A2aPushNotificationListDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "desc": "Lists push notification configurations for a task",
            "type": "A2aPushNotificationListDataProvider",
            "supports_request": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "list";
    }

    *string getDesc() {
        return "Lists push notification configurations for a task";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aPushNotificationTaskRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aPushNotificationResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @debug("Listing push notification configs for task %y", req.id);
        return a2a.listPushNotificationConfigs(req.id);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Delete push notification config data provider
public class A2aPushNotificationDeleteDataProvider inherits A2aClientDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Deletes push notification configuration for a task",
            "type": "A2aPushNotificationDeleteDataProvider",
            "supports_request": True,
        };
    }

    constructor(A2aClient a2a) : A2aClientDataProviderBase(a2a) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Deletes push notification configuration for a task";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return A2aPushNotificationDeleteRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return A2aPushNotificationResultDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id);
        @assert(req.pushNotificationConfigId);
        @debug("Deleting push notification config %y for task %y", req.pushNotificationConfigId, req.id);
        return a2a.deletePushNotificationConfig(req.id, req.pushNotificationConfigId);
    }

    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
