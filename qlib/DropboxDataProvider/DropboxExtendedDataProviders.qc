# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxExtendedDataProviders.qc further action providers for the DropboxDataProvider module

/*  DropboxExtendedDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Common base class for the operations Dropbox may perform in the background
/** Several Dropbox operations answer either with their result or with a job identifier, depending on how much
    work they turn out to be.  The caller cannot know which in advance, so every one of them is normalized to
    the same shape here: \c complete says whether the work is already done, and \c async_job_id names the job to
    follow when it is not.
*/
public class DropboxAsyncOperationBase inherits DropboxDataProviderBase {
    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    #! Normalizes a response that may carry either a result or a job to follow
    /** @param body the response body

        @return the response in this module's representation
    */
    static hash<auto> getAsyncResult(hash<auto> body) {
        string tag = body.".tag" ?? "complete";
        # NOTE: "auto!"; the entries list cannot go into a literal narrowed by its initial values
        hash<auto!> rv = {
            "complete": tag == "complete",
            "async_job_id": body.async_job_id,
        };
        if (tag == "complete") {
            rv.entries = DropboxAsyncOperationBase::getEntries(body);
        }
        return rv;
    }

    #! Returns the entries from a completed batch or single-file result
    /** A batch answers with a list of per-entry results, while a single-file operation answers with the file
        metadata itself; both are reported as a list so that a caller does not have to tell them apart.

        @param body the response body

        @return the entries

        @since DropboxDataProvider 1.1
    */
    static list<auto> getEntries(hash<auto> body) {
        if (body.entries.typeCode() == NT_LIST) {
            return map DropboxAsyncOperationBase::getEntryResult($1), body.entries;
        }
        # a save_url that completed immediately carries the file metadata at the top level
        if (body.name.val() || body.id.val()) {
            return (DropboxDataProviderBase::getFileInfo(body),);
        }
        return ();
    }

    #! Converts one per-entry batch result into this module's representation
    /** Dropbox reports each entry as a tagged union: \c success carrying the metadata, or \c failure carrying
        the reason.  **A batch does not fail as a whole**, so a caller must be able to see which entries
        worked, which is why the outcome is reported per entry rather than raised.

        @param entry the per-entry result

        @return the entry in this module's representation

        @since DropboxDataProvider 1.1
    */
    static hash<auto> getEntryResult(auto entry) {
        if (entry.typeCode() != NT_HASH) {
            return {"success": False, "error": sprintf("%y", entry)};
        }
        string tag = entry.".tag" ?? "";
        if (tag == "success") {
            # a relocation reports the moved entry under "success", a delete under "metadata"
            *hash<auto> meta = entry.success ?? entry.metadata;
            return {
                "success": True,
                "entry": meta ? DropboxDataProviderBase::getFileInfo(meta) : NOTHING,
            };
        }
        return {
            "success": False,
            "error": entry.failure.".tag" ?? tag,
        };
    }
}

#! Has Dropbox fetch a URL directly into the account
/** The content never passes through this system: Dropbox fetches the URL from its own servers, so a large file
    costs nothing here in memory, time, or bandwidth.  The URL must be reachable from the public internet.
*/
public class DropboxSaveUrlDataProvider inherits DropboxAsyncOperationBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "save-url",
            "type": "DropboxSaveUrlDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxSaveUrlRequestDataType;
        const ResponseType = DropboxAsyncJobResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxAsyncOperationBase(rest) {}
    constructor(*hash<auto> options) : DropboxAsyncOperationBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return DropboxAsyncOperationBase::getAsyncResult(doRpc("/2/files/save_url", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
            "url": req.url,
        }));
    }
}

#! Follows an operation Dropbox is performing in the background
public class DropboxCheckJobStatusDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "check-job-status",
            "type": "DropboxCheckJobStatusDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCheckJobStatusRequestDataType;
        const ResponseType = DropboxJobStatusResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Dropbox has no single job-status endpoint, so the kind of job selects the one to call
        *string path = AsyncJobStatusPaths{req.job_type};
        if (!path.val()) {
            throw "DROPBOX-ERROR", sprintf("unknown job type %y; expecting one of: %y", req.job_type,
                keys AsyncJobStatusPaths);
        }
        hash<auto> body = doRpc(path, {"async_job_id": req.async_job_id});

        string tag = body.".tag" ?? "";
        # NOTE: "auto!"; the entries list cannot go into a literal narrowed by its initial values
        hash<auto!> rv = {
            "status": tag == "in_progress" ? "in_progress" : (tag == "failed" ? "failed" : "complete"),
            "complete": tag == "complete",
        };
        if (tag == "complete") {
            rv.entries = DropboxAsyncOperationBase::getEntries(body);
        } else if (tag == "failed") {
            rv.failure = body.failed.".tag" ?? sprintf("%y", body.failed);
        }
        return rv;
    }
}

