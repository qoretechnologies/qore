# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveDataProviderBase.qc common base class for the GoogleDriveDataProvider module

/*  GoogleDriveDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Google Drive data provider common base class
/** Holds the REST client and the request plumbing shared by every action provider in this module.

    Uses \c RestClientIo for async-capable multiplexed connections.
*/
public class GoogleDriveDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID for the Google Drive API",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client ID from the Google Cloud project used to reach Drive",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret for the Google Drive API",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client secret from the Google Cloud project used to reach Drive",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClientIo::RestClientIo"), NOTHING, {
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
                "desc": "Options to the `GoogleRestClientIo` constructor; only used if a REST client object is "
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
    }

    private {
        #! The REST client object for API calls
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
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
                    "required for Google Drive API communication";
            }
            rest = new GoogleRestClient::GoogleRestClientIo((copts.restclient_options ?? {}) + {
                "oauth2_client_id": copts.oauth2_client_id,
                "oauth2_client_secret": copts.oauth2_client_secret,
                "api_profile": "drive",
            });
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
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
            "shared_drives": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "folders": return getReferenceItems(True);
            case "files": return getReferenceItems(False);
            case "shared_drives": return getSharedDriveReferenceData();
        }
    }

    #! Returns the shared drives the user can reach as reference values
    /** @return the reference values, or an empty list if they cannot be retrieved

        @note Errors are logged and swallowed: reference data populates a UI picker, and a personal account -
        which has no shared drives at all - must degrade to an empty list rather than break the form
    */
    private *list<hash<AllowedValueInfo>> getSharedDriveReferenceData() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> resp = doRestCommand("GET", sprintf("/drive/v3/drives?pageSize=%d&fields=%s", MaxDrivePageSize,
                encode_url("drives(id,name)", True)));
            foreach hash<auto> d in (resp.body.drives) {
                if (!d.id.val()) {
                    continue;
                }
                rv += <AllowedValueInfo>{
                    "display_name": d.name ?? d.id,
                    "short_desc": d.name ?? d.id,
                    "desc": d.name ?? d.id,
                    "value": d.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getSharedDriveReferenceData(): %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for files or folders
    /** @param folders if folders rather than files should be listed

        @return the reference values, or an empty list if they cannot be retrieved

        @note Errors are logged and swallowed: reference data populates a UI picker, and an expired token or a
        narrow scope must degrade to an empty list rather than break the form
    */
    private *list<hash<AllowedValueInfo>> getReferenceItems(bool folders) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string q = folders
                ? "mimeType = '" + FolderMimeType + "' and trashed = false"
                : "mimeType != '" + FolderMimeType + "' and trashed = false";
            hash<auto> resp = doRestCommand("GET", "/drive/v3/files?pageSize=200&fields="
                + encode_url("files(id,name,mimeType)", True) + "&q=" + encode_url(q, True));
            foreach hash<auto> f in (resp.body.files) {
                rv += <AllowedValueInfo>{
                    "display_name": f.name,
                    "short_desc": f.name,
                    "desc": f.name,
                    "value": f.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceItems(folders: %y): %s: %s", folders, ex.err, ex.desc);
        }
        return rv;
    }

    #! Makes a REST call and returns the response; retries transient I/O errors
    /** @param method the HTTP method
        @param path the URI path
        @param body the request body, if any
        @param hdr any additional request headers

        @return the response

        @throw GOOGLE-DRIVE-ERROR the request failed; the Drive error reason and message are included when the
        response carried them
    */
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        return doRestCommandIntern(method, path, body, hdr, False);
    }

    #! Makes a REST call with a body that must reach the server exactly as given; retries transient I/O errors
    /** The REST client serializes a request body into its configured data format, so a \c binary body is
        base64-encoded into a JSON or YAML document and a \c Content-Type set by the caller has \c ";charset="
        appended to it.  For a Drive upload that is fatal twice over: the server receives a quoted base64 string
        rather than the bytes, and the \c multipart/related boundary parameter is corrupted -- which is exactly
        what Drive reports as <tt>Invalid multipart request with 0 mime parts</tt>.

        This sends the body through unchanged and leaves \c Content-Type to the caller.

        @param method the HTTP method
        @param path the URI path
        @param body the already-serialized request body
        @param hdr the request headers; the caller sets \c Content-Type

        @return the response

        @throw GOOGLE-DRIVE-ERROR the request failed; the Drive error reason and message are included when the
        response carried them
    */
    private hash<auto> doRawRestCommand(string method, string path, *data body, *hash<auto> hdr) {
        return doRestCommandIntern(method, path, body, hdr, True);
    }

    #! Common request path for @ref doRestCommand() and @ref doRawRestCommand()
    /** @param method the HTTP method
        @param path the URI path
        @param body the request body, if any
        @param hdr any additional request headers
        @param raw if the body is already serialized and must be sent unchanged

        @return the response

        @throw GOOGLE-DRIVE-ERROR the request failed
    */
    private hash<auto> doRestCommandIntern(string method, string path, auto body, *hash<auto> hdr, bool raw) {
        path = GoogleDriveDataProviderBase::addSharedDriveSupport(path);
        int retries = 0;
        while (True) {
            try {
                debug("Google Drive %s %y", method, path);
                if (raw) {
                    return rest.restDoRawRequest(method.upr(), path, body, hdr);
                }
                switch (method.upr()) {
                    case "GET":    return rest.restGet(path, hdr);
                    case "POST":   return rest.restPost(path, body, hdr);
                    case "PUT":    return rest.restPut(path, body, hdr);
                    case "PATCH":  return rest.restPatch(path, body, hdr);
                    case "DELETE": return rest.restDel(path, body, hdr);
                }
                throw "GOOGLE-DRIVE-ERROR", sprintf("unsupported HTTP method %y", method);
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err} && retries < MaxIoRetries) {
                    ++retries;
                    debug("Google Drive I/O error %s; retrying (%d/%d)", ex.err, retries, MaxIoRetries);
                    continue;
                }
                throwDriveError(ex, method, path);
            }
        }
    }

    #! Throws an exception carrying the Drive error reason and message when the response provided them
    /** The Drive API reports failures as a JSON document naming a reason such as \c notFound,
        \c insufficientFilePermissions, or \c storageQuotaExceeded.  Without unpacking it the caller sees only an
        HTTP status, which does not distinguish a missing file from one the token's scope cannot reach - the
        single most common misconfiguration with the narrower \c drive.file scope.

        @param ex the original exception
        @param method the HTTP method used
        @param path the URI path
    */
    private nothing throwDriveError(hash<ExceptionInfo> ex, string method, string path) {
        *hash<auto> arg = GoogleDriveDataProviderBase::getErrorArg(ex.arg);
        *hash<auto> err = arg."response-body".error ?? arg.body.error;
        if (err.message.val()) {
            *string reason = err.errors[0].reason ?? err.status;
            throw "GOOGLE-DRIVE-ERROR", sprintf("%s %s failed: %s%s", method, path,
                reason.val() ? reason + ": " : "", err.message), ex.arg;
        }
        throw "GOOGLE-DRIVE-ERROR", sprintf("%s %s failed: %s: %s", method, path, ex.err, ex.desc), ex.arg;
    }

    #! Returns an exception argument as a plain hash, so that keys it may not carry can be probed
    /** The REST client raises its errors with a \c hash<RestResponseInfo> argument, and reading a member a
        hashdecl does not declare throws \c INVALID-MEMBER rather than returning @ref nothing.  Probing for the
        response body therefore replaced every failed request's real error -- the Drive reason and message this
        method exists to surface -- with an unrelated \c INVALID-MEMBER from the error path itself.

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

    #! Adds shared drive support to a Drive API request path
    /** Every Drive endpoint addressing a file, a permission, a revision, or a change accepts
        \c supportsAllDrives.  Without it a business account cannot reach any content held on a shared drive
        (formerly a "Team Drive") at all: the API answers as though the item did not exist, so the failure looks
        like a missing file rather than a missing capability.

        Google's guidance is that an application supporting shared drives sends the parameter on every such
        request, so it is added centrally here rather than at each call site, where a newly added action would
        silently omit it and reintroduce the gap.

        The parameter is added only to this module's own API paths, and never when the caller has already set it
        (which is how @ref GoogleDriveListFilesDataProvider passes the rest of the shared-drive parameters).

        @param path the URI path

        @return the path with the parameter added, or unchanged when it does not apply
    */
    static string addSharedDriveSupport(string path) {
        # NOTE: "/drive/v3/drives" is deliberately not matched; the shared-drive collection endpoint itself takes
        # no such parameter, and it is the one endpoint that is always about shared drives
        if (!(path =~ /^\/(upload\/)?drive\/v3\/(files|permissions|changes)/)) {
            return path;
        }
        if (path.find("supportsAllDrives=") != -1) {
            return path;
        }
        return path + (path.find("?") == -1 ? "?" : "&") + "supportsAllDrives=true";
    }

    #! Retrieves a file's metadata resource
    /** @param file_id the Drive file ID

        @return the Drive file resource
    */
    private hash<auto> getFileMetadata(string file_id) {
        return doRestCommand("GET", sprintf("/drive/v3/files/%s?fields=%s", encode_url(file_id, True),
            encode_url(GoogleDriveDataProviderBase::getFileFields(), True))).body;
    }

    #! Retrieves raw content from the given path
    /** The REST client deserializes according to the response content type, so a binary body may arrive as a
        string; both forms are normalized here

        @param path the URI path

        @return the content as binary
    */
    private binary getRawContent(string path) {
        hash<auto> resp = doRestCommand("GET", path, NOTHING, {"Accept": "*/*"});
        if (resp.body.typeCode() == NT_BINARY) {
            return resp.body;
        }
        return binary(resp.body ?? "");
    }

    #! Sends a Drive multipart upload carrying metadata and content in one request
    /** Drive's multipart upload format is a \c multipart/related body whose first part is the file metadata as
        JSON and whose second is the content.  It is assembled here rather than with a generic multipart helper
        because Drive requires the metadata part to come first and because the boundary has to be chosen so that
        it cannot occur in the content.

        @param method the HTTP method
        @param path the upload URI path
        @param metadata the file metadata
        @param content the file content
        @param mime the content MIME type

        @return the response
    */
    private hash<auto> uploadMultipart(string method, string path, hash<auto> metadata, binary content,
            string mime) {
        string boundary = "qore-drive-" + make_hex_string(get_random_bytes(16));
        # a boundary occurring in the content would corrupt the message; with 16 random bytes the probability is
        # negligible, but the check costs nothing next to the transfer and turns silent corruption into an error
        if (content.toString("ISO-8859-1").find(boundary) != -1) {
            throw "GOOGLE-DRIVE-ERROR", "internal error: the generated multipart boundary occurs in the content";
        }

        binary body = binary(sprintf("--%s\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n%s\r\n"
            "--%s\r\nContent-Type: %s\r\n\r\n", boundary, make_json(metadata), boundary, mime));
        body += content;
        body += binary(sprintf("\r\n--%s--\r\n", boundary));

        # NOTE: the raw path is required.  A body sent through the ordinary path is serialized into the client's
        # data format - so these bytes would arrive as a base64 string inside a JSON document - and the
        # Content-Type set here would have ";charset=UTF-8" appended to it, corrupting the boundary parameter
        return doRawRestCommand(method, path + "?uploadType=multipart&fields="
            + encode_url(GoogleDriveDataProviderBase::getFileFields(), True), body, {
            "Content-Type": "multipart/related; boundary=" + boundary,
        });
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

    #! Returns the MIME type to store with a file
    /** @param file the file value
        @param name the name the file will be stored under

        @return the MIME type carried by the file, or one inferred from the name

        @note \c "auto" is the @ref DataProvider::FileDataType "FileDataType" placeholder meaning "work it out
        from the name", so it is treated as absent rather than sent literally
    */
    static string getMimeType(hash<auto> file, string name) {
        if (file.mime_type.val() && file.mime_type != "auto") {
            return file.mime_type;
        }
        return get_mime_type_from_ext(name) ?? MimeTypeOctetStream;
    }

    #! Returns the standard file metadata field list for a Drive request
    /** Drive returns only \c id, \c name, and \c mimeType unless the fields wanted are named explicitly, so
        every request that reads metadata has to ask for the rest
    */
    static string getFileFields() {
        return "id,name,mimeType,size,createdTime,modifiedTime,parents,webViewLink,webContentLink,trashed,"
            "description,md5Checksum,shortcutDetails,driveId";
    }

    #! Converts a Drive file resource into this module's file representation
    /** @param f the Drive file resource

        @return the file metadata in this module's representation
    */
    static hash<auto> getFileInfo(hash<auto> f) {
        return {
            "id": f.id,
            "name": f.name,
            "mime_type": f.mimeType,
            # Drive reports the size as a string, and omits it entirely for folders and Google-native documents
            "size": f.size.val() ? f.size.toInt() : NOTHING,
            "created": f.createdTime.val() ? date(f.createdTime) : NOTHING,
            "modified": f.modifiedTime.val() ? date(f.modifiedTime) : NOTHING,
            "parents": f.parents,
            "web_view_link": f.webViewLink,
            "web_content_link": f.webContentLink,
            "trashed": f.trashed ?? False,
            "description": f.description,
            "md5": f.md5Checksum,
            # absent for an item in the user's own "My Drive"; present only for shared-drive content
            "drive_id": f.driveId,
            "shortcut_target_id": f.shortcutDetails.targetId,
            "shortcut_target_mime_type": f.shortcutDetails.targetMimeType,
            "is_folder": f.mimeType == FolderMimeType,
            "is_shortcut": f.mimeType == ShortcutMimeType,
            "is_google_doc": f.mimeType =~ /^application\/vnd\.google-apps\./
                && f.mimeType != FolderMimeType,
        };
    }
}
}
