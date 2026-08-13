# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomReplyConversationDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the IntercomDataProvider module
public namespace IntercomDataProvider {
#! The request type of @ref IntercomDataProvider::IntercomReplyConversationDataProvider "the reply action"
/** Written out here because the operation's body cannot be derived from Intercom's description; see that
    class for why.  The dropdowns are the application's own, so the option surface is the same one the
    generated actions offer.
*/
public class IntercomReplyConversationRequestDataType inherits HashDataType {
    private {
        #! The request fields
        const Fields = {
            "conversation_id": {
                "type": StringType,
                "display_name": "Conversation",
                "short_desc": "The conversation to reply to",
                "desc": "The conversation to reply to",
                "attr": {
                    "ref_data": IntercomManifest::RefConversation,
                    "supports_custom_values": True,
                    "preselected": True,
                },
            },
            "message_type": {
                "type": StringType,
                "display_name": "Message Type",
                "short_desc": "Whether the reply is seen by the contact",
                "desc": "Whether the reply is seen by the contact.\n\n"
                    "`comment` is a reply the contact sees and is notified about, `note` is an internal "
                    "remark only teammates see, and `quick_reply` offers the contact the choices in "
                    "`reply_options`.\n\n"
                    "A `note` here is attached to the **conversation**; **Create Note** attaches one to "
                    "the contact's record instead.",
                "default_value": "comment",
                "attr": {
                    "preselected": True,
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "comment",
                            "display_name": "Comment",
                            "desc": "A reply the contact sees and is notified about",
                        },
                        <AllowedValueInfo>{
                            "value": "note",
                            "display_name": "Note",
                            "desc": "An internal remark on the conversation; the contact never sees it",
                        },
                        <AllowedValueInfo>{
                            "value": "quick_reply",
                            "display_name": "Quick Reply",
                            "desc": "A reply offering the contact the choices in \"reply_options\"",
                        },
                    ),
                },
            },
            "admin_id": {
                "type": StringType,
                "display_name": "Teammate",
                "short_desc": "The teammate the reply is from",
                "desc": "The teammate the reply is attributed to",
                "attr": {
                    "ref_data": IntercomManifest::RefAdmin,
                    "supports_custom_values": True,
                    "preselected": True,
                },
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "The text of the reply, in HTML",
                "desc": "The text of the reply, in HTML.\n\n"
                    "Intercom does not require it for a `quick_reply`, which carries its content in "
                    "`reply_options` instead.",
                "attr": {"preselected": True},
            },
            "reply_options": {
                "type": ListOrNothingType,
                "display_name": "Reply Options",
                "short_desc": "The choices offered by a quick reply",
                "desc": "The choices offered by a `quick_reply`, each `{text, uuid}`.\n\n"
                    "Only meaningful when `message_type` is `quick_reply`.",
            },
            "attachment_urls": {
                "type": ListOrNothingType,
                "display_name": "Attachment URLs",
                "short_desc": "URLs of files to attach, up to five",
                "desc": "The URLs of files to attach to the reply; Intercom accepts up to five",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the reply was written",
                "desc": "When the reply was written, if it is being recorded after the fact.\n\n"
                    "Sent to Intercom as a count of seconds since the epoch; Intercom uses the current "
                    "time when this is not set.",
            },
            "skip_notifications": {
                "type": BoolOrNothingType,
                "display_name": "Skip Notifications",
                "short_desc": "Do not notify the contact about this reply",
                "desc": "Add the reply without notifying the contact about it",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Replies to a conversation as a teammate
/** This action is **hand-written rather than generated**, and it is the only one in this application that
    is.  Intercom declares the reply body as
    \c "oneOf: [contact_reply_conversation_request, admin_reply_conversation_request]", and the first of
    those is itself a \c oneOf of three more.  An action has exactly one option shape, so a union of five
    request contracts cannot be published as one: the generated action came out with the conversation ID
    and **no way to supply the reply at all**.

    The rule that decides this is that the schema must be able to describe the action the application
    means.  Here it can - \c admin_reply_conversation_request describes a teammate's reply exactly - and
    only the union around it cannot be flattened.  So the one branch is written out by hand, the action
    keeps its ID, and it is registered alongside the generated ones.

    @section intercomreply_scope What This Action Covers

    A reply **from a teammate**, which is what the application this module replaces sent and the only
    branch Intercom's description declares in full.  The contact-reply branches declare an identity and an
    attachment list and nothing else - no \c body, no \c message_type - so publishing them would mean
    guessing at a request contract rather than reproducing a documented one.  A contact reply is reachable
    through the generic \c make-api-call action.

    @section intercomreply_types Comment, Note and Quick Reply

    \c message_type is the difference between a reply the contact sees and one only teammates do:

    |!Value|!What it does
    |\c comment|a reply the contact sees, and is notified about
    |\c note|an internal remark on the conversation; the contact never sees it
    |\c quick_reply|a reply offering the contact a set of \c reply_options to choose from

    A \c note here and a **Create Note** on the contact are different things: this one is attached to the
    conversation, that one to the contact's record.
*/
public class IntercomReplyConversationDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The action ID
        /** The ID the superseded TypeScript application registered, which existing workflows call.
        */
        const ActionName = IntercomDataProvider::ReplyConversationAction;

        #! The request path template
        const RequestPath = "/conversations/%s/reply";

        #! The action display name
        const DisplayName = "Reply to Conversation";

        #! The action short description
        const ShortDesc = "Reply to a conversation as a teammate";

        #! The action description
        const Desc = "Adds a reply to an existing conversation, from a teammate.\n\n"
            "`message_type` decides who sees it: `comment` is a reply the contact sees and is notified "
            "about, `note` is an internal remark only teammates see, and `quick_reply` offers the contact "
            "a set of `reply_options` to choose from.\n\n"
            "Use **Create Conversation** to start one, and **Create Message** to write to a contact "
            "without an existing conversation.\n\n"
            "This action is hand-written rather than generated, because Intercom's own description "
            "declares this operation's body as a union of five request contracts, which cannot be "
            "published as one option shape. It covers a teammate's reply, which is the branch Intercom "
            "declares in full; a reply attributed to the contact is reachable through `make-api-call`.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ActionName,
            "type": "IntercomReplyConversationDataProvider",
            "supports_request": True,
            "supports_children": False,
        };

