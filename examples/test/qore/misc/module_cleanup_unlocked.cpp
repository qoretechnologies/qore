/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unistd.h>

// A user module that fails to load is destroyed with its Program, which runs the program cleanup callbacks of binary
// modules.  A callback can take a lock of its own that is also held while calling the module manager, as the jni
// module does with its module namespace cache lock and ModuleManager::getModuleHash(); destroying the module with the
// module manager mutex held deadlocked two such threads (qorus-core: a failing service module load against a Java
// class being generated).  Each callback here calls the module manager while a failing module is destroyed: with the
// mutex held, it would block on the (non-recursive) mutex held by its own thread, so the watchdog in main() fails the
// test instead.

static unsigned checks = 0;

static void require(bool valid, const std::string& message) {
    ++checks;
    if (!valid) {
        throw std::runtime_error(message);
    }
}

// the module loads run on this thread, as do the cleanup callbacks of the Programs they destroy
static thread_local bool probing = false;
static thread_local unsigned probed_cleanups = 0;
// module manager calls made by the callback that failed; the callback is called by libqore, so it cannot throw
static thread_local unsigned failed_calls = 0;

static void cleanup(QoreProgram* pgm) {
    if (!probing) {
        return;
    }
    ++probed_cleanups;
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> modules(MM.getModuleHash(), &xsink);
    if (xsink || !modules) {
        ++failed_calls;
    }
    xsink.clear();
    // the module manager can also be asked about user modules; the module that failed is not one of them
    if (MM.findUserModuleProgram("ModuleCleanupUnlockedNotLoaded") || !pgm) {
        ++failed_calls;
    }
}

class CleanupRegistration {
public:
    CleanupRegistration() {
        qore_register_program_cleanup_callback(cleanup);
    }
    ~CleanupRegistration() {
        qore_deregister_program_cleanup_callback(cleanup);
    }
};

class ProbeHelper {
public:
    ProbeHelper() {
        probing = true;
        probed_cleanups = 0;
        failed_calls = 0;
    }
    ~ProbeHelper() {
        probing = false;
    }
};

class TempDir {
public:
    TempDir() {
        path = (std::filesystem::temp_directory_path() / "qore-module-cleanup-XXXXXX").string();
        if (!mkdtemp(&path[0])) {
            throw std::runtime_error("cannot create a temporary directory");
        }
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        if (ec) {
            std::fprintf(stderr, "cannot remove %s: %s\n", path.c_str(), ec.message().c_str());
        }
    }

    std::string write(const std::string& name, const std::string& src) const {
        std::string file = path + "/" + name;
        FILE* f = fopen(file.c_str(), "w");
        if (!f) {
            throw std::runtime_error("cannot create " + file);
        }
        bool ok = fwrite(src.data(), 1, src.size(), f) == src.size();
        ok = !fclose(f) && ok;
        if (!ok) {
            throw std::runtime_error("cannot write " + file);
        }
        return file;
    }

    std::string mkdir(const std::string& name) const {
        std::string dir = path + "/" + name;
        std::error_code ec;
        if (!std::filesystem::create_directory(dir, ec)) {
            throw std::runtime_error("cannot create " + dir + ": " + ec.message());
        }
        return dir;
    }

    std::string path;
};

static std::string module_header(const char* name, bool version = true) {
    std::string rv = "%new-style\n%require-types\nmodule ";
    rv += name;
    rv += " {\n";
    if (version) {
        rv += "    version = \"1.0\";\n";
    }
    rv += "    desc = \"module cleanup test\";\n    author = \"Qore Technologies, s.r.o.\";\n"
        "    url = \"https://qore.org\";\n    license = \"MIT\";\n}\n";
    return rv;
}

// a statement that does not parse
static const char* broken = "public int sub broken() { return ; + }\n";

// loads a module that fails and checks that the failure is reported and that the module's Program was destroyed
static void failingLoad(const char* what, const std::string& module, const char* name) {
    ExceptionSink pxsink;
    QoreProgramHelper pgm(PO_NEW_STYLE, pxsink);
    ExceptionSink xsink;
    {
        ProbeHelper probe;
        ModuleManager::runTimeLoadModule(&xsink, module.c_str(), *pgm);
        require(static_cast<bool>(xsink), std::string(what) + ": the module loaded");
        require(probed_cleanups > 0, std::string(what) + ": the module's Program was not destroyed");
        require(!failed_calls, "the module manager failed in a cleanup callback");
    }
    xsink.clear();
    require(!ModuleManager::findUserModuleProgram(name), std::string(what) + ": the failed module is registered");
}

static void userModuleFromPath(const TempDir& dir) {
    failingLoad("a user module that does not parse",
        dir.write("CleanupParseError.qm", module_header("CleanupParseError") + broken), "CleanupParseError");
}

static void userModuleMetadata(const TempDir& dir) {
    // the module parses, and fails when it is registered (setupUserModule())
    failingLoad("a user module without a version",
        dir.write("CleanupNoVersion.qm", module_header("CleanupNoVersion", false)), "CleanupNoVersion");
}

static void separatedModule(const TempDir& dir) {
    std::string mod = dir.mkdir("CleanupSeparated");
    dir.write("CleanupSeparated/CleanupSeparated.qm", module_header("CleanupSeparated"));
    dir.write("CleanupSeparated/Broken.qc", broken);
    failingLoad("a separated user module with a source that does not parse", mod, "CleanupSeparated");
}

static void userModuleFromSource() {
    ExceptionSink pxsink;
    QoreProgramHelper pgm(PO_NEW_STYLE, pxsink);
    ExceptionSink xsink;
    {
        ProbeHelper probe;
        MM.registerUserModuleFromSource("CleanupSource", (module_header("CleanupSource") + broken).c_str(), *pgm,
            &xsink);
        require(static_cast<bool>(xsink), "a user module from source that does not parse loaded");
        require(probed_cleanups > 0, "the Program of a user module from source was not destroyed");
        require(!failed_calls, "the module manager failed in a cleanup callback");
    }
    xsink.clear();
}

static void parseRequires(const TempDir& dir) {
    // %requires loads the module while the requiring Program is parsed
    std::string module = dir.write("CleanupRequired.qm", module_header("CleanupRequired") + broken);
    ExceptionSink pxsink;
    QoreProgramHelper pgm(PO_NEW_STYLE, pxsink);
    ExceptionSink xsink;
    {
        ProbeHelper probe;
        std::string src = "%requires " + module + "\n";
        pgm->parse(src.c_str(), "module-cleanup-requires", &xsink);
        require(static_cast<bool>(xsink), "a required module that does not parse loaded");
        require(probed_cleanups > 0, "the Program of a required module was not destroyed");
        require(!failed_calls, "the module manager failed in a cleanup callback");
    }
    xsink.clear();
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    // a cleanup callback run with the module manager mutex held blocks this thread for good: end the test instead
    alarm(300);
    int status = 0;
    try {
        TempDir dir;
        CleanupRegistration registration;
        userModuleFromPath(dir);
        userModuleMetadata(dir);
        separatedModule(dir);
        userModuleFromSource();
        parseRequires(dir);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    alarm(0);
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u module cleanup checks\n", checks);
    }
    return status;
}