#! Deletes several files or folders in one call
public class DropboxDeleteBatchDataProvider inherits DropboxAsyncOperationBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-batch",
            "type": "DropboxDeleteBatchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxDeleteBatchRequestDataType;
        const ResponseType = DropboxAsyncJobResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxAsyncOperationBase(rest) {}
    constructor(*hash<auto> options) : DropboxAsyncOperationBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> paths = req.paths.typeCode() == NT_LIST ? req.paths : (req.paths,);
        if (!paths) {
            throw "DROPBOX-ERROR", "no paths were given to delete";
        }
        # NOTE: built with an explicit loop; the "map {k: v}, list" form returns a single hash with one entry
        # per iteration rather than one hash per element
        list<auto> entries = ();
        foreach auto path in (paths) {
            entries += {"path": DropboxDataProviderBase::normalizePath(path.toString())};
        }
        return DropboxAsyncOperationBase::getAsyncResult(doRpc("/2/files/delete_batch", {
            "entries": entries,
        }));
    }
}

#! Moves or copies several files in one call
/** Both operations take the same arguments and answer the same way, so they share an implementation and differ
    only in the endpoint they call.
*/
public class DropboxRelocationBatchDataProviderBase inherits DropboxAsyncOperationBase {
    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxAsyncOperationBase(rest) {}
    constructor(*hash<auto> options) : DropboxAsyncOperationBase(options) {}

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return DropboxRelocationBatchRequestDataType;
    }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return DropboxAsyncJobResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> entries = req.entries.typeCode() == NT_LIST ? req.entries : (req.entries,);
        if (!entries) {
            throw "DROPBOX-ERROR", sprintf("no entries were given to %s", getEndpointVerb());
        }
        list<auto> arg = ();
        foreach auto e in (entries) {
            if (e.typeCode() != NT_HASH || !e.from_path.val() || !e.to_path.val()) {
                throw "DROPBOX-ERROR", sprintf("each entry must be a hash carrying 'from_path' and "
                    "'to_path'; got %y", e);
            }
            arg += {
                "from_path": DropboxDataProviderBase::normalizePath(e.from_path.toString()),
                "to_path": DropboxDataProviderBase::normalizePath(e.to_path.toString()),
            };
        }
        return DropboxAsyncOperationBase::getAsyncResult(doRpc(getEndpointPath(), {
            "entries": arg,
            "autorename": req.autorename ?? False,
        }));
    }

    #! Returns the RPC endpoint this batch operation calls
    abstract private string getEndpointPath();

    #! Returns the verb naming the operation, for error messages
    abstract private string getEndpointVerb();
}

#! Moves several files in one call
public class DropboxMoveBatchDataProvider inherits DropboxRelocationBatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "move-batch",
            "type": "DropboxMoveBatchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxRelocationBatchRequestDataType;
        const ResponseType = DropboxAsyncJobResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxRelocationBatchDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxRelocationBatchDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEndpointPath() { return "/2/files/move_batch_v2"; }
    private string getEndpointVerb() { return "move"; }
}

#! Copies several files in one call
public class DropboxCopyBatchDataProvider inherits DropboxRelocationBatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-batch",
            "type": "DropboxCopyBatchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxRelocationBatchRequestDataType;
        const ResponseType = DropboxAsyncJobResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxRelocationBatchDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxRelocationBatchDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEndpointPath() { return "/2/files/copy_batch_v2"; }
    private string getEndpointVerb() { return "copy"; }
}

