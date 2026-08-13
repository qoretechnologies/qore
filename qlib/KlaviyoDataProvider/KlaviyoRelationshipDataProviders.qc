# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoRelationshipDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the KlaviyoDataProvider module
public namespace KlaviyoDataProvider {
#! Describes one of the six actions that attach or detach one object from another
/** @see @ref KlaviyoDataProvider::KlaviyoRelationshipDataProvider "KlaviyoRelationshipDataProvider"
*/
public hashdecl KlaviyoRelationshipActionInfo {
    #! The stable action ID
    string action;

    #! The HTTP method: \c POST to attach, \c DELETE to detach
    string method;

    #! The URI path template, with one \c "%s" for the owning object's ID
    string path;

    #! The action display name
    string display_name;

    #! The action short description
    string short_desc;

    #! The action description
    string desc;

    #! The action group
    string group;

    #! The option naming the object the relationship belongs to, which travels in the URI path
    string owner_option;

    #! The display name of the owner option
    string owner_display_name;

    #! The short description of the owner option
    string owner_short_desc;

    #! The reference data filling the owner option's dropdown
    string owner_ref_data;

    #! The option naming the object being attached or detached, which travels in the body
    string member_option;

    #! The display name of the member option
    string member_display_name;

    #! The short description of the member option
    string member_short_desc;

    #! The reference data filling the member option's dropdown
    string member_ref_data;

    #! The JSON:API type discriminator the member element carries
    string member_type;

    #! The order the two options are presented in
    list<string> field_order;
}

#! The six relationship actions this application writes by hand
/** Keyed by action ID, which is the ID the superseded TypeScript application registered.

    @see @ref KlaviyoDataProvider::KlaviyoRelationshipDataProvider "KlaviyoRelationshipDataProvider"
*/
public const KlaviyoRelationshipActions = {
    "add_profile_to_list": <KlaviyoRelationshipActionInfo>{
        "action": "add_profile_to_list",
        "method": "POST",
        "path": "/api/lists/%s/relationships/profiles",
        "display_name": "Add Profile to List",
        "short_desc": "Adds an existing profile to a list",
        "desc": "Adds a profile that already exists to a list.\n\n"
            "The profile is **not** subscribed by this: membership and marketing consent are separate in "
            "Klaviyo, and **Subscribe Profile** is what records consent. Adding a profile that is already "
            "a member succeeds and changes nothing.",
        "group": KlaviyoManifest::GroupLists,
        "owner_option": "list",
        "owner_display_name": "List",
        "owner_short_desc": "The list to add the profile to",
        "owner_ref_data": KlaviyoManifest::RefList,
        "member_option": "profile",
        "member_display_name": "Profile",
        "member_short_desc": "The profile to add",
        "member_ref_data": KlaviyoManifest::RefProfile,
        "member_type": "profile",
        "field_order": ("profile", "list"),
    },
    "remove_profile_from_list": <KlaviyoRelationshipActionInfo>{
        "action": "remove_profile_from_list",
        "method": "DELETE",
        "path": "/api/lists/%s/relationships/profiles",
        "display_name": "Remove Profile from List",
        "short_desc": "Removes a profile from a list",
        "desc": "Removes a profile from a list.\n\n"
            "The profile itself is not deleted, and its marketing consent is unchanged - use "
            "**Unsubscribe Profile** to withdraw consent. Removing a profile that is not a member "
            "succeeds and changes nothing.",
        "group": KlaviyoManifest::GroupLists,
        "owner_option": "list",
        "owner_display_name": "List",
        "owner_short_desc": "The list to remove the profile from",
        "owner_ref_data": KlaviyoManifest::RefList,
        "member_option": "profile",
        "member_display_name": "Profile",
        "member_short_desc": "The profile to remove",
        "member_ref_data": KlaviyoManifest::RefProfile,
        "member_type": "profile",
        "field_order": ("profile", "list"),
    },
    "add_tag_to_list": <KlaviyoRelationshipActionInfo>{
        "action": "add_tag_to_list",
        "method": "POST",
        "path": "/api/tags/%s/relationships/lists",
        "display_name": "Add Tag to List",
        "short_desc": "Adds an existing tag to a list",
        "desc": "Applies a tag that already exists to a list.\n\n"
            "Klaviyo does not create a tag on use; the tag has to exist, and **List Tags** shows which do.",
        "group": KlaviyoManifest::GroupTags,
        "owner_option": "tag",
        "owner_display_name": "Tag",
        "owner_short_desc": "The tag to apply",
        "owner_ref_data": KlaviyoManifest::RefTag,
        "member_option": "list",
        "member_display_name": "List",
        "member_short_desc": "The list to tag",
        "member_ref_data": KlaviyoManifest::RefList,
        "member_type": "list",
        "field_order": ("tag", "list"),
    },
    "remove_tag_from_list": <KlaviyoRelationshipActionInfo>{
        "action": "remove_tag_from_list",
        "method": "DELETE",
        "path": "/api/tags/%s/relationships/lists",
        "display_name": "Remove Tag from List",
        "short_desc": "Removes a tag from a list",
        "desc": "Removes a tag from a list.\n\nThe tag itself is not deleted.",
        "group": KlaviyoManifest::GroupTags,
        "owner_option": "tag",
        "owner_display_name": "Tag",
        "owner_short_desc": "The tag to remove",
        "owner_ref_data": KlaviyoManifest::RefTag,
        "member_option": "list",
        "member_display_name": "List",
        "member_short_desc": "The list to remove the tag from",
        "member_ref_data": KlaviyoManifest::RefList,
        "member_type": "list",
        "field_order": ("tag", "list"),
    },
    "add_tag_to_segment": <KlaviyoRelationshipActionInfo>{
        "action": "add_tag_to_segment",
        "method": "POST",
        "path": "/api/tags/%s/relationships/segments",
        "display_name": "Add Tag to Segment",
        "short_desc": "Adds an existing tag to a segment",
        "desc": "Applies a tag that already exists to a segment.\n\n"
            "Klaviyo does not create a tag on use; the tag has to exist, and **List Tags** shows which do.",
        "group": KlaviyoManifest::GroupTags,
        "owner_option": "tag",
        "owner_display_name": "Tag",
        "owner_short_desc": "The tag to apply",
        "owner_ref_data": KlaviyoManifest::RefTag,
        "member_option": "segment",
        "member_display_name": "Segment",
        "member_short_desc": "The segment to tag",
        "member_ref_data": KlaviyoManifest::RefSegment,
        "member_type": "segment",
        "field_order": ("tag", "segment"),
    },
    "remove_tag_from_segment": <KlaviyoRelationshipActionInfo>{
        "action": "remove_tag_from_segment",
        "method": "DELETE",
        "path": "/api/tags/%s/relationships/segments",
        "display_name": "Remove Tag from Segment",
        "short_desc": "Removes a tag from a segment",
        "desc": "Removes a tag from a segment.\n\nThe tag itself is not deleted.",
        "group": KlaviyoManifest::GroupTags,
        "owner_option": "tag",
        "owner_display_name": "Tag",
        "owner_short_desc": "The tag to remove",
        "owner_ref_data": KlaviyoManifest::RefTag,
        "member_option": "segment",
        "member_display_name": "Segment",
        "member_short_desc": "The segment to remove the tag from",
        "member_ref_data": KlaviyoManifest::RefSegment,
        "member_type": "segment",
        "field_order": ("tag", "segment"),
    },
};

#! Base class for the hand-written Klaviyo actions
/** Carries what they all need: the REST client, the action set that owns their dropdowns, and the request
    hygiene an action generated from the schema would have got for free - an unset option is not a value to
    send, and a path variable is URL-escaped so that an identifier carrying a reserved character stays one
    path segment.
*/
public class KlaviyoHandWrittenDataProviderBase inherits DataProvider::AbstractDataProvider {
    private {
        #! The action set, which supplies the reference data this action's dropdowns use
        RestSchemaActionSet action_set;

        #! The REST client used for API calls
        *KlaviyoRestClient::KlaviyoRestClientIo rest;
    }

