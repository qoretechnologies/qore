# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaImporter.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! An upstream schema document as downloaded, before anything has been done to it
/** The checksum and size describe the **unmodified** bytes, which is what the next import has to be diffed
    against; a checksum of the pruned artifact would be useless for that.
*/
public hashdecl RestSchemaSourceInfo {
    #! The location the document was read from
    string source;

    #! The document text
    string text;

    #! The SHA-256 checksum of the document as downloaded
    string sha256;

    #! The size in bytes of the document as downloaded
    int size;

    #! Whether the document is JSON rather than YAML
    bool json;

    #! The parsed document
    hash<auto> schema;

    #! The number of paths the document declares
    int paths;

    #! The number of method/path operations the document declares
    int operations;

    #! The number of component schemas the document declares
    int schemas;

    #! The number of webhook event types the document declares
    int webhooks;
}

#! One requested operation, resolved against a source document
/** @ref RestSchemaActions::RestSchemaOperationMatchInfo::normalized "normalized" says whether the document
    spells the path differently from the request.  It matters because a manifest has to use the document's
    own spelling to resolve at run time, so a normalized match is a signal that the manifest needs updating —
    not something to be silently accepted.
*/
public hashdecl RestSchemaOperationMatchInfo {
    #! The HTTP method, upper case
    string method;

    #! The path as requested
    string requested_path;

    #! The path as the document declares it
    string path;

    #! Whether the document spells the path differently from the request
    bool normalized;
}

#! One declared repair applied to a source document before it is pruned
/** A vendor's description is sometimes **incomplete** rather than merely unhelpful: it omits something the
    API accepts and the vendor documents elsewhere.  Bitbucket describes \c page and \c pagelen on a shared
    pagination page rather than per endpoint, and declares them on 3 of its 331 operations — so an action
    generated from it cannot ask for the second page of anything.

    That cannot be fixed in a UX overlay: an overlay expresses presentation, and adding a parameter is
    structural.  It is fixed here instead, before pruning, so that:

    - the repair is **declared**, with a reason, in the application's import spec rather than buried in
      code;
    - it is **checked**: naming an operation the document does not declare, or a parameter it already
      declares, is an error — so a normalization that upstream has since fixed fails the next import
      instead of quietly staying forever;
    - it is **recorded** in the committed artifact under \c "x-qore-normalized", so that a reviewer diffing
      the artifact against the upstream document is told why they differ.

    The provenance checksum still describes the unmodified upstream document, because that is the baseline
    the next import has to be diffed against.
*/
public hashdecl RestSchemaNormalizationInfo {
    #! Why the document has to be repaired
    /** Recorded in the artifact; write what the vendor documents and where, so the next person can check
        whether it is still true.
    */
    string reason;

    #! The operations to repair
    list<hash<SchemaOperationInfo>> operations;

    #! Parameters to add, each an OpenAPI parameter object
    list<hash<auto>> add_parameters;
}

#! Describes one document to import
public hashdecl RestSchemaImportInfo {
    #! A label for the document, used in messages and in the summary
    string name;

    #! The URL or file path to import from
    string source;

    #! The path of the pruned document to write
    string output;

    #! The operations to keep
    list<hash<SchemaOperationInfo>> operations;

    #! Repairs applied to the document before it is pruned
    /** @see @ref RestSchemaActions::RestSchemaNormalizationInfo "RestSchemaNormalizationInfo"
    */
    *list<hash<RestSchemaNormalizationInfo>> normalize;

    #! Write JSON rather than YAML; defaults to the format of the source document
    *bool json;
}

