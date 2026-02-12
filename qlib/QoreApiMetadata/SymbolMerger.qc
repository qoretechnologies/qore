# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SymbolMerger.qc Merge metadata from multiple extraction pipelines

/*  SymbolMerger.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Merges metadata from reflection, astparser, and qpp doc index pipelines
/** Priority for documentation:
    - astparser (source-level) is preferred first
    - qpp doc index (builtin docs from .meta.json) as fallback
    - reflection provides structure but no documentation

    Priority for structural information (types, params, access):
    - reflection is authoritative
    - astparser fills in where reflection is missing
*/
public class SymbolMerger {
    #! Merge metadata from all pipelines
    /** @param reflection_meta structural metadata from reflection
        @param ast_meta optional source-level metadata from astparser
        @param doc_index optional qpp .meta.json index from QppDocIndex
        @return enriched NamespaceInfo tree with documentation merged in
    */
    static hash<NamespaceInfo> merge(hash<NamespaceInfo> reflection_meta,
            *hash<NamespaceInfo> ast_meta,
            *hash<string, hash<auto>> doc_index) {
        # flatten AST metadata into qualified-name-keyed lookup tables
        hash<string, hash<auto>> ast_classes();
        hash<string, hash<auto>> ast_functions();
        hash<string, hash<auto>> ast_constants();
        hash<string, hash<auto>> ast_hashdecls();
        hash<string, hash<auto>> ast_enums();
        hash<string, hash<auto>> ast_methods();

        if (exists ast_meta) {
            SymbolMerger::flattenNamespace(ast_meta, \ast_classes,
                \ast_functions, \ast_constants, \ast_hashdecls, \ast_enums,
                \ast_methods);
        }

        if (!exists doc_index) {
            doc_index = {};
        }

        # walk the reflection tree and enrich with docs
        return SymbolMerger::enrichNamespace(reflection_meta, ast_classes,
            ast_functions, ast_constants, ast_hashdecls, ast_enums,
            ast_methods, doc_index);
    }

    #! Flatten a NamespaceInfo tree into qualified-name-keyed hashes
    private static flattenNamespace(hash<NamespaceInfo> ns,
            reference<hash<string, hash<auto>>> classes,
            reference<hash<string, hash<auto>>> functions,
            reference<hash<string, hash<auto>>> constants,
            reference<hash<string, hash<auto>>> hashdecls,
            reference<hash<string, hash<auto>>> enums,
            reference<hash<string, hash<auto>>> methods) {
        foreach hash<ClassInfo> c in (ns.classes) {
            classes{c.full_name} = c;
            # also index methods
            foreach hash<MethodInfo> m in (c.instance_methods) {
                methods{m.full_name} = m;
            }
            foreach hash<MethodInfo> m in (c.static_methods) {
                methods{m.full_name} = m;
            }
        }
        foreach hash<FunctionInfo> f in (ns.functions) {
            functions{f.full_name} = f;
        }
        foreach hash<ConstantInfo> c in (ns.constants) {
            constants{c.full_name} = c;
        }
        foreach hash<HashdeclInfo> h in (ns.hashdecls) {
            hashdecls{h.full_name} = h;
        }
        foreach hash<EnumInfo> e in (ns.enums) {
            enums{e.full_name} = e;
        }
        # recurse into child namespaces
        foreach hash<NamespaceInfo> child in (ns.namespaces) {
            SymbolMerger::flattenNamespace(child, \classes, \functions,
                \constants, \hashdecls, \enums, \methods);
        }
    }

