# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauWatchDatasourceEventDataProviders.qc Tableau webhook-based datasource event providers

/*  TableauWatchDatasourceEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Watch for datasource created events in Tableau
public class TableauWatchDatasourceCreatedDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_CREATED,
            "desc": "Watch for new datasource created events in Tableau",
            "type": "TableauWatchDatasourceCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_CREATED,
                TABLEAU_EVENT_DATASOURCE_CREATED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new datasource created events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Watch for datasource deleted events in Tableau
public class TableauWatchDatasourceDeletedDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_DELETED,
            "desc": "Watch for datasource deleted events in Tableau",
            "type": "TableauWatchDatasourceDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_DELETED,
                TABLEAU_EVENT_DATASOURCE_DELETED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_DELETED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for datasource deleted events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Watch for datasource updated events in Tableau
public class TableauWatchDatasourceUpdatedDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_UPDATED,
            "desc": "Watch for datasource updated events in Tableau",
            "type": "TableauWatchDatasourceUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_UPDATED,
                TABLEAU_EVENT_DATASOURCE_UPDATED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for datasource updated events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Watch for datasource refresh started events in Tableau
public class TableauWatchDatasourceRefreshStartedDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_REFRESH_STARTED,
            "desc": "Watch for datasource refresh started events in Tableau",
            "type": "TableauWatchDatasourceRefreshStartedDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_REFRESH_STARTED,
                TABLEAU_EVENT_DATASOURCE_REFRESH_STARTED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_REFRESH_STARTED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for datasource refresh started events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Watch for datasource refresh succeeded events in Tableau
public class TableauWatchDatasourceRefreshSucceededDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_REFRESH_SUCCEEDED,
            "desc": "Watch for datasource refresh succeeded events in Tableau",
            "type": "TableauWatchDatasourceRefreshSucceededDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_REFRESH_SUCCEEDED,
                TABLEAU_EVENT_DATASOURCE_REFRESH_SUCCEEDED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_REFRESH_SUCCEEDED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for datasource refresh succeeded events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Watch for datasource refresh failed events in Tableau
public class TableauWatchDatasourceRefreshFailedDataProvider inherits TableauWebhookEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_DATASOURCE_REFRESH_FAILED,
            "desc": "Watch for datasource refresh failed events in Tableau",
            "type": "TableauWatchDatasourceRefreshFailedDataProvider",
            "supports_observable": True,
            "constructor_options": WebhookConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauWebhookEventDataProviderBase(rest, EVENT_DATASOURCE_REFRESH_FAILED,
                TABLEAU_EVENT_DATASOURCE_REFRESH_FAILED, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_DATASOURCE_REFRESH_FAILED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for datasource refresh failed events in Tableau";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