#! The result of importing one document
/** Everything a @c \<App\>SchemaProvenanceInfo record needs is here, plus what the import itself did.

    @see @ref RestSchemaActions::RestSchemaImporter::getProvenance() "RestSchemaImporter::getProvenance()"
*/
public hashdecl RestSchemaImportResultInfo {
    #! The document label
    string name;

    #! The location the document was imported from
    string source;

    #! The SHA-256 checksum of the unmodified upstream document
    string source_sha256;

    #! The size in bytes of the unmodified upstream document
    int source_size;

    #! The date of the import
    date imported;

    #! The number of paths the upstream document declares
    int source_paths;

    #! The number of operations the upstream document declares
    int source_operations;

    #! The number of component schemas the upstream document declares
    int source_schemas;

    #! The number of webhook event types the upstream document declares
    int source_webhooks;

    #! The number of paths kept
    int paths_kept;

    #! The number of operations kept
    int operations_kept;

    #! The number of local references reachable from the kept operations
    int refs_kept;

    #! The path the pruned document was written to, or @ref nothing for a dry run
    *string output;

    #! The size in bytes of the pruned document
    int output_size;

    #! Every requested operation, with the path the document actually declares
    list<hash<RestSchemaOperationMatchInfo>> matches;

    #! The number of operations repaired before pruning
    int normalized_operations = 0;

    #! The pruned document
    hash<auto> schema;
}

#! Imports a vendor's REST schema into the pinned, pruned form an application ships
/** An application's actions are generated from a schema **committed to the module**, never one fetched at
    run time, because an upstream change must not be able to alter the contract of an action that existing
    workflows already call.  Producing that committed artifact is therefore a reviewed, reproducible step:

    1. download a named upstream revision, and record its checksum, size and counts;
    2. prune it to the operations the manifest exports, plus everything they reference;
    3. write the pruned document;
    4. compare the result against the action set the application ships, with
       @ref RestSchemaActions::RestSchemaDrift "RestSchemaDrift";
    5. update the provenance record in the same commit as the schema file.

    This class performs 1-3 and 5 and supplies the numbers 5 needs;
    @ref RestSchemaActions::RestSchemaDrift "RestSchemaDrift" performs 4.  The
    \c tools/rest-schema-import.qr program is the command-line front end.

    @par Why path matching is normalized

    A manifest names an operation by method and path template, and the obvious implementation compares that
    path to the document's own as a string.  That produces **false negatives**, and they look exactly like an
    operation that has been withdrawn:

    |!What differs|!Example
    |a trailing slash|the document declares \c "/boards/" where the manifest writes \c "/boards"
    |a path-parameter name|the document declares \c "/contacts/{contact_id}" where the manifest writes \c "/contacts/{id}"

    Neither changes the operation: a path parameter's name is scoped to its own operation, and a trailing
    slash is not a distinct resource in any of these documents.  So both are normalized away when resolving,
    and every match that was only reachable through normalization is **reported** — because the manifest
    still has to be written with the document's own spelling for
    @ref RestSchemaActions::RestSchemaActionSet "RestSchemaActionSet" to resolve it at run time.

    @par Multi-document applications

    An application whose API is split across documents served from different base paths — Xero, for one —
    imports each document separately and ships one @ref RestSchemaActions::RestSchemaActionSet
    "RestSchemaActionSet" per document.  @ref importAll() does the whole set in one call and returns one
    result per document.
*/
public class RestSchemaImporter {
    public {
        #! The document member recording what an import repaired
        /** A vendor extension, so the artifact stays a valid schema; it is what tells a reviewer diffing
            the committed document against the upstream one why they differ.
        */
        const NormalizationMarker = "x-qore-normalized";

        #! The provenance fields this class can supply
        /** Used by @ref updateProvenanceSource() to decide which members of an application's provenance
            record it owns.  Anything else in that record — Stripe's \c api_version, say — is left alone.
        */
        const ProvenanceFields = (
            "source_url",
            "source_sha256",
            "source_size",
            "imported",
            "source_paths",
            "source_operations",
            "source_schemas",
            "source_webhooks",
        );
    }

