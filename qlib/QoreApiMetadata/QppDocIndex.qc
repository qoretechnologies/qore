# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QppDocIndex.qc Load and index qpp .meta.json metadata files

/*  QppDocIndex.qc Copyright 2026 Qore Technologies, s.r.o.

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

%ifdef NoJson
#! Stub class when json module is not available
public class QppDocIndex {
    #! Cannot build index without JSON support
    static hash<string, hash<auto>> buildFromMetaFiles(list<string> paths) {
        throw "JSON-NOT-AVAILABLE",
            "the json module is required for QppDocIndex";
    }

    #! Cannot build index without JSON support
    static hash<string, hash<auto>> buildFromDirectory(string dir_path) {
        throw "JSON-NOT-AVAILABLE",
            "the json module is required for QppDocIndex";
    }
}
%else
#! Loads qpp .meta.json files and builds a qualified-name-keyed index
/** The index maps fully-qualified symbol names (e.g., "Qore::Thread::Mutex",
    "Qore::printf") to their metadata hashes, which include documentation,
    parameters, return types, etc. as written in the qpp source files.
*/
public class QppDocIndex {
    #! Build an index from a list of .meta.json file paths
    /** @param paths list of paths to .meta.json files
        @return hash keyed by qualified name, values are metadata hashes
    */
    static hash<string, hash<auto>> buildFromMetaFiles(list<string> paths) {
        hash<string, hash<auto>> index();
        foreach string path in (paths) {
            if (!is_readable(path)) {
                continue;
            }
            string content = ReadOnlyFile::readTextFile(path);
            hash<auto> meta = parse_json(content);
            QppDocIndex::indexMetadata(meta, \index);
        }
        return index;
    }

    #! Build an index from all .meta.json files in a directory
    /** @param dir_path path to directory containing .meta.json files
        @return hash keyed by qualified name, values are metadata hashes
    */
    static hash<string, hash<auto>> buildFromDirectory(string dir_path) {
        hash<string, hash<auto>> index();
        Dir d();
        if (!d.chdir(dir_path)) {
            return index;
        }
        list<string> files = d.listFiles("\\.meta\\.json$");
        foreach string fname in (files) {
            string path = dir_path + "/" + fname;
            string content = ReadOnlyFile::readTextFile(path);
            hash<auto> meta = parse_json(content);
            QppDocIndex::indexMetadata(meta, \index);
        }
        return index;
    }

    #! Index a single parsed metadata hash into the index
    private static indexMetadata(hash<auto> meta,
            reference<hash<string, hash<auto>>> index) {
        *string source_file = meta.source_file;

        # index classes
        if (meta.classes) {
            foreach hash<auto> cls in (meta.classes) {
                string qname =
                    QppDocIndex::qualifiedName(cls.namespace_path, cls.name);
                cls.source_file = source_file;
                cls.symbol_type = "class";
                index{qname} = cls;

                # also index individual methods with class prefix
                if (cls.instance_methods) {
                    foreach hash<auto> m in (cls.instance_methods) {
                        string mname = qname + "::" + m.name;
                        m.source_file = source_file;
                        m.symbol_type = "method";
                        m.class_name = cls.name;
                        # for overloaded methods, append signature to
                        # disambiguate
                        if (exists index{mname}) {
                            mname += ":" + m.signature;
                        }
                        index{mname} = m;
                    }
                }
                if (cls.static_methods) {
                    foreach hash<auto> m in (cls.static_methods) {
                        string mname = qname + "::" + m.name;
                        m.source_file = source_file;
                        m.symbol_type = "static_method";
                        m.class_name = cls.name;
                        # for overloaded methods, append signature to
                        # disambiguate
                        if (exists index{mname}) {
                            mname += ":" + m.signature;
                        }
                        index{mname} = m;
                    }
                }
            }
        }

        # index functions
        if (meta.functions) {
            foreach hash<auto> func in (meta.functions) {
                string qname = QppDocIndex::qualifiedName(
                    func.namespace_path, func.name);
                func.source_file = source_file;
                func.symbol_type = "function";
                # for overloaded functions, append param count
                if (exists index{qname}) {
                    qname += ":" + func.signature;
                }
                index{qname} = func;
            }
        }

        # index constants
        if (meta.constants) {
            foreach hash<auto> c in (meta.constants) {
                string qname =
                    QppDocIndex::qualifiedName(c.namespace_path, c.name);
                c.source_file = source_file;
                c.symbol_type = "constant";
                index{qname} = c;
            }
        }

        # index hashdecls
        if (meta.hashdecls) {
            foreach hash<auto> hd in (meta.hashdecls) {
                string qname =
                    QppDocIndex::qualifiedName(hd.namespace_path, hd.name);
                hd.source_file = source_file;
                hd.symbol_type = "hashdecl";
                index{qname} = hd;
            }
        }

        # index enums
        if (meta.enums) {
            foreach hash<auto> en in (meta.enums) {
                string qname =
                    QppDocIndex::qualifiedName(en.namespace_path, en.name);
                en.source_file = source_file;
                en.symbol_type = "enum";
                index{qname} = en;
            }
        }
    }

    #! Build a qualified name from namespace path and name
    private static string qualifiedName(*string ns_path, string name) {
        if (exists ns_path && ns_path.length()) {
            return ns_path + "::" + name;
        }
        return name;
    }
}
%endif

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
