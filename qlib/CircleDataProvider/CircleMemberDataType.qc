# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleMemberDataType.qc defines data types for Circle community members

/*  CircleMemberDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle community member response data type
public class CircleMemberDataType inherits HashDataType {
    public {
        #! Field definitions for a Circle community member
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "Unique identifier for the member",
                "desc": "The unique identifier for the community member",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Member name",
                "desc": "The display name of the member",
            },
            "email": {
                "type": EmailOrNothingType,
                "display_name": "Email",
                "short_desc": "Member email address",
                "desc": "The email address of the member",
            },
            "headline": {
                "type": StringOrNothingType,
                "display_name": "Headline",
                "short_desc": "Member headline",
                "desc": "The headline or tagline of the member",
            },
            "bio": {
                "type": StringOrNothingType,
                "display_name": "Bio",
                "short_desc": "Member biography",
                "desc": "The biography of the member",
            },
            "avatar_url": {
                "type": UriOrNothingType,
                "display_name": "Avatar URL",
                "short_desc": "URL of the member avatar",
                "desc": "The URL of the member's avatar image",
            },
            "posts_count": {
                "type": IntOrNothingType,
                "display_name": "Posts Count",
                "short_desc": "Number of posts",
                "desc": "The total number of posts by this member",
            },
            "comments_count": {
                "type": IntOrNothingType,
                "display_name": "Comments Count",
                "short_desc": "Number of comments",
                "desc": "The total number of comments by this member",
            },
            "community_id": {
                "type": IntOrNothingType,
                "display_name": "Community ID",
                "short_desc": "Community the member belongs to",
                "desc": "The ID of the community this member belongs to",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the member was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time the member was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle invite member request data type
public class CircleInviteMemberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for inviting a Circle member
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Email address to invite",
                "desc": "The email address of the person to invite to the community",
                "required": True,
                "example_value": "user@example.com",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Member name",
                "desc": "The name of the person being invited",
                "example_value": "John Doe",
            },
            "headline": {
                "type": StringOrNothingType,
                "display_name": "Headline",
                "short_desc": "Member headline",
                "desc": "The headline or tagline for the member's profile",
                "example_value": "Software Engineer",
            },
            "skip_invitation": {
                "type": SoftBoolOrNothingType,
                "display_name": "Skip Invitation",
                "short_desc": "Skip sending the invitation email",
                "desc": "If `True`, the member will be added without sending an invitation email",
            },
            "space_ids": {
                "type": new ListDataType("SpaceIdList", SoftIntType, True),
                "display_name": "Space IDs",
                "short_desc": "Spaces to add the member to",
                "desc": "List of space IDs to add the member to upon invitation",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle remove member request data type
public class CircleRemoveMemberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for removing a Circle member
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "ID of the member to remove",
                "desc": "The unique identifier of the community member to remove",
                "required": True,
                "ref_data": "members",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle tag member request data type
public class CircleTagMemberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for tagging a Circle member
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "ID of the member to tag",
                "desc": "The unique identifier of the community member to tag",
                "required": True,
                "ref_data": "members",
            },
            "tag": {
                "type": StringType,
                "display_name": "Tag",
                "short_desc": "Tag name",
                "desc": "The tag to apply to the member",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle untag member request data type
public class CircleUntagMemberRequestDataType inherits HashDataType {
    public {
        #! Field definitions for untagging a Circle member
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "ID of the member to untag",
                "desc": "The unique identifier of the community member to untag",
                "required": True,
                "ref_data": "members",
            },
            "tag_id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "ID of the tag to remove",
                "desc": "The ID of the tag to remove from the member",
                "required": True,
                "ref_data": "tags",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CircleDataProvider