    #! Enrich a namespace tree with documentation from AST and doc index
    private static hash<NamespaceInfo> enrichNamespace(
            hash<NamespaceInfo> ns,
            hash<string, hash<auto>> ast_classes,
            hash<string, hash<auto>> ast_functions,
            hash<string, hash<auto>> ast_constants,
            hash<string, hash<auto>> ast_hashdecls,
            hash<string, hash<auto>> ast_enums,
            hash<string, hash<auto>> ast_methods,
            hash<string, hash<auto>> doc_index) {
        hash<NamespaceInfo> result = ns;

        # enrich classes
        list<hash<ClassInfo>> enriched_classes = ();
        foreach hash<ClassInfo> c in (ns.classes) {
            enriched_classes +=
                SymbolMerger::enrichClass(c, ast_classes, ast_methods,
                    doc_index);
        }
        result.classes = enriched_classes;

        # enrich functions
        list<hash<FunctionInfo>> enriched_funcs = ();
        foreach hash<FunctionInfo> f in (ns.functions) {
            enriched_funcs +=
                SymbolMerger::enrichFunction(f, ast_functions, doc_index);
        }
        result.functions = enriched_funcs;

        # enrich constants
        list<hash<ConstantInfo>> enriched_consts = ();
        foreach hash<ConstantInfo> c in (ns.constants) {
            enriched_consts +=
                SymbolMerger::enrichConstant(c, ast_constants, doc_index);
        }
        result.constants = enriched_consts;

        # enrich hashdecls
        list<hash<HashdeclInfo>> enriched_hds = ();
        foreach hash<HashdeclInfo> h in (ns.hashdecls) {
            enriched_hds +=
                SymbolMerger::enrichHashdecl(h, ast_hashdecls, doc_index);
        }
        result.hashdecls = enriched_hds;

        # enrich enums
        list<hash<EnumInfo>> enriched_enums = ();
        foreach hash<EnumInfo> e in (ns.enums) {
            enriched_enums +=
                SymbolMerger::enrichEnum(e, ast_enums, doc_index);
        }
        result.enums = enriched_enums;

        # recurse into child namespaces
        list<hash<NamespaceInfo>> enriched_ns = ();
        foreach hash<NamespaceInfo> child in (ns.namespaces) {
            enriched_ns += SymbolMerger::enrichNamespace(child, ast_classes,
                ast_functions, ast_constants, ast_hashdecls, ast_enums,
                ast_methods, doc_index);
        }
        result.namespaces = enriched_ns;

        return result;
    }

