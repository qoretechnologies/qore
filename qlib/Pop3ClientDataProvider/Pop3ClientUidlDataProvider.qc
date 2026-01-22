# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientUidlDataProvider class definition

/** Pop3ClientUidlDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Pop3ClientDataProvider module
public namespace Pop3ClientDataProvider {
#! Data provider that returns unique message IDs using the UIDL command
/** The UIDL command returns unique identifiers for messages that persist across sessions,
    unlike message numbers which can change.

    @note This requires server support for the UIDL command (RFC 1939 optional, RFC 2449)
*/
public class Pop3ClientUidlDataProvider inherits Pop3ClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "Pop3ClientUidlDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "uidl",
            "desc": "Returns unique message identifiers (UIDL) for all messages",
        });

        #! Response type for UIDL - hash mapping message number to unique ID
        const UidlResponseType = new HashDataType(AutoHashOrNothingType, "UidlResponse");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : Pop3ClientDataProviderBase(options) {
    }

    #! Creates the object from a POP3 connection
    constructor(Pop3Client pop3) : Pop3ClientDataProviderBase(pop3) {
    }

    #! Returns the data provider name
    string getName() {
        return "uidl";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Returns unique message identifiers (UIDL) for all messages on the server";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return a hash mapping message numbers (as strings) to their unique IDs
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return pop3.uidl();
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return UidlResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
