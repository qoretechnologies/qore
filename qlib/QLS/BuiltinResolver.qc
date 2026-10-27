# -*- mode: qore; indent-tabs-mode: nil -*-

/*  BuiltinResolver.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QLS {

#! A builtin type: a class or the pseudo-class of a basic type, a typed hash or a namespace
public hashdecl BuiltinType {
    #! the class, or the pseudo-class of a basic type, such as \c \<string\> for strings
    *hash<auto> cls;

    #! the hashdecl of a typed hash; typed hashes also have the methods of the \c \<hash\> pseudo-class
    *hash<auto> hashDecl;

    #! the namespace of a namespace name
    *hash<auto> ns;
}

#! The builtin symbol of an expression and the type of its value
public hashdecl BuiltinResolution {
    #! the metadata entries of the symbol
    /** The overloads of a function or method, or a member, class, constant, hashdecl, enum or namespace; each
        entry has a \c symbol_type key
    */
    list<hash<auto>> entries = ();

    #! the type of the value of the expression, if it is a builtin type
    *hash<BuiltinType> type;
}

#! Returns @ref True if a name is declared in the source code, so it does not name a builtin symbol
public typedef BuiltinSourceNameCheck = code<bool(string)>;

#! Resolves expressions to the builtin symbols they refer to and to the builtin types of their values
/** The builtin symbols are the functions, classes, constants, hashdecls, enums and namespaces of the builtin
    metadata, and the pseudo-classes of the basic types: the methods of \c \<string\> can be called on strings, for
    example, and the methods of \c \<value\> on all values.  The members of a class include the members of its parent
    classes, of the \c \<object\> pseudo-class and of the \c \<value\> pseudo-class.

    Expressions are given as the receiver chains of @ref astparser::AstTreeSearcher::getExpressionChain(), so
    \c "f.stat().size" is resolved through the type of \c f, the return type of the \c stat() method and the type of
    the \c size member.

    The resolver is immutable after construction and can be shared by threads.

    @par Example:
    @code{.py}
# the type of "now().format" in "now().format(\"YYYY\")": the method of dates that returns a string
*hash<BuiltinResolution> resolution = resolver.resolve(doc.getExpressionChain(position), isSourceName);
if (resolution.entries) {
    printf("%s\n", resolution.entries[0].signature);  # string <date>::format(string format) const
}
    @endcode
*/
public class BuiltinResolver {
    public {
        #! The pseudo-classes of the basic types
        const BasicTypePseudoClasses = {
            "string": "<string>",
            "softstring": "<string>",
            "int": "<int>",
            "softint": "<int>",
            "float": "<float>",
            "softfloat": "<float>",
            "number": "<number>",
            "softnumber": "<number>",
            "bool": "<bool>",
            "softbool": "<bool>",
            "date": "<date>",
            "softdate": "<date>",
            "binary": "<binary>",
            "list": "<list>",
            "softlist": "<list>",
            "hash": "<hash>",
            "object": "<object>",
            "code": "<closure>",
            "closure": "<closure>",
            "callref": "<callref>",
            "nothing": "<nothing>",
        };

        #! The names of special methods that are not called with "."
        const SpecialMethods = {
            "constructor": True,
            "destructor": True,
            "copy": True,
        };
    }

    private {
        #! unqualified name -> metadata entries
        hash<string, list<hash<auto>>> metaByName;

        #! class name -> class metadata
        hash<string, hash<auto>> metaClassByName;

        #! pseudo-class name, such as "<string>" -> pseudo-class metadata
        hash<string, hash<auto>> pseudoClassByName = {};

        #! namespace name, qualified and unqualified -> namespace metadata
        hash<string, hash<auto>> namespaceByName = {};
    }

    #! Creates the resolver from the builtin metadata indexes
    /** @param metaByName unqualified name -> metadata entries with a \c symbol_type key
        @param metaClassByName qualified and unqualified class name -> class metadata
        @param pseudoClasses the pseudo-classes of the basic types, from
        @ref QoreApiMetadata::SymbolMerger::extractPseudoClasses()
        @param rootNamespace the root namespace of the builtin metadata, for namespace names
    */
    constructor(hash<string, list<hash<auto>>> metaByName, hash<string, hash<auto>> metaClassByName,
            *list<hash<auto>> pseudoClasses, *hash<auto> rootNamespace) {
        self.metaByName = metaByName;
        self.metaClassByName = metaClassByName;
        map pseudoClassByName{$1.name} = $1, pseudoClasses;
        if (rootNamespace) {
            indexNamespace(rootNamespace);
        }
    }

