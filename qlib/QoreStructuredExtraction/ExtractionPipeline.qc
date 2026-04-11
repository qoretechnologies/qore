# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ExtractionPipeline.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ExtractionPipeline
public namespace QoreStructuredExtraction {

#! Options for ExtractionPipeline
public hashdecl ExtractionPipelineOptions {
    #! Maximum number of retry attempts on validation failure; default 2
    int max_retries = 2;
    #! Custom system prompt override (default is auto-generated from schema)
    *string system_prompt;
    #! Custom user prompt template; use {text} as placeholder for input text
    *string user_prompt_template;
}

#! LLM-based structured information extraction engine
/** Extracts typed structured data from unstructured text using an LLM backend
    with JSON schema enforcement.  The pipeline:

    1. Builds a system prompt with JSON schema constraints from the extraction schema
    2. Calls the LLM via @ref QoreLlmUtils::LlmBackend "LlmBackend"
    3. Parses the JSON response
    4. Validates the result against the schema
    5. Retries on failure (up to \c max_retries)

    @par Example: NER extraction
    @code{.py}
LlmBackend llm(<LlmBackendConfig>{"mode": "http", "url": "http://localhost:8080",
    "model": "gpt-4o"});
ExtractionPipeline pipeline(llm, ExtractionSchema::NerSchema);
hash<ExtractionResult> result = pipeline.extractData("Apple Inc. was founded by Steve Jobs.");
    @endcode

    @since QoreStructuredExtraction 1.0
*/
public class ExtractionPipeline {
    private {
        #! The LLM backend for making completion calls
        LlmBackend llm;
        #! The extraction schema defining what to extract
        hash<ExtractionSchemaInfo> schema;
        #! The JSON schema derived from the extraction schema
        hash<auto> json_schema;
        #! The system prompt for extraction
        string system_prompt;
        #! Custom user prompt template
        *string user_prompt_template;
        #! Maximum retries
        int max_retries;
    }

    #! Creates an ExtractionPipeline
    /** @param llm_backend the LLM backend to use for completion calls
        @param extraction_schema the schema defining what to extract
        @param options optional pipeline configuration

        @throw EXTRACTION-PIPELINE-ERROR if the schema has no fields
    */
    constructor(LlmBackend llm_backend, hash<ExtractionSchemaInfo> extraction_schema,
            *hash<ExtractionPipelineOptions> options) {
        if (!extraction_schema.fields) {
            throw "EXTRACTION-PIPELINE-ERROR", "extraction schema must have at least one field";
        }

        llm = llm_backend;
        schema = extraction_schema;
        json_schema = ExtractionSchema::toJsonSchema(schema);
        max_retries = options.max_retries ?? 2;
        user_prompt_template = options.user_prompt_template;

        if (options.system_prompt.val()) {
            system_prompt = options.system_prompt;
        } else {
            system_prompt = buildSystemPrompt();
        }
    }

