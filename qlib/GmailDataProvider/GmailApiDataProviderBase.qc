# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailApiDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Base class for hand-written Gmail action providers that wrap one Gmail Discovery API method
/** The Discovery API child providers under \c drafts, \c labels, and \c messages already execute every Gmail
    method; this class exists so that an action can offer dropdowns for its arguments, which a generated
    provider cannot do.  It supplies the reference data shared by the draft and label actions:
    - \c drafts: the account's drafts, from \c users/drafts/list
    - \c labels: the account's labels, from \c users/labels/list; also available as element reference data
      for list-typed options such as the label IDs to add to or remove from a message

    @since GmailDataProvider 1.2
*/
public class GmailApiDataProviderBase inherits GoogleDataProvider::GoogleApiDataProvider {
    public {
        #! Reference data type for the account's drafts
        const ReferenceDataDrafts = "drafts";

        #! Reference data type for the account's labels
        const ReferenceDataLabels = "labels";

        #! The maximum number of drafts returned as reference data
        const MaxReferenceDrafts = 500;

        #! The page size used when listing drafts for reference data
        const ReferenceDraftPageSize = 100;
    }

    #! Creates the object from a REST connection
    /** @param rest the Gmail REST connection
        @param resource_action the Gmail Discovery API method to wrap (ex: \c "users/drafts/get")
    */
    constructor(GoogleRestClient::GoogleRestClientIo rest, string resource_action)
            : GoogleApiDataProvider(rest, GoogleDiscoveryGmailApiName, resource_action, {"userId": "me"}) {
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            ReferenceDataDrafts: True,
            ReferenceDataLabels: True,
        };
    }

    #! Returns information on supported element reference data
    /** @return a hash of supported element reference data; keys in the hash returned are supported in calls to
        @ref getElementReferenceData()
    */
    *hash<string, bool> getSupportedElementReferenceData() {
        return {
            ReferenceDataLabels: True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case ReferenceDataDrafts: return getReferenceDrafts();
            case ReferenceDataLabels: return getReferenceLabels();
        }
    }

    #! Returns element reference data of the given kind if available
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getElementReferenceDataImpl(string type, *hash<auto> action_opts) {
        if (type == ReferenceDataLabels) {
            return getReferenceLabels();
        }
    }

    #! Returns the account's drafts as reference data
    /** Gmail's draft listing carries only the draft ID and the ID and thread ID of the message it holds, so
        each draft is labelled by its ID and described by its message and thread; showing subjects instead
        would cost one further request per draft
    */
    private list<hash<AllowedValueInfo>> getReferenceDrafts() {
        list<hash<AllowedValueInfo>> rv = ();
        *string page_token;
        do {
            string uri = sprintf("gmail/v1/users/me/drafts?maxResults=%d", ReferenceDraftPageSize);
            if (page_token.val()) {
                uri += "&pageToken=" + encode_url(page_token);
            }
            hash<auto> res = rest.restGet(uri).body;
            foreach hash<auto> draft in (res.drafts) {
                rv += <AllowedValueInfo>{
                    "value": draft.id,
                    "display_name": draft.id,
                    "short_desc": sprintf("Draft of message %s in thread %s", draft.message.id,
                        draft.message.threadId),
                    "desc": sprintf("Draft `%s` of message `%s` in thread `%s`", draft.id, draft.message.id,
                        draft.message.threadId),
                };
            }
            page_token = res.nextPageToken;
        } while (page_token.val() && rv.size() < MaxReferenceDrafts);
        return rv;
    }

    #! Returns the account's labels as reference data
    private list<hash<AllowedValueInfo>> getReferenceLabels() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> res = rest.restGet("gmail/v1/users/me/labels").body;
        foreach hash<auto> label in (res.labels) {
            string kind = label.type == "system" ? "System" : "User";
            rv += <AllowedValueInfo>{
                "value": label.id,
                "display_name": label.name,
                "short_desc": sprintf("%s label %s", kind, label.id),
                "desc": sprintf("%s label `%s`", kind, label.id),
            };
        }
        return rv;
    }
}
}
