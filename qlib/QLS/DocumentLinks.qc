# -*- mode: qore; indent-tabs-mode: nil -*-

/*  DocumentLinks.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Finds the files that the module and include directives of a document refer to
/** Module names in @c %%requires and @c %%try-module directives are resolved like %Qore resolves them: in the
    directories of the document's @c %%prepend-module-path directives, the directories in @c QORE_MODULE_DIR,
    the default module path of the %Qore library and the directories of @c %%append-module-path directives.
    Only user modules (@c .qm files) have source files that can be opened.  Paths in @c %%include directives and
    module paths are relative to the document's directory.

    @par Example
    @code{.py}
# links for a script that requires a module from a project library directory
list<hash<auto>> links = DocumentLinks::getLinks("/src/project/bin/report.q", text);
    @endcode
*/
public class DocumentLinks {
    #! Returns the links of the directives in a document
    /** @param path the document's file path
        @param text the document's text

        @return a list of LSP \c DocumentLink hashes with \c range, \c target and \c tooltip keys, for the
        directives whose files exist
    */
    static list<hash<auto>> getLinks(string path, string text) {
        string dir = dirname(path);
        list<string> lines = text.split("\n");
        list<string> prepended = ();
        list<string> appended = ();
        # module path directives apply to the whole document
        foreach string line in (lines) {
            *list<auto> m = (line =~ x/^%(prepend|append)-module-path\s+(.+?)\s*$/);
            if (m) {
                string value = DocumentLinks::expand(DocumentLinks::unquote(m[1]), dir);
                list<string> dirs = map DocumentLinks::absolute($1, dir), (select value.split(":"), $1);
                if (m[0] == "prepend") {
                    prepended = dirs + prepended;
                } else {
                    appended += dirs;
                }
            }
        }
        list<string> moduleDirs = prepended + DocumentLinks::getEnvModuleDirs() + DocumentLinks::getLibraryModuleDirs()
            + appended;

        list<hash<auto>> links = ();
        for (int i = 0; i < lines.size(); ++i) {
            string line = lines[i];
            *hash<auto> link;
            *list<auto> m;
            if (m = (line =~ x/^(%requires(?:\(reexport\))?\s+)([^\s<>=!]+)/)) {
                link = DocumentLinks::moduleLink(m[1], m[0].length(), dir, moduleDirs);
            } else if (m = (line =~ x/^(%try-module(?:\s*\(\s*\$?\w+\s*\))?\s+)([^\s<>=!]+)/)) {
                link = DocumentLinks::moduleLink(m[1], m[0].length(), dir, moduleDirs);
            } else if (m = (line =~ x/^(%include\s+)(.+?)\s*$/)) {
                string target = DocumentLinks::absolute(DocumentLinks::expand(DocumentLinks::unquote(m[1]), dir),
                    dir);
                if (is_file(target)) {
                    link = {"start": m[0].length(), "length": m[1].length(), "target": target};
                }
            }
            if (link) {
                push links, {
                    "range": {
                        "start": {"line": i, "character": link.start},
                        "end": {"line": i, "character": link.start + link.length},
                    },
                    "target": Document::getFileUri(link.target),
                    "tooltip": link.target,
                };
            }
        }
        return links;
    }

    #! Returns the directives of a document that refer to files by path
    /** @param path the document's file path
        @param text the document's text

        @return a list of hashes with \c line, \c start and \c length (the position of the argument), \c arg (the
        argument without quotes), \c module (@ref True for a module path, which can omit the \c .qm extension) and
        \c path (the absolute path that the argument refers to, which need not exist)
    */
    static list<hash<auto>> getPathDirectives(string path, string text) {
        string dir = dirname(path);
        list<string> lines = text.split("\n");
        list<hash<auto>> ret = ();
        for (int i = 0; i < lines.size(); ++i) {
            *list<auto> m;
            bool module;
            if ((m = (lines[i] =~ x/^(%requires(?:\(reexport\))?\s+)([^\s<>=!]+)/))
                    || (m = (lines[i] =~ x/^(%try-module(?:\s*\(\s*\$?\w+\s*\))?\s+)([^\s<>=!]+)/))) {
                # module names are not paths
                if (m[1] !~ /[\/\\]/ && m[1] !~ /\.qm$/) {
                    continue;
                }
                module = True;
            } else if (!(m = (lines[i] =~ x/^(%include\s+)(.+?)\s*$/))) {
                continue;
            }
            string arg = DocumentLinks::unquote(m[1]);
            # the argument starts after a quote, if it is quoted
            int start = m[0].length() + (arg != m[1] ? 1 : 0);
            push ret, {
                "line": i,
                "start": start,
                "length": arg.length(),
                "arg": arg,
                "module": module,
                "path": DocumentLinks::normalizePath(DocumentLinks::absolute(DocumentLinks::expand(arg, dir), dir)),
            };
        }
        return ret;
    }

