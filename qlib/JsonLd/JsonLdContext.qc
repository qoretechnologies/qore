# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdContext.qc @brief JSON-LD Context Processing Algorithm (W3C 4.1.2)

/*  JsonLdContext.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Maximum number of remote contexts that can be loaded
const MAX_REMOTE_CONTEXTS = 256;

#! JSON-LD Context Processing class
/** Implements the W3C JSON-LD 1.1 Context Processing Algorithm (Section 4.1.2)
    and Create Term Definition (Section 4.2.2).

    @since json 1.12
*/
public class JsonLdContext {
    public {
        #! Processing mode
        string processingMode = "json-ld-1.1";
    }

    private {
        #! Document loader
        *AbstractJsonLdDocumentLoader documentLoader;

        #! Context cache
        *JsonLdContextCache contextCache;

        #! Logger
        *LoggerInterface logger;
    }

    #! Creates a new context processor
    /** @param loader optional document loader for remote contexts
        @param cache optional context cache
        @param logger optional logger
        @param processing_mode the processing mode (default "json-ld-1.1")
    */
    constructor(*AbstractJsonLdDocumentLoader loader, *JsonLdContextCache cache,
            *LoggerInterface logger, string processing_mode = "json-ld-1.1") {
        documentLoader = loader;
        contextCache = cache;
        self.logger = logger;
        processingMode = processing_mode;
    }

    #! Processes a local context against an active context
    /** Implements the Context Processing Algorithm (W3C 4.1.2)

        @param active_context the current active context
        @param local_context the local context to process
        @param base_url the base URL for resolving relative references
        @param remote_contexts list of already-fetched remote context URLs (for cycle detection)
        @param override_protected whether protected terms can be overridden
        @param propagate whether the resulting context propagates to nested nodes
        @param validate_scoped whether to validate scoped contexts

        @return the updated active context

        @since json 1.12
    */
    hash<JsonLdActiveContext> processContext(hash<JsonLdActiveContext> active_context, auto local_context,
            *string base_url, *list<string> remote_contexts, bool override_protected = False,
            bool propagate = True, bool validate_scoped = True) {
        # 1. Initialize result to a copy of active context
        hash<JsonLdActiveContext> result = active_context;

        # Ensure remote_contexts is initialized
        if (!exists remote_contexts) {
            remote_contexts = ();
        }

        # 2. If local context is not an array, set it to an array containing only local context
        list<auto> contexts;
        if (local_context.typeCode() == NT_LIST) {
            contexts = local_context;
        } else {
            contexts = (local_context,);
        }

        # 3. For each item context in local context
        foreach auto ctx in (contexts) {
            processContextItem(\result, ctx, base_url, remote_contexts, override_protected,
                propagate, validate_scoped);
        }

        return result;
    }