    #! Imports one document: download, prune, write
    /** @param doc the document to import
        @param opts import options:
        - \c validate: (default @ref True) re-parse the written document with
          @ref OpenApi3::OpenApi3Loader "OpenApi3Loader", so that an artifact that cannot be loaded is
          caught here rather than when the module is next built
        - \c dry_run: (default @ref False) do everything except write the file

        @return what was imported and what was kept

        @throw REST-SCHEMA-IMPORT-ERROR a requested operation does not resolve against the document, or
        resolves ambiguously
        @throw REST-SCHEMA-PRUNE-ERROR pruning failed or produced an invalid document
    */
    static hash<RestSchemaImportResultInfo> import(hash<RestSchemaImportInfo> doc, *hash<auto> opts) {
        on_error rethrow $1.err, sprintf("%s (while importing the %s schema from %y)", $1.desc, doc.name,
            doc.source);

        hash<RestSchemaSourceInfo> src = RestSchemaImporter::fetch(doc.source);

        # repairs are applied before anything else looks at the document, so that pruning, resolution and
        # the written artifact all see the same thing
        hash<auto> source_schema = src.schema;
        int normalized_operations = 0;
        if (doc.normalize) {
            source_schema = RestSchemaImporter::normalize(source_schema, doc.normalize,
                \normalized_operations);
        }

        list<hash<RestSchemaOperationMatchInfo>> matches = RestSchemaImporter::resolveOperations(
            source_schema, doc.operations);
        list<hash<SchemaOperationInfo>> resolved = ();
        foreach hash<RestSchemaOperationMatchInfo> m in (matches) {
            resolved += <SchemaOperationInfo>{"method": m.method, "path": m.path};
        }

        hash<PruneResultInfo> pruned = RestSchemaPruner::prune(source_schema, resolved);

        # the artifact says how it differs from the document it came from.  This is stamped after pruning
        # because the pruner keeps only what the selected operations reach, and a record of what was
        # repaired is reachable from nothing
        hash<auto> out_schema = pruned.schema;
        if (doc.normalize) {
            list<auto> applied = ();
            foreach hash<RestSchemaNormalizationInfo> n in (doc.normalize) {
                applied += {
                    "reason": n.reason,
                    "operations": (map sprintf("%s %s", $1.method.upr(), $1.path), n.operations),
                    "add_parameters": (map $1.name, n.add_parameters),
                };
            }
            out_schema{NormalizationMarker} = applied;
        }

        bool json = exists doc.json ? doc.json : src.json;
        string text = RestSchemaImporter::serialize(out_schema, json);

        if (!opts.dry_run) {
            FileLocationHandler::writeFileToLocation(doc.output, text);
            if (opts.validate ?? True) {
                on_error rethrow $1.err, sprintf("%s (the pruned document written to %y does not load; this "
                    "is a defect in the import, not in the application)", $1.desc, doc.output);
                OpenApi3::OpenApi3Loader::fromFile(doc.output);
            }
        }

        return <RestSchemaImportResultInfo>{
            "name": doc.name,
            "source": doc.source,
            "source_sha256": src.sha256,
            "source_size": src.size,
            # the day, not the instant: a provenance record says when a revision was taken, and a
            # timestamp there would read as precision the artifact does not have
            "imported": date(now().format("YYYYMMDD")),
            "source_paths": src.paths,
            "source_operations": src.operations,
            "source_schemas": src.schemas,
            "source_webhooks": src.webhooks,
            "paths_kept": pruned.paths_kept,
            "operations_kept": pruned.operations_kept,
            "refs_kept": pruned.refs_kept,
            "output": opts.dry_run ? NOTHING : doc.output,
            "output_size": text.strlen(),
            "matches": matches,
            "normalized_operations": normalized_operations,
            "schema": out_schema,
        };
    }

    #! Imports every document of a multi-document application
    /** @param docs the documents to import
        @param opts import options; see @ref import()

        @return one result per document, in the order given
    */
    static list<hash<RestSchemaImportResultInfo>> importAll(list<hash<RestSchemaImportInfo>> docs,
            *hash<auto> opts) {
        list<hash<RestSchemaImportResultInfo>> rv = ();
        foreach hash<RestSchemaImportInfo> doc in (docs) {
            rv += RestSchemaImporter::import(doc, opts);
        }
        return rv;
    }

