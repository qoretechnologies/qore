# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! The operations the Paddle application exports, and how they are presented
/** The manifest is deliberately explicit.  Paddle's schema declares 70 paths and 99 operations; this
    application exports a reviewed subset, and an endpoint appearing in a future schema does not become a
    public action until it is added here with a name, a description, a group and whatever presentation it
    needs.

    @section paddlemanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces — \c "list_customers", not
    \c "list-customers".  Action identity is what existing workflows call, so it is preserved across the
    replacement rather than restyled; an ID is never derived from Paddle's \c operationId, which is checked
    for drift instead.

    @section paddlemanifest_overlay Notes on the Overlay

    The overlay was re-derived against the current schema rather than carried over from the TypeScript
    application, which called the \c "@paddle/paddle-node-sdk" package rather than the REST API directly.
    What that turned up is worth recording:

    - the TypeScript \c list_customers action presented ordering as a two-field \c order hash and rebuilt
      Paddle's \c "field[DIRECTION]" string from it, offering a list of sortable fields that the schema does
      not agree with.  The REST parameter is a single string and each endpoint documents which fields it
      accepts, so it is presented as the schema declares it.
    - the SDK returns camel-cased properties (\c createdAt); the REST API returns \c created_at.  Every
      response type here is the schema's, so field names are Paddle's own.
    - \c per_page is capped at 200 by Paddle for every listing, not at the SDK's page size; the schema
      carries the cap and the derived type enforces it.

    Paddle's timestamps are RFC 3339 strings, not epoch counts, so unlike Stripe no timestamp codec is
    needed anywhere in this application.
*/
public class PaddleManifest {
    public {
        #! Action group: customers and the balances held against them
        const GroupCustomers = "Customers";

        #! Action group: catalog products
        const GroupProducts = "Products";

        #! Action group: catalog prices
        const GroupPrices = "Prices";

        #! Action group: subscriptions
        const GroupSubscriptions = "Subscriptions";

        #! Action group: transactions
        const GroupTransactions = "Transactions";

        #! Action group: reports
        const GroupReports = "Reports";

        #! Reference data name: customers
        const RefCustomer = "paddle-customer";

        #! Reference data name: catalog products
        const RefProduct = "paddle-product";

        #! Reference data name: catalog prices
        const RefPrice = "paddle-price";

        #! Reference data name: subscriptions
        const RefSubscription = "paddle-subscription";

        #! Reference data name: transactions
        const RefTransaction = "paddle-transaction";

        #! Reference data name: reports
        const RefReport = "paddle-report";

        #! Paddle's maximum page size for a listing
        const MaxPerPage = 200;

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.

            \c per_page is set to Paddle's maximum, so that filling a dropdown is a single request rather
            than a walk through the whole collection, and the records are read from the \c data member of
            Paddle's response envelope.
        */
        const ReferenceData = {
            RefCustomer: <RestSchemaReferenceDataInfo>{
                "name": RefCustomer,
                "action": "list_customers",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "customers in this Paddle account",
            },
            RefProduct: <RestSchemaReferenceDataInfo>{
                "name": RefProduct,
                "action": "list_products",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "products in the Paddle catalog",
            },
            RefPrice: <RestSchemaReferenceDataInfo>{
                "name": RefPrice,
                "action": "list_prices",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                "value_field": "id",
                # a price's "name" is optional, so the description -- which is required -- identifies it
                "display_field": "description",
                "desc": "prices in the Paddle catalog",
            },
            RefSubscription: <RestSchemaReferenceDataInfo>{
                "name": RefSubscription,
                "action": "list_subscriptions",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                # a subscription carries no name of its own; the ID identifies it
                "value_field": "id",
                "desc": "subscriptions in this Paddle account",
            },
            RefTransaction: <RestSchemaReferenceDataInfo>{
                "name": RefTransaction,
                "action": "list_transactions",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                "value_field": "id",
                # only a billed transaction has an invoice number; the ID is shown for the rest
                "display_field": "invoice_number",
                "desc": "transactions in this Paddle account",
            },
            RefReport: <RestSchemaReferenceDataInfo>{
                "name": RefReport,
                "action": "list_reports",
                "options": {"per_page": MaxPerPage},
                "records_path": "data",
                "value_field": "id",
                "display_field": "type",
                "desc": "reports requested from this Paddle account",
            },
        };

        #! Overlay shared by the \c per_page option of every listing action
        const PerPageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Per Page",
            "short_desc": "How many records to return, up to 200",
            "desc": "How many records to return in one request, up to Paddle's maximum of `200`. Asking "
                "for more returns the maximum rather than failing.\n\n"
                "`meta.pagination.per_page` in the response reports how many were actually returned, and "
                "`meta.pagination.has_more` says whether another page exists.",
            "preselected": True,
            "example_value": 50,
        };

