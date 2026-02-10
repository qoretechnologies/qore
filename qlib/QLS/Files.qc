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
    "qwf":      "Qorus workflow definition",
    "qsd":      "Qorus service",
    "qfd":      "Qorus functions",
    "qjob":     "Qorus job",
    "qconst":   "Qorus constant",
    "qsm":      "Qorus schema module",
};

public bool sub is_valid_qore_file(string path) {
    *hash<StatInfo> info = hstat(path);
    if (!info) {
        # return False if we can't stat() the file
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

#! Find files in given root path
/**
    @return list of file paths
*/
public list<string> sub find_qore_files(string path) {
    Dir d();
    if (!d.chdir(path)) {
        throw "WORKSPACE-PATH-ERROR", sprintf("%s: cannot open directory: %s", path, strerror());
    }

    list<string> ret = ();

    ListIterator it = d.listFiles().iterator();
    while (it.next()) {
        string fpath = sprintf("%s%s%s", path, DirSep, it.getValue());
        if (is_valid_qore_file(fpath)) {
            push ret, fpath;
        }
    }

    it = d.listDirs().iterator();
    while (it.next()) {
        string targ = path + DirSep + it.getValue();
        if (hstat(targ).type == "DIRECTORY") {
            try {
                ret += find_qore_files(targ);
            } catch (hash<ExceptionInfo> ex) {
                # ignore errors and continue
            }
        }
    }

    return ret;
}

#! Find standard Qore module files.
/**
    @return list of file paths
*/
public list<string> sub find_std_modules() {
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

    # find modules
    list<string> moduleFiles = find_qore_files(modulePath);
    return moduleFiles;
}

} # namespace Files

} # namespace QLS
