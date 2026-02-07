# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProvider webhook support

/** WebhookSupport.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {

#! Static webhook API callbacks
/** This class provides a static API for data providers to create and delete webhooks.
    The webhook infrastructure must be registered by the runtime environment (e.g., Qorus)
    before data providers can use these methods.

    @par Webhook Creation Callback
    The creation callback takes a hash with the following keys:
    - \c method (string): the HTTP method for the webhook (e.g., "POST")
    - \c callback (code): a callback taking the webhook payload to generate events
    - \c auth (*int): optional authentication requirement
    - \c perms (*list&lt;string&gt;): optional list of permissions

    Returns a hash with:
    - \c url (string): the public URL for the webhook
    - \c ctx (auto): context object for webhook deletion

    @par Webhook Deletion Callback
    The deletion callback takes a hash with \c url and \c ctx keys (as returned by the create callback)
*/
public class DataProviderWebhook {
    public {
        #! Webhook creation callback
        static *code webhookCreate;

        #! Webhook deletion callback
        static *code webhookDelete;
    }

    #! Register webhook creation and deletion APIs
    /** Called by the runtime (e.g., Qorus) to provide webhook infrastructure

        @param create a callback for creating webhooks; takes a hash with webhook config,
               returns a hash with \c url and \c ctx keys
        @param destroy a callback for deleting webhooks; takes the hash returned by \a create
    */
    static registerWebhookApis(code create, code destroy) {
        if (webhookCreate) {
            throw "WEBHOOK-API-ERROR", "Webhook APIs are already registered";
        }
        webhookCreate = create;
        webhookDelete = destroy;
    }

    #! Check if webhook infrastructure is available
    /** @return True if webhook APIs have been registered, False otherwise
    */
    static bool hasWebhookSupport() {
        return exists webhookCreate;
    }

    #! Create a webhook and return the public URL for it
    /** @param config a hash with the following keys:
        - \c method (string): the HTTP method for the webhook (e.g., "POST")
        - \c callback (code): a callback taking the webhook payload to generate events
        - \c auth (*int): optional authentication requirement (0 = none, 1 = required)
        - \c perms (*list&lt;string&gt;): optional list of permissions when auth is required

        @return a hash with:
        - \c url (string): the public URL for the webhook
        - \c ctx (auto): context object needed for webhook deletion

        @throw WEBHOOK-API-ERROR if webhook infrastructure is not available
    */
    static hash<auto> createWebhook(hash<auto> config) {
        if (!webhookCreate) {
            throw "WEBHOOK-API-ERROR", "Cannot create a webhook; no webhook infrastructure is available";
        }
        return DataProviderWebhook::webhookCreate(config);
    }

    #! Delete a webhook
    /** @param info the hash returned by createWebhook() containing \c url and \c ctx keys

        @throw WEBHOOK-API-ERROR if webhook infrastructure is not available
    */
    static deleteWebhook(hash<auto> info) {
        if (!webhookDelete) {
            throw "WEBHOOK-API-ERROR", "Cannot delete a webhook; no webhook infrastructure is available";
        }
        DataProviderWebhook::webhookDelete(info);
    }
}

} # namespace DataProvider
