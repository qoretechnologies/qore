# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxFileDataProviders.qc file action providers for the DropboxDataProvider module

/*  DropboxFileDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Lists the contents of a folder
public class DropboxListFolderDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-folder",
            "type": "DropboxListFolderDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            # the action and the record source are the same provider: the action answers one page with an
            # explicit cursor, while the record source walks the whole folder itself
            "supports_read": True,
            "has_record": True,
            "search_options": SearchOptions,
        };

        #! Search options for reading a folder as a record source
        const SearchOptions = {
            "path": <DataProviderOptionInfo>{
                "display_name": "Folder",
                "short_desc": "The folder to read",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The folder to read; the empty string reads the account root",
                "example_value": "/incoming",
                "ref_data": "folders",
                "preselected": True,
            },
            "recursive": <DataProviderOptionInfo>{
                "display_name": "Recursive",
                "short_desc": "Include the contents of subfolders",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Include the contents of subfolders as well as of the folder itself",
                "default_value": False,
                "preselected": True,
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "How many entries to fetch per request",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "How many entries to fetch per request; Dropbox treats this as a hint.\n\n"
                    "This affects how the walk is split into requests, not how many records are returned - "
                    "the iterator fetches further pages as they are needed",
                "default_value": DefaultPageSize,
            },
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxListFolderRequestDataType;
        const ResponseType = DropboxListFolderResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the record type for a folder listing
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return DropboxFileDataType.getFields();
    }

    #! Reads a folder as a record source, walking the whole listing rather than one page of it
    /** @param where_cond any further conditions, evaluated over the records as they arrive
        @param search_options the search options

        @return an iterator over every entry in the folder
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> req = search_options ?? {};
        int limit = req.page_size ?? DefaultPageSize;
        if (limit > MaxPageSize) {
            limit = MaxPageSize;
        }
        hash<auto> arg = {
            "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
            "recursive": req.recursive ?? False,
            "limit": limit,
        };
        # the request plumbing is private to the provider hierarchy, so the iterator is given closures
        code<hash<auto>()> first = hash<auto> sub () {
            return doRpc("/2/files/list_folder", arg);
        };
        code<hash<auto>(string)> following = hash<auto> sub (string cursor) {
            return doRpc("/2/files/list_folder/continue", {"cursor": cursor});
        };
        # a "deleted" entry describes something that is no longer there, so it is not a record of the folder
        code<*hash<auto>(hash<auto>)> convert = *hash<auto> sub (hash<auto> e) {
            if (e.".tag" == "deleted") {
                return;
            }
            return DropboxDataProviderBase::getFileInfo(e);
        };
        return new DefaultRecordIterator<hash<auto>>(
            new DropboxEntryPageIterator(first, following, convert), where_cond, search_options,
            getRecordTypeImpl(search_options));
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body;
        if (req.cursor.val()) {
            # a cursor already carries the folder, the recursion flag, and the page size; Dropbox rejects a
            # continuation that repeats them
            body = doRpc("/2/files/list_folder/continue", {"cursor": req.cursor});
        } else {
            int limit = req.limit ?? DefaultPageSize;
            if (limit > MaxPageSize) {
                limit = MaxPageSize;
            }
            body = doRpc("/2/files/list_folder", {
                "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
                "recursive": req.recursive ?? False,
                "limit": limit,
            });
        }
        return {
            "entries": (map DropboxDataProviderBase::getFileInfo($1), body.entries),
            "cursor": body.cursor,
            "has_more": body.has_more ?? False,
        };
    }
}

#! Searches for files and folders
public class DropboxSearchDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "type": "DropboxSearchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            # a search is a find rather than an API call, so it is a record source as well as an action
            "supports_read": True,
            "has_record": True,
            "search_options": SearchOptions,
        };

        #! Search options for a search read as a record source
        const SearchOptions = {
            "query": <DataProviderOptionInfo>{
                "display_name": "Query",
                "short_desc": "What to search for",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "What to search for; matched against file names and, for supported types, content",
                "required": True,
                "example_value": "invoice",
                "preselected": True,
            },
            "path": <DataProviderOptionInfo>{
                "display_name": "Folder",
                "short_desc": "Search only within this folder",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Search only within this folder; the empty string searches the whole account",
                "ref_data": "folders",
                "preselected": True,
            },
            "file_status": <DataProviderOptionInfo>{
                "display_name": "File Status",
                "short_desc": "Which files to search",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Which files to search",
                "allowed_values": SearchFileStatusValues,
                "default_value": "active",
            },
            "filename_only": <DataProviderOptionInfo>{
                "display_name": "Filename Only",
                "short_desc": "Match names only, not content",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Match file names only rather than searching content as well",
                "default_value": False,
            },
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "The sort order for results",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The sort order for results",
                "allowed_values": SearchOrderValues,
            },
            "file_extensions": <DataProviderOptionInfo>{
                "display_name": "File Extensions",
                "short_desc": "Match only these file extensions",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Match only files with these extensions, comma-separated and without a leading `.`",
                "example_value": "pdf,docx",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "How many matches to fetch per request",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "How many matches to fetch per request.\n\n"
                    "This affects how the walk is split into requests, not how many records are returned",
                "default_value": DefaultPageSize,
            },
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxSearchRequestDataType;
        const ResponseType = DropboxSearchResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the record type for a search
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return DropboxFileDataType.getFields();
    }

    #! Reads a search as a record source, walking every page of matches
    /** @param where_cond any further conditions, evaluated over the records as they arrive
        @param search_options the search options

        @return an iterator over every match

        @throw SEARCH-OPTION-ERROR no query was given
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> req = search_options ?? {};
        if (!req.query.val()) {
            throw "SEARCH-OPTION-ERROR", "cannot search: no query was given; set the 'query' search option";
        }
        hash<auto!> opts = {
            "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
            "max_results": req.page_size ?? DefaultPageSize,
            "file_status": req.file_status ?? "active",
            "filename_only": req.filename_only ?? False,
        };
        if (req.order_by.val()) {
            opts.order_by = {".tag": req.order_by};
        }
        if (req.file_extensions.val()) {
            opts.file_extensions = map trim($1), req.file_extensions.split(","), trim($1).val();
        }

        code<hash<auto>()> first = hash<auto> sub () {
            return doRpc("/2/files/search_v2", {"query": req.query, "options": opts});
        };
        code<hash<auto>(string)> following = hash<auto> sub (string cursor) {
            # NOTE: "/2/files/search/continue_v2", not "/2/files/search_v2/continue".  Dropbox names the
            # continuation of "search_v2" inconsistently with the endpoint it continues; the more obvious
            # spelling is a 404 that arrives as an HTML error page rather than as an API error
            return doRpc("/2/files/search/continue_v2", {"cursor": cursor});
        };
        # a match wraps the entry twice: once as the match's metadata union, once as the entry itself; a
        # match carrying neither is dropped rather than turned into an empty record
        code<*hash<auto>(hash<auto>)> convert = *hash<auto> sub (hash<auto> m) {
            *hash<auto> e = m.metadata.metadata;
            if (!e) {
                return;
            }
            return DropboxDataProviderBase::getFileInfo(e);
        };
        return new DefaultRecordIterator<hash<auto>>(
            new DropboxEntryPageIterator(first, following, convert), where_cond, search_options,
            getRecordTypeImpl(search_options));
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> opts = {
            "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
            "max_results": req.max_results ?? DefaultPageSize,
            "file_status": req.file_status ?? "active",
            "filename_only": req.filename_only ?? False,
        };
        if (req.order_by.val()) {
            # Dropbox unions are wire-encoded as an object with a ".tag" key naming the member
            opts.order_by = {".tag": req.order_by};
        }
        if (req.file_extensions.val()) {
            opts.file_extensions = map trim($1), req.file_extensions.split(","), trim($1).val();
        }

        # a cursor already carries the query and every option it was created with, so continuing a search
        # sends the cursor alone; the action returned a cursor with no way to use it before this
        # NOTE: "/2/files/search/continue_v2", not "/2/files/search_v2/continue"; Dropbox names the
        # continuation inconsistently with the endpoint it continues, and the obvious spelling is a 404
        hash<auto> body = req.cursor.val()
            ? doRpc("/2/files/search/continue_v2", {"cursor": req.cursor})
            : doRpc("/2/files/search_v2", {
                "query": req.query,
                "options": opts,
            });
        list<auto> matches = ();
        foreach hash<auto> m in (body.matches) {
            # a match wraps the entry twice: once as the match's metadata union, once as the entry itself
            *hash<auto> e = m.metadata.metadata;
            if (e) {
                matches += DropboxDataProviderBase::getFileInfo(e);
            }
        }
        return {
            "matches": matches,
            "cursor": body.cursor,
            "has_more": body.has_more ?? False,
        };
    }
}

#! Retrieves the metadata of a file or folder
public class DropboxGetMetadataDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-metadata",
            "type": "DropboxGetMetadataDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxGetMetadataRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        if (path == RootPath) {
            # Dropbox has no metadata resource for the account root and rejects the request as malformed
            throw "DROPBOX-ERROR", "the account root has no metadata; name a file or folder in 'path'";
        }
        return DropboxDataProviderBase::getFileInfo(doRpc("/2/files/get_metadata", {"path": path}));
    }
}

#! Downloads a file's content
public class DropboxDownloadFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "DropboxDownloadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxDownloadFileRequestDataType;
        const ResponseType = DropboxContentResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        hash<auto> resp = doContent("/2/files/download", {"path": path});
        # a content response carries the file bytes in the body and the file's metadata in a header
        *hash<auto> meta = resp.api_result;
        binary content = DropboxDownloadFileDataProvider::getBinaryBody(resp.body);
        return {
            "id": meta.id,
            "name": meta.name,
            "path": meta.path_display ?? path,
            "content": make_base64_string(content),
            "size": content.size(),
            "rev": meta.rev,
        };
    }

    #! Returns a response body as binary
    /** The REST client deserializes according to the response content type, so a body that is in fact bytes may
        arrive as a string; both forms are normalized here

        @param body the response body

        @return the body as binary
    */
    static binary getBinaryBody(auto body) {
        if (body.typeCode() == NT_BINARY) {
            return body;
        }
        return binary(body ?? "");
    }
}

