# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudCreateReturnDataProvider.qc SendCloud create-return action data provider

/*  SendCloudCreateReturnDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Create-return action provider
/** Calls the v3 endpoint @c POST /api/v3/returns/announce-synchronously so the integration
    works on accounts where v2 \c POST /api/v2/returns has been put into maintenance mode
    (the same v2-write maintenance gate that affects new accounts also covers returns —
    confirmed empirically: \c POST /api/v2/returns returns 405 on v3-cohort accounts).

    The v3 returns API is conceptually a return-shipment from the customer's address
    (\c from_address) back to the merchant (\c to_address) using a return-capable
    \c shipping_option_code.  This is a different model from v2's "return parcel X with
    refund Y" flow.  For backward compatibility, the request type still accepts the v2
    \c outgoing_parcel reference and \c products list, and \c doRequestImpl() derives the
    v3 fields:
    - \c from_address = the outgoing parcel's recipient address (or the explicit \c address
      override on the request)
    - \c to_address = the outgoing parcel's sender address (resolved via the v2
      \c /user/addresses/sender endpoint)
    - \c ship_with = the \c shipping_option_code field on the request (required); if absent,
      the data provider attempts to translate the outgoing parcel's shipping method via
      the SendCloud compatibility endpoint and uses that, but a return-capable code is
      strongly recommended
    - \c weight = explicit \c weight on the request, otherwise the outgoing parcel's
      weight, otherwise \c "1.000"
    - \c parcel_items = translated from \c products (when supplied)

    v2-only concepts that have no v3 equivalent (\c refund, \c reason, \c message) are
    accepted on the request for backward compatibility but ignored when calling v3, since
    v3 treats refunds as a merchant-side concern outside the API.
*/
public class SendCloudCreateReturnDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-return",
            "desc": "Create a new SendCloud return via the v3 Returns API (synchronous announce)",
            "type": "SendCloudCreateReturnDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudReturnCreateRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudReturnDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "create-return";
    }

    *string getDesc() {
        return "Create a new SendCloud return";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> r = cast<hash>(req);
        # Look up the outgoing parcel to derive customer + merchant addresses and weight.
        hash<auto> parcel = lookupOutgoingParcel(int(r.outgoing_parcel));
        hash<auto> v3_body = buildV3ReturnBody(r, parcel);
        hash<auto> response = cast<hash>(doV3Post("returns/announce-synchronously", v3_body));
        return flattenV3ReturnResponse(response, r, parcel);
    }

    #! Fetches the outgoing parcel via the v2 GET /parcels/{id} endpoint
    private hash<auto> lookupOutgoingParcel(int id) {
        hash<auto> resp = doGet(sprintf("parcels/%d", id));
        return resp.parcel ?? resp;
    }

    #! Builds the v3 returns request body from the v2-style request and the outgoing parcel
    private hash<auto> buildV3ReturnBody(hash<auto> r, hash<auto> parcel) {
        # from_address (customer pickup) — explicit override on the request, otherwise
        # the outgoing parcel's recipient address
        hash<auto> from_address;
        if (r.address.typeCode() == NT_HASH && (r.address.address_line_1 || r.address.address)) {
            from_address = mapAddressToV3(cast<hash>(r.address));
        } else {
            from_address = mapParcelRecipientToV3(parcel);
        }

        # to_address (merchant return destination) — resolved from the parcel's sender
        hash<auto> to_address = resolveSenderAddressV3(int(parcel.sender_address));

        # ship_with — explicit shipping_option_code OR translate parcel.shipment.id via compat
        *string code = r.shipping_option_code;
        if (!code.val()) {
            *softint smid;
            if (parcel.shipment.typeCode() == NT_HASH && exists parcel.shipment.id) {
                smid = parcel.shipment.id;
            }
            if (exists smid) {
                code = translateShippingMethodId(smid);
            }
        }
        if (!code.val()) {
            throw "SENDCLOUD-ERROR",
                "could not determine a v3 shipping_option_code for this return: please supply "
                "the `shipping_option_code` field (e.g. `dpd:return/return`) — return-capable "
                "options usually have a `:return/...` suffix";
        }

        # weight — explicit, otherwise outgoing parcel's, otherwise 1.000
        string weight_value = r.weight.val()
            ? sprintf("%s", r.weight)
            : (parcel.weight.val() ? sprintf("%s", parcel.weight) : "1.000");
        string weight_unit = parcel.weight_unit.val() ? sprintf("%s", parcel.weight_unit) : "kg";

        hash<auto> body = {
            "from_address": from_address,
            "to_address": to_address,
            "ship_with": {
                "type": "shipping_option_code",
                "shipping_option_code": code,
            },
            "weight": {"value": weight_value, "unit": weight_unit},
        };

        # parcel_items (optional — required for non-EU returns)
        if (r.products.typeCode() == NT_LIST && elements r.products > 0) {
            body.parcel_items = map mapReturnProduct($1), r.products;
        }

        # order_number / external_reference — propagate from outgoing parcel when set
        if (parcel.order_number.val()) {
            body.order_number = parcel.order_number;
        }
        if (parcel.external_reference.val()) {
            body.external_reference = parcel.external_reference;
        }
        return body;
    }

    #! Translates a v2 SendCloudAddressDataType hash to a v3 address object
    private hash<auto> mapAddressToV3(hash<auto> a) {
        hash<auto> v3 = {};
        if (a.name) v3.name = a.name;
        if (a.company_name) v3.company_name = a.company_name;
        if (a.address) v3.address_line_1 = a.address;
        if (a.address_2) v3.address_line_2 = a.address_2;
        if (a.house_number) v3.house_number = a.house_number;
        if (a.postal_code) v3.postal_code = a.postal_code;
        if (a.city) v3.city = a.city;
        if (a.country) v3.country_code = a.country;
        if (a.country_state) v3.state_province_code = a.country_state;
        if (a.email) v3.email = a.email;
        if (a.telephone) v3.phone_number = a.telephone;
        return v3;
    }

    #! Builds a v3 from_address from the outgoing parcel's flat recipient fields
    private hash<auto> mapParcelRecipientToV3(hash<auto> p) {
        hash<auto> v3 = {};
        if (p.name) v3.name = p.name;
        if (p.company_name) v3.company_name = p.company_name;
        if (p.address) v3.address_line_1 = p.address;
        if (p.address_2) v3.address_line_2 = p.address_2;
        if (p.house_number) v3.house_number = p.house_number;
        if (p.postal_code) v3.postal_code = p.postal_code;
        if (p.city) v3.city = p.city;
        # parcel.country may be either a flat ISO-2 string OR a typed country object
        if (p.country.typeCode() == NT_STRING) {
            v3.country_code = p.country;
        } else if (p.country.typeCode() == NT_HASH && p.country.iso_2) {
            v3.country_code = p.country.iso_2;
        }
        if (p.country_state) v3.state_province_code = p.country_state;
        if (p.email) v3.email = p.email;
        if (p.telephone) v3.phone_number = p.telephone;
        return v3;
    }

    #! Resolves a sender_address by ID via v2 /user/addresses/sender into a v3 address hash
    private hash<auto> resolveSenderAddressV3(int id) {
        hash<auto> resp = doGet("user/addresses/sender");
        list<auto> addrs = (resp.sender_addresses ?? ());
        foreach hash<auto> a in (addrs) {
            if (int(a.id) == id) {
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
        }
        throw "SENDCLOUD-ERROR",
            sprintf("the outgoing parcel's sender_address id %d was not found in the account's "
                "sender addresses (cannot determine the v3 return `to_address`)", id);
    }

    #! Translates a single v2 return product to a v3 parcel_item
    private hash<auto> mapReturnProduct(hash<auto> p) {
        hash<auto> v3 = {};
        if (exists p.description) v3.description = p.description;
        if (exists p.quantity) v3.quantity = int(p.quantity);
        if (exists p.value) {
            v3.price = {
                "value": sprintf("%s", p.value),
                "currency": p.currency ?? "EUR",
            };
        }
        if (exists p.sku) v3.sku = p.sku;
        if (exists p.product_id) v3.product_id = p.product_id;
        if (exists p.return_reason) v3.properties = {"return_reason": p.return_reason};
        return v3;
    }

    #! Flattens the v3 sync-create response into a v2-like return record
    /** v3 sync POST response is \c {return_id, parcel_id, multi_collo_ids}.  We expand it
        into a hash with the v2 \c id field, attach the v2-shaped \c parcel reference, echo
        request fields where appropriate, and surface the v3-only fields under v3-prefixed
        keys (\c parcel_id, \c multi_collo_ids).
    */
    private hash<auto> flattenV3ReturnResponse(hash<auto> resp, hash<auto> req, hash<auto> parcel) {
        hash<auto> out = {};
        out.id = resp.return_id;
        # v3-specific fields preserved for callers that need them
        out.parcel_id = resp.parcel_id;
        out.multi_collo_ids = resp.multi_collo_ids;
        # v2-shape compatibility
        out.status = "created";
        out.created_at = now();
        out.updated_at = now();
        out.reason = req.reason;
        out.parcel = {"id": parcel.id, "tracking_number": parcel.tracking_number};
        out.products = req.products;
        return out;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
