# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file MetadataStore.qc Public API facade for QoreApiMetadata

/*  MetadataStore.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @cond nodoc
public namespace QoreApiMetadata {
#! @endcond

#! Main facade class for the QoreApiMetadata module
/** Provides unified entry points for metadata extraction, merging,
    serialization, and search.

    @par Example: Extract from source
    @code{.py}
hash<NamespaceInfo> meta =
    QoreApiMetadata::fromSourceFile("/path/to/module.qm");
    @endcode

    @par Example: Full merge pipeline
    @code{.py}
hash<NamespaceInfo> meta = QoreApiMetadata::fromRuntimeWithDocs(
    "Qore",
    ("/path/to/source.q",),
    ("/path/to/metadata/",),
);
    @endcode

    @par Example: Search
    @code{.py}
list<hash<auto>> results =
    QoreApiMetadata::searchSymbols(meta, "Mutex*");
    @endcode
*/
public class QoreApiMetadata {
    #! Extract metadata from a source file via astparser
    /** @param path file path to parse
        @param options optional hash with:
        - \c defines: list of define symbols for conditional parsing
        @return NamespaceInfo tree
    */
    static hash<NamespaceInfo> fromSourceFile(string path,
            *hash<auto> options) {
        return AstMetadataExtractor::extractFromFile(path, options);
    }

    #! Extract metadata from a source string via astparser
    /** @param source Qore source code
        @param options optional hash with:
        - \c defines: list of define symbols for conditional parsing
        - \c uri: virtual URI for the source
        @return NamespaceInfo tree
    */
    static hash<NamespaceInfo> fromSourceString(string source,
            *hash<auto> options) {
        return AstMetadataExtractor::extractFromString(source, options);
    }

    #! Extract metadata from the runtime via reflection
    /** @param namespace_path namespace to extract (e.g., "Qore::Thread"),
        or NOTHING for the root "Qore" namespace
        @return NamespaceInfo tree
    */
    static hash<NamespaceInfo> fromRuntime(*string namespace_path) {
        return ReflectionMetadataExtractor::extractFromRuntime(
            namespace_path);
    }

    #! Extract runtime metadata enriched with documentation
    /** This is the full merge pipeline: extracts runtime structure via
        reflection, optionally parses source files for documentation,
        and optionally loads qpp .meta.json documentation. The merger
        prioritizes source-level documentation over qpp documentation.

        @param namespace_path namespace to extract (or NOTHING for "Qore")
        @param source_paths optional list of source file paths to parse
            for documentation
        @param meta_json_paths optional list of .meta.json file paths
            or directory paths containing .meta.json files
        @return enriched NamespaceInfo tree
    */
    static hash<NamespaceInfo> fromRuntimeWithDocs(
            *string namespace_path,
            *list<string> source_paths,
            *list<string> meta_json_paths) {
        # step 1: extract runtime structure
        hash<NamespaceInfo> reflection_meta =
            ReflectionMetadataExtractor::extractFromRuntime(
                namespace_path);

        # step 2: extract source-level metadata (if available)
        *hash<NamespaceInfo> ast_meta;
        if (exists source_paths && source_paths.size()) {
            # merge all source files into one namespace tree
            foreach string path in (source_paths) {
                hash<NamespaceInfo> file_meta =
                    AstMetadataExtractor::extractFromFile(path);
                if (!exists ast_meta) {
                    ast_meta = file_meta;
                } else {
                    QoreApiMetadata::mergeNamespaceInto(\ast_meta,
                        file_meta);
                }
            }
        }

        # step 3: build qpp doc index (if available)
        *hash<string, hash<auto>> doc_index;
        if (exists meta_json_paths && meta_json_paths.size()) {
            list<string> meta_files = ();
            foreach string path in (meta_json_paths) {
                if (is_dir(path)) {
                    # build from directory
                    hash<string, hash<auto>> dir_index =
                        QppDocIndex::buildFromDirectory(path);
                    if (!exists doc_index) {
                        doc_index = dir_index;
                    } else {
                        doc_index += dir_index;
                    }
                } else {
                    meta_files += path;
                }
            }
            if (meta_files.size()) {
                hash<string, hash<auto>> file_index =
                    QppDocIndex::buildFromMetaFiles(meta_files);
                if (!exists doc_index) {
                    doc_index = file_index;
                } else {
                    doc_index += file_index;
                }
            }
        }

        # step 4: merge
        return SymbolMerger::merge(reflection_meta, ast_meta, doc_index);
    }

%ifdef NoJson
    #! Export metadata as JSON
    /** @param metadata the NamespaceInfo tree to serialize
        @return JSON string
        @throws JSON-NOT-AVAILABLE when json module is not available
    */
    static string toJson(hash<NamespaceInfo> metadata) {
        throw "JSON-NOT-AVAILABLE",
            "the json module is required for JSON export";
    }

