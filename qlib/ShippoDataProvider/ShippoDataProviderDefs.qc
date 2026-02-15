# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {
    #! Application name constant
    public const AppName = "Shippo";

    #! Shippo logo
    public const ShippoLogo = File::readTextFile(get_script_dir() + "/shippo-logo.svg");

    #! Default pagination page size
    public const DefaultPageSize = 100;

    #! Maximum pagination page size
    public const MaxPageSize = 100;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 120;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Event type for new parcel detection
    public const EVENT_NEW_PARCEL = "new-parcel";

    #! Event type for new shipment detection
    public const EVENT_NEW_SHIPMENT = "new-shipment";

    #! Event type for new shipping label detection
    public const EVENT_NEW_LABEL = "new-label";

    #! Event type for new order detection
    public const EVENT_NEW_ORDER = "new-order";

    #! Shipment status: Waiting for rates
    public const SHIPMENT_STATUS_WAITING = "WAITING";

    #! Shipment status: Queued for processing
    public const SHIPMENT_STATUS_QUEUED = "QUEUED";

    #! Shipment status: Successfully processed
    public const SHIPMENT_STATUS_SUCCESS = "SUCCESS";

    #! Shipment status: Error occurred
    public const SHIPMENT_STATUS_ERROR = "ERROR";

    #! Transaction (label) status: Queued for processing
    public const TRANSACTION_STATUS_QUEUED = "QUEUED";

    #! Transaction (label) status: Waiting for carrier response
    public const TRANSACTION_STATUS_WAITING = "WAITING";

    #! Transaction (label) status: Successfully created
    public const TRANSACTION_STATUS_SUCCESS = "SUCCESS";

    #! Transaction (label) status: Error occurred
    public const TRANSACTION_STATUS_ERROR = "ERROR";

    #! Transaction (label) status: Refunded
    public const TRANSACTION_STATUS_REFUNDED = "REFUNDED";

    #! Order status: Unknown/not set
    public const ORDER_STATUS_UNKNOWN = "UNKNOWN";

    #! Order status: Paid
    public const ORDER_STATUS_PAID = "PAID";

    #! Order status: Shipped
    public const ORDER_STATUS_SHIPPED = "SHIPPED";

    #! Order status: Delivered
    public const ORDER_STATUS_DELIVERED = "DELIVERED";

    #! Order status: Cancelled
    public const ORDER_STATUS_CANCELLED = "CANCELLED";

    #! Order status: Returned
    public const ORDER_STATUS_RETURNED = "RETURNED";

    #! Distance unit: inches
    public const DISTANCE_UNIT_INCHES = "in";

    #! Distance unit: centimeters
    public const DISTANCE_UNIT_CM = "cm";

    #! Distance unit: feet
    public const DISTANCE_UNIT_FEET = "ft";

    #! Distance unit: millimeters
    public const DISTANCE_UNIT_MM = "mm";

    #! Distance unit: meters
    public const DISTANCE_UNIT_M = "m";

    #! Distance unit: yards
    public const DISTANCE_UNIT_YD = "yd";

    #! Mass unit: grams
    public const MASS_UNIT_G = "g";

    #! Mass unit: ounces
    public const MASS_UNIT_OZ = "oz";

    #! Mass unit: pounds
    public const MASS_UNIT_LB = "lb";

    #! Mass unit: kilograms
    public const MASS_UNIT_KG = "kg";

    #! List of valid distance units
    public const DistanceUnits = (
        DISTANCE_UNIT_INCHES,
        DISTANCE_UNIT_CM,
        DISTANCE_UNIT_FEET,
        DISTANCE_UNIT_MM,
        DISTANCE_UNIT_M,
        DISTANCE_UNIT_YD,
    );

    #! List of valid mass units
    public const MassUnits = (
        MASS_UNIT_G,
        MASS_UNIT_OZ,
        MASS_UNIT_LB,
        MASS_UNIT_KG,
    );

    #! List of valid shipment statuses
    public const ShipmentStatuses = (
        SHIPMENT_STATUS_WAITING,
        SHIPMENT_STATUS_QUEUED,
        SHIPMENT_STATUS_SUCCESS,
        SHIPMENT_STATUS_ERROR,
    );

    #! List of valid transaction statuses
    public const TransactionStatuses = (
        TRANSACTION_STATUS_QUEUED,
        TRANSACTION_STATUS_WAITING,
        TRANSACTION_STATUS_SUCCESS,
        TRANSACTION_STATUS_ERROR,
        TRANSACTION_STATUS_REFUNDED,
    );

    #! List of valid order statuses
    public const OrderStatuses = (
        ORDER_STATUS_UNKNOWN,
        ORDER_STATUS_PAID,
        ORDER_STATUS_SHIPPED,
        ORDER_STATUS_DELIVERED,
        ORDER_STATUS_CANCELLED,
        ORDER_STATUS_RETURNED,
    );
}
