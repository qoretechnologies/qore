# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ExtractionSchema.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ExtractionSchema
public namespace QoreStructuredExtraction {

#! Defines a single field in an extraction schema
public hashdecl ExtractionFieldDef {
    #! Field name in the output
    string name;
    #! JSON schema type: "string", "number", "integer", "boolean", "array", "object"
    string type;
    #! Description of what this field should contain (used in the LLM prompt)
    string description;
    #! Whether this field is required in the output; default True
    bool required = True;
    #! For array fields: the item schema
    *hash<auto> items;
    #! For object fields: nested properties
    *hash<auto> properties;
    #! Enum constraint: list of allowed values
    *list<auto> enum_values;
}

#! Defines a complete extraction schema
public hashdecl ExtractionSchemaInfo {
    #! Schema name (used as tool/function name in LLM calls)
    string name;
    #! Human-readable description of what is being extracted
    string description;
    #! List of fields to extract
    list<hash<ExtractionFieldDef>> fields;
}

#! Result from an extraction operation
public hashdecl ExtractionResult {
    #! The extracted data (parsed JSON matching the schema)
    hash<auto> extracted;
    #! Whether validation passed
    bool valid;
    #! List of validation error messages (empty if valid)
    list<string> validation_errors;
    #! Token usage from the LLM call
    hash<auto> usage;
    #! Number of extraction attempts (1 = succeeded on first try)
    int attempt_count;
}

#! Provides pre-built schema templates for common data extraction tasks, plus utilities for converting schemas to JSON Schema format used by LLMs.
/** The built-in templates cover the most common business extraction needs:
    - \b NER: Find people, companies, locations, dates, and monetary amounts in text
    - \b Invoice: Extract invoice numbers, dates, vendors, line items, and totals
    - \b Contact: Extract names, emails, phone numbers, companies, and addresses
    - \b Sentiment: Analyze whether text is positive, negative, neutral, or mixed

    @since QoreStructuredExtraction 1.0
*/
public class ExtractionSchema {
    #! NER (Named Entity Recognition) schema template
    /** Extracts entities with type, text, start/end character offsets, and confidence.
    */
    public const NerSchema = <ExtractionSchemaInfo>{
        "name": "extract_entities",
        "description": "Extract named entities from the text with their types, "
            "positions, and confidence scores",
        "fields": (
            <ExtractionFieldDef>{
                "name": "entities",
                "type": "array",
                "description": "List of named entities found in the text",
                "required": True,
                "items": {
                    "type": "object",
                    "properties": {
                        "entity_type": {
                            "type": "string",
                            "description": "Entity type: PERSON, ORGANIZATION, LOCATION, "
                                "DATE, MONEY, PRODUCT, EVENT, or OTHER",
                        },
                        "text": {
                            "type": "string",
                            "description": "The entity text as it appears in the source",
                        },
                        "start": {
                            "type": "integer",
                            "description": "Start character offset in the source text",
                        },
                        "end": {
                            "type": "integer",
                            "description": "End character offset in the source text (exclusive)",
                        },
                        "confidence": {
                            "type": "number",
                            "description": "Confidence score between 0.0 and 1.0",
                        },
                    },
                    "required": ("entity_type", "text", "start", "end", "confidence"),
                },
            },
        ),
    };

    #! Invoice extraction schema template
    /** Extracts invoice number, date, vendor, total, currency, and line items.
    */
    public const InvoiceSchema = <ExtractionSchemaInfo>{
        "name": "extract_invoice",
        "description": "Extract structured invoice data including header fields and line items",
        "fields": (
            <ExtractionFieldDef>{
                "name": "invoice_number",
                "type": "string",
                "description": "The invoice number or identifier",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "invoice_date",
                "type": "string",
                "description": "Invoice date in ISO 8601 format (YYYY-MM-DD)",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "due_date",
                "type": "string",
                "description": "Payment due date in ISO 8601 format (YYYY-MM-DD)",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "vendor_name",
                "type": "string",
                "description": "Name of the vendor or seller",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "vendor_address",
                "type": "string",
                "description": "Full address of the vendor",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "customer_name",
                "type": "string",
                "description": "Name of the customer or buyer",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "currency",
                "type": "string",
                "description": "Three-letter ISO 4217 currency code (e.g., USD, EUR)",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "subtotal",
                "type": "number",
                "description": "Subtotal before tax",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "tax_amount",
                "type": "number",
                "description": "Total tax amount",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "total_amount",
                "type": "number",
                "description": "Total amount due including tax",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "line_items",
                "type": "array",
                "description": "Individual line items on the invoice",
                "required": True,
                "items": {
                    "type": "object",
                    "properties": {
                        "description": {
                            "type": "string",
                            "description": "Description of the item or service",
                        },
                        "quantity": {
                            "type": "number",
                            "description": "Quantity of items",
                        },
                        "unit_price": {
                            "type": "number",
                            "description": "Price per unit",
                        },
                        "amount": {
                            "type": "number",
                            "description": "Total amount for this line item",
                        },
                    },
                    "required": ("description", "amount"),
                },
            },
        ),
    };

