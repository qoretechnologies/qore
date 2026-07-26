# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdProcessor.qc @brief JSON-LD Processor top-level API

/*  JsonLdProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! JSON-LD 1.1 Processor
/** Top-level facade for JSON-LD processing operations.
    Provides expand, compact, flatten, frame, toRdf, and fromRdf operations.

    @par Example:
    @code{.py}
    JsonLdProcessor proc();

    # Expand
    list<auto> expanded = proc.expand({
        "\@context": {"@vocab": "http://schema.org/"},
        "\@type": "Person",
        "name": "Jane",
    });

    # Compact
    hash<auto> compacted = proc.compact(expanded, {"\@vocab": "http://schema.org/"});

    # Flatten
    hash<auto> flattened = proc.flatten(expanded);

    # To RDF
    hash<RdfDataset> dataset = proc.toRdf(expanded);
    @endcode

    @since json 1.12
*/
public class JsonLdProcessor {
    private {
        #! Context cache
        JsonLdContextCache contextCache;

        #! Document loader
        AbstractJsonLdDocumentLoader documentLoader;

        #! Context processor
        JsonLdContext contextProcessor;
    }

    #! Creates a new JSON-LD processor
    /** @param cache optional context cache (a default one is created if not provided)
        @param loader optional document loader (HTTP+caching loader is created if not provided)
    */
    constructor(*JsonLdContextCache cache, *AbstractJsonLdDocumentLoader loader) {
        contextCache = cache ?? new JsonLdContextCache();
        if (exists loader) {
            documentLoader = loader;
        } else {
            HttpJsonLdDocumentLoader http_loader();
            documentLoader = new CachingJsonLdDocumentLoader(http_loader, contextCache);
        }
        contextProcessor = new JsonLdContext(documentLoader, contextCache);
    }

    #! Creates a context processor with the specified processing mode
    private JsonLdContext getContextProcessor(string processing_mode) {
        if (processing_mode != contextProcessor.processingMode) {
            return new JsonLdContext(documentLoader, contextCache, NOTHING, processing_mode);
        }
        return contextProcessor;
    }

