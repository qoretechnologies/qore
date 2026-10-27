# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Files.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

public namespace Files {

public const QORE_EXTENSIONS = {
    # Qore file extensions
    "q":        "Qore script",
    "ql":       "Qore library",
    "qc":       "Qore class",
    "qm":       "Qore user module",
    "qr":       "Qore script",
    "qtest":    "Qore unit test",

    # Qorus file extensions
    "qclass":   "Qorus class",
    "qconn":    "Qorus connection",
    "qwf":      "Qorus workflow definition",
    "qsd":      "Qorus service",
    "qfd":      "Qorus functions",
    "qjob":     "Qorus job",
    "qconst":   "Qorus constant",
    "qsm":      "Qorus schema module",
    "qstep":    "Qorus workflow step",
    "qmc":      "Qorus mapper code",
};

public bool sub is_valid_qore_file(string path) {
    *hash<StatInfo> info = hstat(path);
    if (!info) {
        # return False if we can't stat() the file
        return False;
    }

    # AOT build artifacts are binary metadata/code, not parseable Qore source.
    # Avoid probing them as text when qlib contains generated .qmod symlinks.
    if (path =~ /\.(qmod|qo|qoa)(\.d)?$/i) {
        return False;
    }

    *string ext = (path =~ x/\.([a-z0-9]+)$/i)[0];
    if (ext && QORE_EXTENSIONS{ext}) {
        return True;
    }

    # here we are trying to simulate magic as specified in Freedesktop's
    # Shared MIME-info Database specification.
    # For Qore we just assume that "#!/usr/bin/env qore" is used
    try {
        ReadOnlyFile f(path);
        *string line = f.readLine(False);
        return (line =~ /^#!\/usr\/bin\/env qore/)
            || (line =~ /^#![^ ]+\/qore($| )/);
    } catch (hash<ExceptionInfo> ex) {
        # ignore exceptions and return False
    }

    return False;
}

#! Directory name patterns that are not searched for source files by default
/** Patterns are shell-style globs (@c * and @c ?) matched against the name of each subdirectory; version control
    metadata, dependency trees, agent worktrees and build output hold copies of sources or generated files that
    would otherwise be indexed again.
*/
public const DefaultExcludeDirs = (
    ".git",
    ".hg",
    ".svn",
    ".claude",
    ".vscode-test",
    "node_modules",
    "build",
    "build-*",
);

#! Source files larger than this are not indexed by default (8 MiB)
public const DefaultMaxIndexFileSize = 8 * 1024 * 1024;

#! Options for finding source files
public hashdecl FileScanOptions {
    #! Shell-style glob patterns for subdirectory names that are not searched
    list<string> excludeDirs = DefaultExcludeDirs;

    #! Files larger than this many bytes are skipped; 0 means no limit
    int maxFileSize = DefaultMaxIndexFileSize;
}

#! Returns a regular expression matching any of the given shell-style glob patterns
/** @param patterns glob patterns using @c * and @c ?

    @return an anchored regular expression, or @ref nothing if there are no patterns
*/
public *string sub glob_list_to_regex(list<string> patterns) {
    list<string> alternatives = ();
    foreach string pattern in (patterns) {
        string re;
        for (int i = 0; i < pattern.size(); ++i) {
            string c = pattern[i];
            switch (c) {
                case "*":
                    re += ".*";
                    break;
                case "?":
                    re += ".";
                    break;
                default:
                    if (c =~ /[\\^$.|+(){}\[\]]/) {
                        re += "\\";
                    }
                    re += c;
                    break;
            }
        }
        push alternatives, re;
    }
    return alternatives ? "^(" + alternatives.join("|") + ")$" : NOTHING;
}

#! Returns @ref True if the given path is not indexed according to the options
/** A path is excluded if any directory component below @p root matches an exclude pattern, or if the file is
    larger than the maximum file size.

    @param path the absolute file path
    @param root the root directory that was scanned
    @param opts the scan options

    @return @ref True if the file would not have been found by find_qore_files() with the same options
*/
public bool sub is_excluded_path(string path, string root, hash<FileScanOptions> opts = <FileScanOptions>{}) {
    if (opts.maxFileSize > 0) {
        *hash<StatInfo> info = hstat(path);
        if (info && info.size > opts.maxFileSize) {
            return True;
        }
    }
    *string excludeRegex = glob_list_to_regex(opts.excludeDirs);
    if (!excludeRegex) {
        return False;
    }
    string rel = path;
    string prefix = root + DirSep;
    if (path.equalPartial(prefix)) {
        rel = path.substr(prefix.size());
    }
    list<string> components = rel.split(DirSep);
    # the last component is the file name
    pop components;
    foreach string component in (components) {
        if (regex(component, excludeRegex)) {
            return True;
        }
    }
    return False;
}

#! Find files in given root path
/** Subdirectories matching @ref QLS::Files::FileScanOptions::excludeDirs "FileScanOptions.excludeDirs" are not
    searched, files larger than @ref QLS::Files::FileScanOptions::maxFileSize "FileScanOptions.maxFileSize" are
    skipped, and each real directory is searched once, so symbolic links cannot cause loops or duplicate results.

    @param path the root directory to search
    @param opts the scan options
    @param skippedFiles if given, receives the paths of Qore files skipped because of their size

    @return list of file paths

    @throw WORKSPACE-PATH-ERROR the root directory cannot be opened

    @par Example
    @code{.py}
# index a project but skip generated code and files over 4 MiB
list<string> skipped;
list<string> files = Files::find_qore_files("/src/project", <FileScanOptions>{
    "excludeDirs": Files::DefaultExcludeDirs + ("generated",),
    "maxFileSize": 4 * 1024 * 1024,
}, \skipped);
    @endcode
*/
public list<string> sub find_qore_files(string path, hash<FileScanOptions> opts = <FileScanOptions>{},
        *reference<list<string>> skippedFiles) {
    hash<string, bool> visited = {};
    list<string> skipped = ();
    list<string> ret = find_qore_files_intern(path, opts, glob_list_to_regex(opts.excludeDirs), \visited,
        \skipped);
    skippedFiles = skipped;
    return ret;
}

#! Searches one directory recursively for find_qore_files()
list<string> sub find_qore_files_intern(string path, hash<FileScanOptions> opts, *string excludeRegex,
        reference<hash<string, bool>> visited, reference<list<string>> skipped) {
    Dir d();
    if (!d.chdir(path)) {
        throw "WORKSPACE-PATH-ERROR", sprintf("%s: cannot open directory: %s", path, strerror());
    }
    visited{realpath(path)} = True;

    list<string> ret = ();

    foreach string entry in (d.listFileNames()) {
        string fpath = sprintf("%s%s%s", path, DirSep, entry);
        if (!is_valid_qore_file(fpath)) {
            continue;
        }
        if (opts.maxFileSize > 0 && hstat(fpath).size > opts.maxFileSize) {
            push skipped, fpath;
            continue;
        }
        push ret, fpath;
    }

    foreach string entry in (d.listDirNames()) {
        if (excludeRegex && regex(entry, excludeRegex)) {
            continue;
        }
        string targ = path + DirSep + entry;
        if (hstat(targ).type != "DIRECTORY") {
            continue;
        }
        try {
            if (visited{realpath(targ)}) {
                continue;
            }
            ret += find_qore_files_intern(targ, opts, excludeRegex, \visited, \skipped);
        } catch (hash<ExceptionInfo> ex) {
            # ignore errors and continue
        }
    }

    return ret;
}

#! Find standard Qore module files.
/**
    @param opts the scan options

    @return list of file paths
*/
public list<string> sub find_std_modules(hash<FileScanOptions> opts = <FileScanOptions>{}) {
    # find module path
    Program p(PO_NO_CHILD_PO_RESTRICTIONS | PO_NEW_STYLE);
    p.disableParseOptions(PO_NO_TOP_LEVEL_STATEMENTS);
    p.parse("%requires Util\nint x = 0;", "label", 0);
    hash<auto> mh = p.callFunction("get_module_hash");
    *string filename = mh.Util.filename;
    if (!filename) {
        return ();
    }
    string modulePath = dirname(filename);

    list<string> candidatePaths = ();
    hash<auto> seen = {};

    seen{modulePath} = True;
    push candidatePaths, modulePath;

    # When modules are loaded from generated qmods, QLS still needs parseable
    # source files.  In in-tree builds, build/qlib-qmod maps back to qlib.
    string sourceModulePath = regex_subst(modulePath, "/build/qlib-qmod($|/.*$)", "/qlib");
    if (sourceModulePath != modulePath && is_dir(sourceModulePath)) {
        seen{sourceModulePath} = True;
        push candidatePaths, sourceModulePath;
    }

    # Also honor the runtime module path.  In qmod-first setups the source qlib
    # path normally follows the qmod directory.
    if (ENV.QORE_MODULE_DIR) {
        foreach string path in (ENV.QORE_MODULE_DIR.split(":")) {
            if (path && !seen{path}) {
                seen{path} = True;
                push candidatePaths, path;
            }
        }
    }

    foreach string path in (candidatePaths) {
        if (!is_dir(path)) {
            continue;
        }
        list<string> moduleFiles = find_qore_files(path, opts);
        if (moduleFiles.size()) {
            return moduleFiles;
        }
    }

    return ();
}

} # namespace Files

} # namespace QLS
