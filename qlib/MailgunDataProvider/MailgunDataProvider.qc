# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! The main Mailgun data provider class (root provider)
public class MailgunDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "mailgun",
            "desc": "Mailgun data provider for email sending and management",
            "type": "MailgunDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection providers
            "domains": Class::forName("MailgunDomainsDataProvider"),
            "mailing-lists": Class::forName("MailgunMailingListsDataProvider"),
            "routes": Class::forName("MailgunRoutesDataProvider"),

            # Action providers
            "send-email": Class::forName("MailgunSendEmailDataProvider"),
            "validate-email": Class::forName("MailgunValidateEmailDataProvider"),
            "add-mailing-list-member": Class::forName("MailgunAddMailingListMemberDataProvider"),
            "delete-mailing-list-member": Class::forName("MailgunDeleteMailingListMemberDataProvider"),
            "get-mailing-list-member": Class::forName("MailgunGetMailingListMemberDataProvider"),
            "list-mailing-list-members": Class::forName("MailgunListMailingListMembersDataProvider"),
            "create-route": Class::forName("MailgunCreateRouteDataProvider"),

            # Event providers
            "events": Class::forName("MailgunEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : MailgunDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) : MailgunDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "mailgun";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Mailgun data provider for email sending and management";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest, api_domain);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace MailgunDataProvider
