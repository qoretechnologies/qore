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
    #! Number of parallel extractions for extractBatch(); default 1 (sequential)
    int batch_parallel = 1;
    #! Optional thread-start callable used for parallel batch extraction
    /** Required when the enclosing @ref Qore::Program "Program" has
        @ref Qore::PO_NO_THREAD_CONTROL set and \c batch_parallel > 1; the
        callable must take a zero-arg code reference and return the integer
        thread ID.  When not set in a non-sandboxed program, Qore's
        @ref background operator is used directly.
    */
    *code<int(code<nothing()>)> start_thread;
}

#! LLM-based structured information extraction engine
/** Extracts typed structured data from text or images using an LLM with
    tool-based schema enforcement.  The pipeline:

    1. Builds messages with the extraction schema as a tool definition
    2. Calls the LLM via @ref QoreLlmUtils::LlmBackend::chat() "LlmBackend.chat()"
       with the tool, so the LLM is forced to return data matching the schema
    3. Parses and validates the result
    4. Retries on failure (up to \c max_retries)

    Supports both text and image inputs — for images, pass binary data
    and MIME type to \c extractFromImage().

    Few-shot examples in the schema improve extraction quality significantly.

    @par Example: NER extraction from text
    @code{.py}
%modern
%requires QoreStructuredExtraction

LlmBackend llm(<LlmBackendConfig>{"mode": "http", "url": "http://localhost:8080",
    "model": "gpt-4o"});
ExtractionPipeline pipeline(llm, ExtractionSchema::NerSchema);
hash<ExtractionResult> result = pipeline.extractData("Apple Inc. was founded by Steve Jobs.");
    @endcode

    @par Example: Extract from image
    @code{.py}
binary img = ReadOnlyFile::readBinaryFile("receipt.jpg");
hash<ExtractionResult> result = pipeline.extractFromImage(img, "image/jpeg");
    @endcode
*/
public class ExtractionPipeline {
    private {
        #! The LLM backend
        LlmBackend llm;
        #! The extraction schema
        hash<ExtractionSchemaInfo> schema;
        #! The JSON schema for tool definitions
        hash<auto> json_schema;
        #! Tool definition for chat-based extraction
        list<hash<QoreLlmUtils::ToolDefinition>> tools;
        #! The system prompt
        string system_prompt;
        #! Custom user prompt template
        *string user_prompt_template;
        #! Maximum retries
        int max_retries;
        #! Batch parallelism
        int batch_parallel;
        #! Optional thread-start callable for parallel batch extraction
        *code<int(code<nothing()>)> start_thread;
    }

    #! Creates an ExtractionPipeline
    /** @param llm_backend the LLM backend to use
        @param extraction_schema the schema defining what to extract; optionally
            includes few-shot examples for improved quality
        @param options optional pipeline configuration

        @throw EXTRACTION-PIPELINE-ERROR if the schema has no fields
        @throw EXTRACTION-ARG-ERROR if the pipeline is constructed in a program
            with @ref Qore::PO_NO_THREAD_CONTROL set and \c batch_parallel > 1
            but no \c start_thread callable is supplied
    */
    constructor(LlmBackend llm_backend, hash<ExtractionSchemaInfo> extraction_schema,
            *hash<ExtractionPipelineOptions> options) {
        if (!extraction_schema.fields) {
            throw "EXTRACTION-PIPELINE-ERROR",
                "extraction schema must have at least one field";
        }

        llm = llm_backend;
        schema = extraction_schema;
        json_schema = ExtractionSchema::toJsonSchema(schema);
        max_retries = options.max_retries ?? 2;
        user_prompt_template = options.user_prompt_template;
        batch_parallel = options.batch_parallel ?? 1;
        start_thread = options.start_thread;

%ifdef PO_NO_THREAD_CONTROL
        if (batch_parallel > 1 && !start_thread) {
            throw "EXTRACTION-ARG-ERROR",
                "ExtractionPipeline requires the \"start_thread\" option when "
                "\"batch_parallel\" > 1 in a program with PO_NO_THREAD_CONTROL "
                "set; see the QoreStructuredExtraction module documentation "
                "for sandbox integration details";
        }
%endif

        if (options.system_prompt.val()) {
            system_prompt = options.system_prompt;
        } else {
            system_prompt = buildSystemPrompt();
        }

        # Build tool definition for chat-based extraction
        tools = (
            <QoreLlmUtils::ToolDefinition>{
                "name": schema.name ?? "extract",
                "description": schema.description,
                "parameters": json_schema,
            },
        );
    }

    #! Extracts structured data from text
    /** @param text the unstructured text to extract from
        @return an @ref ExtractionResult hash

        @throw EXTRACTION-ERROR if all retry attempts fail
    */
    hash<ExtractionResult> extractData(string text) {
        list<hash<QoreLlmUtils::ChatMessage>> messages = buildTextMessages(text);
        return runExtraction(messages);
    }

