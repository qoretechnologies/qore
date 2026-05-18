# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AstMetadataExtractor.qc Source-level metadata extraction via astparser

/*  AstMetadataExtractor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Symbol kind constants from astparser (ASTSymbolKind enum)
const ASYK_Namespace = 3;
const ASYK_Class = 5;
const ASYK_Method = 6;
const ASYK_Field = 8;
const ASYK_Constructor = 9;
const ASYK_Enum = 10;
const ASYK_Interface = 11;  # used for hashdecls
const ASYK_Function = 12;
const ASYK_Variable = 13;
const ASYK_Constant = 14;
const ASYK_TypeAlias = 19;  # used for typedef

#! Extracts structured metadata from Qore source files using the astparser module
public class AstMetadataExtractor {
    #! Extract metadata from a source file
    /** @param path file path to parse
        @param options optional hash with:
        - \c defines: list of define symbols for conditional parsing
        @return NamespaceInfo tree
    */
    static hash<NamespaceInfo> extractFromFile(string path, *hash<auto> options) {
        AstParser parser();
        AstMetadataExtractor::applyOptions(parser, options);
        *AstTree tree = parser.parseFile(path);
        if (!exists tree) {
            return AstMetadataExtractor::emptyRoot();
        }
        string source_text = ReadOnlyFile::readTextFile(path);
        return AstMetadataExtractor::extractFromTree(parser, tree, path, options,
            AstMetadataExtractor::sourceLines(source_text));
    }

    #! Extract metadata from a source string
    /** @param source Qore source code
        @param options optional hash with:
        - \c defines: list of define symbols for conditional parsing
        - \c uri: virtual URI for the source (default: "string.q")
        @return NamespaceInfo tree
    */
    static hash<NamespaceInfo> extractFromString(string source, *hash<auto> options) {
        AstParser parser();
        AstMetadataExtractor::applyOptions(parser, options);
        *AstTree tree = parser.parseString(source);
        if (!exists tree) {
            return AstMetadataExtractor::emptyRoot();
        }
        string uri = options.uri ?? "string.q";
        return AstMetadataExtractor::extractFromTree(parser, tree, uri, options,
            AstMetadataExtractor::sourceLines(source));
    }

    #! Apply options to the parser
    private static applyOptions(AstParser parser, *hash<auto> options) {
        if (exists options.defines) {
            foreach auto d in (options.defines) {
                parser.addDefine(string(d));
            }
        }
    }

    #! Extract metadata from a parsed tree
    private static hash<NamespaceInfo> extractFromTree(AstParser parser, AstTree tree,
            string uri, *hash<auto> options, *list<string> source_lines) {
        AstTreeSearcher searcher();

        # get all symbols
        *list<auto> symbols = searcher.findSymbols(tree, uri);
        if (!exists symbols) {
            return AstMetadataExtractor::emptyRoot();
        }

        # collect items into flat buckets keyed by namespace path ("" = root)
        hash<string, list<hash<ClassInfo>>> ns_classes;
        hash<string, list<hash<FunctionInfo>>> ns_functions;
        hash<string, list<hash<ConstantInfo>>> ns_constants;
        hash<string, list<hash<HashdeclInfo>>> ns_hashdecls;
        hash<string, list<hash<EnumInfo>>> ns_enums;

        *string src_file;
        if (exists options.source_file) {
            src_file = string(options.source_file);
        }

        # detect enums via getNodesInfo (findSymbols reports them as ASYK_Constant)
        # store enum name → node info for extraction in the symbol loop
        hash<string, hash<auto>> enum_nodes;
        hash<string, hash<auto>> class_nodes;
        hash<string, hash<auto>> hashdecl_nodes;
        *list<auto> nodes = tree.getNodesInfo();
        if (exists nodes) {
            foreach hash<auto> n in (nodes) {
                *string decl_name = AstMetadataExtractor::extractNodeName(n);
                if (!exists decl_name) {
                    continue;
                }
                switch (string(n.type)) {
                    case "enum_declaration":
                        enum_nodes{decl_name} = n;
                        break;
                    case "class_declaration":
                        class_nodes{decl_name} = n;
                        break;
                    case "hashdecl_declaration":
                        hashdecl_nodes{decl_name} = n;
                        break;
                }
            }
        }

        foreach hash<auto> sym in (symbols) {
            int kind = int(sym.kind);
            string name = string(sym.name);
            *string doc;
            if (exists sym.docComment) {
                doc = string(sym.docComment);
            }
            *hash<auto> parsed_doc;
            if (exists doc && doc.length()) {
                parsed_doc = DocCommentParser::parse(doc);
            }

            int line = int(sym.location.range.start.line);
            *string ns_path = AstMetadataExtractor::namespacePath(name);
            string ns_key = ns_path ?? "";

            switch (kind) {
                case ASYK_Class: {
                    *hash<auto> class_node =
                        class_nodes{AstMetadataExtractor::baseName(name)};
                    hash<ClassInfo> ci = AstMetadataExtractor::extractClassInfo(
                        searcher, tree, uri, name, doc, parsed_doc, src_file,
                        class_node, source_lines);
                    if (!exists ns_classes{ns_key}) {
                        ns_classes{ns_key} = ();
                    }
                    ns_classes{ns_key} += ci;
                    break;
                }
                case ASYK_Function: {
                    # get detailed info for this function
                    *list<auto> detailed = searcher.findScopeSymbolsDetailed(
                        tree, uri, line, 0);
                    *hash<auto> func_detail;
                    if (exists detailed) {
                        foreach hash<auto> d in (detailed) {
                            if (string(d.name)
                                    == AstMetadataExtractor::baseName(name)
                                    && int(d.kind) == ASYK_Function) {
                                func_detail = d;
                                break;
                            }
                        }
                    }
                    hash<FunctionInfo> fi =
                        AstMetadataExtractor::buildFunctionInfo(
                            name, doc, parsed_doc, func_detail, src_file);
                    fi.source_line = line;
                    if (!exists ns_functions{ns_key}) {
                        ns_functions{ns_key} = ();
                    }
                    ns_functions{ns_key} += fi;
                    break;
                }
                case ASYK_Constant: {
                    # check if this is actually an enum
                    string base_name =
                        AstMetadataExtractor::baseName(name);
                    if (exists enum_nodes{base_name}) {
                        # build enum info from CST node + symbol doc
                        hash<EnumInfo> ei =
                            AstMetadataExtractor::extractEnumFromNode(
                                enum_nodes{base_name}, src_file);
                        # overlay doc from findSymbols
                        if (exists parsed_doc) {
                            ei.description =
                                string(parsed_doc.description);
                        }
                        ei.full_name = name;
                        ei.namespace_path =
                            AstMetadataExtractor::namespacePath(name);
                        if (!exists ns_enums{ns_key}) {
                            ns_enums{ns_key} = ();
                        }
                        ns_enums{ns_key} += ei;
                        break;
                    }
                    hash<ConstantInfo> ci =
                        AstMetadataExtractor::buildConstantInfo(
                            name, doc, parsed_doc, src_file);
                    if (!exists ns_constants{ns_key}) {
                        ns_constants{ns_key} = ();
                    }
                    ns_constants{ns_key} += ci;
                    break;
                }
                case ASYK_Interface: {
                    *hash<auto> hashdecl_node =
                        hashdecl_nodes{AstMetadataExtractor::baseName(name)};
                    hash<HashdeclInfo> hi =
                        AstMetadataExtractor::extractHashdeclInfo(
                            searcher, tree, uri, name, doc, parsed_doc,
                            src_file, hashdecl_node, source_lines);
                    if (!exists ns_hashdecls{ns_key}) {
                        ns_hashdecls{ns_key} = ();
                    }
                    ns_hashdecls{ns_key} += hi;
                    break;
                }
                case ASYK_Enum: {
                    hash<EnumInfo> ei = AstMetadataExtractor::extractEnumInfo(
                        searcher, tree, uri, name, doc, parsed_doc, src_file);
                    if (!exists ns_enums{ns_key}) {
                        ns_enums{ns_key} = ();
                    }
                    ns_enums{ns_key} += ei;
                    break;
                }
            }
        }

        # build namespace tree from flat buckets
        return AstMetadataExtractor::buildNamespaceTree(
            ns_classes, ns_functions, ns_constants, ns_hashdecls, ns_enums);
    }

    #! Build namespace tree from flat namespace-keyed collections
    private static hash<NamespaceInfo> buildNamespaceTree(
            *hash<string, list<hash<ClassInfo>>> ns_classes,
            *hash<string, list<hash<FunctionInfo>>> ns_functions,
            *hash<string, list<hash<ConstantInfo>>> ns_constants,
            *hash<string, list<hash<HashdeclInfo>>> ns_hashdecls,
            *hash<string, list<hash<EnumInfo>>> ns_enums) {
        # collect all namespace paths
        hash<string, bool> all_paths;
        foreach string k in (keys ns_classes) {
            all_paths{k} = True;
        }
        foreach string k in (keys ns_functions) {
            all_paths{k} = True;
        }
        foreach string k in (keys ns_constants) {
            all_paths{k} = True;
        }
        foreach string k in (keys ns_hashdecls) {
            all_paths{k} = True;
        }
        foreach string k in (keys ns_enums) {
            all_paths{k} = True;
        }

        # build NamespaceInfo for each path, sorted longest-first so children
        # are built before parents
        hash<string, hash<NamespaceInfo>> ns_map;
        list<string> paths = ();
        if (all_paths) {
            paths = sort(keys all_paths,
                int sub (string a, string b) {
                    return b.length() <=> a.length();
                });
        }

        foreach string path in (paths) {
            if (path == "") {
                continue;
            }
            string base = AstMetadataExtractor::baseName(path);
            hash<NamespaceInfo> ns({
                "name": base,
                "full_name": path,
            });
            if (exists ns_classes{path}) {
                ns.classes = ns_classes{path};
            }
            if (exists ns_functions{path}) {
                ns.functions = ns_functions{path};
            }
            if (exists ns_constants{path}) {
                ns.constants = ns_constants{path};
            }
            if (exists ns_hashdecls{path}) {
                ns.hashdecls = ns_hashdecls{path};
            }
            if (exists ns_enums{path}) {
                ns.enums = ns_enums{path};
            }
            ns_map{path} = ns;
        }

        # link children to parents
        foreach string path in (paths) {
            if (path == "") {
                continue;
            }
            *string parent_path = AstMetadataExtractor::namespacePath(path);
            if (exists parent_path && exists ns_map{parent_path}) {
                ns_map{parent_path}.namespaces += ns_map{path};
            }
        }

        # build root
        hash<NamespaceInfo> root = AstMetadataExtractor::emptyRoot();
        if (exists ns_classes{""}) {
            root.classes = ns_classes{""};
        }
        if (exists ns_functions{""}) {
            root.functions = ns_functions{""};
        }
        if (exists ns_constants{""}) {
            root.constants = ns_constants{""};
        }
        if (exists ns_hashdecls{""}) {
            root.hashdecls = ns_hashdecls{""};
        }
        if (exists ns_enums{""}) {
            root.enums = ns_enums{""};
        }

        # add top-level namespaces (those without a parent in the map)
        foreach string path in (keys ns_map) {
            *string parent_path = AstMetadataExtractor::namespacePath(path);
            if (!exists parent_path || !exists ns_map{parent_path}) {
                root.namespaces += ns_map{path};
            }
        }

        return root;
    }

    #! Create an empty root namespace
    private static hash<NamespaceInfo> emptyRoot() {
        return <NamespaceInfo>{
            "name": "",
            "full_name": "",
        };
    }

    #! Get the base (unqualified) name from a possibly qualified name
    private static string baseName(string name) {
        int pos = name.rfind("::");
        if (pos >= 0) {
            return name.substr(pos + 2);
        }
        return name;
    }

    #! Get the namespace path from a qualified name (everything before the last ::)
    private static *string namespacePath(string name) {
        int pos = name.rfind("::");
        if (pos >= 0) {
            return name.substr(0, pos);
        }
        return NOTHING;
    }

    #! Extract class info including members
    private static hash<ClassInfo> extractClassInfo(AstTreeSearcher searcher,
            AstTree tree, string uri, string name, *string doc,
            *hash<auto> parsed_doc, *string source_file,
            *hash<auto> class_node, *list<string> source_lines) {
        string base = AstMetadataExtractor::baseName(name);

        *string class_desc;
        if (exists parsed_doc) {
            class_desc = string(parsed_doc.description);
        }
        hash<ClassInfo> ci({
            "name": base,
            "full_name": name,
            "namespace_path": AstMetadataExtractor::namespacePath(name),
            "description": class_desc,
            "source_file": source_file,
        });

        if (exists class_node) {
            hash<auto> type_info =
                AstMetadataExtractor::extractTypeParameterInfo(class_node, source_lines);
            ci.type_parameters = cast<list<string>>(type_info.type_parameters);
            ci.type_parameter_defaults =
                cast<hash<string, string>>(type_info.type_parameter_defaults);
            ci.type_parameter_bounds =
                cast<hash<string, string>>(type_info.type_parameter_bounds);
            AstMetadataExtractor::applyClassCompatibilityInfo(\ci, class_node);
        }

        # get class members via findTypeMembers
        *list<auto> members = searcher.findTypeMembers(tree, uri, base, False);
        if (exists members) {
            foreach hash<auto> m in (members) {
                int mkind = int(m.kind);
                switch (mkind) {
                    case ASYK_Method:
                    case ASYK_Constructor: {
                        hash<MethodInfo> mi = AstMetadataExtractor::buildMethodInfo(
                            m, name, parsed_doc);
                        if (m.isStatic) {
                            ci.static_methods += mi;
                        } else {
                            ci.instance_methods += mi;
                        }
                        break;
                    }
                    case ASYK_Field: {
                        string f_type = "auto";
                        if (exists m.typeName) {
                            f_type = string(m.typeName);
                        }
                        string f_access = "public";
                        if (exists m.access) {
                            f_access = string(m.access);
                        }
                        hash<MemberInfo> fi({
                            "name": string(m.name),
                            "type_name": f_type,
                            "access": f_access,
                            "is_static": m.isStatic ? True : False,
                        });
                        ci.members += fi;
                        break;
                    }
                    case ASYK_Constant: {
                        hash<ConstantInfo> cst({
                            "name": string(m.name),
                            "full_name": name + "::" + string(m.name),
                        });
                        ci.constants += cst;
                        break;
                    }
                }
            }
        }

        return ci;
    }

    #! Build a MethodInfo from a type member detail hash
    private static hash<MethodInfo> buildMethodInfo(hash<auto> m, string class_name,
            *hash<auto> class_doc) {
        string method_name = string(m.name);
        string full_name = class_name + "::" + method_name;

        # build params
        list<hash<ParameterInfo>> params = ();
        if (exists m.params) {
            foreach hash<auto> p in (m.params) {
                string p_name = string(p.name);
                string p_type = "auto";
                if (exists p.typeName) {
                    p_type = string(p.typeName);
                }
                *string p_default;
                if (exists p.defaultVal) {
                    p_default = string(p.defaultVal);
                }
                hash<ParameterInfo> pi({
                    "name": p_name,
                    "type_name": p_type,
                    "default_value": p_default,
                });
                params += pi;
            }
        }

        # parse method doc comment if available
        *hash<auto> method_doc;
        if (exists m.docComment && string(m.docComment).length()) {
            method_doc = DocCommentParser::parse(string(m.docComment));
            # match param descriptions
            foreach hash<ParameterInfo> p in (\params) {
                if (exists method_doc.params{p.name}) {
                    p.description = string(method_doc.params{p.name});
                }
            }
        }

        # build signature
        *string ret_type;
        if (exists m.returnType) {
            ret_type = string(m.returnType);
        }
        string sig = AstMetadataExtractor::buildSignature(
            ret_type, method_name, params);

        # extract doc fields into typed locals
        *string meth_desc;
        *string meth_ret_desc;
        *list<hash<ThrowInfo>> meth_throws;
        *string meth_since;
        *list<string> meth_see;
        bool meth_depr = False;
        *string meth_depr_reason;
        if (exists method_doc) {
            meth_desc = string(method_doc.description);
            if (exists method_doc.return_description) {
                meth_ret_desc = string(method_doc.return_description);
            }
            if (exists method_doc.throws) {
                meth_throws = cast<list<hash<ThrowInfo>>>(method_doc.throws);
            }
            if (exists method_doc.since) {
                meth_since = string(method_doc.since);
            }
            if (method_doc.see_also.size()) {
                meth_see = cast<list<string>>(method_doc.see_also);
            }
            if (exists method_doc.deprecated) {
                meth_depr = True;
                if (method_doc.deprecated.typeCode() == NT_STRING) {
                    meth_depr_reason = string(method_doc.deprecated);
                }
            }
        }

        string meth_access = "public";
        if (exists m.access) {
            meth_access = string(m.access);
        }

        return <MethodInfo>{
            "name": method_name,
            "full_name": full_name,
            "signature": sig,
            "return_type": ret_type,
            "description": meth_desc,
            "return_description": meth_ret_desc,
            "params": params,
            "named_callable": AstMetadataExtractor::isNamedCallable(params),
            "named_parameters": AstMetadataExtractor::extractNamedParams(params),
            "throws": meth_throws,
            "access": meth_access,
            "is_static": m.isStatic ? True : False,
            "is_deprecated": meth_depr,
            "deprecated_reason": meth_depr_reason,
            "since": meth_since,
            "see_also": meth_see,
        };
    }

    #! Build a FunctionInfo from symbol data
    private static hash<FunctionInfo> buildFunctionInfo(string name, *string doc,
            *hash<auto> parsed_doc, *hash<auto> detail, *string source_file) {
        string base = AstMetadataExtractor::baseName(name);

        list<hash<ParameterInfo>> params = ();
        *string return_type;

        if (exists detail) {
            if (exists detail.returnType) {
                return_type = string(detail.returnType);
            }
            if (exists detail.params) {
                foreach hash<auto> p in (detail.params) {
                    string p_name = string(p.name);
                    string p_type = "auto";
                    if (exists p.typeName) {
                        p_type = string(p.typeName);
                    }
                    *string p_default;
                    if (exists p.defaultVal) {
                        p_default = string(p.defaultVal);
                    }
                    hash<ParameterInfo> pi({
                        "name": p_name,
                        "type_name": p_type,
                        "default_value": p_default,
                    });
                    # match param description from doc
                    if (exists parsed_doc
                            && exists parsed_doc.params{p_name}) {
                        pi.description =
                            string(parsed_doc.params{p_name});
                    }
                    params += pi;
                }
            }
        }

        string sig = AstMetadataExtractor::buildSignature(return_type, base, params);

        *string func_desc;
        *string func_ret_desc;
        *string func_since;
        *list<string> func_see;
        *list<hash<ThrowInfo>> func_throws;
        bool func_depr = False;
        if (exists parsed_doc) {
            func_desc = string(parsed_doc.description);
            if (exists parsed_doc.return_description) {
                func_ret_desc = string(parsed_doc.return_description);
            }
            if (exists parsed_doc.since) {
                func_since = string(parsed_doc.since);
            }
            if (parsed_doc.see_also.size()) {
                func_see = cast<list<string>>(parsed_doc.see_also);
            }
            if (exists parsed_doc.throws) {
                func_throws = cast<list<hash<ThrowInfo>>>(parsed_doc.throws);
            }
            if (exists parsed_doc.deprecated) {
                func_depr = True;
            }
        }

        return <FunctionInfo>{
            "name": base,
            "full_name": name,
            "namespace_path": AstMetadataExtractor::namespacePath(name),
            "signature": sig,
            "return_type": return_type,
            "description": func_desc,
            "return_description": func_ret_desc,
            "params": params,
            "named_callable": AstMetadataExtractor::isNamedCallable(params),
            "named_parameters": AstMetadataExtractor::extractNamedParams(params),
            "throws": func_throws,
            "is_deprecated": func_depr,
            "since": func_since,
            "see_also": func_see,
            "source_file": source_file,
        };
    }

    #! Build a ConstantInfo from symbol data
    private static hash<ConstantInfo> buildConstantInfo(string name, *string doc,
            *hash<auto> parsed_doc, *string source_file) {
        *string const_desc;
        if (exists parsed_doc) {
            const_desc = string(parsed_doc.description);
        }
        return <ConstantInfo>{
            "name": AstMetadataExtractor::baseName(name),
            "full_name": name,
            "description": const_desc,
            "source_file": source_file,
        };
    }

    #! Extract hashdecl info including members
    private static hash<HashdeclInfo> extractHashdeclInfo(AstTreeSearcher searcher,
            AstTree tree, string uri, string name, *string doc,
            *hash<auto> parsed_doc, *string source_file,
            *hash<auto> hashdecl_node, *list<string> source_lines) {
        string base = AstMetadataExtractor::baseName(name);

        *string hd_desc;
        if (exists parsed_doc) {
            hd_desc = string(parsed_doc.description);
        }
        hash<HashdeclInfo> hi({
            "name": base,
            "full_name": name,
            "namespace_path": AstMetadataExtractor::namespacePath(name),
            "description": hd_desc,
            "source_file": source_file,
        });

        if (exists hashdecl_node) {
            hash<auto> type_info =
                AstMetadataExtractor::extractTypeParameterInfo(hashdecl_node, source_lines);
            hi.type_parameters = cast<list<string>>(type_info.type_parameters);
            hi.type_parameter_defaults =
                cast<hash<string, string>>(type_info.type_parameter_defaults);
            hi.type_parameter_bounds =
                cast<hash<string, string>>(type_info.type_parameter_bounds);
            *hash<auto> superclass_list =
                AstMetadataExtractor::findChildByType(hashdecl_node, "superclass_list");
            if (exists superclass_list) {
                *hash<auto> superclass =
                    AstMetadataExtractor::findChildByType(superclass_list, "superclass");
                if (exists superclass) {
                    hi.parent_name = AstMetadataExtractor::nodeText(superclass, source_lines);
                }
            }
        }

        *list<auto> members = searcher.findTypeMembers(tree, uri, base, False);
        if (exists members) {
            foreach hash<auto> m in (members) {
                string m_type = "auto";
                if (exists m.typeName) {
                    m_type = string(m.typeName);
                }
                hash<MemberInfo> mi({
                    "name": string(m.name),
                    "type_name": m_type,
                });
                hi.members += mi;
            }
        }

        return hi;
    }

    #! Extract formal type parameter metadata from a class or hashdecl CST node
    private static hash<auto> extractTypeParameterInfo(hash<auto> declaration_node,
            *list<string> source_lines) {
        list<string> params = ();
        hash<string, string> defaults = {};
        hash<string, string> bounds = {};
        *hash<auto> type_params =
            AstMetadataExtractor::findChildByField(declaration_node,
                "type_parameters");
        if (!exists type_params || !exists type_params.children) {
            return {
                "type_parameters": params,
                "type_parameter_defaults": defaults,
                "type_parameter_bounds": bounds,
            };
        }

        foreach hash<auto> param in (type_params.children) {
            if (!exists param.type || string(param.type) != "type_parameter") {
                continue;
            }
            *hash<auto> name_node =
                AstMetadataExtractor::findChildByField(param, "name");
            if (!exists name_node) {
                continue;
            }
            string name = AstMetadataExtractor::nodeText(name_node, source_lines);
            params += name;

            *hash<auto> default_node =
                AstMetadataExtractor::findChildByField(param, "default");
            if (exists default_node) {
                defaults{name} = AstMetadataExtractor::nodeText(default_node, source_lines);
            }

            *hash<auto> bound_node =
                AstMetadataExtractor::findChildByField(param, "bound");
            if (exists bound_node) {
                bounds{name} = AstMetadataExtractor::nodeText(bound_node, source_lines);
            }
        }

        return {
            "type_parameters": params,
            "type_parameter_defaults": defaults,
            "type_parameter_bounds": bounds,
        };
    }

    #! Apply generic raw-compatibility metadata from a class CST node
    private static applyClassCompatibilityInfo(reference<hash<ClassInfo>> result,
            hash<auto> class_node) {
        *hash<auto> attrs =
            AstMetadataExtractor::findChildByType(class_node,
                "class_compatibility_attributes");
        if (!exists attrs || !exists attrs.children) {
            return;
        }

        foreach hash<auto> attr in (attrs.children) {
            if (!exists attr.type
                    || string(attr.type) != "class_compatibility_attribute") {
                continue;
            }
            *hash<auto> name_node =
                AstMetadataExtractor::findChildByField(attr, "name");
            if (!exists name_node) {
                continue;
            }
            switch (AstMetadataExtractor::nodeText(name_node)) {
                case "legacy_raw":
                    result.raw_accepts_parameterized = True;
                    result.raw_construction_defaults_to_auto = True;
                    break;
                case "legacy_raw_accepts":
                    result.raw_accepts_parameterized = True;
                    break;
                case "legacy_raw_construct":
                    result.raw_construction_defaults_to_auto = True;
                    break;
            }
        }
    }

    #! Returns the first child with the given CST field name
    private static *hash<auto> findChildByField(hash<auto> node, string field) {
        if (!exists node.children) {
            return NOTHING;
        }
        foreach hash<auto> child in (node.children) {
            if (exists child.field && string(child.field) == field) {
                return child;
            }
        }
        return NOTHING;
    }

    #! Returns the first direct child with the given CST node type
    private static *hash<auto> findChildByType(hash<auto> node, string type) {
        if (!exists node.children) {
            return NOTHING;
        }
        foreach hash<auto> child in (node.children) {
            if (exists child.type && string(child.type) == type) {
                return child;
            }
        }
        return NOTHING;
    }

    #! Split source text into lines for location-based CST text extraction
    private static list<string> sourceLines(string source_text) {
        return source_text.split("\n");
    }

    #! Extract exact source text for a CST node location
    private static *string sourceText(hash<auto> node, *list<string> source_lines) {
        if (!exists source_lines || !exists node.loc) {
            return NOTHING;
        }
        int start_line = int(node.loc.start_line);
        int start_column = int(node.loc.start_column);
        int end_line = int(node.loc.end_line);
        int end_column = int(node.loc.end_column);

        if (start_line < 0 || end_line < start_line
                || start_line >= source_lines.size()
                || end_line >= source_lines.size()) {
            return NOTHING;
        }

        if (start_line == end_line) {
            return source_lines[start_line].substr(start_column,
                end_column - start_column);
        }

        string rv = source_lines[start_line].substr(start_column);
        for (int i = start_line + 1; i < end_line; ++i) {
            rv += "\n" + source_lines[i];
        }
        rv += "\n" + source_lines[end_line].substr(0, end_column);
        return rv;
    }

    #! Returns the source text for a CST node, reconstructing it from children when needed
    private static string nodeText(hash<auto> node, *list<string> source_lines) {
        if (exists node.text) {
            return string(node.text);
        }
        *string exact = AstMetadataExtractor::sourceText(node, source_lines);
        if (exists exact) {
            return exact;
        }

        if (exists node.type && string(node.type) == "scoped_identifier"
                && exists node.children) {
            list<string> parts = ();
            foreach hash<auto> child in (node.children) {
                parts += AstMetadataExtractor::nodeText(child, source_lines);
            }
            return parts.join("::");
        }

        if (exists node.type && string(node.type) == "generic_type"
                && exists node.children && node.children.size() > 0) {
            string base = AstMetadataExtractor::nodeText(node.children[0], source_lines);
            list<string> args = ();
            for (int i = 1; i < node.children.size(); ++i) {
                args += AstMetadataExtractor::nodeText(node.children[i], source_lines);
            }
            return base + "<" + args.join(", ") + ">";
        }

        string rv = "";
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                rv += AstMetadataExtractor::nodeText(child, source_lines);
            }
        }
        return rv;
    }

    #! Extract enum info including values
    private static hash<EnumInfo> extractEnumInfo(AstTreeSearcher searcher,
            AstTree tree, string uri, string name, *string doc,
            *hash<auto> parsed_doc, *string source_file) {
        string base = AstMetadataExtractor::baseName(name);

        *string enum_desc;
        if (exists parsed_doc) {
            enum_desc = string(parsed_doc.description);
        }
        hash<EnumInfo> ei({
            "name": base,
            "full_name": name,
            "namespace_path": AstMetadataExtractor::namespacePath(name),
            "description": enum_desc,
            "source_file": source_file,
        });

        *list<auto> members = searcher.findTypeMembers(tree, uri, base, False);
        if (exists members) {
            foreach hash<auto> m in (members) {
                hash<EnumValueInfo> vi({
                    "name": string(m.name),
                });
                ei.values += vi;
            }
        }

        return ei;
    }

    #! Extract the name from a CST node (looks for child with field "name")
    private static *string extractNodeName(hash<auto> node) {
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (exists child.field && string(child.field) == "name"
                        && exists child.text) {
                    return string(child.text);
                }
            }
        }
        return NOTHING;
    }

    #! Extract enum info directly from an enum_declaration CST node
    /** @param node the CST node for the enum_declaration
        @param source_file optional source file path
        @return EnumInfo with values extracted from enum_member children
    */
    private static hash<EnumInfo> extractEnumFromNode(hash<auto> node,
            *string source_file) {
        *string name = AstMetadataExtractor::extractNodeName(node);
        if (!exists name) {
            name = "unknown";
        }

        list<hash<EnumValueInfo>> values = ();
        if (exists node.children) {
            foreach hash<auto> child in (node.children) {
                if (exists child.type
                        && string(child.type) == "enum_member") {
                    *string val_name =
                        AstMetadataExtractor::extractNodeName(child);
                    if (exists val_name) {
                        hash<EnumValueInfo> vi({
                            "name": val_name,
                        });
                        values += vi;
                    }
                }
            }
        }

        return <EnumInfo>{
            "name": name,
            "full_name": name,
            "values": values,
            "source_file": source_file,
        };
    }

    #! Build a human-readable signature string
    private static string buildSignature(*string return_type, string name,
            list<hash<ParameterInfo>> params) {
        string sig = "";
        if (exists return_type && return_type.length()) {
            sig += return_type + " ";
        }
        sig += name + "(";
        for (int i = 0; i < params.size(); ++i) {
            if (i > 0) {
                sig += ", ";
            }
            hash<ParameterInfo> p = params[i];
            sig += p.type_name + " " + p.name;
            if (exists p.default_value) {
                sig += " = " + p.default_value;
            }
        }
        sig += ")";
        return sig;
    }

    #! Return parameter names that are valid in named-argument calls
    private static list<string> extractNamedParams(list<hash<ParameterInfo>> params) {
        list<string> names = ();
        foreach hash<ParameterInfo> p in (params) {
            if (p.type_name == "..." || p.name == "...") {
                break;
            }
            if (p.name.length()) {
                names += p.name;
            }
        }
        return names;
    }

    #! Source variants are named-callable unless they are varargs-only
    private static bool isNamedCallable(list<hash<ParameterInfo>> params) {
        return params.size() != 1 || (params[0].type_name != "..." && params[0].name != "...");
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
