# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

/*  EasyPostDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost app name constant
public const AppName = "EasyPost";

#! Default page size for pagination
public const DefaultPageSize = 25;

#! Maximum page size for EasyPost API
public const MaxPageSize = 100;

# ============================================================================
# Wrapper keys (request body wrappers used by the EasyPost API)
# ID prefixes: address=adr_, parcel=prcl_, shipment=shp_, tracker=trk_,
#   insurance=ins_, pickup=pickup_, refund=rfnd_, batch=batch_, webhook=hook_,
#   event=evt_, customs_info=cstinfo_, customs_item=cstitem_, order=order_,
#   carrier_account=ca_, end_shipper=es_
# ============================================================================

#! Wrapper key: address
public const WrapperKeyAddress = "address";

#! Wrapper key: parcel
public const WrapperKeyParcel = "parcel";

#! Wrapper key: shipment
public const WrapperKeyShipment = "shipment";

#! Wrapper key: tracker
public const WrapperKeyTracker = "tracker";

#! Wrapper key: insurance
public const WrapperKeyInsurance = "insurance";

#! Wrapper key: pickup
public const WrapperKeyPickup = "pickup";

#! Wrapper key: refund
public const WrapperKeyRefund = "refund";

#! Wrapper key: batch
public const WrapperKeyBatch = "batch";

#! Wrapper key: webhook
public const WrapperKeyWebhook = "webhook";

#! Wrapper key: customs_info
public const WrapperKeyCustomsInfo = "customs_info";

#! Wrapper key: customs_item
public const WrapperKeyCustomsItem = "customs_item";

#! Wrapper key: order
public const WrapperKeyOrder = "order";

#! Wrapper key: carrier_account
public const WrapperKeyCarrierAccount = "carrier_account";

# ============================================================================
# Local event types (logical names used in event provider registration)
# ============================================================================

#! Event type: tracker created
public const EventTrackerCreated = "tracker.created";

#! Event type: tracker updated
public const EventTrackerUpdated = "tracker.updated";

#! Event type: batch created
public const EventBatchCreated = "batch.created";

#! Event type: batch updated
public const EventBatchUpdated = "batch.updated";

#! Event type: insurance purchased
public const EventInsurancePurchased = "insurance.purchased";

#! Event type: refund successful
public const EventRefundSuccessful = "refund.successful";

#! Event type: shipment invoice created
public const EventShipmentInvoiceCreated = "shipment.invoice.created";

#! Event type: payment completed
public const EventPaymentCompleted = "payment.completed";

#! Event type: payment failed
public const EventPaymentFailed = "payment.failed";

#! Event type: payment created
public const EventPaymentCreated = "payment.created";

#! Event type: insurance cancelled (also fired when a standalone Insurance fails purchasing)
public const EventInsuranceCancelled = "insurance.cancelled";

#! Event type: scan form created
public const EventScanFormCreated = "scan_form.created";

#! Event type: scan form updated
public const EventScanFormUpdated = "scan_form.updated";

#! Event type: shipment invoice updated
public const EventShipmentInvoiceUpdated = "shipment.invoice.updated";

# ============================================================================
# EasyPost webhook event description strings (these are the values returned
# by the API in `description` of an Event object)
# ============================================================================

#! Webhook event: tracker created
public const WebhookEventTrackerCreated = "tracker.created";

#! Webhook event: tracker updated
public const WebhookEventTrackerUpdated = "tracker.updated";

#! Webhook event: batch created
public const WebhookEventBatchCreated = "batch.created";

#! Webhook event: batch updated
public const WebhookEventBatchUpdated = "batch.updated";

#! Webhook event: insurance purchased
public const WebhookEventInsurancePurchased = "insurance.purchased";

#! Webhook event: refund successful
public const WebhookEventRefundSuccessful = "refund.successful";

#! Webhook event: shipment invoice created
public const WebhookEventShipmentInvoiceCreated = "shipment.invoice.created";

#! Webhook event: payment completed
public const WebhookEventPaymentCompleted = "payment.completed";

#! Webhook event: payment failed
public const WebhookEventPaymentFailed = "payment.failed";

#! Webhook event: payment created
public const WebhookEventPaymentCreated = "payment.created";

#! Webhook event: insurance cancelled
public const WebhookEventInsuranceCancelled = "insurance.cancelled";

#! Webhook event: scan form created
public const WebhookEventScanFormCreated = "scan_form.created";

#! Webhook event: scan form updated
public const WebhookEventScanFormUpdated = "scan_form.updated";

#! Webhook event: shipment invoice updated
public const WebhookEventShipmentInvoiceUpdated = "shipment.invoice.updated";

#! All supported event types
public const AllEventTypes = (
    EventTrackerCreated,
    EventTrackerUpdated,
    EventBatchCreated,
    EventBatchUpdated,
    EventInsurancePurchased,
    EventInsuranceCancelled,
    EventRefundSuccessful,
    EventShipmentInvoiceCreated,
    EventShipmentInvoiceUpdated,
    EventPaymentCompleted,
    EventPaymentCreated,
    EventPaymentFailed,
    EventScanFormCreated,
    EventScanFormUpdated,
);

#! EasyPost logo in SVG format (loaded from file)
public const EasyPostLogo = File::readTextFile(get_script_dir() + "/easypost-logo.svg");

} # namespace EasyPostDataProvider
