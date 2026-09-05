# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailGetDraftDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GmailDataProvider module
public namespace GmailDataProvider {
#! Data provider for retrieving a Gmail draft
/** Wraps the Discovery API \c users/drafts/get method so that the draft can be chosen from reference data

    @since GmailDataProvider 1.2
*/
public class GmailGetDraftDataProvider inherits GmailApiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-draft",
            "desc": "Gmail retrieve draft data provider",
            "type": "GmailGetDraftDataProvider",
            "supports_request": True,
            "mutates_state": False,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Gmail draft get resource action
        const ResourceAction = "users/drafts/get";
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest) : GmailApiDataProviderBase(rest, ResourceAction) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