    #! Extracts structured data from the given text
    /** @param text the unstructured text to extract from
        @return an @ref ExtractionResult hash with extracted data, validation status,
                and usage information

        @throw EXTRACTION-ERROR if all retry attempts fail
    */
    hash<ExtractionResult> extractData(string text) {
        string user_prompt = buildUserPrompt(text);
        hash<auto> total_usage = {};
        int attempts = 0;
        list<string> last_errors = ();

        while (attempts <= max_retries) {
            ++attempts;

            # Add retry context to the prompt if this is a retry
            string effective_prompt = user_prompt;
            if (attempts > 1 && last_errors) {
                effective_prompt += sprintf("\n\nPrevious attempt had validation errors: %s\n"
                    "Please fix these issues in your response.", last_errors.join("; "));
            }

            hash<LlmCompletionResult> llm_result = llm.complete(system_prompt,
                effective_prompt, json_schema);

            # Accumulate usage
            total_usage = mergeUsage(total_usage, llm_result.usage);

            # Parse JSON response
            hash<auto> parsed;
            try {
                parsed = parseResponse(llm_result.content);
            } catch (hash<ExceptionInfo> ex) {
                last_errors = (sprintf("JSON parse error: %s", ex.desc),);
                if (attempts > max_retries) {
                    return <ExtractionResult>{
                        "extracted": {},
                        "valid": False,
                        "validation_errors": last_errors,
                        "usage": total_usage,
                        "attempt_count": attempts,
                    };
                }
                continue;
            }

            # Validate against schema
            list<string> errors = validateExtracted(parsed);
            if (!errors) {
                return <ExtractionResult>{
                    "extracted": parsed,
                    "valid": True,
                    "validation_errors": (),
                    "usage": total_usage,
                    "attempt_count": attempts,
                };
            }

            last_errors = errors;
            if (attempts > max_retries) {
                return <ExtractionResult>{
                    "extracted": parsed,
                    "valid": False,
                    "validation_errors": errors,
                    "usage": total_usage,
                    "attempt_count": attempts,
                };
            }
        }

        # Unreachable, but satisfy the compiler
        throw "EXTRACTION-ERROR", "extraction failed after all retry attempts";
    }

    #! Extracts structured data from a batch of texts
    /** @param texts list of texts to extract from
        @return list of @ref ExtractionResult hashes, one per input text
    */
    list<hash<ExtractionResult>> extractBatch(list<string> texts) {
        list<hash<ExtractionResult>> results = ();
        foreach string text in (texts) {
            push results, extractData(text);
        }
        return results;
    }

    #! Returns the extraction schema
    hash<ExtractionSchemaInfo> getSchema() {
        return schema;
    }

    #! Returns the JSON schema used for LLM calls
    hash<auto> getJsonSchema() {
        return json_schema;
    }

    #! Builds the default system prompt from the schema
    private string buildSystemPrompt() {
        string prompt = "You are a structured information extraction engine. "
            "Extract information from the provided text according to the following schema.\n\n";

        prompt += sprintf("Task: %s\n\n", schema.description);
        prompt += "Output format: Respond with a JSON object containing the following fields:\n\n";

        foreach hash<ExtractionFieldDef> field in (schema.fields) {
            string req = field.required ? "REQUIRED" : "optional";
            prompt += sprintf("- \"%s\" (%s, %s): %s\n", field.name, field.type, req,
                field.description);
            if (field.enum_values) {
                prompt += sprintf("  Allowed values: %s\n",
                    (map sprintf("\"%s\"", $1), field.enum_values).join(", "));
            }
        }

        prompt += "\nRules:\n"
            "- Extract only information explicitly present in the text\n"
            "- Use null for optional fields when the information is not found\n"
            "- Do not invent or hallucinate data\n"
            "- Respond ONLY with the JSON object, no additional text\n";

        return prompt;
    }

    #! Builds the user prompt for a given text
    private string buildUserPrompt(string text) {
        if (user_prompt_template.val()) {
            return regex_subst(user_prompt_template, "\\{text\\}", text);
        }
        return sprintf("Extract structured data from the following text:\n\n---\n%s\n---", text);
    }

    #! Parses the LLM response as JSON
    private hash<auto> parseResponse(string content) {
        # Strip markdown code fences if present
        string cleaned = content;
        if (cleaned =~ /^```/) {
            # Remove opening fence (with optional language tag)
            cleaned = regex_subst(cleaned, "^```[a-zA-Z]*\\s*\n?", "");
            # Remove closing fence
            cleaned = regex_subst(cleaned, "\n?```\\s*$", "");
        }
        cleaned = trim(cleaned);

        auto parsed = parse_json(cleaned);
        if (parsed.typeCode() != NT_HASH) {
            throw "EXTRACTION-PARSE-ERROR",
                sprintf("expected JSON object but got %s", parsed.type());
        }
        return parsed;
    }

