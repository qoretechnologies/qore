# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveFileDataProviders.qc action providers for the GoogleDriveDataProvider module

/*  GoogleDriveFileDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! Lists files and folders
public class GoogleDriveListFilesDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-files",
            "type": "GoogleDriveListFilesDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveListFilesRequestDataType;
        const ResponseType = GoogleDriveListFilesResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int page_size = req.page_size ?? DefaultPageSize;
        if (page_size > MaxPageSize) {
            page_size = MaxPageSize;
        }

        string path = sprintf("/drive/v3/files?pageSize=%d&fields=%s", page_size,
            encode_url("nextPageToken,files(" + GoogleDriveDataProviderBase::getFileFields() + ")", True));
        string q = GoogleDriveListFilesDataProvider::buildQuery(req);
        if (q.val()) {
            path += "&q=" + encode_url(q, True);
        }
        if (req.order_by.val()) {
            path += "&orderBy=" + encode_url(req.order_by, True);
        }
        if (req.page_token.val()) {
            path += "&pageToken=" + encode_url(req.page_token, True);
        }
        path += GoogleDriveListFilesDataProvider::getSharedDriveQuery(req);

        hash<auto> resp = doRestCommand("GET", path);
        list<auto> files = ();
        foreach hash<auto> f in (resp.body.files) {
            files += GoogleDriveDataProviderBase::getFileInfo(f);
        }
        return {
            "files": files,
            "next_page_token": resp.body.nextPageToken,
        };
    }

    #! Builds the shared-drive query arguments for a list request
    /** \c supportsAllDrives alone only lets an operation address shared-drive content that it was given the ID
        of; a *listing* additionally needs \c includeItemsFromAllDrives before shared-drive content appears in
        the results at all, and \c corpora to say which collection to search.

        Drive rejects \c corpora=drive without a \c driveId, so naming a shared drive selects that collection
        automatically unless the caller asked for a different one.

        @param req the request

        @return the query arguments, with a leading \c "&", or an empty string when none apply
    */
    static string getSharedDriveQuery(hash<auto> req) {
        string rv = "";
        # NOTE: "supportsAllDrives" is added centrally by
        # GoogleDriveDataProviderBase::addSharedDriveSupport(); setting it here as well would produce it twice
        if (req.include_shared_drives ?? True) {
            rv += "&includeItemsFromAllDrives=true";
        }
        if (req.drive_id.val()) {
            rv += "&driveId=" + encode_url(req.drive_id, True);
        }
        *string corpora = req.corpora;
        if (!corpora.val() && req.drive_id.val()) {
            corpora = "drive";
        }
        if (corpora.val()) {
            if (corpora == "drive" && !req.drive_id.val()) {
                throw "GOOGLE-DRIVE-ERROR", "'corpora' is set to 'drive', which searches one shared drive, but "
                    "'drive_id' names no shared drive to search; either set 'drive_id' or use 'allDrives' to "
                    "search every shared drive the user can reach";
            }
            rv += "&corpora=" + encode_url(corpora, True);
        }
        return rv;
    }

    #! Builds the Drive query expression for a list request
    /** A raw expression supplied in \c custom_query replaces the generated one entirely, so that callers can use
        the parts of the Drive query language this action does not model

        @param req the request

        @return the query expression, or an empty string when no constraints apply
    */
    static string buildQuery(hash<auto> req) {
        if (req.custom_query.val()) {
            return req.custom_query;
        }
        list<string> terms = ();
        if (req.folder_id.val()) {
            terms += sprintf("'%s' in parents", GoogleDriveListFilesDataProvider::escape(req.folder_id));
        }
        if (req.name.val()) {
            terms += sprintf("name contains '%s'", GoogleDriveListFilesDataProvider::escape(req.name));
        }
        if (req.mime_type.val()) {
            terms += sprintf("mimeType = '%s'", GoogleDriveListFilesDataProvider::escape(req.mime_type));
        }
        if (!req.include_trashed) {
            terms += "trashed = false";
        }
        return terms ? (foldl $1 + " and " + $2, terms) : "";
    }

    #! Escapes a value for inclusion in a single-quoted Drive query term
    /** Without this a name containing an apostrophe terminates the quoted string, and the query is either
        rejected or silently matches something other than what was asked for

        @param str the value to escape

        @return the escaped value
    */
    static string escape(string str) {
        str =~ s/\\/\\\\/g;
        str =~ s/'/\\'/g;
        return str;
    }
}

