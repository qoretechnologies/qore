# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxDataProviderBase.qc common base class for the DropboxDataProvider module

/*  DropboxDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Dropbox data provider common base class
/** Holds the REST client and the request plumbing shared by every action provider in this module.

    Uses \c DropboxRestClientIo for async-capable multiplexed connections.
*/
public class DropboxDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID for the Dropbox API",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client ID (\"app key\") of the Dropbox app used to reach Dropbox",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret for the Dropbox API",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client secret (\"app secret\") of the Dropbox app used to reach Dropbox",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("DropboxRestClient::DropboxRestClientIo"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options used when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the `DropboxRestClientIo` constructor; only used if a REST client object is "
                    "created for a call",
            },
        };

        #! I/O errors worth retrying
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Maximum I/O retries
        const MaxIoRetries = 3;

        #! The maximum number of entries offered as reference data for a path option
        const MaxReferenceItems = 200;
    }

    private {
        #! The REST client object for API calls
        *DropboxRestClient::DropboxRestClientIo rest;
    }

    #! Creates the object from a DropboxRestClientIo client
    constructor(DropboxRestClient::DropboxRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.oauth2_client_id || !copts.oauth2_client_secret) {
                throw "CONSTRUCTOR-ERROR", "missing 'oauth2_client_id' and 'oauth2_client_secret' options "
                    "required for Dropbox API communication";
            }
            rest = new DropboxRestClient::DropboxRestClientIo((copts.restclient_options ?? {}) + {
                "oauth2_client_id": copts.oauth2_client_id,
                "oauth2_client_secret": copts.oauth2_client_secret,
            });
        }
    }

    #! Returns the REST client object
    *DropboxRestClient::DropboxRestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the reference data types supported by this provider
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "folders": True,
            "files": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "folders": return getReferenceItems("folder");
            case "files": return getReferenceItems("file");
        }
    }

    #! Returns reference data for files or folders
    /** @param tag the Dropbox entry tag to select: \c "file" or \c "folder"

        @return the reference values, or an empty list if they cannot be retrieved

        @note Errors are logged and swallowed: reference data populates a UI picker, and an expired token or a
        narrow scope must degrade to an empty list rather than break the form
    */
    private *list<hash<AllowedValueInfo>> getReferenceItems(string tag) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> body = doRpc("/2/files/list_folder", {
                "path": RootPath,
                "recursive": True,
                "limit": MaxPageSize,
            });
            while (True) {
                foreach hash<auto> e in (body.entries) {
                    if (e.".tag" != tag) {
                        continue;
                    }
                    string path = e.path_display ?? e.path_lower;
                    rv += <AllowedValueInfo>{
                        "display_name": path,
                        "short_desc": e.name,
                        "desc": path,
                        "value": path,
                    };
                    if (rv.size() >= MaxReferenceItems) {
                        return rv;
                    }
                }
                if (!body.has_more || !body.cursor.val()) {
                    break;
                }
                body = doRpc("/2/files/list_folder/continue", {"cursor": body.cursor});
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceItems(tag: %y): %s: %s", tag, ex.err, ex.desc);
        }
        return rv;
    }

    #! Makes a Dropbox RPC call and returns the response body; retries transient I/O errors
    /** Every Dropbox RPC endpoint is a \c POST carrying its arguments as a JSON body, including the ones that
        only read, so there is one method here rather than one per HTTP verb.

        @param path the RPC endpoint path
        @param body the request arguments

        @return the response body

        @throw DROPBOX-ERROR the request failed; the Dropbox error summary is included when the response
        carried one
    */
    private hash<auto> doRpc(string path, auto body) {
        int retries = 0;
        while (True) {
            try {
                debug("Dropbox RPC %y", path);
                return rest.restPost(path, body).body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err} && retries < MaxIoRetries) {
                    ++retries;
                    debug("Dropbox I/O error %s; retrying (%d/%d)", ex.err, retries, MaxIoRetries);
                    continue;
                }
                throwDropboxError(ex, path);
            }
        }
    }

    #! Makes a Dropbox content-endpoint call; retries transient I/O errors
    /** @param path the content endpoint path
        @param arg the operation arguments, sent in the \c Dropbox-API-Arg header
        @param body the request body, if any
        @param content_type the content type of \a body

        @return the response, with the decoded \c Dropbox-API-Result header in the \c api_result key

        @throw DROPBOX-ERROR the request failed; the Dropbox error summary is included when the response
        carried one
    */
    private hash<auto> doContent(string path, auto arg, *binary body, *string content_type) {
        int retries = 0;
        while (True) {
            try {
                debug("Dropbox content %y", path);
                return rest.contentRequest(path, arg, body, content_type);
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err} && retries < MaxIoRetries) {
                    ++retries;
                    debug("Dropbox I/O error %s; retrying (%d/%d)", ex.err, retries, MaxIoRetries);
                    continue;
                }
                throwDropboxError(ex, path);
            }
        }
    }

    #! Throws an exception carrying the Dropbox error summary when the response provided one
    /** Dropbox reports an endpoint-specific failure with HTTP status 409 and a JSON document naming the reason
        in \c error_summary, such as \c path/not_found/ or \c path/conflict/file/.  Without unpacking it the
        caller sees a bare 409, which says only that something about the request did not fit the current state of
        the account.

        Content endpoints answer with an error document as plain text rather than JSON, so both forms are
        handled here.

        @param ex the original exception
        @param path the endpoint path

        @throw DROPBOX-ERROR always
    */
    private nothing throwDropboxError(hash<ExceptionInfo> ex, string path) {
        *hash<auto> arg = DropboxDataProviderBase::getErrorArg(ex.arg);
        auto resp = arg."response-body" ?? arg.body;
        *string summary = DropboxDataProviderBase::getErrorSummary(resp);
        if (summary.val()) {
            # a rate-limited request names how long to wait before retrying; it is reported rather than waited
            # out here, so that the caller decides whether the delay is acceptable
            *int retry_after = arg.hdr."retry-after" ?? arg.headers."retry-after";
            throw "DROPBOX-ERROR", sprintf("%s failed: %s%s", path, summary,
                retry_after ? sprintf(" (retry after %d seconds)", retry_after) : ""), ex.arg;
        }
        throw "DROPBOX-ERROR", sprintf("%s failed: %s: %s", path, ex.err, ex.desc), ex.arg;
    }

    #! Returns an exception argument as a plain hash, so that keys it may not carry can be probed
    /** The REST client raises its errors with a \c hash<RestResponseInfo> argument, and reading a member a
        hashdecl does not declare throws \c INVALID-MEMBER rather than returning @ref nothing.  Probing for the
        response body therefore replaced every failed request's real error -- the Dropbox error summary this
        method exists to surface -- with an unrelated \c INVALID-MEMBER raised by the error path itself.

        Adding an empty hash widens the value to a plain hash, after which an undeclared key simply reads as
        @ref nothing.

        @param arg the exception argument

        @return the argument as a plain hash, or @ref nothing if it is not a hash
    */
    static *hash<auto> getErrorArg(auto arg) {
        if (arg.typeCode() != NT_HASH) {
            return;
        }
        return arg + {};
    }

    #! Returns the Dropbox error summary from a response body, if it holds one
    /** @param resp the response body, as JSON, a decoded hash, or plain text

        @return the error summary, or @ref nothing if the body does not hold one
    */
    static *string getErrorSummary(auto resp) {
        if (resp.typeCode() == NT_HASH) {
            if (resp.error_summary.typeCode() == NT_STRING) {
                return resp.error_summary;
            }
            return;
        }
        if (resp.typeCode() == NT_BINARY) {
            resp = resp.toString();
        }
        if (resp.typeCode() != NT_STRING || !resp.val()) {
            return;
        }
        # content endpoints answer with a bare error document; try JSON first, then use the text itself
        try {
            auto h = parse_json(resp);
            if (h.typeCode() == NT_HASH && h.error_summary.typeCode() == NT_STRING) {
                return h.error_summary;
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return resp;
    }

    #! Uploads content to Dropbox, using an upload session when the content is large
    /** A single-request upload has to be restarted from the beginning if the connection drops, and Dropbox
        rejects one above 150 MB outright.  Above @ref UploadSessionThreshold the content is therefore sent as an
        upload session, which commits each chunk as it arrives.

        @param path the destination path
        @param content the file content
        @param mode the write mode, \c "add" or \c "overwrite"
        @param autorename if a conflicting file should cause the new file to be renamed
        @param mute if the upload should not raise a user notification

        @return the file metadata of the uploaded file
    */
    private hash<auto> uploadContent(string path, binary content, string mode, bool autorename, bool mute) {
        hash<auto> commit = {
            "path": path,
            "mode": mode,
            "autorename": autorename,
            "mute": mute,
        };
        if (content.size() <= UploadSessionThreshold) {
            return doContent("/2/files/upload", commit, content, MimeTypeOctetStream).body;
        }

        int size = content.size();
        int offset = UploadChunkSize;
        # the session is opened with the first chunk, so a session that starts is always one that carries data
        hash<auto> start = doContent("/2/files/upload_session/start", {"close": False},
            content.substr(0, UploadChunkSize), MimeTypeOctetStream).body;
        *string session_id = start.session_id;
        if (!session_id.val()) {
            throw "DROPBOX-ERROR", sprintf("upload session for %y was not given a session ID by the server", path);
        }
        debug("Dropbox upload session %y: %d bytes in chunks of %d", session_id, size, UploadChunkSize);

        while (size - offset > UploadChunkSize) {
            doContent("/2/files/upload_session/append_v2", {
                "cursor": {"session_id": session_id, "offset": offset},
                "close": False,
            }, content.substr(offset, UploadChunkSize), MimeTypeOctetStream);
            offset += UploadChunkSize;
        }

        # the final chunk is committed with the session; a zero-length remainder is legal and commits the session
        return doContent("/2/files/upload_session/finish", {
            "cursor": {"session_id": session_id, "offset": offset},
            "commit": commit,
        }, content.substr(offset, size - offset), MimeTypeOctetStream).body;
    }

    #! Returns the binary content of a FileDataType value
    /** The content is declared as base64 but may already have been decoded upstream, so both forms are accepted

        @param file the file value

        @return the content as binary
    */
    static binary getFileContent(hash<auto> file) {
        if (file.content.typeCode() == NT_BINARY) {
            return file.content;
        }
        return parse_base64_string(file.content.toString());
    }

    #! Converts a Dropbox metadata entry into this module's file representation
    /** Dropbox tags each entry in a listing with its kind in a \c ".tag" key and returns different fields for
        each kind; a folder has no size, revision, or content hash

        @param e the Dropbox metadata entry

        @return the entry in this module's representation
    */
    static hash<auto> getFileInfo(hash<auto> e) {
        return {
            "id": e.id,
            "name": e.name,
            "path": e.path_display ?? e.path_lower,
            "path_lower": e.path_lower,
            "is_folder": e.".tag" == "folder",
            "size": e.size,
            "rev": e.rev,
            "modified": e.server_modified.val() ? date(e.server_modified) : NOTHING,
            "client_modified": e.client_modified.val() ? date(e.client_modified) : NOTHING,
            "content_hash": e.content_hash,
        };
    }

    #! Converts a Dropbox shared-link entry into this module's representation
    /** Dropbox reports the visibility Dropbox actually applied under \c link_permissions rather than at the top
        level, because a requested visibility the account does not support is silently narrowed

        @param l the Dropbox shared-link entry

        @return the shared link in this module's representation
    */
    static hash<auto> getSharedLinkInfo(hash<auto> l) {
        return {
            "url": l.url,
            "id": l.id,
            "name": l.name,
            "path": l.path_lower,
            "visibility": l.link_permissions.resolved_visibility.".tag"
                ?? l.link_permissions.requested_visibility.".tag",
            "expires": l.expires.val() ? date(l.expires) : NOTHING,
        };
    }

    #! Normalizes a Dropbox path
    /** Dropbox addresses the root as the empty string and every other path with a leading slash and no trailing
        slash; \c "/" in particular is rejected as an invalid path rather than understood as the root, which is
        the single most common way a path argument fails

        @param path the path to normalize

        @return the normalized path
    */
    static string normalizePath(string path) {
        # trim trailing separators; the root then becomes the empty string Dropbox expects
        while (path.length() && path[path.length() - 1] == "/") {
            path = path.substr(0, path.length() - 1);
        }
        if (!path.val()) {
            return RootPath;
        }
        if (path[0] != "/") {
            path = "/" + path;
        }
        return path;
    }
}
}
