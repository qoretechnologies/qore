# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsTextractDataProvider request/response data type definitions

/** AwsTextractDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AwsTextractDataProvider {
#! Typed sub-hash for a single Textract Block element
/** Textract returns text + structure as a flat list of typed Block
    objects.  This hashdecl captures the common Block fields documented
    in the [AWS Textract API reference](https://docs.aws.amazon.com/textract/latest/dg/API_Block.html).
    Block-type-specific fields (Query / Page / etc.) flow through via
    `auto`-typed sub-fields so downstream consumers can access them
    without losing the documented common shape.
*/
public class AwsTextractBlockDataType inherits HashDataType {
    public {
        const Fields = {
            "BlockType": {
                "display_name": "Block Type",
                "desc": "Block type: `PAGE`, `LINE`, `WORD`, `TABLE`, `CELL`, "
                    "`KEY_VALUE_SET`, `SELECTION_ELEMENT`, `QUERY`, "
                    "`QUERY_RESULT`, `SIGNATURE`, `LAYOUT_TITLE`, "
                    "`LAYOUT_HEADER`, `LAYOUT_TEXT`, `LAYOUT_FIGURE`, etc.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Id": {
                "display_name": "Block ID",
                "desc": "Textract-assigned block identifier (UUID).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Text": {
                "display_name": "Text",
                "desc": "Extracted text content (for `WORD`, `LINE`, "
                    "`LAYOUT_*`, `QUERY_RESULT` blocks).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Confidence": {
                "display_name": "Confidence",
                "desc": "OCR / detection confidence (0–100).",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "Page": {
                "display_name": "Page Number",
                "desc": "1-indexed page number this block belongs to.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "Geometry": {
                "display_name": "Geometry",
                "desc": "Bounding box + polygon vertices in document-relative "
                    "(0–1) coordinates.  Has sub-keys `BoundingBox` "
                    "(`Width`, `Height`, `Left`, `Top`) and `Polygon` "
                    "(list of `{X, Y}` vertices).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "Relationships": {
                "display_name": "Relationships",
                "desc": "List of `{Type, Ids}` relationship objects linking "
                    "this block to children (`CHILD`), keys/values (`KEY` / "
                    "`VALUE`), answers (`ANSWER`), or merged cells (`MERGED_CELL`).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "TextType": {
                "display_name": "Text Type",
                "desc": "For `WORD` blocks: `PRINTED` or `HANDWRITING`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "RowIndex": {
                "display_name": "Row Index",
                "desc": "For `CELL` blocks: 1-indexed row position.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "ColumnIndex": {
                "display_name": "Column Index",
                "desc": "For `CELL` blocks: 1-indexed column position.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "RowSpan": {
                "display_name": "Row Span",
                "desc": "For `CELL` blocks: how many rows this cell spans.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "ColumnSpan": {
                "display_name": "Column Span",
                "desc": "For `CELL` blocks: how many columns this cell spans.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "SelectionStatus": {
                "display_name": "Selection Status",
                "desc": "For `SELECTION_ELEMENT` blocks: `SELECTED` or "
                    "`NOT_SELECTED`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Query": {
                "display_name": "Query",
                "desc": "For `QUERY` blocks: the submitted query "
                    "(`{Text, Alias}`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "EntityTypes": {
                "display_name": "Entity Types",
                "desc": "For `KEY_VALUE_SET` blocks: list of `KEY` or `VALUE`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Typed sub-hash for Textract DocumentMetadata
public class AwsTextractDocumentMetadataDataType inherits HashDataType {
    public {
        const Fields = {
            "Pages": {
                "display_name": "Pages",
                "desc": "Number of pages in the analyzed document.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Detect-document-text request: extract raw text from a document
public class AwsTextractDetectDocumentTextRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "document_bytes": {
                "display_name": "Document Bytes",
                "short_desc": "Raw bytes of the document (PNG / JPEG / PDF / TIFF)",
                "type": AbstractDataProviderTypeMap."*binary",
                "desc": "Raw bytes of the document.  Supported formats: PNG, JPEG, "
                    "PDF, TIFF.  Maximum 10 MB for sync calls.  Mutually exclusive "
                    "with `document_path` and the `s3_*` fields.",
            },
            "document_path": {
                "display_name": "Document Path",
                "short_desc": "Local file path to read and upload as bytes",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Local file path.  The file is read and uploaded as "
                    "`Bytes`.  Mutually exclusive with `document_bytes` and "
                    "the `s3_*` fields.",
            },
            "s3_bucket": {
                "display_name": "S3 Bucket",
                "short_desc": "S3 bucket containing the document",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Name of the S3 bucket containing the document.  Use "
                    "together with `s3_name`.  Mutually exclusive with "
                    "`document_bytes` and `document_path`.",
            },
            "s3_name": {
                "display_name": "S3 Object Name",
                "short_desc": "S3 object key (path within the bucket)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "S3 object key (path within the bucket).",
            },
            "s3_version": {
                "display_name": "S3 Object Version",
                "short_desc": "Optional S3 object version ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional S3 object version ID for versioned buckets.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Detect-document-text response: Textract DetectDocumentText output
public class AwsTextractDetectDocumentTextResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "DocumentMetadata": {
                "display_name": "Document Metadata",
                "desc": "Document-level metadata (`Pages` count).",
                "type": new AwsTextractDocumentMetadataDataType(),
            },
            "Blocks": {
                "display_name": "Blocks",
                "desc": "Extracted text blocks — each is an "
                    "@ref AwsTextractDataProvider::AwsTextractBlockDataType "
                    "\"AwsTextractBlock\" with `BlockType`, `Text`, "
                    "`Confidence`, `Geometry`, and relationship IDs.",
                "type": new ListDataType("AwsTextractBlockList",
                    new AwsTextractBlockDataType(), True),
            },
            "DetectDocumentTextModelVersion": {
                "display_name": "Model Version",
                "desc": "Textract model version that produced this result.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Analyze-document request: extract text + structure (tables, forms, etc.)
public class AwsTextractAnalyzeDocumentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "feature_types": {
                "display_name": "Feature Types",
                "short_desc": "Which structural features to extract (list)",
                "type": new ListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "Which structural features to extract.  Pass at least one.",
                "required": True,
                "example_value": ("TABLES", "FORMS"),
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "TABLES",     "display_name": "Tables"},
                    <AllowedValueInfo>{"value": "FORMS",      "display_name": "Forms (key/value pairs)"},
                    <AllowedValueInfo>{"value": "SIGNATURES", "display_name": "Signatures"},
                    <AllowedValueInfo>{"value": "LAYOUT",     "display_name": "Layout (titles, headers, paragraphs)"},
                    <AllowedValueInfo>{"value": "QUERIES",    "display_name": "Queries (ask questions of the document)"},
                ),
            },
            "queries": {
                "display_name": "Queries",
                "short_desc": "Custom queries to ask of the document (with `QUERIES` feature)",
                "type": AbstractDataProviderTypeMap."*softlist",
                "desc": "List of query objects when `QUERIES` is in `feature_types`.  "
                    "Each query has a `Text` field (the question to ask) plus an "
                    "optional `Alias` and `Pages` filter.\n\n"
                    "**Example**: `[{\"Text\": \"What is the invoice total?\", "
                    "\"Alias\": \"total\"}]`",
            },
            "document_bytes": {
                "display_name": "Document Bytes",
                "short_desc": "Raw bytes of the document (PNG / JPEG / PDF / TIFF)",
                "type": AbstractDataProviderTypeMap."*binary",
                "desc": "Raw bytes of the document.  See "
                    "`AwsTextractDetectDocumentTextRequestDataType.document_bytes` "
                    "for source-selection rules.",
            },
            "document_path": {
                "display_name": "Document Path",
                "short_desc": "Local file path to read and upload as bytes",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Local file path.  The file is read and uploaded as `Bytes`.",
            },
            "s3_bucket": {
                "display_name": "S3 Bucket",
                "short_desc": "S3 bucket containing the document",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Name of the S3 bucket containing the document.",
            },
            "s3_name": {
                "display_name": "S3 Object Name",
                "short_desc": "S3 object key",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "S3 object key (path within the bucket).",
            },
            "s3_version": {
                "display_name": "S3 Object Version",
                "short_desc": "Optional S3 object version ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional S3 object version ID for versioned buckets.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Analyze-document response: Textract AnalyzeDocument output
public class AwsTextractAnalyzeDocumentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "DocumentMetadata": {
                "display_name": "Document Metadata",
                "desc": "Document-level metadata (`Pages` count).",
                "type": new AwsTextractDocumentMetadataDataType(),
            },
            "Blocks": {
                "display_name": "Blocks",
                "desc": "Extracted blocks — types include `PAGE`, `LINE`, "
                    "`WORD`, `TABLE`, `CELL`, `KEY_VALUE_SET`, `SELECTION_ELEMENT`, "
                    "`QUERY`, `QUERY_RESULT`, `SIGNATURE`, `LAYOUT_TEXT`, "
                    "and other `LAYOUT_*` types depending on the `feature_types` "
                    "requested.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "AnalyzeDocumentModelVersion": {
                "display_name": "Model Version",
                "desc": "Textract model version that produced this result.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
