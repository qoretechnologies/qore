# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ReflectionMetadataExtractor.qc Runtime metadata extraction via reflection

/*  ReflectionMetadataExtractor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Extracts structured metadata from the Qore runtime via the Reflection API
public class ReflectionMetadataExtractor {
    #! Extract metadata from the runtime namespace hierarchy
    /** @param namespace_path namespace to extract (e.g., "Qore::Thread"),
        or NOTHING for the root namespace
        @return NamespaceInfo tree with classes, functions, constants, etc.
    */
    static hash<NamespaceInfo> extractFromRuntime(*string namespace_path) {
        Qore::Reflection::Namespace ns;
        if (exists namespace_path) {
            ns = Qore::Reflection::Namespace::forName(namespace_path);
        } else {
            ns = Qore::Reflection::Namespace::forName("Qore");
        }
        return ReflectionMetadataExtractor::extractNamespace(ns);
    }

    #! Extract metadata for a single class by name
    /** @param class_name fully or partially qualified class name
        @return ClassInfo with methods, members, etc.
    */
    static hash<ClassInfo> extractClass(string class_name) {
        Qore::Reflection::Class cls =
            Qore::Reflection::Class::forName(class_name);
        return ReflectionMetadataExtractor::buildClassInfo(cls);
    }

    #! Extract metadata for a single function by name
    /** @param func_name fully or partially qualified function name
        @return FunctionInfo for the first variant
    */
    static hash<FunctionInfo> extractFunction(string func_name) {
        Qore::Reflection::Function func =
            Qore::Reflection::Function::forName(func_name);
        string name = string(func.getName());
        list<auto> variants = func.getVariants();
        if (!variants.size()) {
            return <FunctionInfo>{
                "name": name,
                "full_name": name,
                "signature": name + "()",
            };
        }
        # try to determine namespace path from func_name
        *string ns_path;
        int pos = func_name.rfind("::");
        if (pos >= 0) {
            ns_path = func_name.substr(0, pos);
        }
        return ReflectionMetadataExtractor::buildFunctionVariantInfo(
            func, variants[0], ns_path ?? "");
    }

    #! Extract a full namespace recursively
    private static hash<NamespaceInfo> extractNamespace(
            Qore::Reflection::Namespace ns) {
        hash<NamespaceInfo> result({
            "name": ns.getName(),
            "full_name": ns.getPathName(),
        });

        # extract classes
        list<auto> classes = ns.getClasses();
        foreach auto cls in (classes) {
            result.classes += ReflectionMetadataExtractor::buildClassInfo(cls);
        }

        # extract functions (one FunctionInfo per variant)
        string ns_full = ns.getPathName();
        list<auto> funcs = ns.getFunctions();
        foreach auto func in (funcs) {
            list<auto> variants = func.getVariants();
            foreach auto v in (variants) {
                result.functions +=
                    ReflectionMetadataExtractor::buildFunctionVariantInfo(
                        func, v, ns_full);
            }
        }

        # extract constants
        list<auto> consts = ns.getConstants();
        foreach auto c in (consts) {
            result.constants +=
                ReflectionMetadataExtractor::buildConstantInfo(c);
        }

        # extract typed hashes (hashdecls)
        list<auto> hashes = ns.getTypedHashes();
        foreach auto h in (hashes) {
            result.hashdecls +=
                ReflectionMetadataExtractor::buildHashdeclInfo(h);
        }

        # recurse into child namespaces
        list<auto> child_namespaces = ns.getNamespaces();
        foreach auto child_ns in (child_namespaces) {
            result.namespaces +=
                ReflectionMetadataExtractor::extractNamespace(child_ns);
        }

        return result;
    }

    #! Build ClassInfo from a reflection Class object
    private static hash<ClassInfo> buildClassInfo(
            Qore::Reflection::Class cls) {
        string name = cls.getName();
        string full_name = cls.getPathName();

        hash<ClassInfo> ci({
            "name": name,
            "full_name": full_name,
            "is_abstract": cls.isAbstract(),
        });

        # extract module name
        *string mod = cls.getModuleName();
        if (exists mod) {
            ci.module_name = mod;
        }

        # extract parent classes
        list<auto> parents = cls.getParentClasses();
        if (parents.size()) {
            list<string> parent_names = ();
            foreach auto p in (parents) {
                parent_names += p.cls.getName();
            }
            ci.parents = parent_names;
        }

        # extract namespace path
        int pos = full_name.rfind("::");
        if (pos >= 0) {
            ci.namespace_path = full_name.substr(0, pos);
        }

        # extract methods (one MethodInfo per variant)
        list<auto> methods = cls.getMethods();
        foreach auto m in (methods) {
            list<auto> variants = m.getVariants();
            foreach auto v in (variants) {
                hash<MethodInfo> mi =
                    ReflectionMetadataExtractor::buildMethodVariantInfo(
                        m, v, full_name);
                if (mi.is_static) {
                    ci.static_methods += mi;
                } else {
                    ci.instance_methods += mi;
                }
            }
        }

        return ci;
    }

    #! Build MethodInfo from a reflection Method + Variant
    private static hash<MethodInfo> buildMethodVariantInfo(
            auto method, auto variant, string class_full_name) {
        string method_name = method.getName();
        string sig = variant.toString();

        # extract modifiers
        list<auto> modifiers = variant.getModifierList();
        string access = "public";
        bool is_static = False;
        bool is_abstract = False;
        foreach auto mod in (modifiers) {
            string mod_str = string(mod);
            switch (mod_str) {
                case "private":
                    access = "private";
                    break;
                case "private:internal":
                    access = "private:internal";
                    break;
                case "static":
                    is_static = True;
                    break;
                case "abstract":
                    is_abstract = True;
                    break;
            }
        }

        # extract params
        list<hash<ParameterInfo>> params =
            ReflectionMetadataExtractor::extractParams(variant);

        # check deprecation
        int flags = variant.getCodeFlags();
        bool is_deprecated = (flags & Qore::CF_DEPRECATED) ? True : False;

        # domain — getDomainStringList() can throw for non-standard bits
        *string domain;
        try {
            list<auto> dom_list = variant.getDomainStringList();
            if (dom_list.size()) {
                domain = dom_list.join(", ");
            }
        } catch (hash<ExceptionInfo> ex) {
            domain = sprintf("0x%x", variant.getDomain());
        }

        return <MethodInfo>{
            "name": method_name,
            "full_name": class_full_name + "::" + method_name,
            "signature": sig,
            "return_type": variant.getReturnTypeString(),
            "params": params,
            "access": access,
            "is_static": is_static,
            "is_abstract": is_abstract,
            "is_deprecated": is_deprecated,
            "functional_domain": domain,
        };
    }

    #! Build FunctionInfo from a reflection Function + Variant
    private static hash<FunctionInfo> buildFunctionVariantInfo(
            auto func, auto variant, string ns_full_name) {
        string name = string(func.getName());
        string full_name = ns_full_name + "::" + name;
        string sig = string(variant.toString());

        # extract params
        list<hash<ParameterInfo>> params =
            ReflectionMetadataExtractor::extractParams(variant);

        # check deprecation
        int flags = variant.getCodeFlags();
        bool is_deprecated = (flags & Qore::CF_DEPRECATED) ? True : False;

        # domain — getDomainStringList() can throw for non-standard bits
        *string domain;
        try {
            list<auto> dom_list = variant.getDomainStringList();
            if (dom_list.size()) {
                domain = dom_list.join(", ");
            }
        } catch (hash<ExceptionInfo> ex) {
            # ignore DOMAIN-ERROR
            domain = sprintf("0x%x", variant.getDomain());
        }

        return <FunctionInfo>{
            "name": name,
            "full_name": full_name,
            "namespace_path": ns_full_name,
            "signature": sig,
            "return_type": string(variant.getReturnTypeString()),
            "params": params,
            "is_deprecated": is_deprecated,
            "functional_domain": domain,
        };
    }

    #! Extract parameter info from a variant
    private static list<hash<ParameterInfo>> extractParams(auto variant) {
        list<hash<ParameterInfo>> params = ();
        list<auto> names = variant.getParamNames();
        list<auto> types = variant.getParamTypes();
        list<auto> defaults = variant.getDefaultArgs();

        for (int i = 0; i < names.size(); ++i) {
            string p_name = string(names[i]);
            string p_type = "auto";
            if (i < types.size() && exists types[i]) {
                p_type = types[i].getName();
            }
            *string p_default;
            if (i < defaults.size() && exists defaults[i]) {
                # use sprintf to handle any type including binary
                p_default = sprintf("%y", defaults[i]);
            }
            hash<ParameterInfo> pi({
                "name": p_name,
                "type_name": p_type,
                "default_value": p_default,
            });
            params += pi;
        }
        return params;
    }

    #! Build ConstantInfo from a reflection Constant
    private static hash<ConstantInfo> buildConstantInfo(auto constant) {
        string name = constant.getName();
        string type_name = constant.getType().getName();
        return <ConstantInfo>{
            "name": name,
            "full_name": name,
            "type_name": type_name,
        };
    }

    #! Build HashdeclInfo from a reflection TypedHash
    private static hash<HashdeclInfo> buildHashdeclInfo(auto typed_hash) {
        string name = typed_hash.getName();
        string full_name = typed_hash.getPathName();

        hash<HashdeclInfo> hi({
            "name": name,
            "full_name": full_name,
        });

        # extract namespace path
        int pos = full_name.rfind("::");
        if (pos >= 0) {
            hi.namespace_path = full_name.substr(0, pos);
        }

        # extract members
        list<auto> members = typed_hash.getMembers();
        foreach auto m in (members) {
            hash<MemberInfo> mi({
                "name": string(m.getName()),
                "type_name": string(m.getType().getName()),
            });
            hi.members += mi;
        }

        return hi;
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