    #! Import metadata from JSON
    /** @param json_str JSON string to parse
        @return hash following NamespaceInfo structure
        @throws JSON-NOT-AVAILABLE when json module is not available
    */
    static hash<auto> fromJson(string json_str) {
        throw "JSON-NOT-AVAILABLE",
            "the json module is required for JSON import";
    }
%else
    #! Export metadata as JSON
    /** @param metadata the NamespaceInfo tree to serialize
        @return JSON string with schema_version and data fields
    */
    static string toJson(hash<NamespaceInfo> metadata) {
        hash<auto> envelope = {
            "schema_version": SCHEMA_VERSION,
            "data": metadata,
        };
        return make_json(envelope, JGF_ADD_FORMATTING);
    }

    #! Import metadata from JSON
    /** @param json_str JSON string to parse
        @return hash following NamespaceInfo structure (untyped due to
            JSON deserialization limitations)
        @throws SCHEMA-VERSION-ERROR if the schema version is incompatible
    */
    static hash<auto> fromJson(string json_str) {
        hash<auto> envelope = parse_json(json_str);
        QoreApiMetadata::checkSchemaVersion(envelope);
        return envelope.data;
    }
%endif

%ifdef NoYaml
    #! Export metadata as YAML
    /** @param metadata the NamespaceInfo tree to serialize
        @return YAML string
        @throws YAML-NOT-AVAILABLE when yaml module is not available
    */
    static string toYaml(hash<NamespaceInfo> metadata) {
        throw "YAML-NOT-AVAILABLE",
            "the yaml module is required for YAML export";
    }

    #! Import metadata from YAML
    /** @param yaml_str YAML string to parse
        @return hash following NamespaceInfo structure
        @throws YAML-NOT-AVAILABLE when yaml module is not available
    */
    static hash<auto> fromYaml(string yaml_str) {
        throw "YAML-NOT-AVAILABLE",
            "the yaml module is required for YAML import";
    }
%else
    #! Export metadata as YAML
    /** @param metadata the NamespaceInfo tree to serialize
        @return YAML string with schema_version and data fields
    */
    static string toYaml(hash<NamespaceInfo> metadata) {
        hash<auto> envelope = {
            "schema_version": SCHEMA_VERSION,
            "data": metadata,
        };
        return make_yaml(envelope, YAML::BlockStyle);
    }

    #! Import metadata from YAML
    /** @param yaml_str YAML string to parse
        @return hash following NamespaceInfo structure (untyped due to
            YAML deserialization limitations)
        @throws SCHEMA-VERSION-ERROR if the schema version is incompatible
    */
    static hash<auto> fromYaml(string yaml_str) {
        hash<auto> envelope = parse_yaml(yaml_str);
        QoreApiMetadata::checkSchemaVersion(envelope);
        return envelope.data;
    }
%endif

    #! Search for symbols matching a glob pattern
    /** Searches classes, functions, constants, hashdecls, and enums
        in the metadata tree by matching their full_name against the
        given glob pattern.

        @param metadata the NamespaceInfo tree to search
        @param pattern glob pattern (e.g., "Mutex*", "*::lock", "Qore::*")
        @return list of matching symbol info hashes with:
        - \c name: symbol name
        - \c full_name: fully qualified name
        - \c symbol_type: "class", "function", "constant", "hashdecl",
            "enum", "method"
        - \c description: symbol description (if available)
    */
    static list<hash<auto>> searchSymbols(hash<NamespaceInfo> metadata,
            string pattern) {
        list<hash<auto>> results = ();
        Regex re(QoreApiMetadata::globToRegex(pattern));
        QoreApiMetadata::searchInNamespace(metadata, re, \results);
        return results;
    }

