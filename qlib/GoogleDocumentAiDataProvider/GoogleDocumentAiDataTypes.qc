# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDocumentAiDataProvider request/response data type definitions

/** GoogleDocumentAiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace GoogleDocumentAiDataProvider {
public class GoogleDocumentAiProcessDocumentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "processor_id": {
                "display_name": "Processor ID",
                "short_desc": "Document AI processor ID or resource name",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Processor ID or full resource name in the connection's project and location.",
                "required": True,
                "ref_data": "processors",
                "supports_custom_values": True,
                "example_value": "a1b2c3d4e5f6abcd",
            },
            "document_bytes": {
                "display_name": "Document Bytes",
                "short_desc": "Raw document bytes (PDF / TIFF / image)",
                "type": AbstractDataProviderTypeMap."*binary",
                "sensitive": True,
                "desc": "Raw bytes of the document to process.  Mutually exclusive "
                    "with `document_path` and the `gcs_*` fields.",
            },
            "document_path": {
                "display_name": "Document Path",
                "short_desc": "Local file path to read and upload as bytes",
                "type": AbstractDataProviderTypeMap."*string",
                "sensitive": True,
                "desc": "Local file path to read.  Mutually exclusive with "
                    "`document_bytes` and the `gcs_*` fields.",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "MIME type of the document",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "MIME type of the document — required when using "
                    "`document_bytes` or `document_path`.  Common values: "
                    "`application/pdf`, `image/png`, `image/jpeg`, `image/tiff`.",
                "default_value": "application/pdf",
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "application/pdf",  "display_name": "PDF"},
                    <AllowedValueInfo>{"value": "image/png",        "display_name": "PNG"},
                    <AllowedValueInfo>{"value": "image/jpeg",       "display_name": "JPEG"},
                    <AllowedValueInfo>{"value": "image/tiff",       "display_name": "TIFF"},
                    <AllowedValueInfo>{"value": "image/gif",        "display_name": "GIF"},
                    <AllowedValueInfo>{"value": "image/bmp",        "display_name": "BMP"},
                    <AllowedValueInfo>{"value": "image/webp",       "display_name": "WebP"},
                ),
            },
            "gcs_uri": {
                "display_name": "GCS URI",
                "short_desc": "GCS object reference (e.g., gs://bucket/path)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "GCS URI for the document (e.g., `gs://my-bucket/docs/invoice.pdf`).  "
                    "Mutually exclusive with `document_bytes` and `document_path`.  "
                    "When set, `mime_type` should also be provided.",
                "example_value": "gs://my-bucket/docs/invoice.pdf",
                "sensitive": True,
            },
            "process_options": {
                "display_name": "Process Options",
                "short_desc": "Optional per-processor process options",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Optional per-processor `processOptions` hash forwarded "
                    "to Document AI — used for OCR overrides, layout config, "
                    "schema overrides on custom-trained processors, etc.",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
        RestSchemaActionOperation op = GoogleDocumentAiSchema::getActionSets()[0].getOperationEx("_process-version");
        hash<string, AbstractDataField> native = op.getRequestFields();
        addField(native.processOptions.getRenamedField("process_options", "Process Options"));
        AbstractDataField inline_document = native.inlineDocument.getRenamedField("inline_document", "Inline Document");
        inline_document.setAttributes((inline_document.getAttributes() ?? {}) + {"sensitive": True});
        addField(inline_document);
        addField(native.fieldMask.getRenamedField("field_mask", "Output Field Mask"));
        addField(native.imagelessMode.getRenamedField("imageless_mode", "Omit Document Images"));
        addField(native.labels);
        addField(native.processor_version_id.getOrNothingType());
    }
}

#! Typed sub-hash for the top-level Google Document AI `Document` object
/** Covers the common top-level fields of the
    [`Document`](https://cloud.google.com/document-ai/docs/reference/rest/v1/Document)
    proto, including recursively typed pages, entities, tables, text anchors, and normalized values.
    The retained Fields constant is legacy metadata; instances use the complete pinned contract.
*/
public class GoogleDocumentAiDocumentDataType inherits HashDataType {
    public {
        const Fields = {
            "mimeType": {
                "display_name": "MIME Type",
                "desc": "MIME type of the source document.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Text",
                "desc": "Full extracted text content (UTF-8).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "pages": {
                "display_name": "Pages",
                "desc": "Per-page layout — each entry has `pageNumber`, "
                    "`dimension`, `layout`, `paragraphs`, `lines`, `tokens`, "
                    "`visualElements`, `tables`, `formFields`, and "
                    "`blocks`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "entities": {
                "display_name": "Entities",
                "desc": "Typed entities extracted by specialist processors "
                    "(invoice, receipt, contract, etc.) — each entry has "
                    "`type`, `mentionText`, `confidence`, `normalizedValue`, "
                    "`pageAnchor`, and `properties` (nested sub-entities).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "entityRelations": {
                "display_name": "Entity Relations",
                "desc": "Relationships between extracted entities (parent/"
                    "child IDs plus a `relation` string).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "textChanges": {
                "display_name": "Text Changes",
                "desc": "Edits applied to the OCR text by post-processing "
                    "(e.g. translation, redaction) — list of "
                    "`{textAnchor, changedText, provenance}` objects.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "textStyles": {
                "display_name": "Text Styles",
                "desc": "Font + colour styling info per text span.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "uri": {
                "display_name": "Source URI",
                "desc": "GCS URI of the source document (when "
                    "`gcs_uri` was used for submission).",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() {
        map addField($1), OpenApi3::OpenApi3HashDataType::forResponse(GoogleDocumentAiSchema::getSchema()
            .getTypeForReference("GoogleCloudDocumentaiV1Document")).getFields().iterator();
    }
}

public class GoogleDocumentAiProcessDocumentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "document": {
                "display_name": "Document",
                "desc": "The parsed "
                    "@ref GoogleDocumentAiDataProvider::GoogleDocumentAiDocumentDataType "
                    "\"GoogleDocumentAiDocument\" — contains `text`, `pages`, "
                    "`entities`, `mimeType`, and processor-specific extras.",
                "type": AbstractDataProviderTypeMap."hash",
            },
            "humanReviewStatus": {
                "display_name": "Human Review Status",
                "desc": "Status of any configured Human-in-the-Loop review.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() {
        map addField($1), OpenApi3::OpenApi3HashDataType::forResponse(GoogleDocumentAiSchema::getSchema()
            .getTypeForReference("GoogleCloudDocumentaiV1ProcessResponse")).getFields().iterator();
    }
}
}