    #! Processes a single context item
    private processContextItem(reference<hash<JsonLdActiveContext>> result, auto ctx,
            *string base_url, list<string> remote_contexts, bool override_protected,
            bool propagate, bool validate_scoped) {
        # 5.1 If context is null
        if (!exists ctx) {
            # Check if there are protected terms that cannot be overridden
            if (!override_protected) {
                foreach string term in (keys result.termDefinitions) {
                    if (result.termDefinitions{term}.protected_) {
                        throwJsonLdError(JSONLD_ERR_INVALID_CONTEXT_NULLIFICATION,
                            "cannot nullify context with protected term definitions");
                    }
                }
            }

            # Reset to initial context, preserving only the original base IRI
            hash<JsonLdActiveContext> prev = result;
            result = <JsonLdActiveContext>{
                "originalBaseIri": prev.originalBaseIri,
                "baseIri": prev.originalBaseIri,
                "termDefinitions": {},
            };
            if (!propagate) {
                result.previousContext = prev;
            }
            return;
        }

        # W3C 4.1.2 step 5.2: If propagate is false, store previous context
        if (!propagate && !exists result.previousContext) {
            result.previousContext = result;
        }

        # 5.3 If context is a string (remote context reference)
        if (ctx.typeCode() == NT_STRING) {
            processRemoteContext(\result, ctx, base_url, remote_contexts, override_protected,
                propagate, validate_scoped);
            return;
        }

        # 5.3 If context is not a hash, it's an invalid local context
        if (ctx.typeCode() != NT_HASH) {
            throwJsonLdError(JSONLD_ERR_INVALID_LOCAL_CONTEXT,
                sprintf("local context must be null, string, or object; got %s", ctx.type()));
        }

        hash<auto> context_hash = ctx;

        # 5.4 If context has @version
        if (exists context_hash."@version") {
            auto version = context_hash."@version";
            if (version != 1.1 && version != "1.1") {
                throwJsonLdError(JSONLD_ERR_INVALID_VERSION_VALUE,
                    sprintf("unsupported JSON-LD version: %y", version));
            }
            # Processing mode conflict: 1.0 mode + @version 1.1
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_PROCESSING_MODE_CONFLICT,
                    "processing mode json-ld-1.0 conflicts with @version: 1.1");
            }
        }

        # 5.5 If context has @import
        if (exists context_hash."@import") {
            processImport(\context_hash, base_url, remote_contexts);
        }

        # 5.6 If context has @base (and we're not processing a remote context)
        if (context_hash.hasKey("@base") && remote_contexts.empty()) {
            auto base_val = context_hash."@base";
            if (!exists base_val) {
                result.baseIri = NOTHING;
            } else if (base_val.typeCode() == NT_STRING) {
                if (isAbsoluteIri(base_val)) {
                    result.baseIri = base_val;
                } else if (exists result.baseIri) {
                    result.baseIri = resolveIri(result.baseIri, base_val);
                } else {
                    throwJsonLdError(JSONLD_ERR_INVALID_BASE_IRI,
                        sprintf("cannot resolve relative @base %y without existing base IRI", base_val));
                }
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_BASE_IRI,
                    sprintf("@base must be a string or null; got %s", base_val.type()));
            }
        }

        # 5.7 If context has @vocab
        if (context_hash.hasKey("@vocab")) {
            auto vocab = context_hash."@vocab";
            if (!exists vocab) {
                result.vocabularyMapping = NOTHING;
            } else if (vocab.typeCode() == NT_STRING) {
                if (isAbsoluteIri(vocab) || isBlankNodeIdentifier(vocab) || vocab == "") {
                    # Check if it looks like a compact IRI with a defined prefix
                    if (isAbsoluteIri(vocab)) {
                        int colon_pos = index(vocab, ":");
                        if (colon_pos > 0) {
                            string prefix = substr(vocab, 0, colon_pos);
                            string suffix = substr(vocab, colon_pos + 1);
                            # Only expand as compact IRI if prefix is a defined term
                            # with @prefix flag and suffix doesn't start with //
                            if (suffix.find("//") != 0
                                    && exists result.termDefinitions{prefix}
                                    && result.termDefinitions{prefix}.prefix) {
                                result.vocabularyMapping =
                                    result.termDefinitions{prefix}.iriMapping + suffix;
                            } else {
                                result.vocabularyMapping = vocab;
                            }
                        } else {
                            result.vocabularyMapping = vocab;
                        }
                    } else {
                        result.vocabularyMapping = vocab;
                    }
                } else {
                    # Expand @vocab using IRI expansion (W3C spec)
                    # This handles compact IRIs, term references, and relative IRIs
                    *string expanded = JsonLdIri::expandIri(result, vocab, True, True);
                    if (exists expanded) {
                        result.vocabularyMapping = expanded;
                    } else {
                        throwJsonLdError(JSONLD_ERR_INVALID_VOCAB_MAPPING,
                            sprintf("invalid @vocab value: %y", vocab));
                    }
                }
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_VOCAB_MAPPING,
                    sprintf("@vocab must be a string or null; got %s", vocab.type()));
            }
        }

        # 5.8 If context has @language
        if (context_hash.hasKey("@language")) {
            auto lang = context_hash."@language";
            if (!exists lang) {
                result.defaultLanguage = NOTHING;
            } else if (lang.typeCode() == NT_STRING) {
                result.defaultLanguage = lang.lwr();
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_DEFAULT_LANGUAGE,
                    sprintf("@language must be a string or null; got %s", lang.type()));
            }
        }

        # 5.9 If context has @direction
        if (context_hash.hasKey("@direction")) {
            auto dir = context_hash."@direction";
            if (!exists dir) {
                result.defaultBaseDirection = NOTHING;
            } else if (dir.typeCode() == NT_STRING && (dir == "ltr" || dir == "rtl")) {
                result.defaultBaseDirection = dir;
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_BASE_DIRECTION,
                    sprintf("@direction must be \"ltr\", \"rtl\", or null; got %y", dir));
            }
        }

        # 5.10 If context has @propagate
        if (exists context_hash."@propagate") {
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_INVALID_CONTEXT_ENTRY,
                    "@propagate not allowed in JSON-LD 1.0");
            }
            auto prop = context_hash."@propagate";
            if (prop.typeCode() != NT_BOOLEAN) {
                throwJsonLdError(JSONLD_ERR_INVALID_PROPAGATE_VALUE,
                    sprintf("@propagate must be a boolean; got %s", prop.type()));
            }
            if (!prop && !exists result.previousContext) {
                result.previousContext = result;
            }
        }

        # 5.11 Create term definitions for all non-keyword entries
        hash<string, bool> defined = {};
        foreach string key in (keys context_hash) {
            if (key == "@base" || key == "@direction" || key == "@import" || key == "@language"
                    || key == "@propagate" || key == "@protected" || key == "@version"
                    || key == "@vocab") {
                continue;
            }
            createTermDefinition(\result, context_hash, key, \defined, override_protected,
                base_url, remote_contexts, validate_scoped);
        }
    }

    #! Processes a remote context reference
    private processRemoteContext(reference<hash<JsonLdActiveContext>> result, string ctx_url,
            *string base_url, list<string> remote_contexts, bool override_protected,
            bool propagate, bool validate_scoped) {
        # Resolve relative URL
        if (exists base_url && !isAbsoluteIri(ctx_url)) {
            ctx_url = resolveIri(base_url, ctx_url);
        }

        # Check for context overflow
        if (remote_contexts.size() >= MAX_REMOTE_CONTEXTS) {
            throwJsonLdError(JSONLD_ERR_CONTEXT_OVERFLOW,
                sprintf("too many remote contexts (max %d)", MAX_REMOTE_CONTEXTS));
        }

        # Check for cycles — per W3C 4.1.2 step 5.2.4: if already loaded, skip
        if (inlist(ctx_url, remote_contexts)) {
            return;
        }

        # Add to remote contexts for cycle detection
        push remote_contexts, ctx_url;

        # Check cache first
        if (exists contextCache) {
            *auto cached = contextCache.get(ctx_url);
            if (exists cached) {
                result = processContext(result, cached, ctx_url, remote_contexts, override_protected,
                    propagate, validate_scoped);
                return;
            }
        }

        # Load the remote document
        if (!exists documentLoader) {
            throwJsonLdError(JSONLD_ERR_LOADING_REMOTE_CONTEXT_FAILED,
                sprintf("no document loader available to fetch remote context: %y", ctx_url));
        }

        hash<JsonLdRemoteDocument> remote_doc;
        try {
            remote_doc = documentLoader.loadDocument(ctx_url);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "JSON-LD-ERROR") {
                rethrow;
            }
            throwJsonLdError(JSONLD_ERR_LOADING_REMOTE_CONTEXT_FAILED,
                sprintf("failed to load remote context %y: %s: %s", ctx_url, ex.err, ex.desc));
        }

        auto remote_context = remote_doc.document;

        # Remote context document must be an object with @context
        if (remote_context.typeCode() != NT_HASH) {
            throwJsonLdError(JSONLD_ERR_INVALID_REMOTE_CONTEXT,
                sprintf("remote context %y is not a JSON object", ctx_url));
        }

        hash<auto> remote_hash = remote_context;
        if (!exists remote_hash."@context") {
            throwJsonLdError(JSONLD_ERR_INVALID_REMOTE_CONTEXT,
                sprintf("remote context %y does not contain @context", ctx_url));
        }

        auto context_value = remote_hash."@context";

        # Cache the context
        if (exists contextCache) {
            contextCache.set(ctx_url, context_value);
        }

        # Process the remote context
        result = processContext(result, context_value, ctx_url, remote_contexts,
            override_protected, propagate, validate_scoped);
    }

    #! Processes an \@import directive
    private processImport(reference<hash<auto>> context_hash, *string base_url,
            list<string> remote_contexts) {
        auto import_val = context_hash."@import";
        if (import_val.typeCode() != NT_STRING) {
            throwJsonLdError(JSONLD_ERR_INVALID_IMPORT_VALUE,
                sprintf("@import must be a string; got %s", import_val.type()));
        }

        string import_url = import_val;
        if (exists base_url && !isAbsoluteIri(import_url)) {
            import_url = resolveIri(base_url, import_url);
        }

        if (!exists documentLoader) {
            throwJsonLdError(JSONLD_ERR_LOADING_REMOTE_CONTEXT_FAILED,
                sprintf("no document loader available to fetch @import context: %y", import_url));
        }

        hash<JsonLdRemoteDocument> import_doc;
        try {
            import_doc = documentLoader.loadDocument(import_url);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "JSON-LD-ERROR") {
                rethrow;
            }
            throwJsonLdError(JSONLD_ERR_LOADING_REMOTE_CONTEXT_FAILED,
                sprintf("failed to load @import context %y: %s: %s", import_url, ex.err, ex.desc));
        }

        auto import_context = import_doc.document;
        if (import_context.typeCode() != NT_HASH) {
            throwJsonLdError(JSONLD_ERR_INVALID_IMPORT_VALUE,
                sprintf("@import document %y is not a JSON object", import_url));
        }

        hash<auto> import_hash = import_context;
        if (!exists import_hash."@context") {
            throwJsonLdError(JSONLD_ERR_INVALID_IMPORT_VALUE,
                sprintf("@import document %y does not contain @context", import_url));
        }

        auto imported = import_hash."@context";
        if (imported.typeCode() != NT_HASH) {
            throwJsonLdError(JSONLD_ERR_INVALID_IMPORT_VALUE,
                "@import context must resolve to a single object context");
        }

        if (exists imported."@import") {
            throwJsonLdError(JSONLD_ERR_INVALID_IMPORT_VALUE,
                "@import contexts must not themselves contain @import");
        }

        # Merge: imported entries are defaults, local context entries override
        hash<auto> merged = imported;
        foreach string key in (keys context_hash) {
            if (key != "@import") {
                merged{key} = context_hash{key};
            }
        }
        context_hash = merged;
    }

    #! Creates a term definition in the active context
    /** Implements the Create Term Definition algorithm (W3C 4.2.2)

        @param result the active context being built
        @param local_context the local context hash
        @param term the term to define
        @param defined tracks which terms have been defined (cycle detection)
        @param override_protected whether protected terms can be overridden
        @param base_url base URL for resolving
        @param remote_contexts remote context list
        @param validate_scoped whether to validate scoped contexts
    */
    createTermDefinition(reference<hash<JsonLdActiveContext>> result, hash<auto> local_context,
            string term, reference<hash<string, bool>> defined, bool override_protected = False,
            *string base_url, *list<string> remote_contexts, bool validate_scoped = True) {
        # 1. If defined contains term with value true, return (already defined)
        if (defined{term} == True) {
            return;
        }

        # 2. If defined contains term with value false, cyclic definition
        if (exists defined{term} && defined{term} == False) {
            throwJsonLdError(JSONLD_ERR_CYCLIC_IRI_MAPPING,
                sprintf("cyclic IRI mapping detected for term %y", term));
        }

        # 3. Set defined[term] = false (being defined)
        defined{term} = False;

        # 4. Get the value from local context
        auto value = local_context{term};

        # 4a. Reject empty string term definition
        if (term == "") {
            throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                "the empty string cannot be used as a term");
        }

        # 5. Handle keyword aliases: @type can be aliased in 1.1
        if (term == "@type") {
            # @type can be set to null (to clear) or a hash with @container/@protected
            if (!exists value) {
                # null: clear the @type term definition
                result.termDefinitions{term} = <JsonLdTermDefinition>{"iriMapping": "@type"};
                defined{term} = True;
                return;
            } else if (value.typeCode() == NT_HASH) {
                hash<auto> val_hash = value;
                # Empty map is invalid for @type
                if (val_hash.empty()) {
                    throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                        "@type term definition must not be an empty object");
                }
                # In 1.0 mode, @type cannot be redefined at all
                if (processingMode == "json-ld-1.0") {
                    throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                        "@type cannot be redefined in JSON-LD 1.0");
                }
                # Only @container and @protected allowed
                foreach string key in (keys val_hash) {
                    if (key != "@container" && key != "@protected") {
                        throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                            sprintf("@type term definition can only contain @container and @protected; got %y", key));
                    }
                }
                if (exists val_hash."@container" && val_hash."@container" != "@set") {
                    throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                        "@type @container must be @set");
                }
            } else {
                # @type can only be redefined with a hash (for @container: @set) or null
                throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                    sprintf("@type can only be redefined with an object containing @container; got %s", value.type()));
            }
        } else if (isJsonLdKeyword(term)) {
            # Keywords (other than @type) cannot be redefined
            throwJsonLdError(JSONLD_ERR_KEYWORD_REDEFINITION,
                sprintf("cannot redefine keyword %y", term));
        } else if (isKeywordLike(term)) {
            # Terms that look like keywords (@ + alpha chars) are ignored
            defined{term} = True;
            return;
        }

        # 6. Get previous definition
        *hash<JsonLdTermDefinition> previous_definition = result.termDefinitions{term};

        # 7. Check protected term redefinition
        if (exists previous_definition && previous_definition.protected_ && !override_protected) {
            # Allow identical redefinition
            if (value.typeCode() == NT_HASH || value.typeCode() == NT_STRING) {
                # Check if new definition is identical - done after creating new def
            }
            # We'll check at the end
        }

        # 8. Remove any existing definition
        remove result.termDefinitions{term};

        # Initialize definition
        hash<JsonLdTermDefinition> definition();

        # 9. If value is null, the term is explicitly set to nothing
        if (!exists value) {
            definition.iriMapping = NOTHING;
            # Apply context-level @protected to null definitions
            if (exists local_context."@protected") {
                definition.protected_ = local_context."@protected" ? True : False;
            }
            result.termDefinitions{term} = definition;
            defined{term} = True;
            checkProtectedRedefinition(previous_definition, definition, term,
                override_protected, \result);
            return;
        }

        # 10. If value is a string, it's a simple term definition (IRI mapping)
        if (value.typeCode() == NT_STRING) {
            string str_value = value;
            # Expand as IRI
            string expanded = expandTermIri(\result, local_context, str_value, \defined, base_url,
                remote_contexts, validate_scoped);
            definition.iriMapping = expanded;

            # W3C step 14.4: If term contains slash, verify IRI expansion matches
            if (term.find("/") >= 0) {
                defined{term} = True;
                *string term_expanded = JsonLdIri::expandIri(result, term, True,
                    NOTHING, local_context, \defined);
                if (exists term_expanded && term_expanded != definition.iriMapping) {
                    throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                        sprintf("term %y expands to %y but IRI mapping is %y",
                            term, term_expanded, definition.iriMapping));
                }
            }

            # Apply context-level @protected
            if (exists local_context."@protected") {
                definition.protected_ = local_context."@protected" ? True : False;
            }

            # Check if this is a prefix (namespace-like, ends with gen-delim)
            if (expanded =~ /[\/:?#\[\]@]$/) {
                definition.prefix = True;
            }

            result.termDefinitions{term} = definition;
            defined{term} = True;
            checkProtectedRedefinition(previous_definition, definition, term, override_protected, \result);
            return;
        }

        # 11. Value must be a hash
        if (value.typeCode() != NT_HASH) {
            throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                sprintf("term definition for %y must be a string, object, or null; got %s", term, value.type()));
        }

        hash<auto> val = value;

        # 12. Process @protected
        if (exists val."@protected") {
            if (val."@protected".typeCode() != NT_BOOLEAN) {
                throwJsonLdError(JSONLD_ERR_INVALID_PROTECTED_VALUE,
                    sprintf("@protected must be a boolean; got %s", val."@protected".type()));
            }
            definition.protected_ = val."@protected";
        } else if (exists local_context."@protected") {
            definition.protected_ = local_context."@protected" ? True : False;
        }

        # 13. Process @type
        if (exists val."@type") {
            auto type_val = val."@type";
            if (type_val.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_TYPE_MAPPING,
                    sprintf("@type in term definition must be a string; got %s", type_val.type()));
            }

            string type_str = type_val;
            if (type_str == "@json" || type_str == "@none") {
                if (processingMode == "json-ld-1.0") {
                    throwJsonLdError(JSONLD_ERR_INVALID_TYPE_MAPPING,
                        sprintf("@type: %y not allowed in JSON-LD 1.0", type_str));
                }
                definition.typeMapping = type_str;
            } else if (type_str == "@id" || type_str == "@vocab") {
                definition.typeMapping = type_str;
            } else {
                # Expand as IRI
                string expanded_type = expandTermIri(\result, local_context, type_str, \defined,
                    base_url, remote_contexts, validate_scoped);
                if (!isAbsoluteIri(expanded_type)) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TYPE_MAPPING,
                        sprintf("@type mapping %y does not expand to an absolute IRI", type_str));
                }
                definition.typeMapping = expanded_type;
            }
        }

        # 14. Process @reverse
        if (exists val."@reverse") {
            if (exists val."@id" || exists val."@nest") {
                throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_PROPERTY,
                    sprintf("term definition for %y cannot have @reverse with @id or @nest", term));
            }
            auto rev_val = val."@reverse";
            if (rev_val.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                    sprintf("@reverse must be a string; got %s", rev_val.type()));
            }

            # If @reverse value looks like a keyword but isn't, silently ignore
            if (isKeywordLike(rev_val) && !isJsonLdKeyword(rev_val)) {
                defined{term} = True;
                return;
            }

            string expanded_rev = expandTermIri(\result, local_context, rev_val, \defined,
                base_url, remote_contexts, validate_scoped);
            if (!isAbsoluteIri(expanded_rev) && !isBlankNodeIdentifier(expanded_rev)) {
                throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                    sprintf("@reverse value %y does not expand to an absolute IRI", rev_val));
            }

            definition.iriMapping = expanded_rev;
            definition.reverse = True;

            # Process @container for reverse
            if (exists val."@container") {
                auto container = val."@container";
                if (exists container && container != "@set" && container != "@index") {
                    throwJsonLdError(JSONLD_ERR_INVALID_REVERSE_PROPERTY,
                        sprintf("@reverse property @container must be @set, @index, or null; got %y", container));
                }
                definition.containerMapping = exists container ? (container,) : NOTHING;
            }

            # Process @index for reverse (property-valued index)
            if (exists val."@index") {
                if (!exists definition.containerMapping
                        || !inlist("@index", definition.containerMapping)) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                        sprintf("@index requires @container to include @index for term %y",
                            term));
                }
                auto idx_val = val."@index";
                if (idx_val.typeCode() != NT_STRING) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                        sprintf("@index must be a string; got %s", idx_val.type()));
                }
                if (isJsonLdKeyword(idx_val)) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                        sprintf("@index value must not be a keyword; got %y", idx_val));
                }
                *string expanded_idx = JsonLdIri::expandIri(result, idx_val, True);
                if (!exists expanded_idx || !isAbsoluteIri(expanded_idx)) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                        sprintf("@index value %y does not expand to an IRI", idx_val));
                }
                definition.indexProperty = idx_val;
            }

            result.termDefinitions{term} = definition;
            defined{term} = True;
            checkProtectedRedefinition(previous_definition, definition, term, override_protected, \result);
            return;
        }

        # 15. Process @id
        if (val.hasKey("@id") && val."@id" != term) {
            auto id_val = val."@id";
            if (!exists id_val) {
                # null @id: no IRI mapping
            } else if (id_val.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                    sprintf("@id must be a string or null; got %s", id_val.type()));
            } else {
                string id_str = id_val;
                if (isJsonLdKeyword(id_str)) {
                    # Keyword alias — @context and @preserve cannot be aliased
                    if (id_str == "@context") {
                        throwJsonLdError(JSONLD_ERR_INVALID_KEYWORD_ALIAS,
                            sprintf("term %y cannot alias @context", term));
                    }
                    definition.iriMapping = id_str;
                } else if (isKeywordLike(id_str)) {
                    # Unknown keyword-like values are silently ignored
                    defined{term} = True;
                    return;
                } else {
                    string expanded_id = expandTermIri(\result, local_context, id_str, \defined,
                        base_url, remote_contexts, validate_scoped);
                    if (!isAbsoluteIri(expanded_id) && !isBlankNodeIdentifier(expanded_id)
                            && !isJsonLdKeyword(expanded_id)) {
                        throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                            sprintf("@id value %y does not expand to an absolute IRI, blank node, or keyword",
                                id_str));
                    }
                    definition.iriMapping = expanded_id;
                }

                # W3C step 14.4: If term contains colon or slash, verify
                # IRI expansion matches the @id mapping
                if ((term.find(":") > 0 || term.find("/") >= 0)
                        && exists definition.iriMapping) {
                    defined{term} = True;
                    *string term_expanded = JsonLdIri::expandIri(result, term, True,
                        NOTHING, local_context, \defined);
                    if (exists term_expanded && term_expanded != definition.iriMapping) {
                        throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                            sprintf("term %y expands to %y but @id is %y",
                                term, term_expanded, definition.iriMapping));
                    }
                }

                # Check if term can be used as a prefix
                if (term.find(":") < 0 && term.find("/") < 0 && exists definition.iriMapping) {
                    if (val."@prefix" == True
                            || (definition.iriMapping =~ /:$/ || definition.iriMapping.find("/") >= 0)) {
                        definition.prefix = val."@prefix" ?? False;
                    }
                }
            }
        } else if (term.find(":") > 0) {
            # Term contains a colon - treat as compact IRI
            *list<string> parts = (term =~ x/^([^:]+):(.*)$/);
            if (parts) {
                string prefix = parts[0];
                string suffix = parts[1];

                if (exists local_context{prefix}) {
                    createTermDefinition(\result, local_context, prefix, \defined, override_protected,
                        base_url, remote_contexts, validate_scoped);
                }

                if (exists result.termDefinitions{prefix}) {
                    definition.iriMapping = result.termDefinitions{prefix}.iriMapping + suffix;
                } else {
                    definition.iriMapping = term;
                }
            }
        } else if (term.find("/") >= 0) {
            # Term contains "/" - treat as relative IRI
            definition.iriMapping = resolveTermIri(result, term);
            if (!isAbsoluteIri(definition.iriMapping)) {
                throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                    sprintf("term %y with slash does not expand to absolute IRI", term));
            }
        } else if (term == "@type") {
            definition.iriMapping = "@type";
        } else if (exists result.vocabularyMapping) {
            definition.iriMapping = result.vocabularyMapping + term;
        } else {
            throwJsonLdError(JSONLD_ERR_INVALID_IRI_MAPPING,
                sprintf("term %y has no IRI mapping and no vocabulary mapping is set", term));
        }

        # 16. Process @container
        if (exists val."@container") {
            auto container = val."@container";
            list<string> container_values;
            if (container.typeCode() == NT_STRING) {
                container_values = (container,);
            } else if (container.typeCode() == NT_LIST) {
                container_values = container;
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                    sprintf("@container must be a string or array; got %s", container.type()));
            }

            # Validate individual container values
            foreach string cv in (container_values) {
                if (!JSONLD_VALID_CONTAINERS{cv}) {
                    throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                        sprintf("invalid @container value: %y", cv));
                }
            }

            # Processing mode 1.0 restrictions
            if (processingMode == "json-ld-1.0") {
                if (container.typeCode() == NT_LIST) {
                    throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                        "@container arrays not allowed in JSON-LD 1.0");
                }
                string cv = container_values[0];
                if (cv == "@graph" || cv == "@id" || cv == "@type") {
                    throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                        sprintf("@container %y not allowed in JSON-LD 1.0", cv));
                }
            }

            # Validate container combinations (W3C step 19.1)
            if (container_values.size() > 1) {
                # @list cannot combine with anything
                if (inlist("@list", container_values)) {
                    throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                        "@list container cannot be combined with other containers");
                }
                # Valid multi-container combos: @graph+@id, @graph+@index,
                # any of those +@set, or @set+single-other
                bool has_graph = inlist("@graph", container_values);
                bool has_set = inlist("@set", container_values);
                if (!has_graph && !has_set) {
                    throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                        sprintf("invalid @container combination: %y", container_values));
                }
                if (has_graph) {
                    foreach string cv in (container_values) {
                        if (cv != "@graph" && cv != "@id" && cv != "@index"
                                && cv != "@set") {
                            throwJsonLdError(JSONLD_ERR_INVALID_CONTAINER_MAPPING,
                                sprintf("@graph container can only combine with "
                                    "@id, @index, or @set; got %y", cv));
                        }
                    }
                }
            }

            definition.containerMapping = container_values;

            # W3C step 19.4: If container includes @type
            if (inlist("@type", container_values)) {
                if (!exists definition.typeMapping) {
                    # Default to @id if no type mapping set
                    definition.typeMapping = "@id";
                } else if (definition.typeMapping != "@id"
                        && definition.typeMapping != "@vocab") {
                    throwJsonLdError(JSONLD_ERR_INVALID_TYPE_MAPPING,
                        sprintf("@container @type requires @type mapping of @id or @vocab; got %y",
                            definition.typeMapping));
                }
            }
        }

        # 17. Process @index
        if (exists val."@index") {
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    "@index property not allowed in JSON-LD 1.0");
            }
            if (!exists definition.containerMapping
                    || !inlist("@index", definition.containerMapping)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("@index requires @container to include @index for term %y", term));
            }
            auto idx_val = val."@index";
            if (idx_val.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("@index must be a string; got %s", idx_val.type()));
            }
            # @index value must not be a keyword
            if (isJsonLdKeyword(idx_val)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("@index value must not be a keyword; got %y", idx_val));
            }
            # Expand and validate the index property
            *string expanded_idx = JsonLdIri::expandIri(result, idx_val, True);
            if (!exists expanded_idx || !isAbsoluteIri(expanded_idx)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("@index value %y does not expand to an IRI", idx_val));
            }
            definition.indexProperty = idx_val;
        }

        # 18. Process @context (scoped context) — use hasKey to detect null @context
        if (val.hasKey("@context")) {
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    "@context in term definition not allowed in JSON-LD 1.0");
            }
            definition.localContext = val."@context";
            definition.hasLocalContext = True;
            definition.baseUrl = base_url;

            # Validate scoped context if required
            # Use validate_scoped=false and empty remote_contexts to allow
            # recursive scoped context references (they're only a problem at use time)
            if (validate_scoped) {
                try {
                    processContext(result, val."@context", base_url, (),
                        True, True, False);  # fresh remote_contexts, validate_scoped=false
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "JSON-LD-ERROR") {
                        throwJsonLdError(JSONLD_ERR_INVALID_SCOPED_CONTEXT,
                            sprintf("invalid scoped context for term %y: %s", term, ex.desc));
                    }
                    rethrow;
                }
            }
        }

        # 19. Process @language (only if @type is not present per W3C step 22)
        if (val.hasKey("@language") && !val.hasKey("@type")) {
            auto lang = val."@language";
            if (!exists lang) {
                definition.languageMapping = NOTHING;
            } else if (lang.typeCode() == NT_STRING) {
                definition.languageMapping = lang.lwr();
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_LANGUAGE_MAPPING,
                    sprintf("@language must be a string or null; got %s", lang.type()));
            }
            definition.hasLanguageMapping = True;
        }

        # 20. Process @direction (only if @type is not present per W3C step 23)
        if (val.hasKey("@direction") && !val.hasKey("@type")) {
            auto dir = val."@direction";
            if (!exists dir) {
                definition.directionMapping = NOTHING;
            } else if (dir.typeCode() == NT_STRING && (dir == "ltr" || dir == "rtl")) {
                definition.directionMapping = dir;
            } else {
                throwJsonLdError(JSONLD_ERR_INVALID_BASE_DIRECTION,
                    sprintf("@direction must be \"ltr\", \"rtl\", or null; got %y", dir));
            }
            definition.hasDirectionMapping = True;
        }

        # 21. Process @nest
        if (exists val."@nest") {
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    "@nest in term definition not allowed in JSON-LD 1.0");
            }
            # @nest cannot be used with @reverse
            if (definition.reverse) {
                throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                    sprintf("@nest cannot be used with @reverse for term %y", term));
            }
            auto nest = val."@nest";
            if (nest.typeCode() != NT_STRING) {
                throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                    sprintf("@nest must be a string; got %s", nest.type()));
            }
            if (isJsonLdKeyword(nest) && nest != "@nest") {
                throwJsonLdError(JSONLD_ERR_INVALID_NEST_VALUE,
                    sprintf("@nest must be @nest or a non-keyword; got %y", nest));
            }
            definition.nestValue = nest;
        }

        # 22. Process @prefix
        if (exists val."@prefix") {
            if (processingMode == "json-ld-1.0") {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    "@prefix in term definition not allowed in JSON-LD 1.0");
            }
            if (term.find(":") >= 0 || term.find("/") >= 0) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("@prefix not allowed on term %y containing colon or slash", term));
            }
            if (val."@prefix".typeCode() != NT_BOOLEAN) {
                throwJsonLdError(JSONLD_ERR_INVALID_PREFIX_VALUE,
                    sprintf("@prefix must be a boolean; got %s", val."@prefix".type()));
            }
            definition.prefix = val."@prefix";
            if (definition.prefix && isJsonLdKeyword(definition.iriMapping)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("term %y with keyword IRI mapping cannot be a prefix", term));
            }
            # A relative IRI cannot be used as a prefix
            if (definition.prefix && exists definition.iriMapping
                    && !isAbsoluteIri(definition.iriMapping)
                    && !isBlankNodeIdentifier(definition.iriMapping)) {
                throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                    sprintf("term %y with relative IRI mapping cannot be a prefix", term));
            }
        }

        # W3C step 26: Validate no unknown keys in term definition object
        if (val.typeCode() == NT_HASH) {
            hash<string, bool> valid_td_keys = {
                "@id": True, "@reverse": True, "@container": True, "@context": True,
                "@direction": True, "@index": True, "@language": True, "@nest": True,
                "@prefix": True, "@protected": True, "@type": True,
            };
            foreach string vk in (keys val) {
                if (!valid_td_keys{vk}) {
                    throwJsonLdError(JSONLD_ERR_INVALID_TERM_DEFINITION,
                        sprintf("unknown key %y in term definition for %y", vk, term));
                }
            }
        }

        # Set the term definition
        result.termDefinitions{term} = definition;
        defined{term} = True;

        # Check protected term redefinition
        checkProtectedRedefinition(previous_definition, definition, term, override_protected, \result);
    }

    #! Checks if a protected term is being redefined incompatibly
    /** When a redefinition is identical (modulo protected flag), the term retains
        its protected status per W3C spec step 7.

        @param result reference to the active context being built (to update protected flag)
    */
    private checkProtectedRedefinition(*hash<JsonLdTermDefinition> previous,
            hash<JsonLdTermDefinition> current, string term, bool override_protected,
            *reference<hash<JsonLdActiveContext>> result) {
        if (!exists previous || !previous.protected_ || override_protected) {
            return;
        }

        # Allow identical redefinition (modulo protected flag and scoped context)
        if (previous.iriMapping == current.iriMapping
                && previous.typeMapping == current.typeMapping
                && previous.containerMapping == current.containerMapping
                && previous.languageMapping == current.languageMapping
                && previous.directionMapping == current.directionMapping
                && previous.reverse == current.reverse
                && previous.hasLocalContext == current.hasLocalContext) {
            # Retain protected flag from previous definition
            if (exists result) {
                result.termDefinitions{term}.protected_ = True;
            }
            return;
        }

        throwJsonLdError(JSONLD_ERR_PROTECTED_TERM_REDEFINITION,
            sprintf("cannot redefine protected term %y", term));
    }

    #! Expands a term IRI during term definition creation
    private string expandTermIri(reference<hash<JsonLdActiveContext>> result, hash<auto> local_context,
            string value, reference<hash<string, bool>> defined, *string base_url,
            *list<string> remote_contexts, bool validate_scoped) {
        if (isJsonLdKeyword(value)) {
            return value;
        }

        if (isKeywordLike(value)) {
            # Unknown keyword-like values stay as-is
            return value;
        }

        # Check for compact IRI FIRST (before term lookup to avoid false cycles)
        *list<string> parts = (value =~ x/^([^:]+):(.*)$/);
        if (parts) {
            string prefix = parts[0];
            string suffix = parts[1];

            if (prefix == "_" || suffix =~ /^\/\//) {
                # Blank node or absolute IRI
                return value;
            }

            # If prefix is in local context, ensure it's defined
            if (exists local_context{prefix}) {
                createTermDefinition(\result, local_context, prefix, \defined, False,
                    base_url, remote_contexts, validate_scoped);
            }

            if (exists result.termDefinitions{prefix}) {
                return result.termDefinitions{prefix}.iriMapping + suffix;
            }

            return value;
        }

        # If value is in local context and not yet defined, define it first
        # (only for non-compact-IRI values to avoid false cycles)
        # Skip if defined{value} is explicitly False (being defined - would cause cycle)
        if (local_context.hasKey(value) && !exists defined{value}) {
            createTermDefinition(\result, local_context, value, \defined, False,
                base_url, remote_contexts, validate_scoped);
        }

        # If value is an already-defined term, use its IRI mapping
        if (exists result.termDefinitions{value} && exists result.termDefinitions{value}.iriMapping) {
            return result.termDefinitions{value}.iriMapping;
        }

        # Check for vocab mapping
        if (exists result.vocabularyMapping) {
            return result.vocabularyMapping + value;
        }

        return value;
    }

    #! Resolves a term IRI against the active context
    private string resolveTermIri(hash<JsonLdActiveContext> ctx, string term) {
        if (isAbsoluteIri(term)) {
            return term;
        }
        if (exists ctx.vocabularyMapping) {
            return ctx.vocabularyMapping + term;
        }
        if (exists ctx.baseIri) {
            return resolveIri(ctx.baseIri, term);
        }
        return term;
    }

    #! Expands a compact IRI in the context of the active context (for \@vocab expansion)
    private string expandIriInContext(hash<JsonLdActiveContext> ctx, string value, bool vocab = False) {
        if (isJsonLdKeyword(value)) {
            return value;
        }

        # Check for compact IRI
        *list<string> parts = (value =~ x/^([^:]+):(.*)$/);
        if (parts) {
            string prefix = parts[0];
            string suffix = parts[1];

            if (prefix == "_" || suffix =~ /^\/\//) {
                return value;
            }

            if (exists ctx.termDefinitions{prefix}) {
                return ctx.termDefinitions{prefix}.iriMapping + suffix;
            }

            return value;
        }

        if (vocab && exists ctx.vocabularyMapping) {
            return ctx.vocabularyMapping + value;
        }

        return value;
    }

    #! Creates a new active context with base IRI set
    /** @param base_iri the base IRI
        @return a new active context

        @since json 1.12
    */
    static hash<JsonLdActiveContext> newActiveContext(*string base_iri) {
        return <JsonLdActiveContext>{
            "baseIri": base_iri,
            "originalBaseIri": base_iri,
            "termDefinitions": {},
        };
    }
}

} # namespace JsonLd
