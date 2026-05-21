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
#! Typed sub-hash for a single Azure DI Page
/** Covers the common top-level fields of an Azure Document
    Intelligence `Page` object as documented in the
    [analyzeResult.pages schema](https://learn.microsoft.com/en-us/rest/api/aiservices/document-models/analyze-document).
    Container fields (`lines`, `words`, `selectionMarks`, `spans`,
    `barcodes`, `formulas`) flow through as untyped lists so downstream
    consumers can access any field the service returns.
*/
public class AzureDocIntelPageDataType inherits HashDataType {
    public {
        const Fields = {
            "pageNumber": {
                "display_name": "Page Number",
                "desc": "1-indexed page number.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "angle": {
                "display_name": "Angle",
                "desc": "Rotation angle of the page in degrees (–180 to 180).",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "width": {
                "display_name": "Width",
                "desc": "Page width in `unit` units.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "height": {
                "display_name": "Height",
                "desc": "Page height in `unit` units.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "unit": {
                "display_name": "Unit",
                "desc": "Unit of measure for `width` / `height` — `pixel` "
                    "or `inch`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "lines": {
                "display_name": "Lines",
                "desc": "List of text-line objects with `content`, `polygon`, "
                    "and `spans`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "words": {
                "display_name": "Words",
                "desc": "List of word objects with `content`, `polygon`, "
                    "`confidence`, and `span`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "selectionMarks": {
                "display_name": "Selection Marks",
                "desc": "List of selection-mark objects (checkboxes / radio "
                    "buttons) with `state` (`selected`/`unselected`), "
                    "`polygon`, and `confidence`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "spans": {
                "display_name": "Spans",
                "desc": "Text spans on this page — list of `{offset, length}` "
                    "objects pointing into the top-level `content` string.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Typed sub-hash for a single Azure DI typed-entity Document
/** Each prebuilt model returns one or more `Document` objects, each
    with a model-specific schema in `fields`.
*/
public class AzureDocIntelDocumentDataType inherits HashDataType {
    public {
        const Fields = {
            "docType": {
                "display_name": "Document Type",
                "desc": "The doctype the model classified — e.g. "
                    "`invoice`, `receipt.retailMeal`, `idDocument.passport`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "fields": {
                "display_name": "Fields",
                "desc": "Typed-entity fields specific to the model — e.g. "
                    "for `prebuilt-invoice`, `VendorName`, `InvoiceTotal`, "
                    "`InvoiceDate`, `Items` (list of line items).  Each value "
                    "is a `Field` object with `type`, `content`, "
                    "`confidence`, and a value-specific key "
                    "(`valueString` / `valueNumber` / `valueDate` / "
                    "`valueArray` / `valueObject`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "confidence": {
                "display_name": "Confidence",
                "desc": "Overall confidence the document matches its `docType`.",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "boundingRegions": {
                "display_name": "Bounding Regions",
                "desc": "List of `{pageNumber, polygon}` regions covering "
                    "the typed-entity document in the source.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "spans": {
                "display_name": "Spans",
                "desc": "Text spans covered by this document — list of "
                    "`{offset, length}` objects pointing into the top-level "
                    "`content` string.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class AzureDocumentIntelligenceAnalyzeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model_id": {
                "display_name": "Model ID",
                "short_desc": "Document Intelligence model identifier",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Document Intelligence model identifier — prebuilt or "
                    "custom.  The dropdown lists the documented prebuilt "
                    "models; custom-model GUIDs trained in the "
                    "[Azure portal](https://portal.azure.com) can be entered "
                    "as free-form text (`allowed_values_creatable: True`).",
                "required": True,
                "default_value": "prebuilt-document",
                "example_value": "prebuilt-invoice",
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "prebuilt-document",            "display_name": "Document (general layout)"},
                    <AllowedValueInfo>{"value": "prebuilt-layout",              "display_name": "Layout (tables + lines)"},
                    <AllowedValueInfo>{"value": "prebuilt-invoice",             "display_name": "Invoice"},
                    <AllowedValueInfo>{"value": "prebuilt-receipt",             "display_name": "Receipt"},
                    <AllowedValueInfo>{"value": "prebuilt-idDocument",          "display_name": "ID Document"},
                    <AllowedValueInfo>{"value": "prebuilt-businessCard",        "display_name": "Business Card"},
                    <AllowedValueInfo>{"value": "prebuilt-tax.us.w2",           "display_name": "US W-2 Tax Form"},
                    <AllowedValueInfo>{"value": "prebuilt-tax.us.1098",         "display_name": "US 1098 Tax Form"},
                    <AllowedValueInfo>{"value": "prebuilt-contract",            "display_name": "Contract"},
                    <AllowedValueInfo>{"value": "prebuilt-healthInsuranceCard.us", "display_name": "US Health Insurance Card"},
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
                "desc": "Per-page layout — each entry is an "
                    "@ref AzureDocumentIntelligenceDataProvider::AzureDocIntelPageDataType "
                    "\"AzureDocIntelPage\" with lines, words, selection marks, "
                    "spans, and page dimensions.",
                "type": new ListDataType("AzureDocIntelPageList",
                    new AzureDocIntelPageDataType(), True),
            },
            "tables": {
                "display_name": "Tables",
                "desc": "Extracted tables — each entry has `rowCount`, "
                    "`columnCount`, `cells` (list of per-cell objects with "
                    "`kind`, `rowIndex`, `columnIndex`, `content`, "
                    "`boundingRegions`).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "keyValuePairs": {
                "display_name": "Key/Value Pairs",
                "desc": "Detected key/value pairs (with the `prebuilt-document` "
                    "model) — list of `{key, value, confidence}` objects where "
                    "`key` and `value` each have `content` + `boundingRegions`.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "documents": {
                "display_name": "Documents",
                "desc": "Typed-entity documents (with prebuilt-invoice / "
                    "receipt / idDocument / W-2 / etc.) — each entry is an "
                    "@ref AzureDocumentIntelligenceDataProvider::AzureDocIntelDocumentDataType "
                    "\"AzureDocIntelDocument\" with `docType`, `fields`, and "
                    "`confidence`.",
                "type": new ListDataType("AzureDocIntelDocumentList",
                    new AzureDocIntelDocumentDataType(), True),
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