    #! Validates extracted data against the schema
    /** @return list of error messages (empty if valid)
    */
    private list<string> validateExtracted(hash<auto> data) {
        list<string> errors = ();

        foreach hash<ExtractionFieldDef> field in (schema.fields) {
            auto val = data{field.name};

            if (field.required && !exists val) {
                push errors, sprintf("required field \"%s\" is missing", field.name);
                continue;
            }

            if (!exists val) {
                continue;
            }

            # Type validation
            list<string> type_errors = validateFieldType(field.name, val, field.type);
            if (type_errors) {
                errors += type_errors;
                continue;
            }

            # Enum validation
            if (field.enum_values && val.typeCode() == NT_STRING) {
                if (!inlist(val, field.enum_values)) {
                    push errors, sprintf("field \"%s\" value %y is not one of: %s",
                        field.name, val,
                        (map sprintf("\"%s\"", $1), field.enum_values).join(", "));
                }
            }

            # Array item validation
            if (field.type == "array" && field.items && val.typeCode() == NT_LIST) {
                list<string> item_errors = validateArrayItems(field.name, val, field.items);
                if (item_errors) {
                    errors += item_errors;
                }
            }
        }

        return errors;
    }

    #! Validates the type of a field value
    private list<string> validateFieldType(string field_name, auto val, string expected_type) {
        list<string> errors = ();

        switch (expected_type) {
            case "string":
                if (val.typeCode() != NT_STRING) {
                    push errors, sprintf("field \"%s\" expected string but got %s",
                        field_name, val.type());
                }
                break;
            case "number":
                if (val.typeCode() != NT_FLOAT && val.typeCode() != NT_INT
                        && val.typeCode() != NT_NUMBER) {
                    push errors, sprintf("field \"%s\" expected number but got %s",
                        field_name, val.type());
                }
                break;
            case "integer":
                if (val.typeCode() != NT_INT) {
                    push errors, sprintf("field \"%s\" expected integer but got %s",
                        field_name, val.type());
                }
                break;
            case "boolean":
                if (val.typeCode() != NT_BOOLEAN) {
                    push errors, sprintf("field \"%s\" expected boolean but got %s",
                        field_name, val.type());
                }
                break;
            case "array":
                if (val.typeCode() != NT_LIST) {
                    push errors, sprintf("field \"%s\" expected array but got %s",
                        field_name, val.type());
                }
                break;
            case "object":
                if (val.typeCode() != NT_HASH) {
                    push errors, sprintf("field \"%s\" expected object but got %s",
                        field_name, val.type());
                }
                break;
        }

        return errors;
    }

    #! Validates array items against their schema
    private list<string> validateArrayItems(string field_name, list<auto> items,
            hash<auto> item_schema) {
        list<string> errors = ();

        if (item_schema.type == "object" && item_schema.required) {
            int idx = 0;
            foreach auto item in (items) {
                if (item.typeCode() == NT_HASH) {
                    foreach string req_field in (item_schema.required) {
                        if (!exists item{req_field}) {
                            push errors, sprintf("field \"%s[%d]\" is missing required "
                                "property \"%s\"", field_name, idx, req_field);
                        }
                    }
                } else {
                    push errors, sprintf("field \"%s[%d]\" expected object but got %s",
                        field_name, idx, item.type());
                }
                ++idx;
            }
        }

        return errors;
    }

    #! Merges usage hashes (sums numeric values)
    private hash<auto> mergeUsage(hash<auto> existing, hash<auto> new_usage) {
        if (!existing) {
            return new_usage;
        }
        hash<auto> merged = {} + existing;
        foreach string key in (keys new_usage) {
            if (merged.hasKey(key) && merged{key}.typeCode() == NT_INT
                    && new_usage{key}.typeCode() == NT_INT) {
                merged{key} = merged{key} + new_usage{key};
            } else {
                merged{key} = new_usage{key};
            }
        }
        return merged;
    }
}

} # namespace QoreStructuredExtraction
