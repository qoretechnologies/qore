# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DiscordDataProvider module definition

/** DiscordMessageSend.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Outbound Discord message fields and wire-format validation for channel and webhook sends
public class DiscordMessageSend {
    public {
        #! Mention policy. An empty parse list disables category-wide mentions.
        const AllowedMentions = new HashDataType(AutoHashOrNothingType, "DiscordAllowedMentions").addQoreFields({
            "parse": {"type": new ListDataType(StringType, True), "desc": "Mention categories to parse (everyone, users, roles). An empty list permits none."},
            "users": {"type": new ListDataType(StringType, True), "desc": "User IDs explicitly allowed to receive mentions."},
            "roles": {"type": new ListDataType(StringType, True), "desc": "Role IDs explicitly allowed to receive mentions."},
            "replied_user": {"type": BoolOrNothingType, "desc": "Whether to mention the author of the message being replied to."},
        });

        #! A name and value displayed in an embed
        const EmbedField = new HashDataType(AutoHashType, "DiscordEmbedField").addQoreFields({
            "name": {"type": StringType, "desc": "Field name (up to 256 characters)."},
            "value": {"type": StringType, "desc": "Field value (up to 1024 characters)."},
            "inline": {"type": BoolOrNothingType, "desc": "Display beside adjacent inline fields."},
        });

        #! Footer of an outbound embed
        const EmbedFooter = new HashDataType(AutoHashOrNothingType, "DiscordEmbedFooter").addQoreFields({
            "text": {"type": StringType, "desc": "Footer text (up to 2048 characters)."},
            "icon_url": {"type": StringOrNothingType, "desc": "URL of the footer icon."},
        });

        #! Author of an outbound embed
        const EmbedAuthor = new HashDataType(AutoHashOrNothingType, "DiscordEmbedAuthor").addQoreFields({
            "name": {"type": StringType, "desc": "Author name (up to 256 characters)."},
            "url": {"type": StringOrNothingType, "desc": "Author link."},
            "icon_url": {"type": StringOrNothingType, "desc": "URL of the author icon."},
        });

        #! Image or thumbnail of an outbound embed
        const EmbedImage = new HashDataType(AutoHashOrNothingType, "DiscordEmbedImage").addQoreFields({
            "url": {"type": StringType, "desc": "Image URL."},
        });

        #! A rich embed in an outbound message
        const Embed = new HashDataType(AutoHashType, "DiscordMessageEmbed").addQoreFields({
            "title": {"type": StringOrNothingType, "desc": "Title (up to 256 characters)."},
            "description": {"type": StringOrNothingType, "desc": "Description (up to 4096 characters)."},
            "url": {"type": StringOrNothingType, "desc": "Title link."},
            "color": {"type": IntOrNothingType, "desc": "RGB color as an integer."},
            "timestamp": {"type": StringOrNothingType, "desc": "ISO 8601 timestamp."},
            "fields": {"type": new ListDataType(EmbedField, True), "desc": "Up to 25 fields."},
            "author": {"type": EmbedAuthor, "desc": "Embed author."},
            "footer": {"type": EmbedFooter, "desc": "Embed footer."},
            "thumbnail": {"type": EmbedImage, "desc": "Thumbnail image."},
            "image": {"type": EmbedImage, "desc": "Main image."},
        });

        #! Up to ten rich embeds with a combined text budget of 6000 characters
        const Embeds = new ListDataType("DiscordMessageEmbeds", Embed, True);
    }

    #! Validates a message and converts the action's boolean flags to Discord's message flags
    /** An omitted mention policy is left out. A supplied empty parse list is sent unchanged.
        Webhooks require content, embeds, files, components or a poll. Channels additionally accept stickers.
        @throw DISCORD-MESSAGE-ERROR the message exceeds a limit or contains an incompatible mention policy
    */
    static hash<auto> prepare(hash<auto> message, bool webhook = False) {
        if (exists message.content && (message.content.typeCode() != NT_STRING || message.content.size() > 2000)) {
            throw "DISCORD-MESSAGE-ERROR", "Message content must be a string of at most 2000 characters";
        }
        if (exists message.allowed_mentions) {
            checkMentions(message.allowed_mentions);
        }
        if (exists message.embeds) {
            if (message.embeds.typeCode() != NT_LIST || message.embeds.size() > 10) {
                throw "DISCORD-MESSAGE-ERROR", "Expected a list of at most ten embeds";
            }
            int total = 0;
            foreach auto embed in (message.embeds) {
                if (embed.typeCode() != NT_HASH) {
                    throw "DISCORD-MESSAGE-ERROR", "Expected a rich embed object";
                }
                total += checkEmbed(embed);
            }
            if (total > 6000) {
                throw "DISCORD-MESSAGE-ERROR", "Combined embed text exceeds 6000 characters";
            }
        }
        # Discord's IS_COMPONENTS_V2 flag reserves the message body for components.
        if ((message.flags & (1 << 15)) && (message.content || message.embeds || message.poll || message.sticker_ids)) {
            throw "DISCORD-MESSAGE-ERROR", "Components v2 cannot be combined with text, embeds, polls or stickers";
        }
        if (webhook && message.sticker_ids) {
            throw "DISCORD-MESSAGE-ERROR", "Webhooks cannot send stickers";
        }
        if (!message.content.size() && !message.embeds && !message.sticker_ids && !message.components
                && !message.attachments && !message.poll && (webhook || message.message_reference.type != 1)) {
            throw "DISCORD-MESSAGE-ERROR", "The message requires content or another sendable message element";
        }
        if (remove message.suppress_notifications) {
            message.flags |= DMT_SUPPRESS_NOTIFICATIONS;
        }
        if (remove message.supress_embeds) {
            message.flags |= DMT_SUPPRESS_EMBEDS;
        }
        return message;
    }

    private static checkMentions(auto policy) {
        if (policy.typeCode() != NT_HASH || (exists policy.parse && policy.parse.typeCode() != NT_LIST)) {
            throw "DISCORD-MESSAGE-ERROR", "Invalid allowed mention policy";
        }
        hash<auto> parsed;
        foreach auto category in (policy.parse) {
            if (category.typeCode() != NT_STRING || category !~ /^(everyone|users|roles)\z/ || parsed{category}) {
                throw "DISCORD-MESSAGE-ERROR", "Invalid or repeated mention category";
            }
            parsed{category} = True;
        }
        foreach string key in (("users", "roles")) {
            if (!exists policy{key}) {
                continue;
            }
            auto ids = policy{key};
            if (ids.typeCode() != NT_LIST || ids.size() > 100 || (ids && parsed{key})) {
                throw "DISCORD-MESSAGE-ERROR", "Invalid or conflicting mention ID list";
            }
            foreach auto id in (ids) {
                if (id.typeCode() != NT_STRING || id !~ /^[1-9][0-9]{0,19}\z/
                        || (id.size() == 20 && id > "18446744073709551615")) {
                    throw "DISCORD-MESSAGE-ERROR", "Expected a Discord ID in the mention list";
                }
            }
        }
        if (exists policy.replied_user && policy.replied_user.typeCode() != NT_BOOLEAN) {
            throw "DISCORD-MESSAGE-ERROR", "Expected a boolean reply mention flag";
        }
    }

    private static int checkText(auto text, int limit, bool required = False) {
        if (!exists text && !required) {
            return 0;
        }
        # Discord trims embed text before measuring it. Preserve the original text on the wire.
        int length = text.typeCode() == NT_STRING ? trim(text).size() : -1;
        if (length < 0 || length > limit || (required && !length)) {
            throw "DISCORD-MESSAGE-ERROR", sprintf("Expected a text field of at most %d characters", limit);
        }
        return length;
    }

    private static checkLink(auto url, bool attachment = False) {
        if (exists url && (url.typeCode() != NT_STRING || (url !~ m{^https?://[^/?#\s]+(?:[/?#][^\s]*)?\z}i
                && !(attachment && url =~ m{^attachment://[^/\s]+\z}i)))) {
            throw "DISCORD-MESSAGE-ERROR", "Invalid embed URL";
        }
    }

    private static int checkEmbed(hash<auto> embed) {
        int total = checkText(embed.title, 256) + checkText(embed.description, 4096);
        if (exists embed.color && (embed.color.typeCode() != NT_INT || embed.color < 0 || embed.color > 0xffffff)) {
            throw "DISCORD-MESSAGE-ERROR", "Embed color must be a 24-bit RGB value";
        }
        checkLink(embed.url);
        foreach string key in (("image", "thumbnail")) {
            if (exists embed{key}) {
                if (!exists embed{key}.url) {
                    throw "DISCORD-MESSAGE-ERROR", "An embed image requires a URL";
                }
                checkLink(embed{key}.url, True);
            }
        }
        if (exists embed.footer) {
            checkLink(embed.footer.icon_url, True);
            total += checkText(embed.footer.text, 2048, True);
        }
        if (exists embed.author) {
            checkLink(embed.author.url);
            checkLink(embed.author.icon_url, True);
            total += checkText(embed.author.name, 256, True);
        }
        if (exists embed.fields) {
            if (embed.fields.typeCode() != NT_LIST || embed.fields.size() > 25) {
                throw "DISCORD-MESSAGE-ERROR", "Expected a list of at most 25 embed fields";
            }
            foreach auto field in (embed.fields) {
                total += checkText(field.name, 256, True) + checkText(field.value, 1024, True);
            }
        }
        return total;
    }
}
}
