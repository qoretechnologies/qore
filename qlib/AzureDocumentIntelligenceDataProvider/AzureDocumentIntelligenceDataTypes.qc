# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AzureDocumentIntelligenceDataProvider data type definitions

/** AzureDocumentIntelligenceDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AzureDocumentIntelligenceDataProvider {
public class AzureDocumentIntelligenceAnalyzeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "Document Intelligence model identifier",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Document Intelligence model identifier — prebuilt or "
                    "custom.  Common prebuilt models: `prebuilt-document` "
                    "(general document layout), `prebuilt-layout` (layout + "
                    "tables), `prebuilt-invoice`, `prebuilt-receipt`, "
                    "`prebuilt-idDocument`, `prebuilt-businessCard`, "
                    "`prebuilt-tax.us.w2`, `prebuilt-tax.us.1098`, "
                    "`prebuilt-contract`, `prebuilt-healthInsuranceCard.us`.  "
                    "Custom models use their GUID identifier.",
                "required": True,
                "default_value": "prebuilt-document",
                "example_value": "prebuilt-invoice",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "prebuilt-document",  "display_name": "Document (general layout)"},
                    <AllowedValueInfo>{"value": "prebuilt-layout",    "display_name": "Layout (tables + lines)"},
                    <AllowedValueInfo>{"value": "prebuilt-invoice",   "display_name": "Invoice"},
                    <AllowedValueInfo>{"value": "prebuilt-receipt",   "display_name": "Receipt"},
                    <AllowedValueInfo>{"value": "prebuilt-idDocument", "display_name": "ID Document"},
                    <AllowedValueInfo>{"value": "prebuilt-businessCard", "display_name": "Business Card"},
                    <AllowedValueInfo>{"value": "prebuilt-tax.us.w2", "display_name": "US W-2 Tax Form"},
                    <AllowedValueInfo>{"value": "prebuilt-contract",  "display_name": "Contract"},
                ),
            },
            "document_url": {
                "display_name": "Document URL",
                "short_desc": "Public URL of the document to analyze",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Public URL of the document to analyze.  Azure fetches "
                    "the document from this URL.  Mutually exclusive with "
                    "`document_bytes` and `document_path`.",
                "example_value": "https://example.com/invoice.pdf",
            },
            "document_bytes": {
                "display_name": "Document Bytes",
                "short_desc": "Raw document bytes (base64-encoded automatically)",
                "type": AbstractDataProviderTypeMap."*binary",
                "desc": "Raw document bytes — sent as base64-encoded `base64Source`.",
            },
            "document_path": {
                "display_name": "Document Path",
                "short_desc": "Local file path to read and base64-encode",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Local file path to read.  Mutually exclusive with "
                    "`document_url` and `document_bytes`.",
            },
            "api_version": {
                "display_name": "API Version",
                "short_desc": "Document Intelligence API version",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Document Intelligence API version (e.g. `2024-11-30`).",
                "default_value": "2024-11-30",
            },
            "poll_interval": {
                "display_name": "Polling Interval",
                "short_desc": "Time between status polls",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "Interval between job-status polls.  Defaults to `2s`.",
            },
            "analysis_timeout": {
                "display_name": "Overall Analysis Timeout",
                "short_desc": "Maximum total wait time for the polling loop",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "Maximum total wait time.  Defaults to `10m`.  When "
                    "exceeded, the action raises `AZURE-DOCUMENT-INTELLIGENCE-ERROR`.",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class AzureDocumentIntelligenceAnalyzeResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "apiVersion": {
                "display_name": "API Version",
                "desc": "Document Intelligence API version that produced this result.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "modelId": {
                "display_name": "Model ID",
                "desc": "The Document Intelligence model that produced this result.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "desc": "Full extracted text content of the document.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "pages": {
                "display_name": "Pages",
                "desc": "Per-page layout — lines, words, spans, page dimensions.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tables": {
                "display_name": "Tables",
                "desc": "Extracted tables — row/column counts plus per-cell content.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "keyValuePairs": {
                "display_name": "Key/Value Pairs",
                "desc": "Detected key/value pairs (with the `prebuilt-document` model).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "documents": {
                "display_name": "Documents",
                "desc": "Typed-entity documents (with prebuilt-invoice / receipt / "
                    "idDocument / W-2 / etc.).  Each document has `docType`, "
                    "`fields` (typed `Field` objects), and `confidence`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "styles": {
                "display_name": "Styles",
                "desc": "Font + handwriting styling info per text span.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