#! Retrieves file metadata
public class GoogleDriveGetFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-file",
            "type": "GoogleDriveGetFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveGetFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return GoogleDriveDataProviderBase::getFileInfo(getFileMetadata(req.file_id));
    }
}

#! Downloads a file's content
/** This action has no counterpart in the v8 TypeScript application, whose \c get_file action returns metadata
    only; there was no way to retrieve a file's bytes at all
*/
public class GoogleDriveDownloadFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "GoogleDriveDownloadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveDownloadFileRequestDataType;
        const ResponseType = GoogleDriveContentResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the metadata is fetched first so that a Google-native document can be reported precisely rather than
        # failing with the API's generic "only files with binary content can be downloaded"
        hash<auto> meta = GoogleDriveDataProviderBase::getFileInfo(getFileMetadata(req.file_id));
        if (meta.is_google_doc) {
            throw "GOOGLE-DRIVE-ERROR", sprintf("file %y (%y) is a Google-native document of type %y, which has "
                "no byte content of its own and cannot be downloaded; use the 'export-file' action to convert it "
                "to a concrete format instead", meta.name, req.file_id, meta.mime_type);
        }
        if (meta.is_folder) {
            throw "GOOGLE-DRIVE-ERROR", sprintf("item %y (%y) is a folder, not a file", meta.name, req.file_id);
        }

        binary content = getRawContent(sprintf("/drive/v3/files/%s?alt=media", encode_url(req.file_id, True)));
        return {
            "id": meta.id,
            "name": meta.name,
            "content": make_base64_string(content),
            "mime_type": meta.mime_type,
            "size": content.size(),
        };
    }
}

#! Exports a Google-native document to a concrete format
/** Google-native documents (Docs, Sheets, Slides) have no byte content of their own; this action converts one to
    a requested format.  Like \c download-file, it has no counterpart in the v8 TypeScript application
*/
public class GoogleDriveExportFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "export-file",
            "type": "GoogleDriveExportFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveExportFileRequestDataType;
        const ResponseType = GoogleDriveContentResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        binary content = getRawContent(sprintf("/drive/v3/files/%s/export?mimeType=%s",
            encode_url(req.file_id, True), encode_url(req.mime_type, True)));
        return {
            "id": req.file_id,
            "content": make_base64_string(content),
            "mime_type": req.mime_type,
            "size": content.size(),
        };
    }
}

#! Uploads a file
public class GoogleDriveUploadFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "GoogleDriveUploadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveUploadFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        string name = req.name.val() ? req.name : file.name;
        if (!name.val()) {
            throw "GOOGLE-DRIVE-ERROR", "neither 'name' nor a file name was supplied; the name for the new file "
                "cannot be determined";
        }

        # NOTE: "hash<auto!>", not "hash<auto>": a single-string literal narrows to hash<string, string> at
        # runtime, and "parents" is a list -- so uploading into a named folder raised RUNTIME-TYPE-ERROR
        hash<auto!> metadata = {"name": name};
        if (req.folder_id.val()) {
            metadata.parents = (req.folder_id,);
        }
        if (req.description.val()) {
            metadata.description = req.description;
        }

        hash<auto> resp = uploadMultipart("POST", "/upload/drive/v3/files", metadata,
            GoogleDriveDataProviderBase::getFileContent(file),
            GoogleDriveDataProviderBase::getMimeType(file, name));
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Creates a file from text content
public class GoogleDriveCreateTextFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-text-file",
            "type": "GoogleDriveCreateTextFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveCreateTextFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # NOTE: "hash<auto!>"; a single-string literal narrows to hash<string, string>, and "parents" is a
        # list, so creating a file inside a named folder raised RUNTIME-TYPE-ERROR
        hash<auto!> metadata = {"name": req.name};
        if (req.folder_id.val()) {
            metadata.parents = (req.folder_id,);
        }
        hash<auto> resp = uploadMultipart("POST", "/upload/drive/v3/files", metadata, binary(req.content),
            req.mime_type ?? "text/plain");
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Replaces a file's content, keeping its ID, name, and sharing settings
public class GoogleDriveReplaceFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "replace-file",
            "type": "GoogleDriveReplaceFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveReplaceFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        # PATCH on the upload endpoint replaces the content of an existing file; the metadata part carries no
        # name, so the file keeps the one it has
        hash<auto> resp = uploadMultipart("PATCH",
            "/upload/drive/v3/files/" + encode_url(req.file_id, True), {},
            GoogleDriveDataProviderBase::getFileContent(file),
            GoogleDriveDataProviderBase::getMimeType(file, file.name ?? ""));
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}
}