    #! Look up a symbol by its fully qualified name
    /** @param metadata the NamespaceInfo tree to search
        @param qualified_name the fully qualified name
            (e.g., "Qore::Thread::Mutex", "Qore::Thread::Mutex::lock")
        @return hash with symbol info, or NOTHING if not found
    */
    static *hash<auto> getSymbolInfo(hash<NamespaceInfo> metadata,
            string qualified_name) {
        return QoreApiMetadata::findSymbolByName(metadata,
            qualified_name);
    }

    #! Merge one namespace tree into another (in-place), deduplicating
    #! by full_name
    private static mergeNamespaceInto(
            reference<hash<NamespaceInfo>> target,
            hash<NamespaceInfo> source) {
        # build sets of existing full_names for O(n) dedup
        hash<string, bool> existing_classes();
        foreach hash<ClassInfo> c in (target.classes) {
            existing_classes{c.full_name} = True;
        }
        foreach hash<ClassInfo> c in (source.classes) {
            if (!existing_classes{c.full_name}) {
                target.classes += c;
            }
        }

        hash<string, bool> existing_functions();
        foreach hash<FunctionInfo> f in (target.functions) {
            existing_functions{f.full_name} = True;
        }
        foreach hash<FunctionInfo> f in (source.functions) {
            if (!existing_functions{f.full_name}) {
                target.functions += f;
            }
        }

        hash<string, bool> existing_constants();
        foreach hash<ConstantInfo> c in (target.constants) {
            existing_constants{c.full_name} = True;
        }
        foreach hash<ConstantInfo> c in (source.constants) {
            if (!existing_constants{c.full_name}) {
                target.constants += c;
            }
        }

        hash<string, bool> existing_hashdecls();
        foreach hash<HashdeclInfo> h in (target.hashdecls) {
            existing_hashdecls{h.full_name} = True;
        }
        foreach hash<HashdeclInfo> h in (source.hashdecls) {
            if (!existing_hashdecls{h.full_name}) {
                target.hashdecls += h;
            }
        }

        hash<string, bool> existing_enums();
        foreach hash<EnumInfo> e in (target.enums) {
            existing_enums{e.full_name} = True;
        }
        foreach hash<EnumInfo> e in (source.enums) {
            if (!existing_enums{e.full_name}) {
                target.enums += e;
            }
        }

        # merge child namespaces: match by name, recurse into matches
        foreach hash<NamespaceInfo> src_ns in (source.namespaces) {
            bool found = False;
            foreach hash<NamespaceInfo> tgt_ns in (\target.namespaces) {
                if (tgt_ns.name == src_ns.name) {
                    QoreApiMetadata::mergeNamespaceInto(\tgt_ns, src_ns);
                    found = True;
                    break;
                }
            }
            if (!found) {
                target.namespaces += src_ns;
            }
        }
    }

    #! Check schema version compatibility
    private static checkSchemaVersion(hash<auto> envelope) {
        if (!exists envelope.schema_version) {
            throw "SCHEMA-VERSION-ERROR",
                "missing schema_version field";
        }
        string version = string(envelope.schema_version);
        # extract major version
        int dot = version.find(".");
        int major;
        if (dot >= 0) {
            major = int(version.substr(0, dot));
        } else {
            major = int(version);
        }
        int our_major = int(SCHEMA_VERSION.substr(
            0, SCHEMA_VERSION.find(".")));
        if (major > our_major) {
            throw "SCHEMA-VERSION-ERROR",
                sprintf("schema version %y is not supported "
                    "(max supported: %s)", version, SCHEMA_VERSION);
        }
    }