    #! Downloads a document and describes it
    /** @param source a URL or file path

        @return the document, its checksum and size as downloaded, and its counts

        @throw REST-SCHEMA-IMPORT-ERROR the document is not a hash, or declares no paths
    */
    static hash<RestSchemaSourceInfo> fetch(string source) {
        binary raw = FileLocationHandler::getBinaryFileFromLocation(source);
        # the checksum and size describe the bytes as downloaded, so they are taken before any decoding
        string sha256 = SHA256(raw);
        int size = raw.size();

        string text = binary_to_string(raw, "UTF-8");
        # the format is sniffed rather than taken from the file name: GitHub serves its description as
        # ".json" and Xero as ".yaml", but a vendor is free to serve either under either name, and a
        # document parsed as the wrong format fails in a way that says nothing useful
        bool json = (text =~ x/^\s*(.)/)[0] == "{";

        auto parsed;
        {
            on_error rethrow $1.err, sprintf("%s (while parsing %y as %s)", $1.desc, source,
                json ? "JSON" : "YAML");
            parsed = json ? parse_json(text) : parse_yaml(text);
        }
        if (parsed.typeCode() != NT_HASH) {
            throw "REST-SCHEMA-IMPORT-ERROR", sprintf("%y parsed as %s, not as a schema document", source,
                parsed.type());
        }
        hash<auto> schema = parsed;
        if (!schema.paths) {
            throw "REST-SCHEMA-IMPORT-ERROR", sprintf("%y declares no paths, so it is not a usable REST "
                "schema; top-level members: %y", source, keys schema);
        }

        int operations = 0;
        foreach auto item in (schema.paths.iterator()) {
            if (item.typeCode() == NT_HASH) {
                operations += RestSchemaPruner::methodsOf(item).size();
            }
        }

        return <RestSchemaSourceInfo>{
            "source": source,
            "text": text,
            "sha256": sha256,
            "size": size,
            "json": json,
            "schema": schema,
            "paths": (keys schema.paths).size(),
            "operations": operations,
            "schemas": (keys (schema.components.schemas ?? schema.definitions ?? {})).size(),
            "webhooks": (keys (schema.webhooks ?? schema."x-webhooks" ?? {})).size(),
        };
    }

    #! Applies declared repairs to a document
    /** @param schema the source document; not modified
        @param normalizations the repairs to apply
        @param operations_normalized a reference receiving the number of operations repaired

        @return a new document with the repairs applied

        @throw REST-SCHEMA-IMPORT-ERROR a repair names an operation the document does not declare, or a
        parameter it already declares

        @note A parameter the document already declares is an error rather than a no-op: it means the
        vendor has fixed the omission, so the repair has done its job and must be removed.  Silently
        accepting it would leave a permanent local edit nobody ever revisits.

        @see @ref RestSchemaActions::RestSchemaNormalizationInfo "RestSchemaNormalizationInfo"
    */
    static hash<auto> normalize(hash<auto> schema, list<hash<RestSchemaNormalizationInfo>> normalizations,
            reference<int> operations_normalized) {
        hash<auto> rv = schema;
        foreach hash<RestSchemaNormalizationInfo> n in (normalizations) {
            if (!n.reason.val()) {
                throw "REST-SCHEMA-IMPORT-ERROR", "a schema repair must say why it is being made; the "
                    "reason is written into the artifact for whoever next diffs it against upstream";
            }
            foreach hash<SchemaOperationInfo> op in (n.operations) {
                string method = op.method.lwr();
                if (!exists rv.paths{op.path}) {
                    throw "REST-SCHEMA-IMPORT-ERROR", sprintf("the repair %y names operation \"%s %s\", "
                        "but the document declares no such path", n.reason, op.method.upr(), op.path);
                }
                if (!exists rv.paths{op.path}{method}) {
                    throw "REST-SCHEMA-IMPORT-ERROR", sprintf("the repair %y names operation \"%s %s\", "
                        "but the document declares only %y for that path", n.reason, op.method.upr(),
                        op.path, RestSchemaPruner::methodsOf(rv.paths{op.path}));
                }
                hash<auto> operation = rv.paths{op.path}{method};
                # a parameter may be declared on the operation or on the path item, and either counts
                hash<string, bool> declared = {};
                foreach auto p in ((rv.paths{op.path}.parameters ?? ()) + (operation.parameters ?? ())) {
                    if (p.typeCode() == NT_HASH && p.name.val()) {
                        declared{p.name} = True;
                    }
                }
                list<auto> add = ();
                foreach hash<auto> p in (n.add_parameters) {
                    if (!p.name.val()) {
                        throw "REST-SCHEMA-IMPORT-ERROR", sprintf("the repair %y adds a parameter with no "
                            "name", n.reason);
                    }
                    if (declared{p.name}) {
                        throw "REST-SCHEMA-IMPORT-ERROR", sprintf("the repair %y adds parameter %y to "
                            "\"%s %s\", which already declares it; the document has been fixed upstream, "
                            "so the repair must be removed", n.reason, p.name, op.method.upr(), op.path);
                    }
                    add += p;
                }
                operation.parameters = (operation.parameters ?? ()) + add;
                rv.paths{op.path}{method} = operation;
                ++operations_normalized;
            }
        }
        return rv;
    }

