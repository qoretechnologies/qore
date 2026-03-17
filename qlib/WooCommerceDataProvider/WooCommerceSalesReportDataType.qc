# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSalesReportDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce sales report data type
public class WooCommerceSalesReportDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "total_sales": {
                "display_name": "Total Sales",
                "short_desc": "Gross sales amount",
                "type": StringOrNothingType,
                "desc": "The gross sales amount in the reporting period",
            },
            "net_sales": {
                "display_name": "Net Sales",
                "short_desc": "Net sales amount",
                "type": StringOrNothingType,
                "desc": "The net sales amount (total sales minus refunds and discounts)",
            },
            "average_sales": {
                "display_name": "Average Sales",
                "short_desc": "Average daily sales amount",
                "type": StringOrNothingType,
                "desc": "The average daily sales amount in the reporting period",
            },
            "total_orders": {
                "display_name": "Total Orders",
                "short_desc": "Total number of orders",
                "type": IntOrNothingType,
                "desc": "The total number of orders in the reporting period",
            },
            "total_items": {
                "display_name": "Total Items",
                "short_desc": "Total number of items sold",
                "type": IntOrNothingType,
                "desc": "The total number of items sold in the reporting period",
            },
            "total_tax": {
                "display_name": "Total Tax",
                "short_desc": "Total tax amount",
                "type": StringOrNothingType,
                "desc": "The total tax collected in the reporting period",
            },
            "total_shipping": {
                "display_name": "Total Shipping",
                "short_desc": "Total shipping amount",
                "type": StringOrNothingType,
                "desc": "The total shipping charges in the reporting period",
            },
            "total_refunds": {
                "display_name": "Total Refunds",
                "short_desc": "Total number of refunds",
                "type": IntOrNothingType,
                "desc": "The total number of refunds issued in the reporting period",
            },
            "total_discount": {
                "display_name": "Total Discount",
                "short_desc": "Total discount amount",
                "type": StringOrNothingType,
                "desc": "The total discount amount applied in the reporting period",
            },
            "totals_grouped_by": {
                "display_name": "Totals Grouped By",
                "short_desc": "Grouping period for totals",
                "type": StringOrNothingType,
                "desc": "The time period used for grouping totals (e.g., `day`, `month`)",
            },
            "totals": {
                "display_name": "Totals",
                "short_desc": "Grouped totals data",
                "type": AutoHashOrNothingType,
                "desc": "A hash of totals grouped by the `totals_grouped_by` period",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