    #! Enrich a class with documentation
    private static hash<ClassInfo> enrichClass(hash<ClassInfo> ci,
            hash<string, hash<auto>> ast_classes,
            hash<string, hash<auto>> ast_methods,
            hash<string, hash<auto>> doc_index) {
        hash<ClassInfo> result = ci;

        # try AST class docs first
        if (exists ast_classes{ci.full_name}) {
            hash<auto> ast = ast_classes{ci.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
            if (!exists result.source_file && exists ast.source_file) {
                result.source_file = string(ast.source_file);
            }
        }

        # try doc_index class docs as fallback
        if (!exists result.description && exists doc_index{ci.full_name}) {
            hash<auto> qpp = doc_index{ci.full_name};
            if (exists qpp.description) {
                *hash<auto> parsed =
                    DocCommentParser::parse(string(qpp.description));
                if (exists parsed && parsed.description.length()) {
                    result.description = string(parsed.description);
                }
            }
        }

        # enrich methods
        list<hash<MethodInfo>> enriched_inst = ();
        foreach hash<MethodInfo> m in (ci.instance_methods) {
            enriched_inst +=
                SymbolMerger::enrichMethod(m, ast_methods, doc_index);
        }
        result.instance_methods = enriched_inst;

        list<hash<MethodInfo>> enriched_static = ();
        foreach hash<MethodInfo> m in (ci.static_methods) {
            enriched_static +=
                SymbolMerger::enrichMethod(m, ast_methods, doc_index);
        }
        result.static_methods = enriched_static;

        return result;
    }

    #! Enrich a method with documentation
    private static hash<MethodInfo> enrichMethod(hash<MethodInfo> mi,
            hash<string, hash<auto>> ast_methods,
            hash<string, hash<auto>> doc_index) {
        hash<MethodInfo> result = mi;

        # try AST method docs first
        if (exists ast_methods{mi.full_name}) {
            hash<auto> ast = ast_methods{mi.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
            if (!exists result.return_description
                    && exists ast.return_description) {
                result.return_description =
                    string(ast.return_description);
            }
            if (!exists result.throws && exists ast.throws) {
                result.throws =
                    cast<list<hash<ThrowInfo>>>(ast.throws);
            }
            if (!exists result.since && exists ast.since) {
                result.since = string(ast.since);
            }
            if (!exists result.see_also && exists ast.see_also) {
                result.see_also = cast<list<string>>(ast.see_also);
            }
            # enrich param descriptions from AST
            SymbolMerger::enrichParamDescriptions(\result.params, ast);
        }

        # try doc_index as fallback
        if (!exists result.description && exists doc_index{mi.full_name}) {
            hash<auto> qpp = doc_index{mi.full_name};
            SymbolMerger::applyQppMethodDocs(\result, qpp);
        }

        return result;
    }

    #! Apply qpp doc index documentation to a method
    private static applyQppMethodDocs(reference<hash<MethodInfo>> result,
            hash<auto> qpp) {
        if (exists qpp.description) {
            hash<auto> parsed =
                DocCommentParser::parse(string(qpp.description));
            if (exists parsed) {
                if (parsed.description.length()) {
                    result.description = string(parsed.description);
                }
                if (!exists result.return_description
                        && exists parsed.return_description) {
                    result.return_description =
                        string(parsed.return_description);
                }
                if (!exists result.throws && parsed.throws.size()) {
                    result.throws =
                        cast<list<hash<ThrowInfo>>>(parsed.throws);
                }
                if (!exists result.since && exists parsed.since) {
                    result.since = string(parsed.since);
                }
                if (!exists result.see_also
                        && parsed.see_also.size()) {
                    result.see_also =
                        cast<list<string>>(parsed.see_also);
                }
                if (!result.is_deprecated && exists parsed.deprecated) {
                    result.is_deprecated = True;
                    if (parsed.deprecated.typeCode() == NT_STRING) {
                        result.deprecated_reason =
                            string(parsed.deprecated);
                    }
                }
                # enrich param descriptions
                SymbolMerger::enrichParamDescriptionsFromParsedDoc(
                    \result.params, parsed);
            }
        }
    }

    #! Enrich parameter descriptions from AST method data
    private static enrichParamDescriptions(
            reference<list<hash<ParameterInfo>>> params,
            hash<auto> source) {
        if (!exists source.params) {
            return;
        }
        # source.params may be a list<ParameterInfo> with descriptions
        if (source.params.typeCode() == NT_LIST) {
            foreach hash<auto> sp in (source.params) {
                if (exists sp.description) {
                    # find matching param by name
                    foreach hash<ParameterInfo> p in (\params) {
                        if (p.name == sp.name && !exists p.description) {
                            p.description = string(sp.description);
                            break;
                        }
                    }
                }
            }
        }
    }

    #! Enrich parameter descriptions from a parsed doc comment
    private static enrichParamDescriptionsFromParsedDoc(
            reference<list<hash<ParameterInfo>>> params,
            hash<auto> parsed_doc) {
        if (!exists parsed_doc.params || !parsed_doc.params.size()) {
            return;
        }
        foreach hash<ParameterInfo> p in (\params) {
            if (!exists p.description
                    && exists parsed_doc.params{p.name}) {
                p.description = string(parsed_doc.params{p.name});
            }
        }
    }

    #! Enrich a function with documentation
    private static hash<FunctionInfo> enrichFunction(hash<FunctionInfo> fi,
            hash<string, hash<auto>> ast_functions,
            hash<string, hash<auto>> doc_index) {
        hash<FunctionInfo> result = fi;

        # try AST function docs first
        if (exists ast_functions{fi.full_name}) {
            hash<auto> ast = ast_functions{fi.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
            if (!exists result.return_description
                    && exists ast.return_description) {
                result.return_description =
                    string(ast.return_description);
            }
            if (!exists result.source_file && exists ast.source_file) {
                result.source_file = string(ast.source_file);
            }
            SymbolMerger::enrichParamDescriptions(\result.params, ast);
        }

        # try doc_index as fallback — functions use qualified name
        if (!exists result.description) {
            *hash<auto> qpp = SymbolMerger::findInDocIndex(
                doc_index, fi.full_name, "function");
            if (exists qpp && exists qpp.description) {
                hash<auto> parsed =
                    DocCommentParser::parse(string(qpp.description));
                if (exists parsed && parsed.description.length()) {
                    result.description = string(parsed.description);
                }
                if (!exists result.return_description
                        && exists parsed.return_description) {
                    result.return_description =
                        string(parsed.return_description);
                }
                SymbolMerger::enrichParamDescriptionsFromParsedDoc(
                    \result.params, parsed);
            }
        }

        return result;
    }

    #! Enrich a constant with documentation
    private static hash<ConstantInfo> enrichConstant(hash<ConstantInfo> ci,
            hash<string, hash<auto>> ast_constants,
            hash<string, hash<auto>> doc_index) {
        hash<ConstantInfo> result = ci;

        # try AST constant docs first
        if (exists ast_constants{ci.full_name}) {
            hash<auto> ast = ast_constants{ci.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
        }

        # try doc_index as fallback
        if (!exists result.description) {
            *hash<auto> qpp = SymbolMerger::findInDocIndex(
                doc_index, ci.full_name, "constant");
            if (exists qpp && exists qpp.description) {
                hash<auto> parsed =
                    DocCommentParser::parse(string(qpp.description));
                if (exists parsed && parsed.description.length()) {
                    result.description = string(parsed.description);
                }
            }
        }

        return result;
    }

    #! Enrich a hashdecl with documentation
    private static hash<HashdeclInfo> enrichHashdecl(hash<HashdeclInfo> hi,
            hash<string, hash<auto>> ast_hashdecls,
            hash<string, hash<auto>> doc_index) {
        hash<HashdeclInfo> result = hi;

        # try AST hashdecl docs first
        if (exists ast_hashdecls{hi.full_name}) {
            hash<auto> ast = ast_hashdecls{hi.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
        }

        # try doc_index as fallback — try full name and base name
        if (!exists result.description) {
            *hash<auto> qpp = SymbolMerger::findInDocIndex(
                doc_index, hi.full_name, "hashdecl");
            if (!exists qpp) {
                # qpp hashdecls often have no namespace path
                qpp = SymbolMerger::findInDocIndex(
                    doc_index, hi.name, "hashdecl");
            }
            if (exists qpp && exists qpp.description) {
                hash<auto> parsed =
                    DocCommentParser::parse(string(qpp.description));
                if (exists parsed && parsed.description.length()) {
                    result.description = string(parsed.description);
                }
            }
            # enrich member descriptions from qpp
            if (exists qpp && exists qpp.members) {
                foreach hash<auto> qm in (qpp.members) {
                    foreach hash<MemberInfo> m in (\result.members) {
                        if (m.name == qm.name && !exists m.description
                                && exists qm.description) {
                            hash<auto> mdoc =
                                DocCommentParser::parse(
                                    string(qm.description));
                            if (exists mdoc
                                    && mdoc.description.length()) {
                                m.description =
                                    string(mdoc.description);
                            }
                            break;
                        }
                    }
                }
            }
        }

        return result;
    }

    #! Enrich an enum with documentation
    private static hash<EnumInfo> enrichEnum(hash<EnumInfo> ei,
            hash<string, hash<auto>> ast_enums,
            hash<string, hash<auto>> doc_index) {
        hash<EnumInfo> result = ei;

        # try AST enum docs first
        if (exists ast_enums{ei.full_name}) {
            hash<auto> ast = ast_enums{ei.full_name};
            if (!exists result.description && exists ast.description) {
                result.description = string(ast.description);
            }
        }

        # try doc_index as fallback
        if (!exists result.description) {
            *hash<auto> qpp = SymbolMerger::findInDocIndex(
                doc_index, ei.full_name, "enum");
            if (exists qpp && exists qpp.description) {
                hash<auto> parsed =
                    DocCommentParser::parse(string(qpp.description));
                if (exists parsed && parsed.description.length()) {
                    result.description = string(parsed.description);
                }
            }
            # enrich value descriptions from qpp
            if (exists qpp && exists qpp.values) {
                foreach hash<auto> qv in (qpp.values) {
                    foreach hash<EnumValueInfo> v in (\result.values) {
                        if (v.name == qv.name && !exists v.description
                                && exists qv.description) {
                            hash<auto> vdoc =
                                DocCommentParser::parse(
                                    string(qv.description));
                            if (exists vdoc
                                    && vdoc.description.length()) {
                                v.description =
                                    string(vdoc.description);
                            }
                            break;
                        }
                    }
                }
            }
        }

        return result;
    }

    #! Find a symbol in the doc index by full name, trying variations
    private static *hash<auto> findInDocIndex(
            hash<string, hash<auto>> doc_index,
            string name, string symbol_type) {
        # direct lookup
        if (exists doc_index{name}
                && doc_index{name}.symbol_type == symbol_type) {
            return doc_index{name};
        }
        # try without leading "Qore::" prefix (qpp may not include it)
        if (name =~ /^Qore::/) {
            string short_name = name.substr(6);
            if (exists doc_index{short_name}
                    && doc_index{short_name}.symbol_type == symbol_type) {
                return doc_index{short_name};
            }
        }
        return NOTHING;
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