    #! Search symbols in a namespace tree matching a precompiled regex
    private static searchInNamespace(hash<NamespaceInfo> ns,
            Regex re, reference<list<hash<auto>>> results) {
        # search classes
        foreach hash<ClassInfo> c in (ns.classes) {
            if (re.match(c.full_name)) {
                results += {
                    "name": c.name,
                    "full_name": c.full_name,
                    "symbol_type": "class",
                    "description": c.description,
                };
            }
            # search methods within classes
            foreach hash<MethodInfo> m in (c.instance_methods) {
                if (re.match(m.full_name)) {
                    results += {
                        "name": m.name,
                        "full_name": m.full_name,
                        "symbol_type": "method",
                        "description": m.description,
                    };
                }
            }
            foreach hash<MethodInfo> m in (c.static_methods) {
                if (re.match(m.full_name)) {
                    results += {
                        "name": m.name,
                        "full_name": m.full_name,
                        "symbol_type": "static_method",
                        "description": m.description,
                    };
                }
            }
        }

        # search functions
        foreach hash<FunctionInfo> f in (ns.functions) {
            if (re.match(f.full_name)) {
                results += {
                    "name": f.name,
                    "full_name": f.full_name,
                    "symbol_type": "function",
                    "description": f.description,
                };
            }
        }

        # search constants
        foreach hash<ConstantInfo> c in (ns.constants) {
            if (re.match(c.full_name)) {
                results += {
                    "name": c.name,
                    "full_name": c.full_name,
                    "symbol_type": "constant",
                    "description": c.description,
                };
            }
        }

        # search hashdecls
        foreach hash<HashdeclInfo> h in (ns.hashdecls) {
            if (re.match(h.full_name)) {
                results += {
                    "name": h.name,
                    "full_name": h.full_name,
                    "symbol_type": "hashdecl",
                    "description": h.description,
                };
            }
        }

        # search enums
        foreach hash<EnumInfo> e in (ns.enums) {
            if (re.match(e.full_name)) {
                results += {
                    "name": e.name,
                    "full_name": e.full_name,
                    "symbol_type": "enum",
                    "description": e.description,
                };
            }
        }

        # recurse into child namespaces
        foreach hash<NamespaceInfo> child in (ns.namespaces) {
            QoreApiMetadata::searchInNamespace(child, re,
                \results);
        }
    }

    #! Find a symbol by fully qualified name in the namespace tree
    private static *hash<auto> findSymbolByName(
            hash<NamespaceInfo> ns, string qualified_name) {
        # check classes
        foreach hash<ClassInfo> c in (ns.classes) {
            if (c.full_name == qualified_name) {
                return c + {"symbol_type": "class"};
            }
            # check methods within this class
            foreach hash<MethodInfo> m in (c.instance_methods) {
                if (m.full_name == qualified_name) {
                    return m + {"symbol_type": "method"};
                }
            }
            foreach hash<MethodInfo> m in (c.static_methods) {
                if (m.full_name == qualified_name) {
                    return m + {"symbol_type": "static_method"};
                }
            }
        }

        # check functions
        foreach hash<FunctionInfo> f in (ns.functions) {
            if (f.full_name == qualified_name) {
                return f + {"symbol_type": "function"};
            }
        }

        # check constants
        foreach hash<ConstantInfo> c in (ns.constants) {
            if (c.full_name == qualified_name) {
                return c + {"symbol_type": "constant"};
            }
        }

        # check hashdecls
        foreach hash<HashdeclInfo> h in (ns.hashdecls) {
            if (h.full_name == qualified_name) {
                return h + {"symbol_type": "hashdecl"};
            }
        }

        # check enums
        foreach hash<EnumInfo> e in (ns.enums) {
            if (e.full_name == qualified_name) {
                return e + {"symbol_type": "enum"};
            }
        }

        # recurse into child namespaces
        foreach hash<NamespaceInfo> child in (ns.namespaces) {
            *hash<auto> found =
                QoreApiMetadata::findSymbolByName(child,
                    qualified_name);
            if (exists found) {
                return found;
            }
        }

        return NOTHING;
    }

    #! Convert a glob pattern to a regex string
    /** Supports \c * (matches any sequence) and \c ? (matches single char).
        The returned string is suitable for the Regex class constructor.
        @param pattern the glob pattern
        @return anchored regex string
    */
    private static string globToRegex(string pattern) {
        string re = "^";
        for (int i = 0; i < pattern.length(); ++i) {
            string ch = pattern[i];
            switch (ch) {
                case "*":
                    re += ".*";
                    break;
                case "?":
                    re += ".";
                    break;
                case ".":
                case "(":
                case ")":
                case "[":
                case "]":
                case "{":
                case "}":
                case "+":
                case "^":
                case "$":
                case "|":
                case "\\":
                    re += "\\" + ch;
                    break;
                default:
                    re += ch;
                    break;
            }
        }
        re += "$";
        return re;
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