    #! Resolves requested operations against a document, normalizing path spelling
    /** @param schema the source document
        @param operations the operations to resolve

        @return one match per requested operation, in the order given

        @throw REST-SCHEMA-IMPORT-ERROR an operation does not resolve, or its normalized form matches more
        than one declared path

        @note An exact match always wins, so normalization can never redirect an operation that the document
        declares literally.
    */
    static list<hash<RestSchemaOperationMatchInfo>> resolveOperations(hash<auto> schema,
            list<hash<SchemaOperationInfo>> operations) {
        if (!operations) {
            throw "REST-SCHEMA-IMPORT-ERROR", "no operations were selected; importing a schema pruned to "
                "nothing is never intended, so it is an error rather than an empty result";
        }

        # normalized path -> the paths the document declares with that normal form
        hash<string, list<string>> by_normal = {};
        foreach string path in (keys schema.paths) {
            by_normal{RestSchemaImporter::normalizePath(path)} += path;
        }

        list<hash<RestSchemaOperationMatchInfo>> rv = ();
        foreach hash<SchemaOperationInfo> op in (operations) {
            string method = op.method.upr();
            string path = op.path;
            if (!exists schema.paths{path}) {
                string normal = RestSchemaImporter::normalizePath(path);
                *list<string> candidates = by_normal{normal};
                if (!candidates) {
                    throw "REST-SCHEMA-IMPORT-ERROR", sprintf("operation \"%s %s\" was selected, but the "
                        "document declares no such path, with or without normalizing trailing slashes and "
                        "path-parameter names; %d path(s) are declared", method, path,
                        (keys schema.paths).size());
                }
                if (candidates.size() > 1) {
                    throw "REST-SCHEMA-IMPORT-ERROR", sprintf("operation \"%s %s\" matches %d declared "
                        "paths once trailing slashes and path-parameter names are normalized: %y; the "
                        "manifest has to name one of them exactly", method, path, candidates.size(),
                        candidates);
                }
                path = candidates[0];
            }
            if (!inlist(method.lwr(), RestSchemaPruner::methodsOf(schema.paths{path}))) {
                throw "REST-SCHEMA-IMPORT-ERROR", sprintf("operation \"%s %s\" was selected, but the "
                    "document declares only %y for %y", method, op.path,
                    RestSchemaPruner::methodsOf(schema.paths{path}), path);
            }
            rv += <RestSchemaOperationMatchInfo>{
                "method": method,
                "requested_path": op.path,
                "path": path,
                "normalized": path != op.path,
            };
        }
        return rv;
    }