        #! Overlay shared by the \c after cursor of every listing action
        const AfterOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "After",
            "short_desc": "Return records after this Paddle ID",
            "desc": "The pagination cursor: return the records that come after this Paddle ID.\n\n"
                "Take it from the `after` query argument of the `meta.pagination.next` URL in the previous "
                "response. Paddle always returns that URL, even on the last page, so page on "
                "`meta.pagination.has_more` rather than on its presence.",
        };

        #! Overlay shared by the \c order_by option of every listing action
        /** Which fields an endpoint accepts differs per endpoint and is documented in the schema's own
            description of the parameter, which the derived option carries.
        */
        const OrderByOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Order By",
            "short_desc": "Order by a field and direction, ex: \"id[ASC]\"",
            "example_value": "id[ASC]",
        };

        #! Overlay shared by the \c Skip-Count header of every listing action
        /** Renamed so that the action option is a %Qore-style identifier rather than an HTTP header name;
            the header is still sent as \c "Skip-Count".
        */
        const SkipCountOverlay = <RestSchemaFieldOverlayInfo>{
            "name": "skip_count",
            "display_name": "Skip Count",
            "short_desc": "Skip the count query, making a large listing cheaper",
            "desc": "Set to `true` to skip the count query Paddle runs for a listing.\n\n"
                "`meta.pagination.estimated_total` then returns `-1` instead of a count. Worth setting when "
                "paging through a large collection, where the count costs time on every page and is not "
                "used.",
        };

        #! Overlay for a customer ID that is the subject of the action
        const CustomerOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Customer",
            "supports_custom_values": True,
            "ref_data": RefCustomer,
            "preselected": True,
        };

        #! Overlay for a customer ID used as a filter or a secondary reference
        const CustomerRefOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Customer",
            "supports_custom_values": True,
            "ref_data": RefCustomer,
        };

        #! Overlay for a product ID that is the subject of the action
        const ProductOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Product",
            "supports_custom_values": True,
            "ref_data": RefProduct,
            "preselected": True,
        };

        #! Overlay for a price ID that is the subject of the action
        const PriceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Price",
            "supports_custom_values": True,
            "ref_data": RefPrice,
            "preselected": True,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Customers ---
            <RestSchemaActionInfo>{
                "action": "list_customers",
                "method": "GET",
                "path": "/customers",
                "operation_id": "list-customers",
                "display_name": "List Customers",
                "short_desc": "List the customers in this Paddle account",
                "desc": "Lists customers, newest first by default.\n\n"
                    "Only `active` customers are returned unless `status` says otherwise, which is worth "
                    "knowing when a lookup by email comes back empty: an archived customer is still there. "
                    "Use `email` for an exact address and `search` for a partial match across `id`, `name` "
                    "and `email`.\n\n"
                    "The response is Paddle's list envelope: the customers are in `data`, and "
                    "`meta.pagination` carries the cursor for the next page.",
                "groups": (GroupCustomers,),
                "field_order": ("search", "email", "status", "per_page", "after", "id", "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "short_desc": "Return only customers with these exact email addresses",
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer",
                        "short_desc": "Return only these customers",
                        "supports_custom_values": True,
                        "element_ref_data": RefCustomer,
                    },
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against id, name and email",
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only customers with these statuses",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_customer",
                "method": "POST",
                "path": "/customers",
                "operation_id": "create-customer",
                "display_name": "Create Customer",
                "short_desc": "Create a customer",
                "desc": "Creates a customer.\n\n"
                    "`email` is the only required field and it must be unique in the account: creating a "
                    "customer with an address that already exists fails with a conflict rather than "
                    "returning the existing one. Look the address up with **List Customers** first if that "
                    "is a possibility.\n\n"
                    "Use `custom_data` to carry your own identifier for the customer, so the two systems "
                    "can be reconciled later.",
                "groups": (GroupCustomers,),
                "field_order": ("email", "name", "locale", "custom_data", "marketing_consent", "id"),
                "response_unwrap": ("data",),
                "fields": {
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer ID",
                        "short_desc": "Import an existing customer under a known Paddle ID",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_customer",
                "method": "GET",
                "path": "/customers/{customer_id}",
                "operation_id": "get-customer",
                "display_name": "Get Customer",
                "short_desc": "Retrieve one customer",
                "desc": "Retrieves a customer by Paddle ID, including any `custom_data` stored against it.\n\n"
                    "This returns a customer whatever its status, so it is how an archived customer is read "
                    "back — **List Customers** omits archived customers by default.",
                "groups": (GroupCustomers,),
                "response_unwrap": ("data",),
                "fields": {
                    "customer_id": CustomerOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_customer",
                "method": "PATCH",
                "path": "/customers/{customer_id}",
                "operation_id": "update-customer",
                "display_name": "Update Customer",
                "short_desc": "Update a customer",
                "desc": "Updates a customer. Only the fields supplied are changed; everything else is left "
                    "as it is.\n\n"
                    "Setting `status` to `archived` is how a customer is retired: an archived customer "
                    "cannot be billed and stops appearing in listings, but its transactions and "
                    "subscriptions are untouched.\n\n"
                    "**`custom_data` is replaced, not merged.** Send the whole object, or previously stored "
                    "keys are lost.",
                "groups": (GroupCustomers,),
                "field_order": ("customer_id", "name", "email", "status", "custom_data", "locale",
                    "marketing_consent"),
                "response_unwrap": ("data",),
                "fields": {
                    "customer_id": CustomerOverlay,
                    "custom_data": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Replaces the stored custom data in full",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Set to \"archived\" to retire the customer",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_customer_auth_token",
                "method": "POST",
                "path": "/customers/{customer_id}/auth-token",
                "operation_id": "generate-customer-authentication-token",
                "display_name": "Get Customer Authentication Token",
                "short_desc": "Generate a short-lived token identifying a customer to Paddle.js",
                "desc": "Generates an authentication token for a customer, so that a checkout opened with "
                    "Paddle.js knows who the buyer is without them signing in again.\n\n"
                    "**The token is short-lived and grants access to that customer's data**, so generate it "
                    "per checkout, server-side, and pass it straight to the browser rather than storing "
                    "it.\n\n"
                    "This needs the `customer_auth_token.write` permission, which is separate from "
                    "`customer.write`.",
                "groups": (GroupCustomers,),
                "response_unwrap": ("data",),
                "fields": {
                    "customer_id": CustomerOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list_customer_credit_balances",
                "method": "GET",
                "path": "/customers/{customer_id}/credit-balances",
                "operation_id": "list-credit-balances",
                "display_name": "List Customer Credit Balances",
                "short_desc": "List a customer's credit balances, by currency",
                "desc": "Lists the credit a customer holds, one entry per currency.\n\n"
                    "Each entry splits the balance into `available` credit, credit `reserved` against an "
                    "unpaid transaction, and the `used` total. Credit arises from a refund or an adjustment "
                    "and is applied automatically to the customer's next transaction in the same "
                    "currency.\n\n"
                    "This listing is not paginated: every currency the customer holds credit in is "
                    "returned.",
                "groups": (GroupCustomers,),
                "response_unwrap": ("data",),
                "fields": {
                    "customer_id": CustomerOverlay,
                    "currency_code": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Currency Code",
                        "short_desc": "Return only balances in these currencies",
                        "preselected": True,
                    },
                },
            },

            # --- Products ---
            <RestSchemaActionInfo>{
                "action": "list_products",
                "method": "GET",
                "path": "/products",
                "operation_id": "list-products",
                "display_name": "List Products",
                "short_desc": "List the products in the catalog",
                "desc": "Lists catalog products.\n\n"
                    "Only `active` products are returned unless `status` says otherwise. Set `include` to "
                    "`prices` to get each product's prices in the same request, which saves a call per "
                    "product when building a pricing page.\n\n"
                    "The products are in `data`, and `meta.pagination` carries the cursor for the next page.",
                "groups": (GroupProducts,),
                "field_order": ("status", "include", "per_page", "after", "id", "tax_category", "type",
                    "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Product",
                        "short_desc": "Return only these products",
                        "supports_custom_values": True,
                        "element_ref_data": RefProduct,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only products with these statuses",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_product",
                "method": "POST",
                "path": "/products",
                "operation_id": "create-product",
                "display_name": "Create Product",
                "short_desc": "Create a catalog product",
                "desc": "Creates a product in the catalog.\n\n"
                    "A product is what you sell; it carries no price of its own. Create at least one price "
                    "against it with **Create Price** before it can be billed.\n\n"
                    "`tax_category` decides how Paddle charges sales tax and **cannot be changed "
                    "afterwards**, so it has to be right at creation. Most software is "
                    "`standard-digital-goods` or `saas`; using the wrong category means tax is calculated "
                    "wrongly for every sale of the product.",
                "groups": (GroupProducts,),
                "field_order": ("name", "tax_category", "description", "type", "image_url", "custom_data",
                    "id"),
                "response_unwrap": ("data",),
                "fields": {
                    "description": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Product ID",
                        "short_desc": "Import an existing product under a known Paddle ID",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "tax_category": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "How Paddle charges tax; cannot be changed later",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_product",
                "method": "GET",
                "path": "/products/{product_id}",
                "operation_id": "get-product",
                "display_name": "Get Product",
                "short_desc": "Retrieve one catalog product",
                "desc": "Retrieves a product by Paddle ID, whatever its status.\n\n"
                    "Set `include` to `prices` to get the product's prices in the same response.",
                "groups": (GroupProducts,),
                "response_unwrap": ("data",),
                "fields": {
                    "product_id": ProductOverlay,
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_product",
                "method": "PATCH",
                "path": "/products/{product_id}",
                "operation_id": "update-product",
                "display_name": "Update Product",
                "short_desc": "Update a catalog product",
                "desc": "Updates a product. Only the fields supplied are changed.\n\n"
                    "`tax_category` is fixed at creation and cannot be changed here; a product in the wrong "
                    "category has to be archived and replaced.\n\n"
                    "**`custom_data` is replaced, not merged.** Send the whole object, or previously stored "
                    "keys are lost.",
                "groups": (GroupProducts,),
                "field_order": ("product_id", "name", "description", "status", "image_url", "custom_data",
                    "type", "tax_category"),
                "response_unwrap": ("data",),
                "fields": {
                    "product_id": ProductOverlay,
                    "custom_data": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Replaces the stored custom data in full",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Set to \"archived\" to retire the product",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "archive_product",
                "method": "PATCH",
                "path": "/products/{product_id}",
                "operation_id": "update-product",
                "variant_of": "update_product",
                "display_name": "Archive Product",
                "short_desc": "Retire a catalog product",
                "desc": "Archives a product, so it can no longer be billed and stops appearing in "
                    "listings.\n\n"
                    "Existing subscriptions and transactions that reference the product are unaffected — "
                    "archiving withdraws it from sale, it does not delete it. Paddle has no delete "
                    "operation for a product.\n\n"
                    "**The product's prices are not archived with it.** Archive each price separately if "
                    "they should also be withdrawn.\n\n"
                    "This is **Update Product** with `status` fixed to `archived`; use that action to "
                    "change anything else, or to un-archive by setting `status` back to `active`.",
                "groups": (GroupProducts,),
                "field_order": ("product_id",),
                "response_unwrap": ("data",),
                "fields": {
                    "product_id": ProductOverlay,
                    # everything the update operation offers other than the status is dropped, so that this
                    # action does one thing; "Update Product" exports the rest
                    "name": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "description": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "type": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "tax_category": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "image_url": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "custom_data": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Always \"archived\" for this action",
                        "desc": "Fixed to `archived`: this action exists to retire a product. Use **Update "
                            "Product** to set any other status.",
                        "required": True,
                        "default_value": "archived",
                        "allowed_values": ("archived",),
                        "preselected": True,
                    },
                },
            },

            # --- Prices ---
            <RestSchemaActionInfo>{
                "action": "list_prices",
                "method": "GET",
                "path": "/prices",
                "operation_id": "list-prices",
                "display_name": "List Prices",
                "short_desc": "List the prices in the catalog",
                "desc": "Lists catalog prices.\n\n"
                    "Only `active` prices are returned unless `status` says otherwise. Filter by "
                    "`product_id` for one product's prices, or by `recurring` to separate subscription "
                    "prices from one-off ones. Set `include` to `product` to get the owning product "
                    "alongside each price.\n\n"
                    "The prices are in `data`, and `meta.pagination` carries the cursor for the next page.",
                "groups": (GroupPrices,),
                "field_order": ("product_id", "status", "recurring", "include", "per_page", "after", "id",
                    "type", "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Price",
                        "short_desc": "Return only these prices",
                        "supports_custom_values": True,
                        "element_ref_data": RefPrice,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "product_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Product",
                        "short_desc": "Return only prices for these products",
                        "supports_custom_values": True,
                        "element_ref_data": RefProduct,
                        "preselected": True,
                    },
                    "recurring": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "True for subscription prices, false for one-off prices",
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only prices with these statuses",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_price",
                "method": "POST",
                "path": "/prices",
                "operation_id": "create-price",
                "display_name": "Create Price",
                "short_desc": "Create a price for a catalog product",
                "desc": "Creates a price against a product.\n\n"
                    "`unit_price` is an amount in the **smallest unit of its currency** given as a string — "
                    "`\"1000\"` is $10.00 in USD, but ¥1000 in JPY, which has no minor unit. Getting this "
                    "wrong is a factor-of-100 error in what customers are charged.\n\n"
                    "Set `billing_cycle` to make it a subscription price; leave it unset for a one-off "
                    "charge. `unit_price_overrides` sets a different amount for specific countries, which "
                    "is how local pricing is done.\n\n"
                    "A price cannot be moved to another product afterwards.",
                "groups": (GroupPrices,),
                "field_order": ("product_id", "description", "unit_price", "billing_cycle", "trial_period",
                    "name", "tax_mode", "quantity", "unit_price_overrides", "custom_data", "type", "id"),
                "response_unwrap": ("data",),
                "fields": {
                    "description": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "An internal description; not shown to customers",
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Price ID",
                        "short_desc": "Import an existing price under a known Paddle ID",
                    },
                    "product_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Product",
                        "short_desc": "The product this price belongs to",
                        "supports_custom_values": True,
                        "ref_data": RefProduct,
                        "preselected": True,
                    },
                    "unit_price": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The amount, in the smallest unit of its currency",
                        "preselected": True,
                    },
                    "billing_cycle": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Set for a subscription price; leave unset for a one-off charge",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_price",
                "method": "GET",
                "path": "/prices/{price_id}",
                "operation_id": "get-price",
                "display_name": "Get Price",
                "short_desc": "Retrieve one catalog price",
                "desc": "Retrieves a price by Paddle ID, whatever its status.\n\n"
                    "Set `include` to `product` to get the owning product in the same response.",
                "groups": (GroupPrices,),
                "response_unwrap": ("data",),
                "fields": {
                    "price_id": PriceOverlay,
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_price",
                "method": "PATCH",
                "path": "/prices/{price_id}",
                "operation_id": "update-price",
                "display_name": "Update Price",
                "short_desc": "Update a catalog price",
                "desc": "Updates a price. Only the fields supplied are changed.\n\n"
                    "**Changing `unit_price` changes what new purchases cost; existing subscriptions keep "
                    "the amount they were created with** until they are explicitly updated. Raising a "
                    "published price is normally done by creating a new price and archiving the old one, so "
                    "that the change is visible in the catalog.\n\n"
                    "Set `status` to `archived` to withdraw the price from sale.",
                "groups": (GroupPrices,),
                "field_order": ("price_id", "description", "unit_price", "status", "billing_cycle",
                    "trial_period", "name", "tax_mode", "quantity", "unit_price_overrides", "custom_data",
                    "type"),
                "response_unwrap": ("data",),
                "fields": {
                    "price_id": PriceOverlay,
                    "custom_data": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Replaces the stored custom data in full",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Set to \"archived\" to withdraw the price from sale",
                        "preselected": True,
                    },
                    "unit_price": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The amount, in the smallest unit of its currency",
                        "preselected": True,
                    },
                },
            },

            # --- Subscriptions ---
            <RestSchemaActionInfo>{
                "action": "list_subscriptions",
                "method": "GET",
                "path": "/subscriptions",
                "operation_id": "list-subscriptions",
                "display_name": "List Subscriptions",
                "short_desc": "List the subscriptions in this Paddle account",
                "desc": "Lists subscriptions.\n\n"
                    "Filter by `customer_id` for one customer's subscriptions, by `status` for the state "
                    "they are in — `active`, `trialing`, `past_due`, `paused` or `canceled` — or by "
                    "`scheduled_change_action` to find the ones with a pause or cancellation already "
                    "booked.\n\n"
                    "The subscriptions are in `data`, and `meta.pagination` carries the cursor for the next "
                    "page.",
                "groups": (GroupSubscriptions,),
                "field_order": ("customer_id", "status", "price_id", "per_page", "after", "id",
                    "collection_mode", "scheduled_change_action", "next_billed_at", "address_id",
                    "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "customer_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer",
                        "short_desc": "Return only subscriptions for these customers",
                        "supports_custom_values": True,
                        "element_ref_data": RefCustomer,
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription",
                        "short_desc": "Return only these subscriptions",
                        "supports_custom_values": True,
                        "element_ref_data": RefSubscription,
                    },
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "price_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Price",
                        "short_desc": "Return only subscriptions billing these prices",
                        "supports_custom_values": True,
                        "element_ref_data": RefPrice,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only subscriptions with these statuses",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_subscription",
                "method": "GET",
                "path": "/subscriptions/{subscription_id}",
                "operation_id": "get-subscription",
                "display_name": "Get Subscription",
                "short_desc": "Retrieve one subscription",
                "desc": "Retrieves a subscription by Paddle ID.\n\n"
                    "`items` is what the customer is billed for, `current_billing_period` is the window "
                    "they have paid for, and `scheduled_change` is a pause or cancellation already booked "
                    "for a future date — a subscription can be `active` and still be due to end.\n\n"
                    "Set `include` to `next_transaction` for what the next renewal will cost, or "
                    "`recurring_transaction_details` for the recurring amount.",
                "groups": (GroupSubscriptions,),
                "response_unwrap": ("data",),
                "fields": {
                    "subscription_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription",
                        "supports_custom_values": True,
                        "ref_data": RefSubscription,
                        "preselected": True,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                },
            },

            # --- Transactions ---
            <RestSchemaActionInfo>{
                "action": "list_transactions",
                "method": "GET",
                "path": "/transactions",
                "operation_id": "list-transactions",
                "display_name": "List Transactions",
                "short_desc": "List the transactions in this Paddle account",
                "desc": "Lists transactions — the records that calculate and capture revenue.\n\n"
                    "Filter by `status` to separate what has been paid (`completed`, `paid`) from what is "
                    "outstanding (`billed`, `past_due`) or still a draft (`draft`, `ready`). `created_at`, "
                    "`updated_at` and `billed_at` take an operator suffix, as in "
                    "`updated_at[GT]=2026-01-01T00:00:00Z`, which is how an incremental sync picks up only "
                    "what changed.\n\n"
                    "`per_page` defaults to 30 here rather than 50, because a transaction is a large "
                    "object.",
                "groups": (GroupTransactions,),
                "field_order": ("customer_id", "status", "subscription_id", "updated_at", "created_at",
                    "billed_at", "include", "per_page", "after", "id", "invoice_number", "origin",
                    "collection_mode", "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "billed_at": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Filter on when the transaction was billed; takes an operator suffix",
                    },
                    "created_at": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Filter on creation time; takes an operator suffix",
                    },
                    "customer_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer",
                        "short_desc": "Return only transactions for these customers",
                        "supports_custom_values": True,
                        "element_ref_data": RefCustomer,
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Transaction",
                        "short_desc": "Return only these transactions",
                        "supports_custom_values": True,
                        "element_ref_data": RefTransaction,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                    },
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only transactions with these statuses",
                        "preselected": True,
                    },
                    "subscription_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription",
                        "short_desc": "Return only transactions for these subscriptions",
                        "supports_custom_values": True,
                        "element_ref_data": RefSubscription,
                        "preselected": True,
                    },
                    "updated_at": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Filter on last-modified time; takes an operator suffix",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_transaction",
                "method": "POST",
                "path": "/transactions",
                "operation_id": "create-transaction",
                "display_name": "Create Transaction",
                "short_desc": "Create a transaction, optionally billing it",
                "desc": "Creates a transaction: the record of what a customer is being charged for.\n\n"
                    "`items` is required — each entry names a `price_id` and a `quantity`, or carries a "
                    "one-off price inline. `status` decides what happens next: leave it unset for a `draft` "
                    "that can still be edited, or set it to `billed` to issue it to the customer.\n\n"
                    "**A transaction with no `customer_id` is a quote nobody can pay**; set it, along with "
                    "`address_id` where tax depends on location, for anything the customer is meant to "
                    "settle.\n\n"
                    "Set `include` to `totals` to get the calculated amounts back in the same response, "
                    "which is the cheapest way to see what a transaction actually comes to.",
                "groups": (GroupTransactions,),
                "field_order": ("items", "customer_id", "status", "currency_code", "collection_mode",
                    "address_id", "business_id", "discount_id", "subscription_id", "billing_period",
                    "billing_details", "custom_data", "include"),
                "response_unwrap": ("data",),
                "fields": {
                    "customer_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer",
                        "short_desc": "The customer being billed",
                        "supports_custom_values": True,
                        "ref_data": RefCustomer,
                        "preselected": True,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities, ex: \"totals\", in the response",
                    },
                    "items": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "What is being billed: a price and a quantity per line",
                        "preselected": True,
                    },
                    # the only option whose schema chain carries no description of its own
                    "origin": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Origin",
                        "short_desc": "How the transaction came about",
                        "desc": "How the transaction came about — `api` for one created through the API, "
                            "`web` for a Paddle.js checkout, or one of the `subscription_…` values for one "
                            "Paddle raised itself for a renewal, charge or update.\n\n"
                            "Leave this unset when creating a transaction: Paddle sets it.",
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Leave unset for a draft; \"billed\" issues it to the customer",
                        "preselected": True,
                    },
                    "subscription_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription",
                        "supports_custom_values": True,
                        "ref_data": RefSubscription,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_transaction",
                "method": "GET",
                "path": "/transactions/{transaction_id}",
                "operation_id": "get-transaction",
                "display_name": "Get Transaction",
                "short_desc": "Retrieve one transaction",
                "desc": "Retrieves a transaction by Paddle ID.\n\n"
                    "`details.totals` breaks the amount down into subtotal, tax, discount and the balance "
                    "still due; `payments` records each attempt to collect it, including failures. `status` "
                    "is what says whether the money actually arrived.\n\n"
                    "Set `include` to `customer` or `address` to resolve the related entities in the same "
                    "response.",
                "groups": (GroupTransactions,),
                "response_unwrap": ("data",),
                "fields": {
                    "transaction_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Transaction",
                        "supports_custom_values": True,
                        "ref_data": RefTransaction,
                        "preselected": True,
                    },
                    "include": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include",
                        "short_desc": "Include related entities in the response",
                        "preselected": True,
                    },
                },
            },

            # --- Reports ---
            <RestSchemaActionInfo>{
                "action": "list_reports",
                "method": "GET",
                "path": "/reports",
                "operation_id": "list-reports",
                "display_name": "List Reports",
                "short_desc": "List the reports requested from this account",
                "desc": "Lists reports and the state each is in.\n\n"
                    "A report is generated asynchronously: it starts `pending`, becomes `ready` when the "
                    "file can be downloaded, and is `expired` once Paddle has removed it. Filter by "
                    "`status` to find the ones worth downloading.\n\n"
                    "Reports are kept for a limited time, so a job that generates one should download it "
                    "rather than assume it will still be there later.",
                "groups": (GroupReports,),
                "field_order": ("status", "per_page", "after", "order_by"),
                "fields": {
                    "after": AfterOverlay,
                    "order_by": OrderByOverlay,
                    "per_page": PerPageOverlay,
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only reports with these statuses",
                        "preselected": True,
                    },
                    "Skip-Count": SkipCountOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_report",
                "method": "POST",
                "path": "/reports",
                "operation_id": "create-report",
                "display_name": "Create Report",
                "short_desc": "Request a report; it is generated asynchronously",
                "desc": "Requests a report. Paddle generates it in the background, so this returns a report "
                    "in the `pending` state rather than any data.\n\n"
                    "The body selects the report type and its filters, and the fields a report accepts "
                    "depend on the type: `transactions`, `adjustments`, `products_prices`, `discounts`, "
                    "`balance`, `payout_reconciliation` or `checkouts`. Because the shape differs per type, "
                    "the body is supplied whole rather than as separate options.\n\n"
                    "**With no `filters`, a report covers only the last 30 days of updates.** Pass an "
                    "`updated_at` filter for any other window.\n\n"
                    "Poll **Get Report** until `status` is `ready`, or subscribe to the `report.updated` "
                    "event, then call **Get Report File** for the download URL.",
                "groups": (GroupReports,),
                "flatten_body": False,
                "response_unwrap": ("data",),
                "fields": {
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Report",
                        "short_desc": "The report type and its filters",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_report",
                "method": "GET",
                "path": "/reports/{report_id}",
                "operation_id": "get-report",
                "display_name": "Get Report",
                "short_desc": "Retrieve one report and its status",
                "desc": "Retrieves a report by Paddle ID.\n\n"
                    "`status` is what matters: `pending` while Paddle is still generating it, `ready` when "
                    "the file can be downloaded, `failed` if generation did not succeed, and `expired` once "
                    "Paddle has removed it. `rows` is how many records it holds.\n\n"
                    "This is the action to poll after **Create Report**.",
                "groups": (GroupReports,),
                "response_unwrap": ("data",),
                "fields": {
                    "report_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Report",
                        "supports_custom_values": True,
                        "ref_data": RefReport,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_report_file",
                "method": "GET",
                "path": "/reports/{report_id}/download-url",
                "operation_id": "get-report-csv",
                "display_name": "Get Report File",
                "short_desc": "Get a download URL for a report's CSV file",
                "desc": "Returns a URL the report's CSV file can be downloaded from.\n\n"
                    "**This is a URL, not the file**: fetch it separately. The URL is signed and "
                    "short-lived — Paddle expires it within the hour — so generate it at the point of "
                    "download rather than storing it.\n\n"
                    "The report must be `ready`; asking for the file of a report that is still `pending` "
                    "fails.",
                "groups": (GroupReports,),
                "response_unwrap": ("data",),
                "fields": {
                    "report_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Report",
                        "supports_custom_values": True,
                        "ref_data": RefReport,
                        "preselected": True,
                    },
                },
            },
        );
    }
}
}
