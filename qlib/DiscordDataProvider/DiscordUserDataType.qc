# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordUserDataType.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Discord user data type
public class DiscordUserDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "display_name": "User ID",
                "type": AbstractDataProviderTypeMap."string",
                "short_desc": "The user's ID",
                "desc": "The user's ID in snowflake format",
            },
            "username": {
                "display_name": "Username",
                "short_desc": "The user's username, not unique across the platform",
                "desc": "The user's username, **not** unique across the platform",
                "type": AbstractDataProviderTypeMap."string",
            },
            "discriminator": {
                "display_name": "Discord Tag",
                "short_desc": "The user's Discord tag",
                "desc": "The user's Discord tag",
                "type": AbstractDataProviderTypeMap."string",
            },
            "global_name": {
                "display_name": "Global Name",
                "short_desc": "The user's display name, if it is set",
                "desc": "The user's display name, if it is set. For bots, this is the application name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "avatar": {
                "display_name": "Avatar",
                "short_desc": "The user's avatar hash",
                "desc": "The user's avatar hash",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "bot": {
                "display_name": "Bot User?",
                "short_desc": "Whether the user belongs to an OAuth2 application",
                "desc": "Whether the user belongs to an OAuth2 application",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "system": {
                "display_name": "System User?",
                "short_desc": "Whether the user is an Official Discord System user (part of the urgent message system)",
                "desc": "Whether the user is an Official Discord System user (part of the urgent message system)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "mfa_enabled": {
                "display_name": "MFA Enabled?",
                "short_desc": "Whether the user has two factor enabled on their account",
                "desc": "Whether the user has two factor enabled on their account",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "banner": {
                "display_name": "User Banner Hash",
                "short_desc": "The user's banner hash",
                "desc": "The user's banner hash",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "accent_color": {
                "display_name": "Accent Color Code",
                "short_desc": "The user's banner color encoded as an integer representation of hexadecimal color "
                    "code",
                "desc": "The user's banner color encoded as an integer representation of hexadecimal color code",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "locale": {
                "display_name": "User Locale",
                "short_desc": "The user's chosen language option",
                "desc": "The user's chosen language option",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "verified": {
                "display_name": "Verified Email?",
                "short_desc": "Whether the email on this account has been verified",
                "desc": "Whether the email on this account has been verified",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "email": {
                "display_name": "EMail Address",
                "short_desc": "The user's email",
                "desc": "The user's email",
                "type": AbstractDataProviderTypeMap."*email",
            },
            "flags": {
                "display_name": "User Flags",
                "short_desc": "The flags on a user's account",
                "desc": "The flags on a user's account",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "premium_type": {
                "display_name": "Nitro Subscription Type Code",
                "short_desc": "The type of Nitro subscription on a user's account",
                "desc": "The type of Nitro subscription on a user's account",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "public_flags": {
                "display_name": "Public User Flags?",
                "short_desc": "The public flags on a user's account",
                "desc": "The public flags on a user's account",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "avatar_decoration": {
                "display_name": "Avatar Decoration Hash",
                "short_desc": "The user's avatar decoration hash",
                "desc": "The user's avatar decoration hash",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("DiscordUserInfo") {
        addQoreFields(Fields);
    }
}

#! Discord user data type constant
public const DiscordUserDataType = new DiscordUserDataType();
}