#! Removes a file or folder outright, bypassing the trash
/** \c delete-file moves a file to the trash, where it remains recoverable and continues to count against the
    account's storage.  This removes it entirely.

    @note The Dropbox app must be granted the permission for this; a personal account without it is refused
    with \c insufficient_permissions rather than the operation silently doing nothing.
*/
public class DropboxPermanentlyDeleteDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "permanently-delete",
            "type": "DropboxPermanentlyDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxPermanentlyDeleteRequestDataType;
        const ResponseType = DropboxSuccessResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # this endpoint answers with an empty body on success, so there is nothing to report but the outcome
        doRpc("/2/files/permanently_delete", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
        });
        return {"success": True};
    }
}

#! Returns a thumbnail image of a file
/** Dropbox renders the thumbnail itself, so a preview can be shown without downloading the file.
*/
public class DropboxGetThumbnailDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-thumbnail",
            "type": "DropboxGetThumbnailDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxThumbnailRequestDataType;
        const ResponseType = DropboxRenderedContentDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        string format = req.format ?? "jpeg";
        # the resource is a tagged union, so that a thumbnail can be taken by path or by shared link
        hash<auto> resp = doContent("/2/files/get_thumbnail_v2", {
            "resource": {".tag": "path", "path": path},
            "format": {".tag": format},
            "size": {".tag": req.size ?? "w256h256"},
        });
        binary content = resp.body.typeCode() == NT_BINARY ? resp.body : binary(resp.body ?? "");
        return {
            "path": path,
            "content": make_base64_string(content),
            "mime_type": format == "png" ? MimeTypePng : MimeTypeJpeg,
            "size": content.size(),
        };
    }
}

#! Returns a PDF preview of a document
/** Dropbox renders the preview itself, so a document in a format this system cannot read -- a Word file, a
    spreadsheet -- can still be shown or archived as a PDF.
*/
public class DropboxGetPreviewDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-preview",
            "type": "DropboxGetPreviewDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxPreviewRequestDataType;
        const ResponseType = DropboxRenderedContentDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        hash<auto> resp = doContent("/2/files/get_preview", {"path": path});
        binary content = resp.body.typeCode() == NT_BINARY ? resp.body : binary(resp.body ?? "");
        return {
            "path": path,
            "content": make_base64_string(content),
            "mime_type": MimeTypePdf,
            "size": content.size(),
        };
    }
}

#! Grants people access to a file
/** \c create-shared-link hands out a link that anyone holding it can use; this grants access to named people,
    which is what an audit trail and a later withdrawal both need.
*/
public class DropboxAddFileMemberDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-file-member",
            "type": "DropboxAddFileMemberDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxAddFileMemberRequestDataType;
        const ResponseType = DropboxFileMembersResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> emails = req.emails.typeCode() == NT_LIST ? req.emails : (req.emails,);
        if (!emails) {
            throw "DROPBOX-ERROR", "no email addresses were given to share the file with";
        }
        # NOTE: "auto!"; the members list cannot go into a literal narrowed by its initial string values
        hash<auto!> body = {
            "file": DropboxAddFileMemberDataProvider::getFileArgument(req.file),
            "access_level": {".tag": req.access_level ?? "viewer"},
            "quiet": req.quiet ?? False,
        };
        # NOTE: an explicit loop for the same reason as above
        list<auto> members = ();
        foreach auto email in (emails) {
            members += {".tag": "email", "email": email.toString()};
        }
        body.members = members;
        if (req.custom_message.val()) {
            body.custom_message = req.custom_message;
        }

        # the response is a list of per-member results rather than the file's membership, so the membership is
        # read back afterwards: a caller asking who has access should be told, not handed a receipt
        doRpc("/2/sharing/add_file_member", body);
        return getMembers(req.file, DefaultPageSize);
    }

    #! Returns the file argument Dropbox expects
    /** The sharing endpoints take a file ID rather than a path, but a path is far easier to supply, so one is
        passed through unchanged and Dropbox resolves it.

        @param file the file ID or path

        @return the value to send
    */
    static string getFileArgument(string file) {
        if (file =~ /^(id:|ns:)/) {
            return file;
        }
        return DropboxDataProviderBase::normalizePath(file);
    }
}

#! Lists the people who have access to a file
public class DropboxListFileMembersDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-file-members",
            "type": "DropboxListFileMembersDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxListFileMembersRequestDataType;
        const ResponseType = DropboxFileMembersResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getMembers(req.file, req.limit ?? 100);
    }
}
}
