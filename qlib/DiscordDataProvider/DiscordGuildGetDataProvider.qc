# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordGuildGetDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Data provider API for retrieving a specific guild
public class DiscordGuildGetDataProvider inherits DiscordDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Discord guild API for retrieving a specific guild",
            "type": "DiscordGuildGetDataProvider",
            "constructor_options": DiscordDataProvider::ConstructorOptions + {
                "id": <DataProviderOptionInfo>{
                    "display_name": "Server ID",
                    "short_desc": "The ID of the server or guild",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The ID of the server or guild",
                },
            },
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = new DiscordGuildDataType();
    }

    private {
        #! The guild's data
        hash<auto> guild;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", DiscordDataProvider::ConstructorOptions, options);
        rest = DiscordDataProvider::getAsyncRestConnection(options);
        guild = getGuild(options.id);
    }

    #! Creates the object from a REST connection
    constructor(DiscordRestClient::DiscordRestClientIo rest, hash<auto> guild) : DiscordDataProviderBase(rest) {
        self.guild = guild;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Discord %s data provider for guild `%s` (%y) for `%s`", ProviderInfo.name, guild.id,
            guild.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return guild;
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