#! Uploads a file
public class DropboxUploadFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "DropboxUploadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxUploadFileRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        string path = req.path.val() ? req.path : ("/" + (file.name ?? ""));
        path = DropboxDataProviderBase::normalizePath(path);
        if (path == RootPath) {
            throw "DROPBOX-ERROR", "neither 'path' nor a file name was supplied; the destination path for the "
                "new file cannot be determined";
        }
        return DropboxDataProviderBase::getFileInfo(uploadContent(path,
            DropboxDataProviderBase::getFileContent(file), req.mode ?? "add", req.autorename ?? False,
            req.mute ?? False));
    }
}

#! Creates a file from text content
public class DropboxCreateTextFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-text-file",
            "type": "DropboxCreateTextFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCreateTextFileRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        if (path == RootPath) {
            throw "DROPBOX-ERROR", "'path' must name the file to create, not the account root";
        }
        return DropboxDataProviderBase::getFileInfo(uploadContent(path, binary(req.content),
            req.mode ?? "add", req.autorename ?? False, req.mute ?? False));
    }
}

#! Returns a temporary direct download link for a file
public class DropboxGetFileLinkDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-file-link",
            "type": "DropboxGetFileLinkDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxGetFileLinkRequestDataType;
        const ResponseType = DropboxFileLinkResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRpc("/2/files/get_temporary_link", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
        });
        return {
            "link": body.link,
            "file": body.metadata ? DropboxDataProviderBase::getFileInfo(body.metadata) : NOTHING,
        };
    }
}
}
