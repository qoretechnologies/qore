# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCustomActionEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Base class for WooCommerce custom action event data providers (enrichment needed)
/** Custom action webhooks deliver payloads like \c {"action": "...", "arg": <id>}.
    This class fetches the full resource data from the API using the \c arg value as the resource ID.
*/
public class WooCommerceCustomActionEventDataProvider inherits WooCommerceBaseEventDataProvider {
    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) : WooCommerceBaseEventDataProvider(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceBaseEventDataProvider(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceBaseEventDataProvider(options) {
    }

    #! Returns the API resource path for enrichment (e.g., "orders", "products")
    /** Must be overridden by subclasses */
    abstract string getEnrichmentResource();

    #! Returns the API resource path - delegates to getEnrichmentResource()
    string getResourcePath() {
        return getEnrichmentResource();
    }

    #! Handle incoming webhook events - enriches custom action payloads with full resource data
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s custom action event", event_type);

        # Custom action payloads: {"action": "...", "arg": <id>}
        auto arg = payload.arg;
        if (!arg || arg == 0) {
            # Non-scalar argument or missing - cannot enrich
            log(LoggerLevel::WARN, "handleWebhookEvent: cannot enrich %s - arg is %y", event_type, arg);
            return;
        }

        # Fetch full resource data
        hash<auto> full_data;
        try {
            full_data = doGet(sprintf("%s/%s", getEnrichmentResource(), string(arg)));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "handleWebhookEvent: error enriching %s/%s: %s",
                getEnrichmentResource(), string(arg), get_exception_string(ex));
            return;
        }

        notifyObservers(event_type, full_data);
    }
}

} # namespace WooCommerceDataProvider
