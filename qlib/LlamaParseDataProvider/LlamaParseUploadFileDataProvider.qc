# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LlamaParseDataProvider module: uploading a document

/** LlamaParseUploadFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace LlamaParseDataProvider {
#! What a file is uploaded for, which decides how long LlamaIndex keeps it
/** Named in the form field's own description; the schema types it as a bare string.
*/
public const LlamaParseFilePurposeValues = (
    <AllowedValueInfo>{"value": "parse", "display_name": "Parsing"},
    <AllowedValueInfo>{"value": "extract", "display_name": "Extraction"},
    <AllowedValueInfo>{"value": "classify", "display_name": "Classification"},
    <AllowedValueInfo>{"value": "split", "display_name": "Splitting"},
    <AllowedValueInfo>{"value": "sheet", "display_name": "Spreadsheet"},
    <AllowedValueInfo>{"value": "agent_app", "display_name": "Agent Application"},
    <AllowedValueInfo>{"value": "user_data", "display_name": "User Data"},
);

#! Request type for @ref LlamaParseUploadFileDataProvider
public class LlamaParseUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_data": {
                "display_name": "File Contents",
                "short_desc": "The document to upload, given inline",
                "desc": "The document to upload, given inline as binary data.  Supply this or "
                    "`file_path`, not both.",
                "type": AbstractDataProviderTypeMap."*data",
            },
            "file_path": {
                "display_name": "File Path",
                "short_desc": "The path of a local document to upload",
                "desc": "The path of a local document to upload.  Supply this or `file_data`, not "
                    "both.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/data/invoices/2026-08.pdf",
            },
            "filename": {
                "display_name": "File Name",
                "short_desc": "The name to store the document under",
                "desc": "The name to store the document under.  Defaults to the base name of "
                    "`file_path`, and to `document` when the contents are given inline, so set it "
                    "when the extension matters - LlamaParse reads the type from the name.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "invoices-2026-08.pdf",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type of the document",
                "desc": "The MIME type of the document.  Defaults to "
                    "`application/octet-stream`, which lets LlamaParse decide from the file name.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "application/pdf",
            },
            "purpose": {
                "display_name": "Purpose",
                "short_desc": "What the file is uploaded for",
                "desc": "What the file is uploaded for.  LlamaIndex uses this to decide the storage "
                    "and retention policy applied to the file.",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "parse",
                "allowed_values": LlamaParseFilePurposeValues,
            },
            "external_file_id": {
                "display_name": "External File ID",
                "short_desc": "The ID this document has in your own system",
                "desc": "The ID this document has in your own system, stored alongside the file so "
                    "that it can be found again with **List Files**.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "project_id": {
                "display_name": "Project",
                "short_desc": "The project to upload into",
                "desc": "The project to upload into.  Leave unset to use the project the API key "
                    "belongs to.",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": LlamaParseManifest::RefProject,
            },
        };
    }

    constructor() : HashDataType("LlamaParseUploadFileRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for @ref LlamaParseUploadFileDataProvider
public class LlamaParseUploadFileResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The ID of the uploaded file",
                "desc": "The ID of the uploaded file, which is what **Parse Document**, **Extract "
                    "Structured Data**, **Classify Document** and **Split Document** take as their "
                    "input.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "File Name",
                "short_desc": "The name the file is stored under",
                "desc": "The name the file is stored under.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "file_type": {
                "display_name": "File Type",
                "short_desc": "The type LlamaIndex determined for the file",
                "desc": "The type LlamaIndex determined for the file.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "project_id": {
                "display_name": "Project",
                "short_desc": "The project the file was uploaded into",
                "desc": "The project the file was uploaded into.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "external_file_id": {
                "display_name": "External File ID",
                "short_desc": "The caller-supplied ID stored with the file",
                "desc": "The caller-supplied ID stored with the file.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("LlamaParseUploadFileResponse") {
        addQoreFields(Fields);
        # the vendor adds fields to this record over time, and dropping them would lose information
        # a caller may already depend on
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Uploads a document so that it can be parsed, extracted from, classified or split
/** Hand-written because the operation takes a \c multipart/form-data body, which
    @ref restschemaactionsintro "RestSchemaActions" does not generate: a generated action would send
    JSON where the server expects a multipart body.

    Uploading is worth doing on its own rather than passing a URL to every job: the document is stored
    once and then referred to by ID, so parsing, extracting and classifying the same document costs one
    transfer rather than three.

    @since %LlamaParseDataProvider 1.0
*/
public class LlamaParseUploadFileDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The name of this provider
        const ProviderName = "upload-file";

        #! The path the file is uploaded to
        const UploadPath = "/api/v1/beta/files";

        #! The path the project dropdown is filled from
        const ProjectListPath = "/api/v2/projects?page_size=100";

        #! The name used when the contents are given inline and no name is supplied
        const DefaultFilename = "document";

        #! The request type
        const RequestType = new LlamaParseUploadFileRequestDataType();

        #! The response type
        const ResponseType = new LlamaParseUploadFileResponseDataType();

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ProviderName,
            "type": "LlamaParseUploadFileDataProvider",
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The REST client
        LlamaParseRestClient::LlamaParseRestClientIo rest;
    }

    #! Creates the object from a REST client
    constructor(LlamaParseRestClient::LlamaParseRestClientIo rest) {
        self.rest = rest;
    }

    string getName() {
        return ProviderName;
    }

    #! The project dropdown this action offers
    /** The generated actions get theirs from the action set; this one is hand-written, so it
        answers the same reference data itself rather than leaving the field as free text.
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {LlamaParseManifest::RefProject: True};
    }

    #! Lists the projects the API key can reach
    /** @param type the reference data name
        @param action_opts the options already filled in on the action; unused here, because the
        project listing is not scoped by anything the caller has chosen yet

        @return the projects this API key can reach, or @ref nothing for any other reference data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        if (type != LlamaParseManifest::RefProject) {
            return;
        }
        on_error rethrow $1.err, sprintf("%s (while listing projects for the %y dropdown)",
            $1.desc, ProviderName);
        auto body = rest.restGet(ProjectListPath).body;
        return map <AllowedValueInfo>{
            "value": $1.id,
            "display_name": $1.name ?? $1.id,
        }, (body.items ?? ());
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Uploads the document and returns the stored file
    /** @throw UPLOAD-FILE-ERROR neither or both of \c file_data and \c file_path were given
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (exists req.file_data && req.file_path.val()) {
            throw "UPLOAD-FILE-ERROR", "give the document either as \"file_data\" or as "
                "\"file_path\", not both";
        }
        data body;
        *string filename = req.filename;
        if (exists req.file_data) {
            body = req.file_data;
            if (!filename.val()) {
                filename = DefaultFilename;
            }
        } else if (req.file_path.val()) {
            body = ReadOnlyFile::readBinaryFile(req.file_path);
            if (!filename.val()) {
                filename = basename(req.file_path);
            }
        } else {
            throw "UPLOAD-FILE-ERROR", "give the document either as \"file_data\" or as "
                "\"file_path\"";
        }

        Mime::MultiPartFormDataMessage msg();
        msg.addPart(body, "file", filename, req.content_type ?? MimeTypeOctetStream);
        msg.addPart(req.purpose ?? "parse", "purpose");
        if (req.external_file_id.val()) {
            msg.addPart(req.external_file_id, "external_file_id");
        }
        hash<MessageInfo> mi = msg.getMsgAndHeaders();

        string path = UploadPath;
        if (req.project_id.val()) {
            path += "?project_id=" + req.project_id;
        }
        # the body is already serialized as a multipart message; restPost() would hand it to the
        # client's data serializer and send a JSON string holding the base64 of it
        return rest.restDoRawRequest("POST", path, mi.body, mi.hdr).body;
    }
}
}