    #! Resolves the receiver chain of an expression
    /** Names and types written in the source code are not resolved as builtin symbols if the source code declares
        them; the types in the builtin metadata, such as the return types of builtin functions, are builtin types.

        @param chain the receiver chain from @ref astparser::AstTreeSearcher::getExpressionChain()
        @param isSourceName returns @ref True for names declared in the source code, which are not resolved as
        builtin symbols

        @return the builtin symbol of the last link and the type of its value, or @ref nothing if the chain does not
        resolve to a builtin symbol or type
    */
    *hash<BuiltinResolution> resolve(*list<hash<auto>> chain, *BuiltinSourceNameCheck isSourceName) {
        *hash<BuiltinResolution> resolution;
        foreach hash<auto> link in (chain) {
            if ($# && !resolution.type) {
                return NOTHING;
            }
            switch (link.kind) {
                case "type":
                    resolution = <BuiltinResolution>{"type": getType(link.typeName, isSourceName)};
                    break;

                case "name":
                    if ($#) {
                        return NOTHING;
                    }
                    if (link.typeName) {
                        # a variable; the result of calling a closure or call reference is unknown
                        if (link.call) {
                            return NOTHING;
                        }
                        resolution = <BuiltinResolution>{"type": getType(link.typeName, isSourceName)};
                        break;
                    }
                    if (isSourceName && isSourceName(BuiltinResolver::getUnqualifiedName(link.name))) {
                        return NOTHING;
                    }
                    resolution = resolveName(link.name, link.call ?? False);
                    break;

                case "member":
                    if (!resolution.type) {
                        return NOTHING;
                    }
                    resolution = resolveMember(resolution.type, link.name, link.call ?? False);
                    break;

                default:
                    return NOTHING;
            }
        }
        return (resolution.entries || resolution.type) ? resolution : NOTHING;
    }

    #! Returns the builtin type of a type name, or @ref nothing if it is not a builtin type
    /** @param typeName a type name, such as \c "*hash<StatInfo>", \c "list<int>" or \c "Qore::Thread::Mutex"
        @param isSourceName returns @ref True for names declared in the source code, which are not builtin types
    */
    *hash<BuiltinType> getType(*string typeName, *BuiltinSourceNameCheck isSourceName) {
        if (!typeName) {
            return NOTHING;
        }
        string text = trim(typeName);
        if (text.equalPartial("*")) {
            text = trim(text.substr(1));
        }
        string base = text;
        *string args;
        int lt = text.find("<");
        if (lt > 0 && text[text.length() - 1] == ">") {
            base = trim(text.substr(0, lt));
            args = trim(text.substr(lt + 1, text.length() - lt - 2));
        }

        # a typed hash
        if (base == "hash" && args && args != "auto" && args.find(",") < 0) {
            if (!isSourceName || !isSourceName(BuiltinResolver::getUnqualifiedName(args))) {
                *hash<auto> hd = findEntry(args, "hashdecl");
                if (hd) {
                    return <BuiltinType>{"hashDecl": hd};
                }
            }
            return NOTHING;
        }

        *string pseudoClassName = BasicTypePseudoClasses{base};
        if (pseudoClassName) {
            *hash<auto> pseudoClass = pseudoClassByName{pseudoClassName};
            return pseudoClass ? <BuiltinType>{"cls": pseudoClass} : NOTHING;
        }

        if (isSourceName && isSourceName(BuiltinResolver::getUnqualifiedName(base))) {
            return NOTHING;
        }
        *hash<auto> cls = findClass(base);
        return cls ? <BuiltinType>{"cls": cls} : NOTHING;
    }

    #! Returns the members that can be accessed with "." on a value of a builtin type
    /** @param type the type of the value

        @return the metadata entries of the methods and public members; a method or member that is redefined in a
        child class is returned once
    */
    list<hash<auto>> getInstanceMembers(hash<BuiltinType> type) {
        hash<string, hash<auto>> result = {};
        if (type.hashDecl) {
            foreach hash<auto> m in (type.hashDecl.members) {
                result{m.name} = BuiltinResolver::makeMemberEntry(type.hashDecl, m);
            }
        }
        foreach hash<auto> cls in (getClassChain(type)) {
            foreach hash<auto> m in (cls.instance_methods) {
                if (!SpecialMethods{m.name} && !result{m.name}) {
                    result{m.name} = BuiltinResolver::makeMethodEntry(cls, m);
                }
            }
            foreach hash<auto> m in (cls.members) {
                if (m.access == "public" && !m.is_static && !result{m.name}) {
                    result{m.name} = BuiltinResolver::makeMemberEntry(cls, m);
                }
            }
        }
        return result.values() ?? ();
    }

    #! Returns the overloads of a method that can be called with "." on a value of a builtin type
    /** @param type the type of the value
        @param name the name of the method

        @return the metadata entries of the overloads of the method in the first class of the type, its parent
        classes or its pseudo-classes that has the method
    */
    list<hash<auto>> getMethods(hash<BuiltinType> type, string name) {
        return resolveMember(type, name, True).entries ?? ();
    }