    #! Contact information extraction schema template
    /** Extracts name, email, phone, company, title, and address.
    */
    public const ContactSchema = <ExtractionSchemaInfo>{
        "name": "extract_contact",
        "description": "Extract contact information from the text",
        "fields": (
            <ExtractionFieldDef>{
                "name": "full_name",
                "type": "string",
                "description": "Full name of the person",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "first_name",
                "type": "string",
                "description": "First/given name",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "last_name",
                "type": "string",
                "description": "Last/family name",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "email",
                "type": "string",
                "description": "Email address",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "phone",
                "type": "string",
                "description": "Phone number (any format)",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "company",
                "type": "string",
                "description": "Company or organization name",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "job_title",
                "type": "string",
                "description": "Job title or role",
                "required": False,
            },
            <ExtractionFieldDef>{
                "name": "address",
                "type": "string",
                "description": "Physical address",
                "required": False,
            },
        ),
    };

    #! Sentiment analysis extraction schema template
    /** Extracts overall sentiment, score, and per-aspect sentiments.
    */
    public const SentimentSchema = <ExtractionSchemaInfo>{
        "name": "extract_sentiment",
        "description": "Analyze the sentiment of the text with overall and per-aspect scores",
        "fields": (
            <ExtractionFieldDef>{
                "name": "overall_sentiment",
                "type": "string",
                "description": "Overall sentiment: POSITIVE, NEGATIVE, NEUTRAL, or MIXED",
                "required": True,
                "enum_values": ("POSITIVE", "NEGATIVE", "NEUTRAL", "MIXED"),
            },
            <ExtractionFieldDef>{
                "name": "sentiment_score",
                "type": "number",
                "description": "Overall sentiment score from -1.0 (very negative) to "
                    "1.0 (very positive)",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "confidence",
                "type": "number",
                "description": "Confidence in the overall sentiment, 0.0 to 1.0",
                "required": True,
            },
            <ExtractionFieldDef>{
                "name": "aspects",
                "type": "array",
                "description": "Per-aspect sentiment breakdown",
                "required": False,
                "items": {
                    "type": "object",
                    "properties": {
                        "aspect": {
                            "type": "string",
                            "description": "The aspect or topic being evaluated",
                        },
                        "sentiment": {
                            "type": "string",
                            "description": "Sentiment for this aspect: POSITIVE, NEGATIVE, "
                                "NEUTRAL, or MIXED",
                        },
                        "score": {
                            "type": "number",
                            "description": "Sentiment score for this aspect, -1.0 to 1.0",
                        },
                    },
                    "required": ("aspect", "sentiment", "score"),
                },
            },
            <ExtractionFieldDef>{
                "name": "key_phrases",
                "type": "array",
                "description": "Key phrases that drove the sentiment assessment",
                "required": False,
                "items": {
                    "type": "string",
                },
            },
        ),
    };

    #! Converts an ExtractionSchemaInfo to a JSON Schema object
    /** @param schema the extraction schema to convert
        @return a JSON Schema object suitable for LLM response_format or tool input_schema
    */
    static hash<auto> toJsonSchema(hash<ExtractionSchemaInfo> schema) {
        hash<auto!> properties();
        list<string> required_fields = ();

        foreach hash<ExtractionFieldDef> field in (schema.fields) {
            hash<auto!> prop();
            prop.type = field.type;
            prop.description = field.description;
            if (field.items) {
                prop.items = field.items;
            }
            if (field.properties) {
                prop.properties = field.properties;
            }
            if (field.enum_values) {
                prop."enum" = field.enum_values;
            }
            properties{field.name} = prop;
            if (field.required) {
                push required_fields, field.name;
            }
        }

        return {
            "type": "object",
            "name": schema.name,
            "description": schema.description,
            "properties": properties,
            "required": required_fields,
        };
    }

    #! Looks up a built-in schema template by name
    /** @param template_name one of "ner", "invoice", "contact", "sentiment"
        @return the corresponding schema template

        @throw EXTRACTION-SCHEMA-ERROR if template_name is not recognized
    */
    static hash<ExtractionSchemaInfo> getTemplate(string template_name) {
        switch (template_name.lwr()) {
            case "ner":
                return ExtractionSchema::NerSchema;
            case "invoice":
                return ExtractionSchema::InvoiceSchema;
            case "contact":
                return ExtractionSchema::ContactSchema;
            case "sentiment":
                return ExtractionSchema::SentimentSchema;
        }
        throw "EXTRACTION-SCHEMA-ERROR",
            sprintf("unknown template %y; expected one of: ner, invoice, contact, sentiment",
                template_name);
    }

    #! Returns the list of available template names
    static list<string> getTemplateNames() {
        return ("ner", "invoice", "contact", "sentiment");
    }
}

} # namespace QoreStructuredExtraction
