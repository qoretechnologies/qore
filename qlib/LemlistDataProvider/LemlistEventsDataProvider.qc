# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Container provider for Lemlist event providers (triggers)
public class LemlistEventsDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Lemlist events data provider - watch for email and engagement events via webhooks",
            "type": "LemlistEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_EMAIL_SENT: Class::forName("LemlistEmailSentEventDataProvider"),
            EVENT_EMAIL_OPENED: Class::forName("LemlistEmailOpenedEventDataProvider"),
            EVENT_EMAIL_CLICKED: Class::forName("LemlistEmailClickedEventDataProvider"),
            EVENT_EMAIL_REPLIED: Class::forName("LemlistEmailRepliedEventDataProvider"),
            EVENT_EMAIL_BOUNCED: Class::forName("LemlistEmailBouncedEventDataProvider"),
            EVENT_LEAD_INTERESTED: Class::forName("LemlistLeadInterestedEventDataProvider"),
            EVENT_LEAD_NOT_INTERESTED: Class::forName("LemlistLeadNotInterestedEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist events data provider - watch for email and engagement events via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EVENT_EMAIL_SENT:
                return new LemlistEmailSentEventDataProvider(rest);
            case EVENT_EMAIL_OPENED:
                return new LemlistEmailOpenedEventDataProvider(rest);
            case EVENT_EMAIL_CLICKED:
                return new LemlistEmailClickedEventDataProvider(rest);
            case EVENT_EMAIL_REPLIED:
                return new LemlistEmailRepliedEventDataProvider(rest);
            case EVENT_EMAIL_BOUNCED:
                return new LemlistEmailBouncedEventDataProvider(rest);
            case EVENT_LEAD_INTERESTED:
                return new LemlistLeadInterestedEventDataProvider(rest);
            case EVENT_LEAD_NOT_INTERESTED:
                return new LemlistLeadNotInterestedEventDataProvider(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
