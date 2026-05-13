# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWebhookEventDataProviderBase.qc Base class for Katana native-webhook polling event providers

/*  KatanaWebhookEventDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {

#! Base class for native Katana webhook-style events implemented via polling.
/** Katana documents webhook event types (sales_order.packed, purchase_order.received, etc.)
    but real-time delivery requires registering a webhook endpoint. For data-provider
    triggers we approximate the same events by polling the list endpoint with a
    \c status query parameter equal to the target status, plus \c updated_at_min set
    to the last check. Subclasses provide the target status via \c getTargetStatus().
*/
public class KatanaWebhookEventDataProviderBase inherits KatanaWatchDataProviderBase {
    #! Creates the object with an async REST client and options
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    #! Returns extra fetch parameters — adds the target \c status filter if present.
    hash<auto> getExtraFetchParams() {
        hash<auto> rv = {};
        *string status = getTargetStatus();
        if (status.val()) {
            rv.status = status;
        }
        return rv;
    }

    #! Returns the target status code that defines this event (e.g. \c PACKED, \c RECEIVED).
    /** Subclasses must override. Return \c NOTHING for an event that fires on any update
        (in that case the base \c KatanaWatchDataProviderBase behavior is used unchanged).
    */
    abstract *string getTargetStatus();
}

} # namespace KatanaDataProvider
