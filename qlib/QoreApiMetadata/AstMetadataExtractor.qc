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
        return AstMetadataExtractor::extractFromTree(parser, tree, path, options);
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
        return AstMetadataExtractor::extractFromTree(parser, tree, uri, options);
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
            string uri, *hash<auto> options) {
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
        *list<auto> nodes = tree.getNodesInfo();
        if (exists nodes) {
            foreach hash<auto> n in (nodes) {
                if (string(n.type) == "enum_declaration") {
                    *string enum_name =
                        AstMetadataExtractor::extractNodeName(n);
                    if (exists enum_name) {
                        enum_nodes{enum_name} = n;
                    }
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
                    hash<ClassInfo> ci = AstMetadataExtractor::extractClassInfo(
                        searcher, tree, uri, name, doc, parsed_doc, src_file);
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
                    hash<HashdeclInfo> hi =
                        AstMetadataExtractor::extractHashdeclInfo(
                            searcher, tree, uri, name, doc, parsed_doc,
                            src_file);
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
            *hash<auto> parsed_doc, *string source_file) {
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
            *hash<auto> parsed_doc, *string source_file) {
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
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
