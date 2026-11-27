# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FileDataProviderBase class definition

/** FileDataProviderBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FileDataProvider module
public namespace FileDataProvider {
#! The base file data provider class
public class FileDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "lock": <DataProviderOptionInfo>{
                "display_name": "Enforce Sandbox",
                "short_desc": "Do not allot access to files outside the root directory",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` and `root` is set, then no files may be accessed outside of `root`; symbolic "
                    "links are resolved, so a link that leads outside of `root` is refused",
                "default_value": True,
            },
            "readonly": <DataProviderOptionInfo>{
                "display_name": "Enforce Read-Only",
                "short_desc": "Enforce read-only operations",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "If `True` then only read operations will be permitted",
            },
            "root": <DataProviderOptionInfo>{
                "display_name": "Root Directory",
                "short_desc": "The root directory for relative paths",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Any root directory for relative paths or for sandboxing access",
            },
        };
    }

    private {
        #! Any anchoring dir for filesystem actions
        *string root;

        #! Lock down access to the root?
        bool lock;

        #! Read only flag
        bool readonly;
    }

    private:internal {
        #! True if code that may use the file system confined the provider to its root (qore#7806)
        transient bool creator_bound;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        options = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (options.root.val()) {
            root = normalize_dir(substitute_env_vars(options.root));
        }
        if (options.lock) {
            lock = True;
        }
        if (options.readonly) {
            readonly = True;
        }
        setCreatorBound();
    }

    #! Creates the object from the arguments
    constructor(*string root, *bool lock, *bool readonly) {
        if (root.val()) {
            self.root = root;
        }
        if (lock) {
            self.lock = True;
        }
        if (readonly) {
            self.readonly = True;
        }
        setCreatorBound();
    }

    #! Refuses a request of code restricted from the file system (qore#7806)
    /** The paths of a request are the caller's input, also when the provider was created by other code and lent to
        it: code restricted from the file system cannot use the provider's actions, unless code that may use the file
        system created the provider with a \c root directory and \c lock (the data provider of a lent file
        connection, for example, see @ref sandbox_lent_connections): the paths are then confined to the root that the
        creator chose.  A host that gives restricted code access to some files can also govern it with a
        @ref Qore::SandboxManager "SandboxManager" whose file-system policy allows them.

        @throw PERMISSION-DENIED the calling code is restricted from the file system (see
        @ref Util::check_caller_file_access() "check_caller_file_access()"), and the provider is not confined to a root
        directory by code that may use the file system
    */
    final private checkCallerFileAccess() {
        if (!creator_bound) {
            check_caller_file_access(sprintf("the paths of a file %y request", getName()));
        }
    }

    #! Creates a child provider with the root, lock and read-only options of this provider (qore#7806)
    /** The child is confined to the root that this provider's creator chose, as this provider is (see
        checkCallerFileAccess())

        @param cls the class of the child provider, a subclass of FileDataProviderBase

        @return the child provider
    */
    final private FileDataProviderBase createChildProvider(Class cls) {
        FileDataProviderBase child = cls.newObject(root, lock, readonly);
        child.creator_bound = creator_bound;
        return child;
    }

    #! Records whether code that may use the file system confined the provider to its root (qore#7806)
    private:internal setCreatorBound() {
        creator_bound = root.val() && lock && !SandboxManager::isCallerRestricted(DOM_FILESYSTEM);
    }

    #! Check the given path when "lock" and "root" are set
    /** The path is normalized relative to the root directory, and it must be under the root directory both as given
        and by its real path, with symbolic links resolved (qore#7821): a symbolic link that leads outside of the root
        directory is refused, also for an action on the link itself (deleting or moving it, getting its own status),
        as the file-system policy of a sandbox refuses it.  For a path that does not exist yet, the real path of its
        nearest existing parent directory is used.  A path that cannot be resolved (a dangling symbolic link, a loop
        of links, or a directory that cannot be searched) is refused.

        A request is run in the root directory with runConfined(), which also refuses a path that a concurrent change
        of the file system made leave the root directory after this check.

        @param path the path to check; normalized relative to the root directory on output

        @throw SANDBOX-ERROR the path is outside of the root directory, or it cannot be resolved
    */
    final private checkPath(reference<string> path) {
        path = normalize_dir(path, root);
        if (!FileDataProviderBase::isUnder(path, root)) {
            throw "SANDBOX-ERROR", sprintf("%y: attempt to access path outside of sandbox %y", path, root);
        }
        checkRealPath(path, getRealRoot());
    }

    #! Removes the given path, as @ref FsUtil::remove_path() "remove_path()" does, confined to the root directory
    /** A directory is checked first: if it holds a symbolic link that leads outside of the root directory, or one
        that cannot be resolved, nothing is removed (the file-system policy of the confinement would refuse to remove
        the link after the rest of the directory was removed).  The removal is then run with runConfined()
        (qore#7821).

        @param path the path to remove, checked with checkPath()

        @throw SANDBOX-ERROR the directory holds a symbolic link that leads outside of the root directory or that
        cannot be resolved
    */
    final private removePathConfined(string path) {
        string real_root = getRealRoot();
        *hash<auto> h = hlstat(path);
        if (h.type == "DIRECTORY") {
            checkTreeLinks(path, real_root);
        }
        runConfined(sub () { remove_path(path); });
    }

    #! Checks that the real path of the given path is under the real path of the root directory
    private:internal checkRealPath(string path, string real_root) {
        *string real = FileDataProviderBase::getRealPath(path);
        if (!real) {
            throw "SANDBOX-ERROR", sprintf("%y: attempt to access a path that cannot be resolved in sandbox %y", path,
                root);
        }
        if (!FileDataProviderBase::isUnder(real, real_root)) {
            throw "SANDBOX-ERROR", sprintf("%y: attempt to access path outside of sandbox %y through a symbolic link",
                path, root);
        }
    }

    #! Checks the symbolic links in the given directory and its subdirectories with checkRealPath()
    private:internal checkTreeLinks(string dir_path, string real_root) {
        # directory entries are file-system bytes, which may not be valid text in the path's encoding
        Dir dir("ISO-8859-1");
        dir.chdir(dir_path);
        string parent = binary_to_string(binary(dir_path), "ISO-8859-1");
        foreach string name in (dir.listNames()) {
            string path = join_paths(parent, name);
            *hash<auto> h = hlstat(path);
            if (h.type == "SYMBOLIC-LINK") {
                checkRealPath(path, real_root);
            } else if (h.type == "DIRECTORY") {
                checkTreeLinks(path, real_root);
            }
        }
    }

    #! Runs the file-system operation of a request, confined to the root directory when "lock" and "root" are set
    /** The operation is run with
        @ref Qore::SandboxManager::callWithFilesystemRoot() "SandboxManager::callWithFilesystemRoot()" in the real path
        of the root directory (read access only for a read-only provider): every file that it reaches must be under
        the root directory by its real path, also if a path that checkPath() checked is changed concurrently, for
        example by a symbolic link that replaces a component of it (qore#7821)

        @param op the operation

        @return the return value of the operation

        @throw SANDBOX-ERROR the root directory cannot be resolved
        @throw FILESYSTEM-ACCESS-DENIED the operation reached a file outside of the root directory
    */
    final private auto runConfined(code op) {
        if (!root.val() || !lock) {
            return op();
        }
        string real_root = getRealRoot();
        # every path is under the root directory of the file system
        if (real_root == DirSep) {
            return op();
        }
        return SandboxManager::callWithFilesystemRoot(real_root, readonly ? QSEC_READ : QSEC_ALL, op);
    }

    #! Returns the real path of the root directory
    /** @throw SANDBOX-ERROR the root directory cannot be resolved
    */
    private:internal string getRealRoot() {
        *string real_root = FileDataProviderBase::getRealPath(root);
        if (!real_root) {
            throw "SANDBOX-ERROR", sprintf("the sandbox directory %y cannot be resolved", root);
        }
        return real_root;
    }

    #! Returns the real path of a normalized absolute path, or @ref nothing if it cannot be resolved
    /** For a path that does not exist, the real path of its nearest existing parent directory followed by the rest
        of the path; a component that exists but cannot be resolved (a dangling symbolic link, for example) makes the
        path unresolvable
    */
    private:internal static *string getRealPath(string path) {
        string rest;
        while (True) {
            # a symbolic link exists as a link, whether its target exists or not
            if (hlstat(path)) {
                string real;
                try {
                    real = realpath(path);
                } catch (hash<ExceptionInfo> ex) {
                    return;
                }
                if (!rest.val()) {
                    return real;
                }
                return real == DirSep ? DirSep + rest : real + DirSep + rest;
            }
            int err = errno();
            # a component that cannot be searched, or that is not a directory, makes the path unresolvable
            if (err != ENOENT) {
                return;
            }
            string parent = dirname(path);
            if (parent == path || !parent.val() || parent == ".") {
                return;
            }
            string name = basename(path);
            rest = rest.val() ? name + DirSep + rest : name;
            path = parent;
        }
    }

    #! Returns @ref True if the normalized path is the given directory or a path under it
    /** Only a directory separator after the directory's name makes a path one under it (<tt>"/a/b?c"</tt> is not
        under <tt>"/a/b"</tt>)
    */
    private:internal static bool isUnder(string path, string dir) {
        if (path == dir) {
            return True;
        }
        # character lengths and offsets: the strings are compared as text
        if (dir.val() && dir[dir.length() - 1] != DirSep) {
            dir += DirSep;
        }
        return path.length() > dir.length() && path.substr(0, dir.length()) == dir;
    }

    #! Checks if the readonly flag is set
    final private checkReadOnly(string op) {
        if (readonly) {
            throw "READONLY-ERROR", sprintf("the read-only flag is set; cannot perform %y operation", op);
        }
    }
}
}