# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientListDataProvider class definition

/** Pop3ClientListDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Message info hashdecl for the list response
public hashdecl Pop3MessageInfo {
    #! Message number (as string)
    string id;

    #! Message size in bytes
    int size;

    #! Unique message identifier (from UIDL command, if available)
    *string uidl;
}

#! The POP3 message list parent data provider
/** This provider lists all messages on the POP3 server. It can be used in two ways:
    1. As a parent provider with child providers for each message
    2. As a request provider that returns a list of all messages with their sizes and UIDLs
*/
public class Pop3ClientListDataProvider inherits Pop3ClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "Pop3ClientListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_identical": True,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "list",
            "desc": "Lists all messages on the POP3 server with sizes and UIDLs",
        });

        #! Response type - list of message info
        const ListResponseType = new ListDataType("Pop3MessageList",
            new HashDataType(TypedHash::forName("Pop3ClientDataProvider::Pop3MessageInfo")), True);
    }

    private {
        #! Message set
        *hash<string, bool> msgs;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : Pop3ClientDataProviderBase(options) {
    }

    #! Creates the object from an POP3 connection
    constructor(Pop3Client pop3) : Pop3ClientDataProviderBase(pop3) {
    }

    #! Returns the data provider name
    string getName() {
        return "list";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Lists all messages on the POP3 server; also serves as parent for individual message providers";
    }

    #! Makes a request and returns the response
    /** @param req the request info (ignored)
        @param request_options the request options; will be processed by validateRequestOptions()

        @return a list of Pop3MessageInfo hashes with id, size, and uidl for each message
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Get message list with sizes
        *hash<auto> listInfo = pop3.list();
        if (!listInfo) {
            return ();
        }

        # Try to get UIDLs (may not be supported by all servers)
        *hash<string, string> uidls;
        try {
            uidls = pop3.uidl();
        } catch (hash<ExceptionInfo> ex) {
            # UIDL not supported, continue without it
        }

        # Build response list
        list<hash<Pop3MessageInfo>> result = ();
        foreach hash<auto> i in (listInfo.pairIterator()) {
            result += <Pop3MessageInfo>{
                "id": i.key,
                "size": i.value.size,
                "uidl": uidls{i.key},
            };
        }

        return result;
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
        return ListResponseType;
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("POP3 message %y", $1),
            "type": "Pop3ClientMessageDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        }, getChildProviderNames();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getAllMessages();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (getAllMessages(){name}) {
            return new Pop3ClientMessageDataProvider(pop3, name);
        }
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the set of all messages
    private *hash<string, bool> getAllMessages() {
        if (exists msgs) {
            return msgs;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (exists msgs) {
            return msgs;
        }

        return msgs = map {$1: True}, keys pop3.list();
    }
}
}