    #! Returns the comparable form of a path template
    /** Removes the two differences that are not differences: a trailing slash, and the **name** of a path
        parameter, which is scoped to the operation that declares it.

        @param path the path template

        @return the path with any trailing slash removed and every \c "{name}" reduced to \c "{}"
    */
    static string normalizePath(string path) {
        string rv = path;
        # "/" itself is a path, not a trailing slash
        if (rv.size() > 1 && rv[rv.size() - 1] == "/") {
            rv = rv.substr(0, rv.size() - 1);
        }
        rv =~ s/{[^}]*}/{}/g;
        return rv;
    }

    #! Returns the distinct operations a manifest exports
    /** Deduplicated, because a manifest may export one operation as more than one action — see
        @ref RestSchemaActions::RestSchemaActionInfo::variant_of "variant_of" — and pruning the same
        operation twice is not meaningful.

        @param manifest the application's manifest

        @return the distinct method/path pairs, in manifest order
    */
    static list<hash<SchemaOperationInfo>> operationsFromManifest(list<hash<RestSchemaActionInfo>> manifest) {
        hash<string, bool> seen = {};
        list<hash<SchemaOperationInfo>> rv = ();
        foreach hash<RestSchemaActionInfo> info in (manifest) {
            string key = info.method.upr() + " " + info.path;
            if (seen{key}) {
                continue;
            }
            seen{key} = True;
            rv += <SchemaOperationInfo>{"method": info.method, "path": info.path};
        }
        return rv;
    }

    #! Parses operations written as \c "METHOD /path"
    /** Blank lines and \c "#" comments are ignored, so a selection can be kept in a file with notes in it.

        @param specs the operation strings

        @return the parsed operations, in the order given

        @throw REST-SCHEMA-IMPORT-ERROR an entry is not \c "METHOD /path"
    */
    static list<hash<SchemaOperationInfo>> parseOperations(softlist<string> specs) {
        list<hash<SchemaOperationInfo>> rv = ();
        foreach string spec in (specs) {
            string line = spec;
            line =~ s/#.*$//;
            trim line;
            if (!line.val()) {
                continue;
            }
            *list<*string> m = (line =~ x/^([A-Za-z]+)[[:space:]]+(\/\S*)$/);
            if (!m) {
                throw "REST-SCHEMA-IMPORT-ERROR", sprintf("operation %y is not of the form "
                    "\"METHOD /path\"", spec);
            }
            rv += <SchemaOperationInfo>{"method": m[0], "path": m[1]};
        }
        if (!rv) {
            throw "REST-SCHEMA-IMPORT-ERROR", "no operations were given";
        }
        return rv;
    }

    #! Serializes a pruned document
    /** @param schema the document
        @param json write JSON rather than YAML

        @return the serialized document, with a trailing newline

        @note %Qore hashes preserve insertion order and
        @ref RestSchemaActions::RestSchemaPruner "RestSchemaPruner" emits members in the source document's
        own order, so the same input and selection always produce byte-identical output.
    */
    static string serialize(hash<auto> schema, bool json) {
        string rv = json
            ? make_json(schema, JGF_ADD_FORMATTING)
            : make_yaml(schema, YAML::BlockStyle);
        if (rv.substr(-1) != "\n") {
            rv += "\n";
        }
        return rv;
    }

    #! Returns the provenance values an import establishes
    /** @param res an import result

        @return the values of the provenance members this class owns, ready to be written into an
        application's \c \<App\>SchemaProvenanceInfo record

        @note \c source_updated is **not** returned: when the upstream document was last changed is a
        property of the vendor's repository, not of the document, so it has to be recorded by whoever ran
        the import.
    */
    static hash<auto> getProvenance(hash<RestSchemaImportResultInfo> res) {
        return {
            "source_url": res.source,
            "source_sha256": res.source_sha256,
            "source_size": res.source_size,
            "imported": res.imported,
            "source_paths": res.source_paths,
            "source_operations": res.source_operations,
            "source_schemas": res.source_schemas,
            "source_webhooks": res.source_webhooks,
        };
    }

    #! Rewrites the provenance values in an application's schema source
    /** Only the members named in @ref values are touched, and only inside the named constant, so an
        application-specific member — Stripe's \c api_version, which has to move with its schema — is left
        exactly as it was.

        @param src the source of the application's \c \<App\>Schema.qc
        @param values the values to write, keyed by member name
        @param const_name the constant holding the provenance record

        @return the rewritten source

        @throw REST-SCHEMA-IMPORT-ERROR the constant was not found, or a member named in @ref values is not
        declared in it

        @note A member the record does not declare is an error rather than an addition: writing one would
        produce source that does not compile, and the failure would surface far from here.
    */
    static string updateProvenanceSource(string src, hash<auto> values, string const_name = "Provenance") {
        list<string> lines = split("\n", src, False, True);
        string decl = sprintf("^[[:space:]]*const[[:space:]]+%s[[:space:]]*=", const_name);
        int start = -1;
        foreach int i in (xrange(lines.size())) {
            if (regex(lines[i], decl)) {
                start = i;
                break;
            }
        }
        if (start < 0) {
            throw "REST-SCHEMA-IMPORT-ERROR", sprintf("no %y constant was found in the given source",
                const_name);
        }

        hash<string, bool> written = {};
        int i = start;
        while (++i < lines.size()) {
            # the record ends at the closing "};" at the constant's own indentation or shallower; a nested
            # hash closes with "}," or a deeper "};" and must not end the scan
            if (lines[i] =~ /^[[:space:]]*};[[:space:]]*$/) {
                break;
            }
            *list<*string> m = (lines[i] =~ x/^([[:space:]]*)"([A-Za-z_][A-Za-z0-9_]*)":[[:space:]]*(.*)$/);
            if (!m) {
                continue;
            }
            string name = m[1];
            if (!values.hasKey(name)) {
                continue;
            }
            # keep whatever trailed the value: a comma, and any comment after it
            string tail = m[2];
            string trailer = (tail =~ x/(,[[:space:]]*(?:#.*)?)$/)[0] ?? "";
            lines[i] = sprintf("%s\"%s\": %s%s", m[0], name, RestSchemaImporter::renderValue(values{name}),
                trailer);
            written{name} = True;
        }

        list<string> missing = ();
        foreach string name in (keys values) {
            if (!written{name}) {
                missing += name;
            }
        }
        if (missing) {
            throw "REST-SCHEMA-IMPORT-ERROR", sprintf("the %y constant does not declare member(s) %y, so "
                "they cannot be updated; declaring them is a source change, not an import", const_name,
                sort(missing));
        }
        return join("\n", lines);
    }

    #! Rewrites the provenance values in an application's schema file
    /** @param path the path of the application's \c \<App\>Schema.qc
        @param values the values to write, keyed by member name
        @param const_name the constant holding the provenance record

        @see @ref updateProvenanceSource()
    */
    static updateProvenanceFile(string path, hash<auto> values, string const_name = "Provenance") {
        on_error rethrow $1.err, sprintf("%s (while updating the provenance record in %y)", $1.desc, path);
        string src = File::readTextFile(path);
        FileLocationHandler::writeFileToLocation(path, RestSchemaImporter::updateProvenanceSource(src, values,
            const_name));
    }

    #! Returns a human-readable summary of an import
    /** @param res the import results

        @return a summary suitable for printing during an import
    */
    static string getReport(softlist<hash<RestSchemaImportResultInfo>> res) {
        string rv = "";
        foreach hash<RestSchemaImportResultInfo> r in (res) {
            rv += sprintf("%s: %s\n", r.name, r.source);
            rv += sprintf("  upstream: %d bytes, %d path(s), %d operation(s), %d schema(s), %d webhook(s)\n",
                r.source_size, r.source_paths, r.source_operations, r.source_schemas, r.source_webhooks);
            rv += sprintf("  sha-256:  %s\n", r.source_sha256);
            rv += sprintf("  pruned:   %d bytes, %d path(s), %d operation(s), %d reference(s)\n",
                r.output_size, r.paths_kept, r.operations_kept, r.refs_kept);
            if (r.normalized_operations) {
                rv += sprintf("  repaired: %d operation(s) before pruning; the artifact records this "
                    "under %y\n", r.normalized_operations, NormalizationMarker);
            }
            list<hash<RestSchemaOperationMatchInfo>> norm = ();
            foreach hash<RestSchemaOperationMatchInfo> m in (r.matches) {
                if (m.normalized) {
                    norm += m;
                }
            }
            if (norm) {
                rv += sprintf("  %d operation(s) matched only after normalizing the path; the manifest must "
                    "use the document's spelling:\n", norm.size());
                map rv += sprintf("    %s %s -> %s\n", $1.method, $1.requested_path, $1.path), norm;
            }
            rv += sprintf("  written:  %s\n", r.output ?? "(dry run)");
        }
        return rv;
    }

    #! Renders a value as %Qore source
    /** @param v the value

        @return the value as it would be written in source

        @throw REST-SCHEMA-IMPORT-ERROR the value is of a type this class does not render
    */
    private static string renderValue(auto v) {
        switch (v.typeCode()) {
            case NT_STRING: {
                return sprintf("%y", v);
            }
            case NT_INT: {
                return sprintf("%d", v);
            }
            case NT_BOOLEAN: {
                return v ? "True" : "False";
            }
            case NT_DATE: {
                # an absolute date is written as a %Qore date literal; the time is dropped because these
                # records carry days, and a literal with a time in it reads as false precision
                return v.format("YYYY-MM-DD");
            }
        }
        throw "REST-SCHEMA-IMPORT-ERROR", sprintf("cannot render a value of type %y as %%Qore source",
            v.type());
    }
}
}
