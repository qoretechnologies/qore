# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FileDataProviderBase class definition

/** FileDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
                "desc": "If `True` and `root` is set, then no files may be accessed outside of `root`",
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
    final private checkPath(reference<string> path) {
        path = normalize_dir(path, root);
        if (!path.equalPartialPath(root)) {
            throw "SANDBOX-ERROR", sprintf("%y: attempt to access path outside of sandbox %y", path, root);
        }
    }

    #! Checks if the readonly flag is set
    final private checkReadOnly(string op) {
        if (readonly) {
            throw "READONLY-ERROR", sprintf("the read-only flag is set; cannot perform %y operation", op);
        }
    }
}
}