    #! Extracts structured data from an image
    /** @param image_data raw image bytes (PNG, JPEG, etc.)
        @param mime_type image MIME type (e.g., "image/png", "image/jpeg")
        @param prompt optional custom user prompt for the image
        @return an @ref ExtractionResult hash

        @throw EXTRACTION-ERROR if all retry attempts fail
    */
    hash<ExtractionResult> extractFromImage(binary image_data, string mime_type,
            *string prompt) {
        string user_prompt = prompt
            ?? "Extract structured data from this image according to the schema.";
        list<hash<QoreLlmUtils::ChatMessage>> messages = (
            <QoreLlmUtils::ChatMessage>{"role": "system", "content": system_prompt},
            <QoreLlmUtils::ChatMessage>{
                "role": "user",
                "content_parts": (
                    <QoreLlmUtils::ContentPart>{
                        "type": "image",
                        "data": image_data,
                        "mime_type": mime_type,
                    },
                    <QoreLlmUtils::ContentPart>{
                        "type": "text",
                        "text": user_prompt,
                    },
                ),
            },
        );
        return runExtraction(messages);
    }

    #! Extracts structured data from a batch of texts
    /** @param texts list of texts to extract from
        @param progress optional callback called after each extraction with
            (int completed, int total) for progress reporting
        @return list of @ref ExtractionResult hashes, one per input text
    */
    list<hash<ExtractionResult>> extractBatch(list<string> texts,
            *code progress) {
        if (!texts) {
            return ();
        }

        if (batch_parallel <= 1) {
            # Sequential
            list<hash<ExtractionResult>> results = ();
            int idx = 0;
            foreach string text in (texts) {
                push results, extractData(text);
                ++idx;
                if (progress) {
                    progress(idx, texts.size());
                }
            }
            return results;
        }

        # Parallel batch processing
        list<hash<ExtractionResult>> results = ();
        # Pre-size
        for (int i = 0; i < texts.size(); ++i) {
            push results, <ExtractionResult>{
                "extracted": {},
                "valid": False,
                "validation_errors": (),
                "usage": {},
                "attempt_count": 0,
                "confidence": 0.0,
            };
        }

        int total = texts.size();
        int completed = 0;
        Mutex progress_mutex();

        int offset = 0;
        while (offset < total) {
            int batch_end = min(offset + batch_parallel, total);
            int batch_size = batch_end - offset;
            Counter cnt(batch_size);
            for (int i = offset; i < batch_end; ++i) {
                int idx = i;
                string text = texts[i];
                code<nothing()> worker = sub () {
                    on_exit cnt.dec();
                    results[idx] = extractData(text);
                    if (progress) {
                        progress_mutex.lock();
                        on_exit progress_mutex.unlock();
                        ++completed;
                        progress(completed, total);
                    }
                };
%ifdef PO_NO_THREAD_CONTROL
                call_function(start_thread, worker);
%else
                if (start_thread) {
                    call_function(start_thread, worker);
                } else {
                    background worker();
                }
%endif
            }
            cnt.waitForZero();
            offset = batch_end;
        }

        return results;
    }

    #! Returns the extraction schema
    hash<ExtractionSchemaInfo> getSchema() {
        return schema;
    }

    #! Returns the JSON schema used for tool definitions
    hash<auto> getJsonSchema() {
        return json_schema;
    }

    #! Runs extraction using chat() with tool definitions
    private hash<ExtractionResult> runExtraction(
            list<hash<QoreLlmUtils::ChatMessage>> messages) {
        hash<auto> total_usage = {};
        int attempts = 0;
        list<string> last_errors = ();

        while (attempts <= max_retries) {
            ++attempts;

            # Build effective messages (add retry context if needed)
            list<hash<QoreLlmUtils::ChatMessage>> effective_messages = messages;
            if (attempts > 1 && last_errors) {
                push effective_messages, <QoreLlmUtils::ChatMessage>{
                    "role": "user",
                    "content": sprintf("Previous attempt had validation errors: %s\n"
                        "Please fix these issues.", last_errors.join("; ")),
                };
            }

            hash<QoreLlmUtils::ChatResponse> resp = llm.chat(effective_messages, tools);
            total_usage = mergeUsage(total_usage, resp.usage);

            # Extract data from tool call or text response
            hash<auto> parsed;
            try {
                if (resp.tool_calls) {
                    # Tool-use response — structured data in arguments
                    parsed = resp.tool_calls[0].arguments ?? {};
                } else if (resp.content.val()) {
                    # Text response — parse JSON
                    parsed = parseResponse(resp.content);
                } else {
                    last_errors = ("LLM returned empty response",);
                    if (attempts > max_retries) {
                        return makeResult(parsed ?? {}, False, last_errors,
                            total_usage, attempts);
                    }
                    continue;
                }
            } catch (hash<ExceptionInfo> ex) {
                last_errors = (sprintf("parse error: %s", ex.desc),);
                if (attempts > max_retries) {
                    return makeResult({}, False, last_errors, total_usage, attempts);
                }
                continue;
            }

            # Validate against schema
            list<string> errors = validateExtracted(parsed);
            if (!errors) {
                return makeResult(parsed, True, (), total_usage, attempts);
            }

            last_errors = errors;
            if (attempts > max_retries) {
                return makeResult(parsed, False, errors, total_usage, attempts);
            }
        }

        throw "EXTRACTION-ERROR", "extraction failed after all retry attempts";
    }