    #! Returns the edits to a document's directives when a file is renamed or moved
    /** The paths of directives that refer to the renamed file are changed to the new path.  If the document is
        the renamed file and it moves to another directory, its relative paths are changed so that they still
        refer to the same files.  Relative paths stay relative, paths starting with \c ${SCRIPT_DIR} keep it, and
        absolute paths stay absolute; paths with other variables are not changed.

        @param docPath the document's file path before the rename
        @param text the document's text
        @param oldPath the path of the renamed file
        @param newPath the new path of the file

        @return the LSP text edits for the document
    */
    static list<hash<auto>> getRenameEdits(string docPath, string text, string oldPath, string newPath) {
        oldPath = DocumentLinks::normalizePath(oldPath);
        newPath = DocumentLinks::normalizePath(newPath);
        docPath = DocumentLinks::normalizePath(docPath);
        string oldDir = dirname(docPath);
        string newDir = docPath == oldPath ? dirname(newPath) : oldDir;
        list<hash<auto>> edits = ();
        foreach hash<auto> d in (DocumentLinks::getPathDirectives(docPath, text)) {
            string target = d.path;
            *string newTarget;
            if (target == oldPath) {
                newTarget = newPath;
            } else if (d.module && target + ".qm" == oldPath) {
                # a module path without the extension keeps it out
                if (newPath !~ /\.qm$/) {
                    continue;
                }
                newTarget = newPath.substr(0, -3);
            } else {
                newTarget = target;
            }
            if (newTarget == target && newDir == oldDir) {
                continue;
            }
            string arg;
            if (absolute_path(d.arg)) {
                if (newTarget == target) {
                    continue;
                }
                arg = newTarget;
            } else if (d.arg.equalPartial("${SCRIPT_DIR}")) {
                arg = "${SCRIPT_DIR}/" + DocumentLinks::relativePath(newDir, newTarget);
            } else if (d.arg =~ /\$/) {
                # paths with other variables cannot be changed reliably
                continue;
            } else {
                arg = DocumentLinks::relativePath(newDir, newTarget);
                if (d.arg.equalPartial("./") && !arg.equalPartial("..")) {
                    arg = "./" + arg;
                }
            }
            if (arg == d.arg) {
                continue;
            }
            push edits, {
                "range": {
                    "start": {"line": d.line, "character": d.start},
                    "end": {"line": d.line, "character": d.start + d.length},
                },
                "newText": arg,
            };
        }
        return edits;
    }

    #! Returns the path with "." and ".." components resolved and duplicate separators removed
    static string normalizePath(string path) {
        bool absolute = path.equalPartial("/");
        list<string> parts = ();
        foreach string part in (path.split("/")) {
            if (part == "" || part == ".") {
                continue;
            }
            if (part == ".." && parts && parts.last() != "..") {
                pop parts;
            } else if (part != ".." || !absolute) {
                push parts, part;
            }
        }
        string ret = parts.join("/");
        return absolute ? "/" + ret : (ret ?: ".");
    }

    #! Returns the relative path from a directory to a path
    static string relativePath(string fromDir, string to) {
        list<string> from = select DocumentLinks::normalizePath(fromDir).split("/"), $1;
        list<string> target = select DocumentLinks::normalizePath(to).split("/"), $1;
        int common = 0;
        while (common < from.size() && common < target.size() && from[common] == target[common]) {
            ++common;
        }
        list<string> parts = map "..", xrange(from.size() - common);
        parts += target[common..];
        return parts.join("/") ?: ".";
    }

    #! Returns the link for a module name or path, or @ref nothing if it has no source file
    private static *hash<auto> moduleLink(string name, int start, string dir, list<string> moduleDirs) {
        # the interpreter's version requirement is not a module
        if (name == "qore") {
            return NOTHING;
        }
        *string target;
        if (name =~ /[\/\\]/ || name =~ /\.qm$/) {
            string path = DocumentLinks::absolute(DocumentLinks::expand(name, dir), dir);
            target = DocumentLinks::findModuleFile(path);
        } else {
            foreach string moduleDir in (moduleDirs) {
                target = DocumentLinks::findModuleFile(moduleDir + DirSep + name);
                if (target) {
                    break;
                }
            }
        }
        return target ? {"start": start, "length": name.length(), "target": target} : NOTHING;
    }

    #! Returns the source file of a module at the given path without extension, a .qm file or a module directory
    private static *string findModuleFile(string path) {
        foreach string candidate in ((path, path + ".qm", path + DirSep + basename(path) + ".qm")) {
            if (candidate =~ /\.qm$/ && is_file(candidate)) {
                return candidate;
            }
        }
        return NOTHING;
    }

    #! Returns the directories in the QORE_MODULE_DIR environment variable
    static list<string> getEnvModuleDirs() {
        # the current environment of the process
        return select (getenv("QORE_MODULE_DIR") ?? "").split(PlatformOS == "Windows" ? ";" : ":"), $1;
    }

    #! Returns the default module directories of the Qore library
    static list<string> getLibraryModuleDirs() {
        return select (get_qore_library_info().ModulePath ?? "").split(PlatformOS == "Windows" ? ";" : ":"), $1;
    }

    #! Removes quotes around a directive argument
    private static string unquote(string arg) {
        *list<auto> m = (arg =~ x/^"(.*)"$/) ?? (arg =~ x/^'(.*)'$/);
        return m ? m[0] : arg;
    }

    #! Expands ${SCRIPT_DIR} and environment variables in a directive argument
    static string expand(string arg, string dir) {
        string ret = replace(arg, "${SCRIPT_DIR}", dir);
        foreach string name in ((ret =~ x/\$\{(\w+)\}/g) ?? ()) {
            ret = replace(ret, "${" + name + "}", getenv(name) ?? "");
        }
        foreach string name in ((ret =~ x/\$(\w+)/g) ?? ()) {
            ret = replace(ret, "$" + name, getenv(name) ?? "");
        }
        return ret;
    }

    #! Returns the path relative to the directory, or the path if it is absolute
    static string absolute(string path, string dir) {
        return absolute_path(path) ? path : dir + DirSep + path;
    }
}

} # namespace QLS