    #! Returns the members that can be accessed with "::" on a builtin namespace or class
    /** @param type the type of the namespace or class name

        @return the metadata entries of the classes, functions, constants, hashdecls, enums and namespaces of a
        namespace, or the methods and constants of a class
    */
    list<hash<auto>> getScopeMembers(hash<BuiltinType> type) {
        hash<string, hash<auto>> result = {};
        if (type.ns) {
            foreach hash<auto> c in (type.ns.classes) {
                result{c.name} = c + {"symbol_type": "class"};
            }
            foreach hash<auto> f in (type.ns.functions) {
                if (!result{f.name}) {
                    result{f.name} = f + {"symbol_type": "function"};
                }
            }
            foreach hash<auto> c in (type.ns.constants) {
                result{c.name} = c + {"symbol_type": "constant"};
            }
            foreach hash<auto> h in (type.ns.hashdecls) {
                result{h.name} = h + {"symbol_type": "hashdecl"};
            }
            foreach hash<auto> e in (type.ns.enums) {
                result{e.name} = e + {"symbol_type": "enum"};
            }
            foreach hash<auto> n in (type.ns.namespaces) {
                result{n.name} = BuiltinResolver::makeNamespaceEntry(n);
            }
        } else if (type.cls) {
            foreach hash<auto> m in (type.cls.static_methods) {
                if (!result{m.name}) {
                    result{m.name} = BuiltinResolver::makeMethodEntry(type.cls, m);
                }
            }
            foreach hash<auto> m in (type.cls.instance_methods) {
                if (!SpecialMethods{m.name} && !result{m.name}) {
                    result{m.name} = BuiltinResolver::makeMethodEntry(type.cls, m);
                }
            }
            foreach hash<auto> c in (type.cls.constants) {
                result{c.name} = c + {"symbol_type": "constant"};
            }
        }
        return result.values() ?? ();
    }

    #! Returns the unqualified name of a possibly qualified name
    /** @param name a name such as \c "Qore::Thread::Mutex"

        @return the name without its namespaces, such as \c "Mutex"
    */
    static string getUnqualifiedName(string name) {
        int i = name.rfind("::");
        return i >= 0 ? name.substr(i + 2) : name;
    }

    #! Resolves a name that is not a variable
    private hash<BuiltinResolution> resolveName(string name, bool call) {
        string unqualifiedName = BuiltinResolver::getUnqualifiedName(name);
        if (call) {
            list<hash<auto>> functions = select (metaByName{unqualifiedName} ?? ()),
                $1.symbol_type == "function" && BuiltinResolver::matchesName($1.full_name, name);
            return <BuiltinResolution>{
                "entries": functions,
                "type": functions ? getType(functions[0].return_type) : NOTHING,
            };
        }

        *hash<auto> ns = namespaceByName{BuiltinResolver::stripScope(name)};
        if (ns) {
            return <BuiltinResolution>{
                "entries": (BuiltinResolver::makeNamespaceEntry(ns),),
                "type": <BuiltinType>{"ns": ns},
            };
        }

        list<hash<auto>> entries = select (metaByName{unqualifiedName} ?? ()),
            $1.symbol_type != "method" && $1.symbol_type != "static_method"
            && BuiltinResolver::matchesName($1.full_name, name);
        if (!entries) {
            return <BuiltinResolution>{};
        }
        *hash<BuiltinType> type;
        switch (entries[0].symbol_type) {
            case "class":
                type = <BuiltinType>{"cls": metaClassByName{entries[0].full_name} ?? entries[0]};
                break;
            case "hashdecl":
                type = <BuiltinType>{"hashDecl": entries[0]};
                break;
            case "constant":
                type = getType(entries[0].type_name);
                break;
        }
        return <BuiltinResolution>{"entries": entries, "type": type};
    }

    #! Resolves a member accessed with "." on a value of a builtin type
    /** The types of builtin members and the return types of builtin methods are builtin types, also when the
        source code declares types with the same names
    */
    private *hash<BuiltinResolution> resolveMember(hash<BuiltinType> type, *string name, bool call) {
        # the members of a namespace are accessed with "::"
        if (!name || type.ns) {
            return NOTHING;
        }
        list<hash<auto>> classes = getClassChain(type);
        if (!call) {
            # a member of a typed hash or of an object
            list<hash<auto>> owners = type.hashDecl ? (type.hashDecl,) : classes;
            foreach hash<auto> owner in (owners) {
                foreach hash<auto> m in (owner.members) {
                    if (m.name == name && !m.is_static) {
                        return <BuiltinResolution>{
                            "entries": (BuiltinResolver::makeMemberEntry(owner, m),),
                            "type": getType(m.type_name),
                        };
                    }
                }
            }
        }
        # a method, also when it is not called, as in a call reference
        foreach hash<auto> cls in (classes) {
            list<hash<auto>> methods = select (cls.instance_methods ?? ()), $1.name == name;
            if (methods) {
                return <BuiltinResolution>{
                    "entries": (map BuiltinResolver::makeMethodEntry(cls, $1), methods),
                    "type": call ? getType(methods[0].return_type) : NOTHING,
                };
            }
        }
        return NOTHING;
    }