    #! Creates an ExtractionResult with confidence scoring
    private hash<ExtractionResult> makeResult(hash<auto> extracted, bool valid,
            list<string> errors, hash<auto> usage, int attempts) {
        # Confidence: 1.0 if valid on first try, decreasing with retries/errors
        float confidence = 0.0;
        if (valid) {
            confidence = 1.0 / attempts.toFloat();
        }

        return <ExtractionResult>{
            "extracted": extracted,
            "valid": valid,
            "validation_errors": errors,
            "usage": usage,
            "attempt_count": attempts,
            "confidence": confidence,
        };
    }

    #! Builds the system prompt, including few-shot examples if present
    private string buildSystemPrompt() {
        string prompt = "You are a structured information extraction engine. "
            "Extract information from the provided text according to the "
            "following schema.\n\n";

        prompt += sprintf("Task: %s\n\n", schema.description);
        prompt += "Output format: Call the extraction tool with the following "
            "fields:\n\n";

        foreach hash<ExtractionFieldDef> field in (schema.fields) {
            string req = field.required ? "REQUIRED" : "optional";
            prompt += sprintf("- \"%s\" (%s, %s): %s\n", field.name, field.type,
                req, field.description);
            if (field.enum_values) {
                prompt += sprintf("  Allowed values: %s\n",
                    (map sprintf("\"%s\"", $1), field.enum_values).join(", "));
            }
        }

        prompt += "\nRules:\n"
            "- Extract only information explicitly present in the input\n"
            "- Use null for optional fields when the information is not found\n"
            "- Do not invent or hallucinate data\n";

        # Add few-shot examples if available
        if (schema.examples) {
            prompt += "\nExamples:\n\n";
            int idx = 1;
            foreach hash<auto> example in (schema.examples) {
                prompt += sprintf("Example %d:\nInput: %s\nOutput: %s\n\n",
                    idx, example.input, make_json(example.output));
                ++idx;
            }
        }

        return prompt;
    }

    #! Builds messages for text extraction
    private list<hash<QoreLlmUtils::ChatMessage>> buildTextMessages(string text) {
        string user_prompt;
        if (user_prompt_template.val()) {
            user_prompt = regex_subst(user_prompt_template, "\\{text\\}", text);
        } else {
            user_prompt = sprintf(
                "Extract structured data from the following text:\n\n---\n%s\n---",
                text);
        }
        return (
            <QoreLlmUtils::ChatMessage>{"role": "system", "content": system_prompt},
            <QoreLlmUtils::ChatMessage>{"role": "user", "content": user_prompt},
        );
    }

    #! Parses a text response as JSON (fallback when LLM doesn't use tools)
    private hash<auto> parseResponse(string content) {
        string cleaned = content;
        if (cleaned =~ /^```/) {
            cleaned = regex_subst(cleaned, "^```[a-zA-Z]*\\s*\n?", "");
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
    private list<string> validateExtracted(hash<auto> data) {
        list<string> errors = ();

        foreach hash<ExtractionFieldDef> field in (schema.fields) {
            auto val = data{field.name};

            # Check required fields — handle both missing and null
            if (field.required && (!exists val || val === NULL)) {
                push errors, sprintf("required field \"%s\" is missing", field.name);
                continue;
            }

            if (!exists val || val === NULL) {
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
                errors += validateArrayItems(field.name, val, field.items);
            }
        }

        return errors;
    }

    #! Validates field type
    private list<string> validateFieldType(string field_name, auto val,
            string expected_type) {
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

    #! Validates array items
    private list<string> validateArrayItems(string field_name, list<auto> items,
            hash<auto> item_schema) {
        list<string> errors = ();

        if (item_schema.type == "object" && item_schema.required) {
            int idx = 0;
            foreach auto item in (items) {
                if (item.typeCode() == NT_HASH) {
                    foreach string req_field in (item_schema.required) {
                        if (!exists item{req_field} || item{req_field} === NULL) {
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
