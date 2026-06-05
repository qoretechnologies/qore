# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudCreateParcelDataProvider.qc SendCloud create-parcel action data provider

/*  SendCloudCreateParcelDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Create-parcel action provider
/** Calls the v3 endpoint @c POST /api/v3/shipments/announce (synchronous) so the integration
    works on accounts where v2 \c POST /api/v2/parcels has been put into maintenance mode
    (per the SendCloud v2 maintenance-mode notice: existing accounts created before the
    cutoff still accept v2 writes, but new accounts must use v3).

    The data provider keeps the v2-style flat request shape (\c name, \c address, \c weight,
    \c shipment.id, etc.) for backward compatibility with callers; \c doRequestImpl()
    translates that into the v3 nested shipment body, calls the v3 sync endpoint, and
    flattens the response into a v2-like parcel hash so downstream consumers (\c get-parcel,
    \c cancel-parcel, etc.) keep working unchanged.
*/
public class SendCloudCreateParcelDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-parcel",
            "desc": "Create a new SendCloud parcel via the v3 Shipments API (synchronous announce)",
            "type": "SendCloudCreateParcelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudParcelCreateRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudParcelDataType();
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "create-parcel";
    }

    *string getDesc() {
        return "Create a new SendCloud parcel";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> r = cast<hash>(req);
        hash<auto> v3_body = buildV3ShipmentBody(r);
        hash<auto> response = cast<hash>(doV3Post("shipments/announce", v3_body));
        return flattenV3Shipment(response.data);
    }

    #! Translates the v2-style flat parcel-create request into a v3 shipment body
    private hash<auto> buildV3ShipmentBody(hash<auto> r) {
        # Recipient address (v2 flat → v3 nested)
        hash<auto> to_address = composeAddress(r, "");
        # Sender: v3 requires the full address inline (it does not auto-resolve a saved
        # sender_address_id).  Resolution order:
        #   1. Explicit from_* overrides on the request (composeAddress with "from_" prefix)
        #   2. Saved sender_address (int id) → look up via v2 /user/addresses/sender and inline
        #   3. Fallback: first sender address on the account (v2 default behavior)
        hash<auto> from_address;
        # The request type uses `from_address_1`/`from_address_2` for the street fields;
        # other from_* fields keep the bare suffix.
        if (r.from_name || r.from_address_1 || r.from_country) {
            from_address = composeFromAddress(r);
        } else if (exists r.sender_address) {
            from_address = resolveSenderAddress(int(r.sender_address));
        } else {
            from_address = resolveDefaultSenderAddress();
        }

        # Build the single-parcel inner object (v2 always created exactly one parcel per call;
        # quantity > 1 is not currently surfaced through this action).
        hash<auto> parcel = {};
        if (exists r.weight) {
            parcel.weight = {
                "value": sprintf("%s", r.weight),
                "unit": r.weight_unit ?? "kg",
            };
        }
        if (exists r.length || exists r.width || exists r.height) {
            parcel.dimensions = {
                "length": exists r.length ? sprintf("%s", r.length) : NOTHING,
                "width": exists r.width ? sprintf("%s", r.width) : NOTHING,
                "height": exists r.height ? sprintf("%s", r.height) : NOTHING,
                "unit": r.length_unit ?? "cm",
            };
        }
        if (exists r.total_insured_value) {
            parcel.additional_insured_price = {
                "value": sprintf("%s", r.total_insured_value),
                "currency": r.total_order_value_currency ?? "EUR",
            };
        }
        if (r.parcel_items.typeCode() == NT_LIST && elements r.parcel_items > 0) {
            parcel.parcel_items = map mapParcelItem($1), r.parcel_items;
        }

        # Shipping option: required by the v3 announce endpoint.  Prefer the flat
        # `shipping_method` field (the documented surface); fall back to a legacy
        # `shipment.id` hash for backward compatibility with earlier callers.
        *softint smid;
        if (exists r.shipping_method) {
            smid = r.shipping_method;
        } else if (r.shipment.typeCode() == NT_HASH && exists r.shipment.id) {
            smid = r.shipment.id;
        }
        if (!exists smid) {
            throw "SENDCLOUD-ERROR",
                "create-parcel requires a `shipping_method` ID — the v3 announce endpoint "
                "rejects requests without `ship_with`.  Use `list-shipping-methods` to find "
                "a valid ID for your account (e.g. `8` for *Unstamped letter*)";
        }
        *string code = translateShippingMethodId(smid);
        if (!code.val()) {
            throw "SENDCLOUD-ERROR",
                sprintf("could not translate v2 shipping method id %d to a v3 shipping option "
                    "code; the SendCloud compatibility endpoint returned no mapping", smid);
        }
        hash<auto> ship_with = {
            "type": "shipping_option_code",
            "properties": {"shipping_option_code": code},
        };

        # Top-level shipment body
        hash<auto> body = {
            "to_address": to_address,
            "parcels": (parcel,),
            "ship_with": ship_with,
        };
        if (from_address) {
            body.from_address = from_address;
        }
        if (exists r.order_number) {
            body.order_number = r.order_number;
        }
        if (exists r.external_reference) {
            body.external_reference_id = r.external_reference;
        }
        if (exists r.total_order_value) {
            body.total_order_price = {
                "value": sprintf("%s", r.total_order_value),
                "currency": r.total_order_value_currency ?? "EUR",
            };
        }
        if (exists r.customs_invoice_nr || exists r.customs_shipment_type) {
            hash<auto> customs = {};
            if (exists r.customs_invoice_nr) {
                customs.invoice_number = r.customs_invoice_nr;
            }
            if (exists r.customs_shipment_type) {
                customs.export_reason = mapCustomsExportReason(r.customs_shipment_type);
            }
            body.customs_information = customs;
        }
        return body;
    }

    #! Resolves a saved sender address by ID via the v2 /sender_addresses endpoint and
    #! returns it as a v3 from_address hash.  v3 does not auto-resolve sender_address_id,
    #! so we have to inline the full address ourselves.
    private hash<auto> resolveSenderAddress(int id) {
        hash<auto> resp = doGet("user/addresses/sender");
        list<auto> addrs = (resp.sender_addresses ?? ());
        foreach hash<auto> a in (addrs) {
            if (int(a.id) == id) {
                return mapSenderRecordToV3(cast<hash>(a));
            }
        }
        throw "SENDCLOUD-ERROR",
            sprintf("sender_address id %d not found in this account's sender addresses", id);
    }

    #! Returns the first sender address on the account as a v3 from_address hash.
    /** Used when the request supplies no \c sender_address and no explicit \c from_* fields:
        v2 historically picked the merchant's default sender automatically, and v3 requires
        the full address inline, so we replicate the v2 ergonomic by fetching and inlining
        the first sender address.

        @throw SENDCLOUD-ERROR if the account has no sender addresses configured
    */
    private hash<auto> resolveDefaultSenderAddress() {
        hash<auto> resp = doGet("user/addresses/sender");
        list<auto> addrs = (resp.sender_addresses ?? ());
        if (!elements addrs) {
            throw "SENDCLOUD-ERROR",
                "no sender addresses configured on this SendCloud account; v3 requires either "
                "an explicit `sender_address` id, an inline `from_*` block on the request, or "
                "at least one sender address registered in Settings → Addresses → Sender";
        }
        return mapSenderRecordToV3(cast<hash>(addrs[0]));
    }

    #! Maps a v2 sender-address record to a v3 from_address hash
    private hash<auto> mapSenderRecordToV3(hash<auto> a) {
        hash<auto> v3 = {};
        if (a.contact_name) v3.name = a.contact_name;
        if (a.company_name) v3.company_name = a.company_name;
        if (a.street) v3.address_line_1 = a.street;
        if (a.house_number) v3.house_number = a.house_number;
        if (a.postal_code) v3.postal_code = a.postal_code;
        if (a.city) v3.city = a.city;
        if (a.country) v3.country_code = a.country;
        if (a.country_state) v3.state_province_code = a.country_state;
        if (a.email) v3.email = a.email;
        if (a.telephone) v3.phone_number = a.telephone;
        return v3;
    }

    #! Composes a v3 to_address object from the (un-prefixed) recipient fields on the request
    private hash<auto> composeAddress(hash<auto> r, string p) {
        hash<auto> a = {};
        if (exists r{p + "name"}) a.name = r{p + "name"};
        if (exists r{p + "company_name"}) a.company_name = r{p + "company_name"};
        if (exists r{p + "address"}) a.address_line_1 = r{p + "address"};
        if (exists r{p + "address_2"}) a.address_line_2 = r{p + "address_2"};
        if (exists r{p + "house_number"}) a.house_number = r{p + "house_number"};
        if (exists r{p + "city"}) a.city = r{p + "city"};
        if (exists r{p + "postal_code"}) a.postal_code = r{p + "postal_code"};
        if (exists r{p + "country"}) a.country_code = r{p + "country"};
        if (exists r{p + "country_state"}) a.state_province_code = r{p + "country_state"};
        if (exists r{p + "email"}) a.email = r{p + "email"};
        if (exists r{p + "telephone"}) a.phone_number = r{p + "telephone"};
        return a;
    }

    #! Composes a v3 from_address object from the request's \c from_* override fields.
    /** The request type names the sender street fields \c from_address_1 / \c from_address_2
        (not \c from_address); other \c from_* fields use the bare suffix.
    */
    private hash<auto> composeFromAddress(hash<auto> r) {
        hash<auto> a = {};
        if (exists r.from_name) a.name = r.from_name;
        if (exists r.from_company_name) a.company_name = r.from_company_name;
        if (exists r.from_address_1) a.address_line_1 = r.from_address_1;
        if (exists r.from_address_2) a.address_line_2 = r.from_address_2;
        if (exists r.from_house_number) a.house_number = r.from_house_number;
        if (exists r.from_city) a.city = r.from_city;
        if (exists r.from_postal_code) a.postal_code = r.from_postal_code;
        if (exists r.from_country) a.country_code = r.from_country;
        if (exists r.from_country_state) a.state_province_code = r.from_country_state;
        if (exists r.from_email) a.email = r.from_email;
        if (exists r.from_telephone) a.phone_number = r.from_telephone;
        return a;
    }

    #! Translates a single v2 parcel item to its v3 form
    private hash<auto> mapParcelItem(hash<auto> i) {
        hash<auto> v3 = {};
        if (exists i.description) v3.description = i.description;
        if (exists i.quantity) v3.quantity = int(i.quantity);
        if (exists i.weight) {
            v3.weight = {
                "value": sprintf("%s", i.weight),
                "unit": i.weight_unit ?? "kg",
            };
        }
        if (exists i.value) {
            v3.price = {
                "value": sprintf("%s", i.value),
                "currency": i.currency ?? "EUR",
            };
        }
        if (exists i.hs_code) v3.hs_code = i.hs_code;
        if (exists i.origin_country) v3.origin_country = i.origin_country;
        if (exists i.sku) v3.sku = i.sku;
        if (exists i.product_id) v3.product_id = i.product_id;
        return v3;
    }

    #! Maps the v2 numeric customs_shipment_type to the v3 \c export_reason string
    /** v2 codes (per SendCloud docs): 0=Gift, 1=Documents, 2=Commercial Goods,
        3=Commercial Sample, 4=Returned Goods.
    */
    private string mapCustomsExportReason(softint t) {
        switch (t) {
            case 0: return "gift";
            case 1: return "documents";
            case 2: return "commercial_goods";
            case 3: return "commercial_sample";
            case 4: return "returned_goods";
            default: return "commercial_goods";
        }
    }

    #! Flattens a v3 shipment response to a v2-like parcel hash
    /** The response wrapper is \c {data: {id (uuid), parcels: [...], to_address, from_address,
        ship_with, ...}}.  Downstream consumers expect a v2 parcel hash with flat recipient
        fields, an integer \c id, a \c shipment hash with \c id+name, etc.  We preserve as
        much as possible while keeping the shape stable.
    */
    private hash<auto> flattenV3Shipment(*hash<auto> shipment) {
        if (!shipment) return {};
        hash<auto> p = {};
        if (shipment.parcels.typeCode() == NT_LIST && elements shipment.parcels > 0) {
            p = cast<hash>(shipment.parcels[0]);
        }

        hash<auto> out = {};
        # Parcel-level integer id (v3 parcel.id IS the same integer ID space as v2 parcel.id)
        out.id = p.id;
        out.tracking_number = p.tracking_number;
        out.tracking_url = p.tracking_url;
        out.status = p.status;
        out.weight = p.weight.value ?? p.weight;
        out.length = p.dimensions.length;
        out.width = p.dimensions.width;
        out.height = p.dimensions.height;
        out.weight_unit = p.weight.unit;
        out.length_unit = p.dimensions.unit;
        out.parcel_items = p.parcel_items;
        out.documents = p.documents;
        out.date_created = p.created_at;
        out.date_updated = p.updated_at;
        out.date_announced = p.announced_at;

        # Recipient (un-nest)
        hash<auto> to = (shipment.to_address.typeCode() == NT_HASH)
            ? cast<hash>(shipment.to_address)
            : ({});
        out.name = to.name;
        out.company_name = to.company_name;
        out.address = to.address_line_1;
        out.address_2 = to.address_line_2;
        out.house_number = to.house_number;
        out.city = to.city;
        out.postal_code = to.postal_code;
        out.country = to.country_code;
        out.country_state = to.state_province_code;
        out.email = to.email;
        out.telephone = to.phone_number;

        # Order/reference fields
        out.order_number = shipment.order_number;
        out.external_reference = shipment.external_reference_id;
        out.total_order_value = shipment.total_order_price.value;
        out.total_order_value_currency = shipment.total_order_price.currency;

        # Carrier (v3 returns a {code, name} hash at the shipment top level)
        out.carrier = shipment.carrier;

        # Surface the v3-only shipment UUID (and inline label, if any) under v3-prefixed keys
        # so callers that need them can find them; flat fields above stay v2-shaped.
        out.shipment_uuid = shipment.id;
        if (exists p.label_file) {
            out.label_file = p.label_file;
        }

        return out;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