    #! Returns the class, its parent classes and the pseudo-classes whose methods can be called on a value
    private list<hash<auto>> getClassChain(hash<BuiltinType> type) {
        list<hash<auto>> queue = ();
        if (type.hashDecl) {
            queue += pseudoClassByName."<hash>" ?? ();
        } else if (type.cls) {
            queue += type.cls;
        }
        bool isObject = type.cls && !type.cls.name.equalPartial("<");

        list<hash<auto>> result = ();
        hash<string, bool> seen = {};
        while (True) {
            while (queue) {
                hash<auto> cls = shift queue;
                string key = cls.full_name ?? cls.name;
                if (seen{key}) {
                    continue;
                }
                seen{key} = True;
                result += cls;
                foreach string parent in (cls.parents) {
                    *hash<auto> parentClass = parent.equalPartial("<")
                        ? pseudoClassByName{parent}
                        : findClass(parent);
                    if (parentClass) {
                        queue += parentClass;
                    }
                }
            }
            # objects also have the methods of the object pseudo-class after those of their classes
            if (!isObject || !pseudoClassByName."<object>") {
                break;
            }
            isObject = False;
            queue += pseudoClassByName."<object>";
        }
        return result;
    }

    #! Returns the builtin class with the given name
    private *hash<auto> findClass(string name) {
        name = BuiltinResolver::stripScope(name);
        # generic type arguments do not change the members
        int lt = name.find("<");
        if (lt > 0) {
            name = name.substr(0, lt);
        }
        return metaClassByName{name} ?? metaClassByName{"Qore::" + name};
    }

    #! Returns the first metadata entry of the given symbol type with the given name
    private *hash<auto> findEntry(string name, string symbolType) {
        name = BuiltinResolver::stripScope(name);
        foreach hash<auto> entry in (metaByName{BuiltinResolver::getUnqualifiedName(name)}) {
            if (entry.symbol_type == symbolType && BuiltinResolver::matchesName(entry.full_name, name)) {
                return entry;
            }
        }
        return NOTHING;
    }

    #! Indexes a namespace and its child namespaces by qualified name, by name relative to Qore and by name
    private indexNamespace(hash<auto> ns) {
        list<string> names = (ns.full_name, ns.name);
        if (ns.full_name.equalPartial("Qore::")) {
            names += ns.full_name.substr(6);
        }
        foreach string name in (names) {
            if (!namespaceByName{name}) {
                namespaceByName{name} = ns;
            }
        }
        map indexNamespace($1), ns.namespaces;
    }

    #! Returns @ref True if a qualified name matches a name as written in the source
    /** A name without a namespace matches all symbols with that name; a qualified name matches the end of the
        qualified name of the symbol, as namespaces can be given relative to the root namespace.
    */
    private static bool matchesName(string fullName, string name) {
        name = BuiltinResolver::stripScope(name);
        if (name.find("::") < 0 || fullName == name) {
            return True;
        }
        string suffix = "::" + name;
        return fullName.length() > suffix.length() && fullName.substr(-suffix.length()) == suffix;
    }

    #! Removes the leading "::" of a name relative to the root namespace
    private static string stripScope(string name) {
        return name.equalPartial("::") ? name.substr(2) : name;
    }

    #! Returns the metadata entry of a method
    private static hash<auto> makeMethodEntry(hash<auto> cls, hash<auto> m) {
        return m + {
            "symbol_type": m.is_static ? "static_method" : "method",
            "class_name": cls.name,
        };
    }

    #! Returns the metadata entry of a member of a class or hashdecl
    private static hash<auto> makeMemberEntry(hash<auto> owner, hash<auto> m) {
        return m + {
            "symbol_type": "member",
            "class_name": owner.name,
            "signature": sprintf("%s %s::%s", m.type_name ?? "auto", owner.full_name ?? owner.name, m.name),
        };
    }

    #! Returns the metadata entry of a namespace
    private static hash<auto> makeNamespaceEntry(hash<auto> ns) {
        return {
            "symbol_type": "namespace",
            "name": ns.name,
            "full_name": ns.full_name,
            "signature": "namespace " + ns.full_name,
            "description": ns.description,
        };
    }
}
}
