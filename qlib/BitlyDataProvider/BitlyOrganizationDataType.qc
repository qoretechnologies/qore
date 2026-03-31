# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyOrganizationDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly organization record type
public class BitlyOrganizationDataType inherits HashDataType {
    public {
        #! Field definitions for Bitly organizations
        const Fields = {
            "guid": {
                "type": StringType,
                "display_name": "Organization GUID",
                "short_desc": "The organization identifier",
                "desc": "The unique identifier (GUID) of the organization",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The organization name",
                "desc": "The name of the organization",
            },
            "is_active": {
                "type": BoolType,
                "display_name": "Active",
                "short_desc": "Whether the organization is active",
                "desc": "Whether the organization is currently active",
            },
            "tier": {
                "type": StringOrNothingType,
                "display_name": "Tier",
                "short_desc": "Account tier",
                "desc": "The account tier (subscription level) of the organization",
            },
            "tier_family": {
                "type": StringOrNothingType,
                "display_name": "Tier Family",
                "short_desc": "Tier category",
                "desc": "The tier family/category of the organization's subscription",
            },
            "tier_display_name": {
                "type": StringOrNothingType,
                "display_name": "Tier Display Name",
                "short_desc": "Human-readable tier name",
                "desc": "The human-readable display name of the organization's tier",
            },
            "role": {
                "type": StringOrNothingType,
                "display_name": "Role",
                "short_desc": "User's role",
                "desc": "The current user's role within the organization",
            },
            "created": {
                "type": StringOrNothingType,
                "display_name": "Created",
                "short_desc": "When the organization was created",
                "desc": "The ISO 8601 timestamp when the organization was created",
            },
            "modified": {
                "type": StringOrNothingType,
                "display_name": "Modified",
                "short_desc": "When the organization was last modified",
                "desc": "The ISO 8601 timestamp when the organization was last modified",
            },
            "bsds": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Branded Short Domains",
                "short_desc": "BSDs for this organization",
                "desc": "A list of branded short domains available in this organization",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
