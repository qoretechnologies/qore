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
#! Shared document-source fields used by both detect-document-text and analyze-document
public hashdecl AwsTextractDocumentSourceFields {
    *binary document_bytes;
    *string document_path;
    *string s3_bucket;
    *string s3_name;
    *string s3_version;
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
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "Blocks": {
                "display_name": "Blocks",
                "desc": "Extracted text blocks — each has `BlockType` "
                    "(`PAGE`, `LINE`, `WORD`), `Text`, `Confidence`, `Geometry` "
                    "(bounding box), and ID references.",
                "type": AbstractDataProviderTypeMap."*softlist",
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
                "type": AbstractDataProviderTypeMap."softlist",
                "desc": "Which structural features to extract.  One or more of "
                    "`TABLES`, `FORMS`, `SIGNATURES`, `LAYOUT`, `QUERIES`.  "
                    "Pass at least one.",
                "required": True,
                "example_value": ("TABLES", "FORMS"),
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
                "type": AbstractDataProviderTypeMap."*hash",
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
