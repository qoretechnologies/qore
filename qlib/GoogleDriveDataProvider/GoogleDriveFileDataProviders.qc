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
        #! Search options for reading Drive as a record source
        /** These are the arguments Drive can apply itself.  Drive has a real query language, so most of the
            narrowing happens at the server; a \c where clause beyond what these express is evaluated over the
            records as they arrive.
        */
        const SearchOptions = {
            "folder_id": <DataProviderOptionInfo>{
                "display_name": "Folder",
                "short_desc": "Read only files held in this folder",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Read only files held directly in this folder; if not set, files anywhere the "
                    "connection can see are read",
                "ref_data": "folders",
                "preselected": True,
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Match files whose name contains this text",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Match files whose name contains this text",
                "preselected": True,
            },
            "mime_type": <DataProviderOptionInfo>{
                "display_name": "MIME Type",
                "short_desc": "Match only files of this MIME type",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Match only files of this MIME type; use "
                    "`application/vnd.google-apps.folder` to read folders",
            },
            "include_trashed": <DataProviderOptionInfo>{
                "display_name": "Include Trashed",
                "short_desc": "Include files that are in the trash",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include files that are in the trash; excluded by default",
                "default_value": False,
            },
            "custom_query": <DataProviderOptionInfo>{
                "display_name": "Custom Query",
                "short_desc": "A raw Drive query expression",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "A raw Drive query expression, used instead of the other matching options when set; "
                    "see the Drive API `files.list` documentation for the syntax",
                "example_value": "modifiedTime > '2026-01-01T00:00:00' and trashed = false",
            },
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "The sort order for results",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The sort order for results, as a Drive `orderBy` expression",
                "example_value": "modifiedTime desc",
            },
            "drive_id": <DataProviderOptionInfo>{
                "display_name": "Shared Drive",
                "short_desc": "Read this shared drive rather than My Drive",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Read this shared drive (formerly a \"Team Drive\") rather than the user's own `My "
                    "Drive`",
                "ref_data": "shared_drives",
            },
            "corpora": <DataProviderOptionInfo>{
                "display_name": "Collection",
                "short_desc": "The collection of files to search",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The collection of files to search; if not set, `drive` is used when `drive_id` is set",
                "allowed_values": CorporaValues,
            },
            "include_shared_drives": <DataProviderOptionInfo>{
                "display_name": "Include Shared Drives",
                "short_desc": "Include files held on shared drives",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include files held on shared drives in the results",
                "default_value": True,
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "How many files to fetch per request",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "How many files to fetch per request; Drive caps this at `1000`.\n\n"
                    "This affects how the walk is split into requests, not how many records are returned - "
                    "the iterator fetches further pages as they are needed",
                "default_value": DefaultPageSize,
            },
        };

        const ProviderInfo = <DataProviderInfo>{
            "name": "list-files",
            "type": "GoogleDriveListFilesDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            # the action and the record source are the same provider: the action answers one page with an
            # explicit page token, while the record source walks the whole result itself
            "supports_read": True,
            "has_record": True,
            "search_options": SearchOptions,
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

    #! Returns the record type for a file listing
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return GoogleDriveFileDataType.getFields();
    }

    #! Reads Drive as a record source, walking the whole result rather than one page of it
    /** @param where_cond any further conditions, evaluated over the records as they arrive
        @param search_options the search options

        @return an iterator over every matching file
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> req = search_options ?? {};
        int page_size = req.page_size ?? DefaultPageSize;
        if (page_size > MaxPageSize) {
            page_size = MaxPageSize;
        }
        string base = sprintf("/drive/v3/files?pageSize=%d&fields=%s", page_size,
            encode_url("nextPageToken,files(" + GoogleDriveDataProviderBase::getFileFields() + ")", True));
        string q = GoogleDriveListFilesDataProvider::buildQuery(req);
        if (q.val()) {
            base += "&q=" + encode_url(q, True);
        }
        if (req.order_by.val()) {
            base += "&orderBy=" + encode_url(req.order_by, True);
        }
        base += GoogleDriveListFilesDataProvider::getSharedDriveQuery(req);

        # the request plumbing is private to the provider hierarchy, so the iterator is given a closure
        code<hash<auto>(*string)> fetch = hash<auto> sub (*string page_token) {
            string path = base;
            if (page_token.val()) {
                path += "&pageToken=" + encode_url(page_token, True);
            }
            return doRestCommand("GET", path);
        };
        return new DefaultRecordIterator<hash<auto>>(new GoogleDriveFilePageIterator(fetch), where_cond,
            search_options, getRecordTypeImpl(search_options));
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

        # a large file is sent as a resumable upload rather than one request; a single-request upload has to
        # be retried from the beginning if the connection drops
        hash<auto> resp = upload("POST", "/upload/drive/v3/files", metadata,
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
        hash<auto> resp = upload("POST", "/upload/drive/v3/files", metadata, binary(req.content),
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
        hash<auto> resp = upload("PATCH",
            "/upload/drive/v3/files/" + encode_url(req.file_id, True), {},
            GoogleDriveDataProviderBase::getFileContent(file),
            GoogleDriveDataProviderBase::getMimeType(file, file.name ?? ""));
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}
}