        #! The request type
        static IntercomReplyConversationRequestDataType RequestType();

        #! The option holding the conversation, which travels in the path rather than the body
        const PathOption = "conversation_id";

        #! The option carrying a timestamp Intercom writes as a count of seconds since the epoch
        const TimestampOption = "created_at";
    }

    private {
        #! The action set, which supplies the reference data this action's dropdowns use
        RestSchemaActionSet action_set;

        #! The REST client used for API calls
        *IntercomRestClient::IntercomRestClientIo rest;
    }

    #! Creates the object from the arguments
    /** @param action_set the application's action set, which supplies the dropdowns this action's options
        declare
        @param rest the REST client to use for API calls
    */
    constructor(RestSchemaActionSet action_set, *IntercomRestClient::IntercomRestClientIo rest) {
        self.action_set = action_set;
        self.rest = rest;
    }

    #! Returns the action ID
    string getName() {
        return ActionName;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return DisplayName;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return ShortDesc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return Desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type: the conversation the reply was added to
    private *AbstractDataProviderType getResponseTypeImpl() {
        return IntercomReplyConversationDataProvider::getConversationRecordType(action_set);
    }

    #! Returns the type of the conversation this action answers with
    /** Taken from the action set the application ships, so that this action and the generated ones cannot
        describe the same conversation differently.

        Static because the action's registration needs it too: an action registered without an
        \c output_type gives a caller no visibility into what it returns, and there is no instance to ask
        while the module is still being initialized.
    */
    static *AbstractDataProviderType getConversationRecordType(RestSchemaActionSet action_set) {
        *RestSchemaActionOperation op = action_set.getOperation("searchConversations");
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                *hash<string, AbstractDataField> fields = t.getFields();
                *AbstractDataField f = fields.conversations;
                if (f) {
                    *AbstractDataProviderType vt = f.getType();
                    if (vt && vt.getBaseTypeCode() == NT_LIST) {
                        if (*AbstractDataProviderType et = vt.getElementType()) {
                            return et;
                        }
                    }
                }
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Adds the reply and returns the conversation it was added to
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!rest) {
            throw "INTERCOM-REPLY-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        hash<auto> args = req;
        *string conversation_id = args{PathOption}.toString();
        if (!conversation_id.val()) {
            throw "INTERCOM-REPLY-ERROR", sprintf("the %y option is required; it names the conversation to "
                "reply to", PathOption);
        }
        # escaped exactly as the generated actions escape their path variables, so that an identifier
        # carrying a reserved character survives as one path segment
        string path = sprintf(RequestPath, encode_url(conversation_id, True));

        hash<auto!> body = args - PathOption;
        # "type" is what makes this a teammate's reply, and it is the reason this branch of Intercom's
        # union is the one published; it is not an option because there is nothing to choose
        body.type = "admin";
        # Intercom writes every timestamp as a count of seconds since the epoch
        if (exists body{TimestampOption}) {
            body{TimestampOption} = get_epoch_seconds(body{TimestampOption});
        }
        # an option the caller left unset is not a value to send
        body -= (map $1.key, body.pairIterator(), !exists $1.value);

        on_error rethrow $1.err, sprintf("%s (while replying to Intercom conversation %y)", $1.desc,
            conversation_id);
        hash<RestClient::RestResponseInfo> resp = rest.restPost(path, body);
        return resp.body;
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            IntercomManifest::RefConversation: True,
            IntercomManifest::RefAdmin: True,
        };
    }

    #! Populates a dropdown by delegating to the action set that declares it
    /** The dropdowns this action offers are the same ones the generated actions offer, so they are served
        by the same reference data rather than reimplemented here.
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        *hash<RestSchemaReferenceDataInfo> rd = action_set.getAllReferenceDataDecls(){type};
        if (!rd) {
            return;
        }
        return new RestSchemaActionDataProvider(action_set, action_set.getOperationEx(rd.action), rest)
            .getReferenceData(type, action_opts);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
