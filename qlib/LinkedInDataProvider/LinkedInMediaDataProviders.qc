# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInDataProvider module definition

/** LinkedInMediaDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInDataProvider module
public namespace LinkedInDataProvider {
#! The \c owner request field the media upload actions share
public const LinkedInMediaOwnerField = {
    "owner": {
        "display_name": "Owner URN",
        "short_desc": "The URN of the entity that will own the asset",
        "desc": "The URN of the entity that will own the asset.  Defaults to the authenticated "
            "member (`urn:li:person:...`); an organization URN (`urn:li:organization:...`) requires "
            "an administrator or content-admin role on that page and the `w_organization_social` "
            "scope",
        "type": AbstractDataProviderTypeMap."*string",
        "example_value": "urn:li:organization:2414183",
    },
};

#! Request type for uploading an image
public class LinkedInUploadImageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "image": {
                "display_name": "Image",
                "short_desc": "The image to upload",
                "desc": "The image to upload; LinkedIn accepts JPG, PNG and GIF with fewer than "
                    "36,152,320 pixels",
                "type": FileDataType,
                "required": True,
            },
        } + LinkedInMediaOwnerField;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInUploadImageRequestDataType = new LinkedInUploadImageRequestDataType();

#! Data provider for uploading an image and returning its URN
public class LinkedInUploadImageDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-image",
            "type": "LinkedInUploadImageDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInUploadImageRequestDataType;
        const ResponseType = LinkedInMediaAssetDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.image;
        return uploadImageAsset(LinkedInDataProviderBase::getFileContent(file),
            LinkedInDataProviderBase::getFileContentType(file, MimeTypeJpeg), req.owner);
    }
}

#! Request type for uploading a video
public class LinkedInUploadVideoRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "video": {
                "display_name": "Video",
                "short_desc": "The video to upload",
                "desc": "The video to upload; LinkedIn accepts MP4 between 75 KB and 500 MB and "
                    "between 3 seconds and 30 minutes long",
                "type": FileDataType,
                "required": True,
            },
        } + LinkedInMediaOwnerField;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInUploadVideoRequestDataType = new LinkedInUploadVideoRequestDataType();

#! Data provider for uploading a video and returning its URN
public class LinkedInUploadVideoDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-video",
            "type": "LinkedInUploadVideoDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInUploadVideoRequestDataType;
        const ResponseType = LinkedInMediaAssetDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.video;
        return uploadVideoAsset(LinkedInDataProviderBase::getFileContent(file),
            LinkedInDataProviderBase::getFileContentType(file, VideoContentType), req.owner);
    }
}

#! Data provider for retrieving the authenticated member's profile
public class LinkedInGetCurrentUserDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-current-user",
            "type": "LinkedInGetCurrentUserDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = NOTHING;
        const ResponseType = LinkedInUserInfoDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }

    #! This action takes no request data
    /** Declared explicitly: the inherited implementation throws \c UNIMPLEMENTED, which is what a
        provider that supports requests but forgot to describe them would do
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doLinkedInRequest("GET", UserInfoPath);
        # NOTE: not named "sub"; that is a Qore keyword and declaring a variable with that name is a
        # parse error
        *string subject = resp.body.sub;
        if (!subject.val()) {
            throw "LINKEDIN-USER-ERROR", sprintf("the LinkedIn userinfo endpoint %y returned no "
                "\"sub\" field; the connection needs the \"openid\" and \"profile\" scopes",
                UserInfoPath);
        }
        return {
            "urn": "urn:li:person:" + subject,
            "sub": subject,
            "name": resp.body.name,
            "given_name": resp.body.given_name,
            "family_name": resp.body.family_name,
            "email": resp.body.email,
            "email_verified": resp.body.email_verified,
            "picture": resp.body.picture,
            # LinkedIn returns the locale either as a string or as
            # {"country": "US", "language": "en"}, depending on the endpoint version
            "locale": LinkedInGetCurrentUserDataProvider::normalizeLocale(resp.body.locale),
        };
    }

    #! Returns the member's locale as a single string
    /** The userinfo endpoint returns \c locale either as \c "en-US" or as
        \c {"country": "US", "language": "en"}; both are normalized to the string form so that
        consumers do not have to handle two shapes.
    */
    static *string normalizeLocale(auto locale) {
        if (locale.typeCode() == NT_STRING) {
            return locale;
        }
        if (locale.typeCode() != NT_HASH) {
            return;
        }
        if (locale.language.val() && locale.country.val()) {
            return sprintf("%s-%s", locale.language, locale.country);
        }
        return locale.language ?? locale.country;
    }
}
}