    #! Expands a JSON-LD document to full IRIs
    /** Removes the context and expands all terms to full IRIs.

        @param input the JSON-LD document (hash, list, or string URL)
        @param opts optional processing options
        @return the expanded document as a list of node objects

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    list<auto> expand(auto input, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};

        # If input is a string URL, load the document
        auto document = loadInput(input, options);

        # Get context processor with correct processing mode
        JsonLdContext ctx_proc = getContextProcessor(options.processingMode);

        # Build initial active context
        hash<JsonLdActiveContext> active_context = JsonLdContext::newActiveContext(options.base);

        # Apply expandContext option
        if (exists options.expandContext) {
            auto expand_ctx = options.expandContext;
            # If expandContext is a document with @context, extract the @context value
            if (expand_ctx.typeCode() == NT_HASH && exists expand_ctx."@context") {
                expand_ctx = expand_ctx."@context";
            }
            active_context = ctx_proc.processContext(active_context,
                expand_ctx, options.base);
        }

        # Expand
        JsonLdExpansion expander(ctx_proc, options);
        auto result = expander.expand(active_context, NOTHING, document, options.base,
            options.frameExpansion, options.ordered);

        if (!exists result) {
            return ();
        }
        if (result.typeCode() != NT_LIST) {
            # W3C 4.3.1 step 19: unwrap graph objects with ONLY @graph key at top level
            # Named graphs (with @id) should NOT be unwrapped
            if (result.typeCode() == NT_HASH && isGraphObject(result)
                    && result.size() == 1) {
                auto graph = result."@graph";
                if (!exists graph) {
                    return ();
                }
                if (graph.typeCode() != NT_LIST) {
                    return (graph,);
                }
                return graph;
            }
            return (result,);
        }
        return result;
    }

    #! Compacts a JSON-LD document using the given context
    /** Applies a context to produce a compact representation.

        @param input the JSON-LD document (expanded or unexpanded)
        @param ctx the context to compact with
        @param opts optional processing options
        @return the compacted document

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    hash<auto> compact(auto input, auto ctx, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};

        # Expand first
        list<auto> expanded = expand(input, options);

        # Get context processor with correct processing mode
        JsonLdContext ctx_proc = getContextProcessor(options.processingMode);

        # Build compaction context — extract @context value if ctx is a document
        auto ctx_value = ctx;
        if (ctx_value.typeCode() == NT_HASH && ctx_value.hasKey("@context")) {
            ctx_value = ctx_value."@context";
        }

        hash<JsonLdActiveContext> active_context = JsonLdContext::newActiveContext(options.base);
        if (exists ctx_value) {
            active_context = ctx_proc.processContext(active_context, ctx_value, options.base);
        }

        # Compact
        JsonLdCompaction compactor(ctx_proc, options);
        auto compacted = compactor.compact(active_context, NOTHING, expanded,
            options.compactArrays);

        # Build result with @context
        hash<auto> result = {};
        if (exists ctx_value) {
            # Only include @context if it has content
            bool include_ctx = True;
            if (ctx_value.typeCode() == NT_HASH && ctx_value.empty()) {
                include_ctx = False;
            }
            if (include_ctx) {
                result."@context" = ctx_value;
            }
        }

        if (compacted.typeCode() == NT_LIST) {
            if (compacted.empty()) {
                # Empty result - just return with @context if set
                return result;
            }
            # Compact @graph key
            string graph_alias = JsonLdIri::compactIri(active_context, "@graph", NOTHING, True);
            result{graph_alias} = compacted;
        } else if (compacted.typeCode() == NT_HASH) {
            result += compacted;
        }

        return result;
    }

    #! Flattens a JSON-LD document
    /** Collects all node objects into a single \@graph array.

        @param input the JSON-LD document
        @param ctx optional context to compact the result
        @param opts optional processing options
        @return the flattened document

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    auto flatten(auto input, *auto ctx, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};

        # Expand first
        list<auto> expanded = expand(input, options);

        # Flatten
        JsonLdFlattening flattener();
        hash<auto> flattened = flattener.flatten(expanded);

        # Compact if context provided
        if (exists ctx) {
            # NOTE: compact() internally re-expands the input for correctness (it handles its own
            # context processing). While this means the data is expanded twice, this is by design
            # per the W3C JSON-LD 1.1 specification (section 4.6) to ensure proper compaction.
            hash<auto> compacted = compact(flattened, ctx, options);
            if (!compacted.hasKey("@graph")) {
                # Ensure @graph key exists: move all non-@context keys into @graph
                hash<auto> result = {};
                if (exists compacted."@context") {
                    result."@context" = compacted."@context";
                }
                hash<auto> node = compacted;
                remove node."@context";
                result."@graph" = node.empty() ? () : (node,);
                return result;
            }
            return compacted;
        }

        # No context: return the @graph array directly per W3C spec
        return flattened."@graph" ?? ();
    }

    #! Frames a JSON-LD document using a frame
    /** Applies query-by-example framing to select and reshape graph data.

        @param input the JSON-LD document
        @param frame_doc the frame document
        @param opts optional processing options
        @return the framed document

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    hash<auto> frame(auto input, hash<auto> frame_doc, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};
        options.frameExpansion = True;

        # Expand document
        list<auto> expanded = expand(input, options);

        # Expand frame (separately)
        auto frame_ctx = frame_doc."@context";
        hash<auto> expanded_frame;
        list<auto> exp_frame = expand(frame_doc, options);
        if (exp_frame.size() > 0 && exp_frame[0].typeCode() == NT_HASH) {
            expanded_frame = exp_frame[0];
        } else {
            expanded_frame = {};
        }

        # Frame
        JsonLdFraming framer(options);
        hash<auto> result = framer.frame(expanded, expanded_frame);

        # Compact the result if frame has context
        if (exists frame_ctx) {
            return compact(result, frame_ctx, options);
        }

        return result;
    }

    #! Converts a JSON-LD document to an RDF dataset
    /** @param input the JSON-LD document
        @param opts optional processing options
        @return the RDF dataset

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    hash<RdfDataset> toRdf(auto input, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};

        # Expand first
        list<auto> expanded = expand(input, options);

        # Convert to RDF
        JsonLdRdf rdf(options);
        return rdf.toRdf(expanded);
    }

    #! Converts an RDF dataset to JSON-LD
    /** @param dataset the RDF dataset
        @param opts optional processing options
        @return the expanded JSON-LD

        @throw JSON-LD-ERROR on processing errors

        @since json 1.12
    */
    list<auto> fromRdf(hash<RdfDataset> dataset, *hash<JsonLdOptions> opts) {
        hash<JsonLdOptions> options = opts ?? <JsonLdOptions>{};
        JsonLdRdf rdf(options);
        return rdf.fromRdf(dataset);
    }

    #! Returns the context cache
    /** @return the context cache instance

        @since json 1.12
    */
    JsonLdContextCache getContextCache() {
        return contextCache;
    }

    #! Loads input document (resolves URL strings)
    private auto loadInput(auto input, hash<JsonLdOptions> options) {
        if (input.typeCode() == NT_STRING) {
            # String input - treat as URL
            if (isAbsoluteIri(input)) {
                hash<JsonLdRemoteDocument> doc = documentLoader.loadDocument(input);
                if (!exists options.base) {
                    options.base = doc.documentUrl;
                }
                return doc.document;
            }
            # Not a URL - try to parse as JSON
            try {
                return parse_json(input);
            } catch (hash<ExceptionInfo> ex) {
                throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                    sprintf("failed to parse JSON input: %s: %s", ex.err, ex.desc));
            }
        }
        return input;
    }
}

} # namespace JsonLd
