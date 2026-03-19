# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageWatchDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Gmail attachment matched event
public const EVENT_ATTACHMENT_MATCHED = "ATTACHMENT-MATCHED";

#! The parent class for Gmail REST APIs
public class GmailAttachmentWatchDataProvider inherits GmailMessageWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch-attachment",
            "desc": "Gmail message attachment watch data provider",
            "type": "GmailAttachmentWatchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = GmailMessageWatchDataProviderBase::ConstructorOptions - "retrieve_attachments" + {
            "attachment_glob": <DataProviderOptionInfo>{
                "display_name": "Attachment File Pattern",
                "short_desc": "An optional filename pattern (glob) for matching attachments",
                "desc": "An optional filename pattern (glob) for matching attachments; if not set, then all "
                    "attachments are reported; if set, then only attachments matching the pattern are reported; by "
                    "default, the glob pattern is case-insensitive; to make it case-sensitive, set the `ignore_case` "
                    "option to `false`",
                "type": (AbstractDataProviderTypeMap."string",),
                "example_value": "*.csv",
            },
            "ignore_case": <DataProviderOptionInfo>{
                "display_name": "Ignore Glob Case?",
                "short_desc": "Ignore case in globs",
                "desc": "Ignore case in globs; ignored if `attachment_glob` is not set",
                "type": (AbstractDataProviderTypeMap."bool",),
                "default_value": True,
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Glob for matching attachment filenames
        string attachment_glob;

        #! Regular expression for matching attachment filenames
        string attachment_regex;

        #! Regular expression match flags
        int re_flags = RE_Unicode;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest, *hash<auto> options)
            : GmailMessageWatchDataProviderBase(rest, ConstructorOptions, options) {
        retrieve_attachments = True;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Process constructor options
    private processOptions(*hash<auto> copts) {
        GmailMessageWatchDataProviderBase::processOptions(copts);
        if (copts.attachment_glob.val()) {
            attachment_glob = copts.attachment_glob;
            if (copts.attachment_glob == "*") {
                return;
            }
            attachment_regex = copts.attachment_glob;

            # replace . -> \.
            attachment_regex =~ s/\./\\./g;
            # replace ? -> .
            attachment_regex =~ s/\?/./g;
            # replace * -> .*
            attachment_regex =~ s/\*/.*/g;
            attachment_regex = sprintf("^%s$", attachment_regex);
        }
        if (copts.ignore_case) {
            re_flags |= RE_Caseless;
        }
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_ATTACHMENT_MATCHED: <DataProviderMessageInfo>{
                "desc": "An email attachment has been matched",
                "type": GmailAttachmentEventDataType,
            },
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Raise events
    private messageReceived(hash<auto> msg) {
        int matches;
        foreach hash<auto> att in (msg.attachments) {
            if (attachment_regex && (!att.filename.val() || !regex(att.filename, attachment_regex, re_flags))) {
                LoggerWrapper::debug("Gmail: skipping attachment with filename %y; does not match glob: %y",
                    att.filename, attachment_glob);
                continue;
            }
            notifyObservers(EVENT_ATTACHMENT_MATCHED, att);
            ++matches;
        }
        LoggerWrapper::info("Gmail: message %y matched attachments: %d/%d", msg.id, matches, msg.attachments.size());
    }

    #! Returns example data for the given observable event
    /** @param event_id the event ID of the event

        @return example data for the given observable event

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.0
    */
    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> info = setup() + {"test_mode": True};
        *hash<auto> msg = pollOnceIntern(\info);
        if (msg.attachments) {
            return msg.attachments[0];
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }
}

#! Attachment event data type
public class GmailAttachmentEventDataType inherits HashDataType {
    public {
        const Fields = {
            "hdr": {
                "display_name": "Headers",
                "short_desc": "Attachment headers, if any",
                "desc": "Attachment headers, if any",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "content_type": {
                "display_name": "MIME Type",
                "short_desc": "MIME Content-Type for the attachment, if present",
                "desc": "MIME `Content-Type` for the attachment, if present",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "filename": {
                "display_name": "File Name",
                "short_desc": "The file name for the attachment, if present",
                "desc": "The file name for the attachment, if present in the `Content-Disposition` header",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "encoding": {
                "display_name": "Character Encoding",
                "short_desc": "The character encoding for the attachment, if present",
                "desc": "The character encoding for the attachment, if provided by a `charset` value in the "
                    "`Content-Type` header",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "body": {
                "display_name": "Attachment Body",
                "short_desc": "Attachment file data",
                "desc": "Attachment file data; this is a string if the mime type is known to be a text type, "
                    "otherwise this is binary data",
                "type": AbstractDataProviderTypeMap."data",
            },
        };
    }
    constructor() : HashDataType("GmailAttachmentEvent") {
        addQoreFields(Fields);
    }
}

#! Attachment event data type constant
public const GmailAttachmentEventDataType = new GmailAttachmentEventDataType();
}