    #! Creates the object from the arguments
    /** @param action_set the application's action set, which supplies the dropdowns this action's options
        declare
        @param rest the REST client to use for API calls
    */
    constructor(RestSchemaActionSet action_set, *KlaviyoRestClient::KlaviyoRestClientIo rest) {
        self.action_set = action_set;
        self.rest = rest;
    }

    #! Returns the REST client, or throws if this provider was created without one
    private KlaviyoRestClient::KlaviyoRestClientIo getRestEx() {
        if (!rest) {
            throw "KLAVIYO-ACTION-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        return rest;
    }

    #! Returns a required option's value
    private string getRequiredOption(*hash<auto> req, string name, string what) {
        *string rv = req{name}.toString();
        if (!rv.val()) {
            throw "KLAVIYO-ACTION-ERROR", sprintf("the %y option is required; it names %s", name, what);
        }
        return rv;
    }

    #! Populates a dropdown by delegating to the action set that declares it
    /** The dropdowns these actions offer are the same ones the generated actions offer, so they are served
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
}

#! Attaches one object to another, or detaches it
/** These six actions are **hand-written rather than generated**, and they are all hand-written for the
    same reason: Klaviyo declares the body of a relationship endpoint as a JSON:API relationship **array**,

    @code{.py}
{"data": [{"type": "profile", "id": "01H…"}]}
    @endcode

    and an array has no properties to lift.
    @ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap" walks object levels, so it
    cannot reach inside one; a generated action would publish the whole array as a single \c body option,
    which is neither what the superseded application offered nor a usable form.

    The application being replaced published one identifier - a profile, a list, a segment - and built the
    single-element array itself, and so do these.  Klaviyo accepts several elements in one call; that is
    reachable through \c make-api-call, and adding it here would change the option's type.

    @section klaviyorelationship_semantics Membership Is Not Consent

    Adding a profile to a list makes it a **member**.  It does not record marketing **consent**, which is
    what **Subscribe Profile** does, and removing a profile from a list does not withdraw consent either.
    The two are deliberately separate in Klaviyo: a profile can be on a list and unsubscribed, and a
    subscribed profile can be removed from the list it subscribed through.
*/
public class KlaviyoRelationshipDataProvider inherits KlaviyoHandWrittenDataProviderBase {
    private {
        #! What this action attaches to what
        hash<KlaviyoRelationshipActionInfo> info;

        #! The request type
        HashDataType request_type;
    }

    #! Creates the object from the arguments
    /** @param info the action this provider serves
        @param action_set the application's action set
        @param rest the REST client to use for API calls
    */
    constructor(hash<KlaviyoRelationshipActionInfo> info, RestSchemaActionSet action_set,
            *KlaviyoRestClient::KlaviyoRestClientIo rest)
            : KlaviyoHandWrittenDataProviderBase(action_set, rest) {
        self.info = info;
        request_type = KlaviyoRelationshipDataProvider::getRequestType(info);
    }

    #! Returns the request type for the given action
    /** @param info the action

        @return the request type, whose two options are the ones the superseded application published
    */
    static HashDataType getRequestType(hash<KlaviyoRelationshipActionInfo> info) {
        HashDataType rv(AutoHashType, info.action + ": request");
        hash<auto> fields = {};
        foreach string name in (info.field_order) {
            bool owner = name == info.owner_option;
            fields{name} = {
                "name": name,
                "display_name": owner ? info.owner_display_name : info.member_display_name,
                "short_desc": owner ? info.owner_short_desc : info.member_short_desc,
                "desc": owner ? info.owner_short_desc : info.member_short_desc,
                "type": StringType,
                "attr": {
                    "required": True,
                    "preselected": True,
                    "ref_data": owner ? info.owner_ref_data : info.member_ref_data,
                    "supports_custom_values": True,
                },
            };
        }
        rv.addQoreFields(fields);
        return rv;
    }

    #! Returns the action ID
    string getName() {
        return info.action;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return info.display_name;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return info.short_desc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return info.desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return request_type;
    }

    #! Makes the relationship change and returns nothing
    /** Klaviyo answers \c "204 No Content" for both directions, so there is no output contract to publish.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        KlaviyoRestClient::KlaviyoRestClientIo client = getRestEx();
        hash<auto> args = req;
        string owner = getRequiredOption(args, info.owner_option, info.owner_short_desc);
        string member = getRequiredOption(args, info.member_option, info.member_short_desc);

        # escaped exactly as the generated actions escape their path variables, so that an identifier
        # carrying a reserved character survives as one path segment
        string path = sprintf(info.path, encode_url(owner, True));
        # the "type" discriminator is what makes the element a profile rather than a list; it is not an
        # option because there is nothing to choose
        hash<auto> body = {
            "data": (
                {
                    "type": info.member_type,
                    "id": member,
                },
            ),
        };

        on_error rethrow $1.err, sprintf("%s (while running Klaviyo action %y for %s %y and %s %y)",
            $1.desc, info.action, info.owner_option, owner, info.member_option, member);
        switch (info.method) {
            case "POST":
                client.restPost(path, body);
                break;
            case "DELETE":
                client.restDel(path, body);
                break;
            default:
                throw "KLAVIYO-ACTION-ERROR", sprintf("action %y declares method %y, which is not one this "
                    "provider makes", info.action, info.method);
        }
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            info.owner_ref_data: True,
            info.member_ref_data: True,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": info.action,
            "type": "KlaviyoRelationshipDataProvider",
            "supports_request": True,
            "supports_children": False,
        };
    }
}
}
