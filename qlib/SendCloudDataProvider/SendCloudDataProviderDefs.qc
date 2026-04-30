# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudDataProviderDefs.qc defines constants for the SendCloudDataProvider module

/*  SendCloudDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "SendCloud";

    #! Default page size for list operations
    public const DefaultPageSize = 100;

    #! Maximum page size (SendCloud limit)
    public const MaxPageSize = 100;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of seen IDs tracked for deduplication
    public const MaxSeenIds = 10000;

    #! Service Points base URL (cross-host)
    public const ServicePointsBaseUrl = "https://servicepoints.sendcloud.sc/api/v2";

    # --- Event name constants ---

    #! Event name for parcel status changed
    public const EVENT_PARCEL_STATUS_CHANGED = "parcel-status-changed";

    #! Event name for return updated
    public const EVENT_RETURN_UPDATED = "return-updated";

    #! ISO 3166-1 alpha-2 country codes (static reference data for country dropdowns)
    public const CountryReferenceData = (
        <AllowedValueInfo>{"value": "AT", "display_name": "Austria", "desc": "Austria (AT)"},
        <AllowedValueInfo>{"value": "AU", "display_name": "Australia", "desc": "Australia (AU)"},
        <AllowedValueInfo>{"value": "BE", "display_name": "Belgium", "desc": "Belgium (BE)"},
        <AllowedValueInfo>{"value": "BG", "display_name": "Bulgaria", "desc": "Bulgaria (BG)"},
        <AllowedValueInfo>{"value": "BR", "display_name": "Brazil", "desc": "Brazil (BR)"},
        <AllowedValueInfo>{"value": "CA", "display_name": "Canada", "desc": "Canada (CA)"},
        <AllowedValueInfo>{"value": "CH", "display_name": "Switzerland", "desc": "Switzerland (CH)"},
        <AllowedValueInfo>{"value": "CN", "display_name": "China", "desc": "China (CN)"},
        <AllowedValueInfo>{"value": "CY", "display_name": "Cyprus", "desc": "Cyprus (CY)"},
        <AllowedValueInfo>{"value": "CZ", "display_name": "Czechia", "desc": "Czechia (CZ)"},
        <AllowedValueInfo>{"value": "DE", "display_name": "Germany", "desc": "Germany (DE)"},
        <AllowedValueInfo>{"value": "DK", "display_name": "Denmark", "desc": "Denmark (DK)"},
        <AllowedValueInfo>{"value": "EE", "display_name": "Estonia", "desc": "Estonia (EE)"},
        <AllowedValueInfo>{"value": "ES", "display_name": "Spain", "desc": "Spain (ES)"},
        <AllowedValueInfo>{"value": "FI", "display_name": "Finland", "desc": "Finland (FI)"},
        <AllowedValueInfo>{"value": "FR", "display_name": "France", "desc": "France (FR)"},
        <AllowedValueInfo>{"value": "GB", "display_name": "United Kingdom", "desc": "United Kingdom (GB)"},
        <AllowedValueInfo>{"value": "GR", "display_name": "Greece", "desc": "Greece (GR)"},
        <AllowedValueInfo>{"value": "HK", "display_name": "Hong Kong", "desc": "Hong Kong (HK)"},
        <AllowedValueInfo>{"value": "HR", "display_name": "Croatia", "desc": "Croatia (HR)"},
        <AllowedValueInfo>{"value": "HU", "display_name": "Hungary", "desc": "Hungary (HU)"},
        <AllowedValueInfo>{"value": "IE", "display_name": "Ireland", "desc": "Ireland (IE)"},
        <AllowedValueInfo>{"value": "IL", "display_name": "Israel", "desc": "Israel (IL)"},
        <AllowedValueInfo>{"value": "IN", "display_name": "India", "desc": "India (IN)"},
        <AllowedValueInfo>{"value": "IS", "display_name": "Iceland", "desc": "Iceland (IS)"},
        <AllowedValueInfo>{"value": "IT", "display_name": "Italy", "desc": "Italy (IT)"},
        <AllowedValueInfo>{"value": "JP", "display_name": "Japan", "desc": "Japan (JP)"},
        <AllowedValueInfo>{"value": "LT", "display_name": "Lithuania", "desc": "Lithuania (LT)"},
        <AllowedValueInfo>{"value": "LU", "display_name": "Luxembourg", "desc": "Luxembourg (LU)"},
        <AllowedValueInfo>{"value": "LV", "display_name": "Latvia", "desc": "Latvia (LV)"},
        <AllowedValueInfo>{"value": "MT", "display_name": "Malta", "desc": "Malta (MT)"},
        <AllowedValueInfo>{"value": "MX", "display_name": "Mexico", "desc": "Mexico (MX)"},
        <AllowedValueInfo>{"value": "NL", "display_name": "Netherlands", "desc": "Netherlands (NL)"},
        <AllowedValueInfo>{"value": "NO", "display_name": "Norway", "desc": "Norway (NO)"},
        <AllowedValueInfo>{"value": "NZ", "display_name": "New Zealand", "desc": "New Zealand (NZ)"},
        <AllowedValueInfo>{"value": "PL", "display_name": "Poland", "desc": "Poland (PL)"},
        <AllowedValueInfo>{"value": "PT", "display_name": "Portugal", "desc": "Portugal (PT)"},
        <AllowedValueInfo>{"value": "RO", "display_name": "Romania", "desc": "Romania (RO)"},
        <AllowedValueInfo>{"value": "SE", "display_name": "Sweden", "desc": "Sweden (SE)"},
        <AllowedValueInfo>{"value": "SG", "display_name": "Singapore", "desc": "Singapore (SG)"},
        <AllowedValueInfo>{"value": "SI", "display_name": "Slovenia", "desc": "Slovenia (SI)"},
        <AllowedValueInfo>{"value": "SK", "display_name": "Slovakia", "desc": "Slovakia (SK)"},
        <AllowedValueInfo>{"value": "TR", "display_name": "Turkey", "desc": "Turkey (TR)"},
        <AllowedValueInfo>{"value": "US", "display_name": "United States", "desc": "United States (US)"},
        <AllowedValueInfo>{"value": "ZA", "display_name": "South Africa", "desc": "South Africa (ZA)"},
    );

    #! SendCloud logo in SVG format (loaded from file at init time)
    public const SendCloudLogo = File::readTextFile(get_script_dir() + "/sendcloud-logo.svg");
}
