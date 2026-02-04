# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation app name constant
public const AppName = "ShipStation";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Maximum page size for ShipStation API
public const MaxPageSize = 500;

#! Order status: awaiting payment
public const ORDER_STATUS_AWAITING_PAYMENT = "awaiting_payment";

#! Order status: awaiting shipment
public const ORDER_STATUS_AWAITING_SHIPMENT = "awaiting_shipment";

#! Order status: shipped
public const ORDER_STATUS_SHIPPED = "shipped";

#! Order status: on hold
public const ORDER_STATUS_ON_HOLD = "on_hold";

#! Order status: cancelled
public const ORDER_STATUS_CANCELLED = "cancelled";

#! Order status: pending fulfillment
public const ORDER_STATUS_PENDING_FULFILLMENT = "pending_fulfillment";

#! Order status: rejected fulfillment
public const ORDER_STATUS_REJECTED_FULFILLMENT = "rejected_fulfillment";

#! All supported order statuses
public const AllOrderStatuses = (
    ORDER_STATUS_AWAITING_PAYMENT,
    ORDER_STATUS_AWAITING_SHIPMENT,
    ORDER_STATUS_SHIPPED,
    ORDER_STATUS_ON_HOLD,
    ORDER_STATUS_CANCELLED,
    ORDER_STATUS_PENDING_FULFILLMENT,
    ORDER_STATUS_REJECTED_FULFILLMENT,
);

#! Event type for order notification (new/updated orders)
public const EVENT_ORDER_NOTIFY = "order-notify";

#! Event type for order notification with item details
public const EVENT_ITEM_ORDER_NOTIFY = "item-order-notify";

#! Event type for ship notification (label generated)
public const EVENT_SHIP_NOTIFY = "ship-notify";

#! Event type for ship notification with item details
public const EVENT_ITEM_SHIP_NOTIFY = "item-ship-notify";

#! ShipStation webhook event type for ORDER_NOTIFY
public const WEBHOOK_ORDER_NOTIFY = "ORDER_NOTIFY";

#! ShipStation webhook event type for ITEM_ORDER_NOTIFY
public const WEBHOOK_ITEM_ORDER_NOTIFY = "ITEM_ORDER_NOTIFY";

#! ShipStation webhook event type for SHIP_NOTIFY
public const WEBHOOK_SHIP_NOTIFY = "SHIP_NOTIFY";

#! ShipStation webhook event type for ITEM_SHIP_NOTIFY
public const WEBHOOK_ITEM_SHIP_NOTIFY = "ITEM_SHIP_NOTIFY";

#! Map of event types to webhook event types
public const EventToWebhookMap = {
    EVENT_ORDER_NOTIFY: WEBHOOK_ORDER_NOTIFY,
    EVENT_ITEM_ORDER_NOTIFY: WEBHOOK_ITEM_ORDER_NOTIFY,
    EVENT_SHIP_NOTIFY: WEBHOOK_SHIP_NOTIFY,
    EVENT_ITEM_SHIP_NOTIFY: WEBHOOK_ITEM_SHIP_NOTIFY,
};

#! All supported event types
public const AllEventTypes = (
    EVENT_ORDER_NOTIFY,
    EVENT_ITEM_ORDER_NOTIFY,
    EVENT_SHIP_NOTIFY,
    EVENT_ITEM_SHIP_NOTIFY,
);

#! Confirmation type: none
public const CONFIRMATION_NONE = "none";

#! Confirmation type: delivery
public const CONFIRMATION_DELIVERY = "delivery";

#! Confirmation type: signature
public const CONFIRMATION_SIGNATURE = "signature";

#! Confirmation type: adult signature
public const CONFIRMATION_ADULT_SIGNATURE = "adult_signature";

#! Confirmation type: direct signature
public const CONFIRMATION_DIRECT_SIGNATURE = "direct_signature";

#! All supported confirmation types
public const AllConfirmationTypes = (
    CONFIRMATION_NONE,
    CONFIRMATION_DELIVERY,
    CONFIRMATION_SIGNATURE,
    CONFIRMATION_ADULT_SIGNATURE,
    CONFIRMATION_DIRECT_SIGNATURE,
);

#! ShipStation logo in SVG format (loaded from file)
public const ShipStationLogo = File::readTextFile(get_script_dir() + "/shipstation-logo.svg");

} # namespace ShipStationDataProvider
