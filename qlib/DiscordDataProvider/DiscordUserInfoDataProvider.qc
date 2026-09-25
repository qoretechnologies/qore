# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordUserInfoGetDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DiscordDataProvider module
public namespace DiscordDataProvider {
#! Data provider API for retrieving a specific user
public class DiscordUserInfoGetDataProvider inherits DiscordDataProviderBase {
    public {
        #! Provider info
        static hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "user-info",
            "desc": "Discord API for retrieving the current user",
            "type": "DiscordUserInfoGetDataProvider",
            "constructor_options": DiscordDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = new DiscordUserDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", DiscordDataProvider::ConstructorOptions, options);
        rest = DiscordDataProvider::getAsyncRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(DiscordRestClient::DiscordRestClientIo rest) : DiscordDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord %s data provider for the current user for `%s`", ProviderInfo.name,
            rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the exception argument of an error response includes the response body
        return doRestCommand("GET", "users/@me").body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
