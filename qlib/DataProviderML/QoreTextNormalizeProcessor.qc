# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreTextNormalizeProcessor class definition

/** QoreTextNormalizeProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that normalizes text fields by applying selected operations
/** Applies a sequence of text normalization operations (lowercasing, HTML stripping,
    whitespace collapsing, punctuation removal, Unicode NFKC normalization) to a
    specified text field. The original record is enriched with a new field containing
    the normalized text.

    @par Example:
    @code{.py}
QoreTextNormalizeProcessor proc({
    "text_field": "body",
    "operations": ("strip_html", "collapse_whitespace", "lowercase"),
});
    @endcode

*/
public class QoreTextNormalizeProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreTextNormalizeProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Which text field to normalize",
                "desc": "The name of the text field in each record to normalize. The normalized "
                    "result is stored in a new field named `{text_field}_normalized`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "input_field_names": True,
            },
            "operations": <DataProviderOptionInfo>{
                "display_name": "Normalization Operations",
                "short_desc": "Operations to apply in order",
                "desc": "A list of normalization operations to apply to the text field, in the "
                    "order specified. Available operations:\n\n"
                    "- `lowercase` — convert text to lowercase\n"
                    "- `strip_html` — remove HTML tags and decode common HTML entities\n"
                    "- `collapse_whitespace` — collapse runs of whitespace to a single space "
                    "and trim leading/trailing whitespace\n"
                    "- `strip_punctuation` — remove all non-word, non-space characters\n"
                    "- `unicode_nfkc` — Unicode NFKC normalization (placeholder, pass-through "
                    "for now)",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "lowercase",
                        "display_name": "Lowercase",
                        "desc": "Convert all characters to lowercase",
                    },
                    <AllowedValueInfo>{
                        "value": "strip_html",
                        "display_name": "Strip HTML",
                        "desc": "Remove HTML tags and decode common HTML entities "
                            "(&amp;, &lt;, &gt;, &quot;)",
                    },
                    <AllowedValueInfo>{
                        "value": "collapse_whitespace",
                        "display_name": "Collapse Whitespace",
                        "desc": "Collapse runs of whitespace to a single space and trim",
                    },
                    <AllowedValueInfo>{
                        "value": "strip_punctuation",
                        "display_name": "Strip Punctuation",
                        "desc": "Remove all non-word, non-space characters",
                    },
                    <AllowedValueInfo>{
                        "value": "unicode_nfkc",
                        "display_name": "Unicode NFKC",
                        "desc": "Apply Unicode NFKC normalization (placeholder, pass-through "
                            "for now)",
                    },
                ),
            },
        };

        #! Valid operation names
        const ValidOperations = {
            "lowercase": True,
            "strip_html": True,
            "collapse_whitespace": True,
            "strip_punctuation": True,
            "unicode_nfkc": True,
        };
    }

    private {
        #! Configured text field name
        string text_field;

        #! Ordered list of operations to apply
        list<string> operations;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        text_field = copts.text_field;
        if (!text_field.val()) {
            throw "CONSTRUCTOR-ERROR", "text_field option is required";
        }
        operations = copts.operations;
        if (!operations) {
            throw "CONSTRUCTOR-ERROR", "at least one operation must be specified";
        }
        foreach string op in (operations) {
            if (!ValidOperations{op}) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("invalid operation %y; valid operations: %y", op,
                        keys ValidOperations);
            }
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "text-normalize";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Cleans up messy text by applying selected operations in order — for example, "
            "stripping HTML from web-scraped content, lowercasing for consistent search, or "
            "collapsing extra whitespace in imported data; adds a cleaned field to each record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: applies normalization operations and enriches the record
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreTextNormalizeProcessor: not configured; provide constructor options";
        }

        # start with the original record
        hash<auto> result = {} + record;

        string text =
            DataProvider::AbstractDataProvider::getFieldValue(record, text_field).toString();

        foreach string op in (operations) {
            text = applyOperation(op, text);
        }

        result{text_field + "_normalized"} = text;

        return result;
    }

    #! Applies a single normalization operation to the given text
    private string applyOperation(string op, string text) {
        switch (op) {
            case "lowercase":
                return text.lwr();

            case "strip_html": {
                # Remove HTML tags
                string result = regex_subst(text, "<[^>]*>", "", RE_Global);
                # Decode common HTML entities
                result = regex_subst(result, "&amp;", "&", RE_Global);
                result = regex_subst(result, "&lt;", "<", RE_Global);
                result = regex_subst(result, "&gt;", ">", RE_Global);
                result = regex_subst(result, "&quot;", "\"", RE_Global);
                return result;
            }

            case "collapse_whitespace":
                return trim(regex_subst(text, "\\s+", " ", RE_Global));

            case "strip_punctuation":
                return regex_subst(text, "[^\\w\\s]", "", RE_Global);

            case "unicode_nfkc":
                # Placeholder — pass-through for now
                return text;
        }
        return text;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with normalized field)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
