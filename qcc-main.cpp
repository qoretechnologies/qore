/*
    qcc-main.cpp

    Qore Code Compiler (qcc) - AOT compiler for Qore

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include <qore/ParseOptionMap.h>
#include "qore/intern/QoreAOT.h"
#include "qore/intern/qore_aot_deps.h"
#include "qore/intern/QoreAOTExprNodeRegistry.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/xxhash.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <dlfcn.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <getopt.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <zlib.h>
#include <zstd.h>

#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/Error.h>

static const char* QCC_VERSION = "1.0";

#ifndef QORE_QCC_SOURCE_INCLUDE_DIR
#define QORE_QCC_SOURCE_INCLUDE_DIR ""
#endif

#ifndef QORE_QCC_BUILD_INCLUDE_DIR
#define QORE_QCC_BUILD_INCLUDE_DIR ""
#endif

#ifndef QORE_QCC_INSTALL_INCLUDE_DIR
#define QORE_QCC_INSTALL_INCLUDE_DIR ""
#endif

#ifndef QORE_QCC_BUILD_LIBDIR
#define QORE_QCC_BUILD_LIBDIR ""
#endif

#ifndef QORE_QCC_INSTALL_LIBDIR
#define QORE_QCC_INSTALL_LIBDIR ""
#endif

//! Check if a path is a directory
static bool is_directory(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

//! Check if a path is a regular file
static bool is_file(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static int64_t file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return st.st_size;
    }
    return -1;
}

//! True if a path is suitable as a Make depfile prerequisite.
static bool depfile_dependency_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static const char* source_mode_suffix(bool include_source) {
    return include_source ? ", include-source" : "";
}

//! Join two path components.
static std::string join_path(const std::string& lhs, const char* rhs) {
    if (lhs.empty()) {
        return rhs;
    }
    if (lhs.back() == '/') {
        return lhs + rhs;
    }
    return lhs + "/" + rhs;
}

//! Return the directory part of a path, or empty when no directory is present.
static std::string dirname_of(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string();
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

//! Add a path once, preserving first-match order.
static void add_unique_path(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

//! Split Qore-style env path variables.  QORE_INCLUDE_DIR is documented as colon-separated;
//! semicolons are also accepted so CMake-style values work in developer environments.
static void add_env_paths(std::vector<std::string>& paths, const char* env) {
    if (!env || !*env) {
        return;
    }
    const char* start = env;
    for (const char* p = env; ; ++p) {
        if (*p == ':' || *p == ';' || *p == '\0') {
            if (p > start) {
                add_unique_path(paths, std::string(start, p - start));
            }
            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }
}

//! Add an include directory if it contains either public Qore headers or generated headers.
static void add_qore_include_candidate(std::vector<std::string>& dirs, const std::string& dir) {
    if (dir.empty()) {
        return;
    }
    if (is_file(join_path(dir, "qore/Qore.h")) || is_file(join_path(dir, "qore/qore-version.h"))) {
        add_unique_path(dirs, dir);
    }
}

//! Add a library directory if it exists.
static void add_qore_lib_candidate(std::vector<std::string>& dirs, const std::string& dir) {
    if (!dir.empty() && is_directory(dir.c_str())) {
        add_unique_path(dirs, dir);
    }
}

//! Quote one shell argument.
static std::string shell_quote(const std::string& arg) {
    std::string rv("'");
    for (char c : arg) {
        if (c == '\'') {
            rv += "'\\''";
        } else {
            rv.push_back(c);
        }
    }
    rv.push_back('\'');
    return rv;
}

//! Directory containing the loaded libqore.
static std::string get_loaded_libqore_dir() {
    if (const char* env = std::getenv("QORE_LIBDIR")) {
        if (*env) {
            return env;
        }
    }

    Dl_info info;
    if (!dladdr(reinterpret_cast<void*>(&qore_init), &info) || !info.dli_fname) {
        return std::string();
    }

    std::string path(info.dli_fname);
    if (!path.empty() && path[0] != '/') {
        char* resolved = realpath(path.c_str(), nullptr);
        if (resolved) {
            path = resolved;
            free(resolved);
        }
    }
    return dirname_of(path);
}

//! Derive qcc link-mode include and library search paths from env, build-tree, install-tree,
//! and the loaded libqore path.  CI runs qcc from the build artifact without a matching
//! system install, so link-mode must not rely on compiler defaults.
static void collect_qcc_link_paths(std::vector<std::string>& include_dirs,
        std::vector<std::string>& lib_dirs) {
    std::vector<std::string> env_include_dirs;
    add_env_paths(env_include_dirs, std::getenv("QORE_INCLUDE_DIRS"));
    add_env_paths(env_include_dirs, std::getenv("QORE_INCLUDE_DIR"));
    for (const auto& dir : env_include_dirs) {
        add_qore_include_candidate(include_dirs, dir);
    }

    const std::string loaded_libdir = get_loaded_libqore_dir();
    add_qore_lib_candidate(lib_dirs, loaded_libdir);
    add_qore_lib_candidate(lib_dirs, QORE_QCC_BUILD_LIBDIR);
    add_qore_lib_candidate(lib_dirs, QORE_QCC_INSTALL_LIBDIR);

    std::vector<std::string> lib_based_include_dirs;
    if (!loaded_libdir.empty()) {
        lib_based_include_dirs.push_back(join_path(loaded_libdir, "include"));
        std::string parent = dirname_of(loaded_libdir);
        if (!parent.empty()) {
            lib_based_include_dirs.push_back(join_path(parent, "include"));
            std::string grandparent = dirname_of(parent);
            if (!grandparent.empty()) {
                lib_based_include_dirs.push_back(join_path(grandparent, "include"));
            }
        }
    }

    const bool loaded_from_build_tree = !loaded_libdir.empty()
        && loaded_libdir == QORE_QCC_BUILD_LIBDIR;
    if (loaded_from_build_tree) {
        add_qore_include_candidate(include_dirs, QORE_QCC_BUILD_INCLUDE_DIR);
        add_qore_include_candidate(include_dirs, QORE_QCC_SOURCE_INCLUDE_DIR);
    }
    for (const auto& dir : lib_based_include_dirs) {
        add_qore_include_candidate(include_dirs, dir);
    }
    add_qore_include_candidate(include_dirs, QORE_QCC_INSTALL_INCLUDE_DIR);
    if (!loaded_from_build_tree) {
        add_qore_include_candidate(include_dirs, QORE_QCC_BUILD_INCLUDE_DIR);
        add_qore_include_candidate(include_dirs, QORE_QCC_SOURCE_INCLUDE_DIR);
    }
}

//! Add compiler/linker flags for qcc link mode.
static void append_qcc_link_flags(std::string& cmd, const std::vector<std::string>& lib_dirs) {
    for (const auto& dir : lib_dirs) {
        cmd += " -L" + shell_quote(dir);
    }
    cmd += " -lqore";
    for (const auto& dir : lib_dirs) {
        cmd += " -Wl,-rpath," + shell_quote(dir);
    }
}

// Phase C Item 2: mirror `QoreAOT::sanitizeCIdentifier` for the
// link-mode glue emission.  Replace every char outside [A-Za-z0-9_]
// with `_` so the file basename becomes a valid C identifier —
// matches the register-fn name qcc -c emits per .qo.
static std::string sanitize_c_identifier(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9') || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

// Strip directory + extension, returning the file basename suitable
// for use as a C identifier (after sanitize).  `/tmp/foo/bar.qo`
// → `bar`.
static std::string basename_no_ext(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path
            : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return name;
}

//! Check whether a path ends in a literal extension.
static bool has_extension(const char* path, const char* ext) {
    size_t n = std::strlen(path);
    size_t e = std::strlen(ext);
    return n > e && std::strcmp(path + n - e, ext) == 0;
}

// Check whether a path ends in `.qo`.  Link mode requires every
// positional input to match.
static bool has_qo_extension(const char* path) {
    return has_extension(path, ".qo");
}

static bool qcc_check_cancel(const char* operation) {
    // qcc deliberately performs manifest-current checks before qore_init() so
    // no-op builds can skip without starting the Qore runtime.  The normal
    // qore_check_cancel() path needs initialized thread-local Qore state.
    return q_libqore_initalized() && qore_check_cancel(nullptr, operation);
}

// Write a Make-format dependency file at `path`.
// Target (LHS) is `output`; deps are `source` plus, when `context` is
// non-null, every `.qm`/`.qc`/`.ql` under it — the set the parser actually
// opens in `--context=DIR` mode per `compileSeparatedModuleFile`.
// Consumed by cmake's `add_custom_command(... DEPFILE ...)` so sibling-
// file edits in split-module dirs retrigger the affected per-file `.qo`.
// Only spaces and backslashes in paths are escaped — Make handles both.
static bool write_depfile(const char* path, const std::string& output,
                          const std::string& source, const char* context,
                          const std::vector<std::string>* extra_deps = nullptr) {
    // Canonicalize so the relative vs absolute spelling of the same file
    // (e.g. primary passed by qcc as realpath + same file re-emerging
    // from an opendir(context) scan with a relative prefix) dedupes
    // cleanly.  realpath failure falls through to the input spelling.
    auto canon = [](const std::string& s) -> std::string {
        if (s.empty()) {
            return s;
        }
        char* r = realpath(s.c_str(), nullptr);
        if (!r) {
            return s;
        }
        std::string out = r;
        free(r);
        return out;
    };

    std::vector<std::string> deps;
    if (!source.empty()) {
        std::string full = canon(source);
        if (depfile_dependency_exists(full)) {
            deps.push_back(std::move(full));
        }
    }

    if (context) {
        DIR* d = opendir(context);
        if (!d) {
            fprintf(stderr, "error: --depfile: cannot open context dir '%s': %s\n",
                context, strerror(errno));
            return false;
        }
        struct dirent* ent;
        size_t dep_i = 0;
        while ((ent = readdir(d)) != nullptr) {
            if (dep_i && !(dep_i % 100)
                    && qcc_check_cancel("qcc depfile context scan")) {
                fprintf(stderr, "error: operation cancelled during qcc depfile context scan\n");
                closedir(d);
                return false;
            }
            ++dep_i;
            size_t len = strlen(ent->d_name);
            bool keep = false;
            if (len > 3) {
                const char* tail = ent->d_name + len - 3;
                if (!strcmp(tail, ".qm") || !strcmp(tail, ".qc") || !strcmp(tail, ".ql")) {
                    keep = true;
                }
            }
            if (!keep) {
                continue;
            }
            std::string full = canon(std::string(context) + "/" + ent->d_name);
            if (depfile_dependency_exists(full)
                    && std::find(deps.begin(), deps.end(), full) == deps.end()) {
                deps.emplace_back(std::move(full));
            }
        }
        closedir(d);
        std::sort(deps.begin(), deps.end());
    }

    // merge any extra dependency files (e.g. the .qmod files of modules loaded
    // for a compiled module's %requires closure), canonicalized and deduped
    if (extra_deps) {
        for (size_t i = 0; i < extra_deps->size(); ++i) {
            if (i && !(i % 100)
                    && qcc_check_cancel("qcc depfile extra dependency scan")) {
                fprintf(stderr,
                    "error: operation cancelled during qcc depfile extra dependency scan\n");
                return false;
            }
            const std::string& d = (*extra_deps)[i];
            if (d.empty()) {
                continue;
            }
            std::string full = canon(d);
            if (depfile_dependency_exists(full)
                    && std::find(deps.begin(), deps.end(), full) == deps.end()) {
                deps.emplace_back(std::move(full));
            }
        }
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "error: --depfile: cannot open '%s' for writing: %s\n",
            path, strerror(errno));
        return false;
    }

    auto escape = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == ' ' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        return out;
    };

    fprintf(f, "%s:", escape(output).c_str());
    for (const auto& dep : deps) {
        fprintf(f, " \\\n    %s", escape(dep).c_str());
    }
    fputc('\n', f);
    fclose(f);
    return true;
}

// Write a Make-style depfile from an explicit, already-canonicalized
// dependency list (used by script `-c -L` mode, where compileScriptFile
// reports the exact set of source files it parsed — the target plus its
// `%include` closure — so the build rebuilds the `.qo` when any of them
// changes).
static bool write_depfile_list(const char* path, const std::string& output,
                               const std::vector<std::string>& deps) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "error: --depfile: cannot open '%s' for writing: %s\n",
            path, strerror(errno));
        return false;
    }
    auto escape = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == ' ' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        return out;
    };
    auto canon = [](const std::string& s) -> std::string {
        if (s.empty()) {
            return s;
        }
        char* r = realpath(s.c_str(), nullptr);
        if (!r) {
            return s;
        }
        std::string out = r;
        free(r);
        return out;
    };
    fprintf(f, "%s:", escape(output).c_str());
    for (size_t i = 0; i < deps.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("qcc depfile emission")) {
            fclose(f);
            fprintf(stderr, "error: operation cancelled during qcc depfile emission\n");
            return false;
        }
        const std::string& dep = deps[i];
        std::string full = canon(dep);
        if (!depfile_dependency_exists(full)) {
            continue;
        }
        fprintf(f, " \\\n    %s", escape(full).c_str());
    }
    fputc('\n', f);
    fclose(f);
    return true;
}

// Program options
static const char* output_path = nullptr;
static int opt_level = 3;
// Parallel backend codegen across N threads (split-after-opt). 0 = unset (single-object
// codegen, the reference path). Propagated to libqore via the QCC_JOBS env var.
static int aot_jobs = 0;
static const char* target_triple = nullptr;
static bool static_link = false;
static bool module_mode = false;
// Compile-only mode emits a .qo relocatable object instead of a .qmod.
// Objects can be linked into a generated executable, archived for a C/C++
// host, or aggregated back into a .qmod with qcc -m --from-objects.
static bool compile_only = false;
// --context=DIR passes the owning module directory
// when compiling a single file (`.qm`, `.qc`, or `.ql`) from a split
// module so the parser has the full directory as context while the
// AOT writer emits only the target file's contributions.
static const char* context_dir = nullptr;
// --output-dir=DIR selects the output directory for batch-mode script compile.
// When the user passes multiple positional sources in `-c` script
// mode, qcc parses them all into one QoreProgram (one parse cycle,
// no per-file sibling preload) and emits BASENAME.qo into this dir.
static const char* batch_output_dir = nullptr;
// -L<dir> directories for sibling `.qo` preload
// when compiling a single-file script with `-c` + script context (no
// module wrapper).  Each directory is scanned for `*.qo`; their
// fragment metadata is preloaded into the compile program so the
// target source's cross-file references resolve at parse time.
// Semantically analogous to C's `-L<dir>` for linker library search.
static std::vector<std::string> script_lib_dirs;
// -l/--load=<mod> modules preloaded into the
// compile program before parsing.  Mirrors the runtime-load pattern
// used by Qorus binaries (e.g. qctl_main.cpp req_modules[]) and
// matches `qore -l <mod>` CLI convention.  Types from these modules
// become visible to the parser so sources that reference them
// without %requires directives compile cleanly.  Repeatable;
// loaded in declaration order via MM.parseLoadModule.
static std::vector<std::string> load_modules;
// --stub=<file> declarative-only sources parsed
// into the compile program before target sources.  The host
// synthesizes namespaces, functions, or constants in C++ at runtime
// (e.g. `QoreNamespace* QNS = new QoreNamespace("Qorus")` +
// injection helpers); the stub file mirrors those declarations in
// Qore syntax so the parser can resolve bareword references at
// compile time.  Stubs are NOT emitted as `.qo`s — their per-file
// contributions are excluded because the fragment filter matches
// each target's canonical path, not the stub's path.  Repeatable;
// parsed in declaration order before targets.
static std::vector<std::string> stub_files;
// --define=NAME[=VALUE] preparser defines applied
// via `qpgm->parseDefine()` before any source is parsed.  Mirrors the
// runtime `qpgm->parseDefine(...)` calls emitted by Qorus main.cpp
// files (e.g. `qpgm->parseDefine("NO_ORACLE", true)` in
// exec/qdsp_main.cpp when oracle-datasource-pool is off) so
// AOT-compiled sources honor the same `%ifdef` / `%ifndef` surface
// the runtime sees.  Repeatable; applied in declaration order.
static std::vector<std::string> parse_defines;
// --parse-option=NAME OR's a `PO_*` flag into the
// compile program's parse options.  Mirrors the runtime
// `qpgm->parseSetParseOptions(QORUS_PARSE_OPTIONS)` pattern Qorus
// main.cpp files use to extend the initial `new QoreProgram(po)` set
// (e.g. PO_ALLOW_INJECTION so cross-module `Program::loadApplyTo*`
// calls type-check at parse time).  Without this, methods whose
// `[dom=...]` mask names PO_ALLOW_* report "parse options do not
// allow access" during AOT compile.  Repeatable.
static std::vector<std::string> parse_option_flags;
// --script-aggregate=SYMBOL emits a metadata-only script aggregate object
// with an `init_SYMBOL_qo(QoreProgram*)` registration entry.  It is linked
// alongside the per-file `.qo` objects produced by batch script compilation.
static const char* script_aggregate_symbol = nullptr;
static bool script_aggregate_native_registers = false;
// --link-qo emits an object-driven aggregate register object from existing
// script-context `.qo` inputs, without reparsing the original sources.
static bool link_qo = false;
static bool strict_call_relocations = false;
static bool allow_unresolved_qo_imports = false;
static const char* link_aggregate_symbol = nullptr;
static const char* qolink_map_path = nullptr;
// --from-objects signals aggregator mode: the
// positional inputs are per-file `.qo` objects (produced by
// `qcc -c --context=DIR <file>`) that get linked together plus a
// freshly-computed metadata glue into the output `.qmod`.  See
// design/aot-object-files-and-module-artifacts.md.
static bool from_objects = false;
// -a / --archive mode.  Combined with --context=DIR
// + positional .qo inputs, produces a `.qoa` static archive (ar rcs)
// exposing `qore_qoa_register_all(QoreProgram*)`.  Target use case:
// static linkage into a C++ host (e.g. qorus-core).
static bool archive_mode = false;
static bool include_source = false;
static const char* metadata_compression = nullptr;
static bool verbose = false;
static bool warnings_are_errors = false;
static bool show_help = false;
static bool show_version = false;
static bool dump_info = false;
static bool dump_symbols = false;
static bool dump_sections = false;
static bool dump_index_json = false;

static bool qcc_output_verbose() {
    const char* verbose_env = getenv("QORE_AOT_VERBOSE");
    return verbose || (verbose_env && *verbose_env && strcmp(verbose_env, "0")) || getenv("QORE_AOT_DEBUG");
}

static bool apply_parse_option_flags(QoreParseOptions& po, std::string& error) {
    for (const std::string& raw : parse_option_flags) {
        static const struct {
            const char* name;
            QoreParseOptions value;
        } macro_options[] = {
            {"PO_MODERN",                     PO_MODERN},
            {"PO_NEW_STYLE",                  PO_NEW_STYLE},
            {"PO_REQUIRE_OUR",                PO_REQUIRE_OUR},
            {"PO_REQUIRE_PROTOTYPES",         PO_REQUIRE_PROTOTYPES},
            {"PO_NO_CHILD_PO_RESTRICTIONS",   PO_NO_CHILD_PO_RESTRICTIONS},
            {"PO_ALLOW_INJECTION",            PO_ALLOW_INJECTION},
            {"PO_ALLOW_DEBUGGER",             PO_ALLOW_DEBUGGER},
            {"PO_ALLOW_WEAK_REFERENCES",      PO_ALLOW_WEAK_REFERENCES},
            {"PO_REQUIRE_TYPES",              PO_REQUIRE_TYPES},
            {"PO_STRICT_ARGS",                PO_STRICT_ARGS},
            {"PO_STRONG_ENCAPSULATION",       PO_STRONG_ENCAPSULATION},
            {"PO_ALLOW_BARE_REFS",            PO_ALLOW_BARE_REFS},
            {"PO_ASSUME_LOCAL",               PO_ASSUME_LOCAL},
            {"PO_BROKEN_NARROWED_TYPES",      PO_BROKEN_NARROWED_TYPES},
            {"PO_BROKEN_LIST_PARSING",        PO_BROKEN_LIST_PARSING},
            {"PO_NO_MODULES",                 PO_NO_MODULES},
        };
        bool found = false;
        for (const auto& macro_option : macro_options) {
            if (raw == macro_option.name) {
                po |= macro_option.value;
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }
        std::string name = raw;
        if (name.rfind("PO_", 0) == 0) {
            name.erase(0, 3);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return c == '_' ? '-' : static_cast<char>(std::tolower(c));
            });
        }
        QoreParseOptions opt = ParseOptionMap::find_code(name.c_str());
        if (opt == QoreParseOptions(-1)) {
            error = "unknown --parse-option name: '" + raw + "'";
            return false;
        }
        po |= opt;
    }
    return true;
}
static bool show_targets = false;
// Phase 1 compile-time opt:
static bool strip_debug_info = false;
static const char* time_trace_path = nullptr;
// Phase 5a: OptimizeNone+NoInline for functions exceeding this block count
// (0 = disabled). Trades runtime optimization for dramatic compile-time
// savings on large functions (e.g. HttpServer::handleRequest 905 BBs).
// Default 200 validated on HS (46x compile speedup, ~1-7% runtime cost)
// and SqlUtil (1.70x speedup); OpenApi3 unaffected (no functions >= 200).
static int big_fn_threshold = 200;
// Tracks whether `--big-fn-threshold=N` was explicitly passed on the
// command line.  Needed so the CLI default (200) doesn't silently
// clobber a user-set QORE_AOT_BIG_FN_THRESHOLD env var — getopt can't
// distinguish "user passed --big-fn-threshold=200" from "unset, using
// default 200", so a separate bool carries that intent.
static bool big_fn_threshold_cli_explicit = false;
// --depfile=FILE emits a Make-format dependency
// file after a successful compile, listing every source file the
// target output (re)builds against.  cmake wires it via
// add_custom_command(... DEPFILE ${out}.d) so a touch on any sibling
// `.qc` in a split-dir module's --context=DIR retriggers the
// affected `.qo`.  GCC's `-MMD -MF <path>` pair maps onto this
// single long option.
static const char* depfile_path = nullptr;
// --depfile-target=FILE overrides the Make depfile target.  This is useful
// when build systems track a stamp output while the generated artifact is a
// byproduct.
static const char* depfile_target_path = nullptr;
// --depfile-qo-input-content-stamps rewrites `.qo` entries in explicit depfile
// input lists to the corresponding qcc content stamp.  This lets build systems
// depend on semantic object changes while still passing the real `.qo` files to
// qcc for linking and manifest verification.
static bool depfile_qo_input_content_stamps = false;
// --depfile-dir=DIR emits one Make-format depfile per generated `.qo`
// in batch `-c --output-dir=DIR` mode.  Each depfile basename matches
// the generated object basename with `.d` appended.
static const char* depfile_dir = nullptr;
// --write-index-json=FILE writes the same build-consumable symbol index JSON as
// --dump-index-json, but as a sidecar next to a generated artifact.
static const char* write_index_json_path = nullptr;
// --write-manifest=FILE writes a deterministic content manifest for generated
// artifacts.  --skip-if-manifest-current uses the same manifest content to
// decide whether qcc can skip a compile/link command without touching outputs.
static const char* write_manifest_path = nullptr;
static bool skip_if_manifest_current = false;
// --write-status-json=FILE writes deterministic build-result metadata for build
// system diagnostics.  --success-stamp=FILE is touched on every successful
// command, including manifest-current skips.  --content-stamp=FILE is created on
// first successful output verification, then touched only when the generated
// output's bytes differ from the previous output.
static const char* write_status_json_path = nullptr;
static const char* success_stamp_path = nullptr;
static const char* content_stamp_path = nullptr;
// Extra content dependencies that affect the generated artifact but are not
// necessarily visible to qcc as positional inputs (build context files, generated
// stubs, external tool configuration, etc.).
static std::vector<std::string> manifest_inputs;
// In script `-c -L DIR` mode qcc normally records every preloaded `.qo` from
// `DIR` in the manifest.  Some build systems provide a larger transitive
// preload closure for parse-time declaration loading, while only a smaller
// direct `.qo` set should invalidate the target.  This flag lets such callers
// provide the exact manifest dependencies with --manifest-input.
static bool manifest_skip_qo_library_inputs = false;
// Build-group source-symbol manifest used by script `-c -L DIR` compiles to
// defer local project symbols instead of binding same-name loaded module/stub
// declarations.
static const char* source_symbol_manifest_path = nullptr;

static std::vector<std::string> qcc_depfile_explicit_inputs(
        const std::vector<std::string>& deps) {
    if (!depfile_qo_input_content_stamps) {
        return deps;
    }

    std::vector<std::string> rv;
    rv.reserve(deps.size());
    for (const std::string& dep : deps) {
        if (has_qo_extension(dep.c_str())) {
            rv.push_back(dep + ".content.stamp");
        } else {
            rv.push_back(dep);
        }
    }
    return rv;
}

// Preserve temporary .qo/glue files generated by one-shot multi-source
// executable mode.  Off by default so failed builds do not leave artifacts
// unless explicitly requested for diagnostics.
static bool save_temps = false;
// Phase C Item 2: --entry=<fn> names the Qore function the emitted
// C++ main() dispatches to after registering every script `.qo`
// input.  Meaningful in explicit object link mode and one-shot
// multi-source executable mode.  Defaults to "main" — hosts that use a
// different convention override via `-e <fn>`.
static const char* entry_fn = "main";

static std::string qcc_depfile_target(const std::string& output) {
    return depfile_target_path ? std::string(depfile_target_path) : output;
}

static void print_usage(const char* prog) {
    printf("Qore Code Compiler (qcc) v%s\n", QCC_VERSION);
    printf("Compiles Qore scripts, modules, objects, and archives\n\n");
    printf("Usage: %s [options] <source-file|object...>\n\n", prog);
    printf("Options:\n");
    printf("  -o, --output=FILE      Output file path (default: input name without extension)\n");
    printf("  -O, --opt-level=N      Optimization level 0-3 (default: 3)\n");
    printf("  -m, --module           Compile as module (.qm -> .qmod)\n");
    printf("  -c, --compile-only     Compile to .qo relocatable object\n");
    printf("      --context=DIR      Directory context for per-file .qo compilation\n"
           "                         (parses the full split-module dir but emits only\n"
           "                          the input file's contributions; requires -c)\n");
    printf("  -L DIR                 Script-mode sibling .qo search path.  May be\n"
           "                         repeated.  Each dir is scanned for *.qo; their\n"
           "                         fragment decls are preloaded so cross-file type\n"
           "                         refs in the target source resolve at parse time\n"
           "                         (C-style: .qo ~ .o + .h).  Enables `qcc -c <file>`\n"
           "                         for plain multi-file Qore apps with no .qm.\n");
    printf("      --output-dir=DIR   Output directory for `-c` compile.  With multiple\n"
           "                         sources (batch mode) this is required and all\n"
           "                         .qo files land in DIR sharing one parse cycle.\n"
           "                         With a single source this is an alternative to\n"
           "                         `-o <file>` — writes <basename>.qo into DIR.\n"
           "                         Mutually exclusive with `-o`.\n");
    printf("  -l, --load=MOD         Load Qore module MOD into the compile program\n"
           "                         before parsing sources.  Use when sources reference\n"
           "                         types from external modules without %%requires\n"
           "                         directives (the host loads the module at runtime).\n"
           "                         May be repeated; modules load in declaration order.\n"
           "                         Matches `qore -l <mod>` CLI convention.\n");
    printf("      --stub=FILE        Preload a declarative Qore source file into the\n"
           "                         compile program before targets — its namespaces,\n"
           "                         typedefs, and constants become visible to the\n"
           "                         parser, but no `.qo` is emitted for the stub.\n"
           "                         Use when the host synthesizes decls in C++ at\n"
           "                         runtime (namespaces, injected functions) that\n"
           "                         target sources reference by bare name.  Repeatable;\n"
           "                         parsed in declaration order before targets.\n");
    printf("      --parse-option=NAME  OR a `PO_*` flag into the compile program's parse\n"
           "                         options (e.g. PO_ALLOW_INJECTION, PO_NO_CHILD_PO_RESTRICTIONS).\n"
           "                         Mirrors the runtime `qpgm->parseSetParseOptions(...)` calls\n"
           "                         Qorus main.cpp files issue so method-domain checks line up\n"
           "                         between AOT and runtime paths.  Repeatable; see the name\n"
           "                         lookup table in QoreAOT.cpp for supported flags.\n");
    printf("      --define=NAME[=VAL]  Set a parser define (NAME=VAL, default VAL=True)\n"
           "                         applied via QoreProgram::parseDefine before any source\n"
           "                         parses.  Mirrors the runtime `qpgm->parseDefine(...)`\n"
           "                         calls Qorus main.cpp files issue so `%%ifdef` /\n"
           "                         `%%ifndef` sections stay in sync between AOT and\n"
           "                         runtime paths (e.g. `--define=NO_ORACLE` excludes\n"
           "                         the oracle-only block in QorusOracleDatasourcePool.qc).\n"
           "                         Repeatable; applied in declaration order.\n");
    printf("      --depfile=FILE     Emit Make-format dependency file at FILE after a\n"
           "                         successful compile.  Lists the output, a colon, and\n"
           "                         every source the parser opens (the target source\n"
           "                         plus, in `--context=DIR` mode, all sibling .qm/.qc/.ql).\n"
           "                         Equivalent to GCC's `-MMD -MF FILE`.  Intended for\n"
           "                         cmake's `add_custom_command(... DEPFILE ...)` so\n"
           "                         incremental builds retrigger on sibling-file edits.\n"
           "                         With --link-qo and --script-aggregate, lists the\n"
           "                         aggregate object's direct input files.\n");
    printf("      --depfile-target=FILE\n"
           "                         Override the Make depfile target written before the\n"
           "                         colon.  Requires --depfile=FILE and is intended\n"
           "                         for stamp-output custom commands whose generated\n"
           "                         artifact is a byproduct.\n");
    printf("      --depfile-qo-input-content-stamps\n"
           "                         In explicit depfile input lists, record each .qo\n"
           "                         dependency as .qo.content.stamp.  Intended for\n"
           "                         qo-link build rules that already depend on qcc\n"
           "                         content stamps to avoid mtime-only relinks.\n");
    printf("      --depfile-dir=DIR  Batch `-c --output-dir` mode only: emit one\n"
           "                         Make-format dependency file per generated `.qo` into\n"
           "                         DIR.  Each depfile is named `<object>.qo.d`.\n");
    printf("      --write-index-json=FILE\n"
           "                         Write build-consumable AOT symbol-index JSON for\n"
           "                         the generated artifact to FILE\n");
    printf("      --write-manifest=FILE\n"
           "                         Write a deterministic content manifest for the\n"
           "                         generated artifact to FILE\n");
    printf("      --manifest-input=FILE\n"
           "                         Add FILE as an extra manifest dependency; may repeat\n");
    printf("      --manifest-skip-qo-library-inputs\n"
           "                         Do not auto-record .qo files found through -L\n"
           "                         preload dirs in the manifest; use --manifest-input\n"
           "                         for the exact content dependencies instead\n");
    printf("      --source-symbol-manifest=FILE\n"
           "                         Script -c mode: defer build-group source symbols\n"
           "                         listed in FILE instead of binding same-name loaded\n"
           "                         module or stub declarations\n");
    printf("      --skip-if-manifest-current\n"
           "                         If --write-manifest matches existing output/input\n"
           "                         content, exit successfully without rebuilding\n");
    printf("      --write-status-json=FILE\n"
           "                         Write deterministic build-result JSON to FILE\n");
    printf("      --success-stamp=FILE\n"
           "                         Touch FILE after a successful compile/link or\n"
           "                         manifest-current skip\n");
    printf("      --content-stamp=FILE\n"
           "                         Create FILE after successful output verification;\n"
           "                         after that, touch it only when output bytes change\n");
    printf("      --save-temps       Keep temporary .qo and glue files generated by\n"
           "                         one-shot multi-source executable mode\n");
    printf("      --script-aggregate=SYM\n"
           "                         Emit one metadata-only script aggregate .qo\n"
           "                         exposing init_SYM_qo(); link it with the\n"
           "                         per-file .qo objects from batch script mode\n");
    printf("      --script-aggregate-native-registers\n"
           "                         With --script-aggregate, emit calls to linked\n"
           "                         per-file *_script_native_register() entries so\n"
           "                         native bodies are bound from per-file slot maps\n");
    printf("      --link-qo          Link existing script-context .qo objects into\n"
           "                         one aggregate register object without reparsing\n"
           "                         original sources; requires -o and\n"
           "                         --aggregate-symbol=SYM\n");
    printf("      --aggregate-symbol=SYM\n"
           "                         Aggregate symbol tail for --link-qo; emits\n"
           "                         init_SYM_qo(QoreProgram*)\n");
    printf("      --qolink-map=FILE  Write --link-qo map JSON to FILE (default:\n"
           "                         <output>.qolink.json)\n");
    printf("      --strict-call-relocations\n"
           "                         With --link-qo, fail if any call relocation is\n"
           "                         unresolved, ambiguous, or hash-mismatched.\n"
           "                         Useful for complete closed-world test links;\n"
           "                         production links can keep optional runtime fallback.\n");
    printf("      --allow-unresolved-imports\n"
           "                         With --link-qo, permit required symbol imports to\n"
           "                         remain unresolved in this aggregate and record them\n"
           "                         in the link map. Intended only for intermediate\n"
           "                         partial aggregates that are checked later by a\n"
           "                         complete strict link.\n");
    printf("      --from-objects     Aggregate mode: positional args are per-file .qo\n"
           "                         inputs; requires -m and --context=DIR; produces a\n"
           "                         standard .qmod by linking the .qo's + fresh glue\n");
    printf("  -a, --archive          Archive mode: positional args are per-file .qo inputs;\n"
           "                         requires --context=DIR; produces a .qoa static archive\n"
           "                         (ar rcs) exposing qore_qoa_register_all() for a C++ host\n");
    printf("  -S, --static           Link single-source executables statically against libqore\n");
    printf("  -t, --target=TRIPLE    Target triple for cross-compilation\n");
    printf("      --show-targets     Show supported target architectures and quit\n");
    printf("      --include-source   Embed source text in AOT metadata\n");
    printf("      --strip-source     Strip source text (default)\n");
    printf("      --aot-metadata-compression=MODE\n"
           "                         Metadata compression policy: auto, none, zlib, zstd\n"
           "                         (default: auto)\n");
    printf("      --strip-debug-info Strip DWARF debug info (faster compile, no debugger)\n");
    printf("  -g                     Emit DWARF debug info (default)\n");
    printf("      --time-trace[=PATH]  Emit Chrome-format trace of opt+codegen passes\n");
    printf("                         (default PATH: qcc.trace.json; view at chrome://tracing)\n");
    printf("      --big-fn-threshold=N  Mark functions >= N IR blocks as OptimizeNone+NoInline\n");
    printf("                         (trades ~1-7%% runtime for up to 46x compile speedup;\n");
    printf("                         default: 200; 0 = off)\n");
    printf("      --jobs=N           Parallel backend codegen threads, split-after-opt\n");
    printf("                         (default: CPU count; throttled by the make jobserver;\n");
    printf("                         1 = single-object codegen)\n");
    printf("  -e, --entry=FN         Qore function the emitted C++ main() calls after\n"
           "                         registering script objects (default: main).\n"
           "                         Applies to one-shot multi-source mode and\n"
           "                         `.qo` link mode.\n");
    printf("  -r, --warnings-are-errors\n"
           "                         Treat Qore parser warnings as fatal compile errors\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -V, --version          Show version information\n");
    printf("      --dump-info        Inspect embedded Qore AOT metadata in object files,\n"
           "                         .qmod modules, .qo fragments, .qoa archives, or\n"
           "                         linked AOT executables without executing them\n");
    printf("      --dump-symbols     Include an nm-like symbol table view\n");
    printf("      --dump-sections    Include object and AOT section tables\n");
    printf("      --dump-index-json  Emit build-consumable AOT symbol-index JSON\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s script.qr                   # Compile to 'script' executable\n", prog);
    printf("  %s -o myapp script.qr          # Compile to 'myapp' executable\n", prog);
    printf("  %s -o myapp lib.qr main.qr     # One-shot multi-source executable\n", prog);
    printf("  %s -c --output-dir=aot lib.qr main.qr  # Batch-compile app sources\n", prog);
    printf("  %s -o myapp aot/*.qo           # Link compiled app objects\n", prog);
    printf("  %s -o myapp main.qo lib.qo     # Link-mode: .qo's -> 'myapp' binary\n", prog);
    printf("  %s -m MyModule.qm              # Compile single-file module to 'MyModule.qmod'\n", prog);
    printf("  %s -m qlib/DataProvider        # Compile split module directory\n", prog);
    printf("  %s -S -o myapp script.qr       # Static link (no libqore.so dependency)\n", prog);
    printf("\n");
    printf("Notes:\n");
    printf("  - AOT requires %%modern; .qr and other non-.q source extensions enable it by default\n");
    printf("  - .q remains legacy by default unless the source contains %%modern\n");
    printf("  - Static linking requires libqore_static.a (build with -DBUILD_STATIC_LIBQORE=ON)\n");
    printf("  - Cross-compilation requires LLVM support for the target architecture\n");
}

static void print_version() {
    printf("qcc (Qore Code Compiler) v%s\n", QCC_VERSION);
    printf("Using Qore library v%s\n", qore_version_string);
    printf("Built with LLVM for JIT/AOT compilation\n");
}

static bool qccHandleWarnings(ExceptionSink& wsink) {
    bool has_warnings = wsink.isException();
    wsink.handleWarnings();
    if (!has_warnings || !warnings_are_errors) {
        return false;
    }
    fprintf(stderr, "error: Qore parser warnings treated as fatal compile errors\n");
    return true;
}

static struct option long_options[] = {
    {"output",            required_argument, nullptr, 'o'},
    {"opt-level",         required_argument, nullptr, 'O'},
    {"module",            no_argument,       nullptr, 'm'},
    {"compile-only",      no_argument,       nullptr, 'c'},
    {"static",            no_argument,       nullptr, 'S'},
    {"target",            required_argument, nullptr, 't'},
    {"show-targets",      no_argument,       nullptr, 'T'},
    {"include-source",    no_argument,       nullptr, 'I'},
    {"strip-source",      no_argument,       nullptr, 'P'},
    {"strip-debug-info",  no_argument,       nullptr, 'D'},
    {"time-trace",        optional_argument, nullptr, 'Y'},
    {"big-fn-threshold",  required_argument, nullptr, 'B'},
    {"context",           required_argument, nullptr, 'C'},
    {"library-path",      required_argument, nullptr, 'L'},
    {"load",              required_argument, nullptr, 'l'},
    // long-only options (no short form): use values > 255 so they
    // don't collide with char short codes.
    {"output-dir",        required_argument, nullptr, 0x100},
    {"stub",              required_argument, nullptr, 0x101},
    {"depfile",           required_argument, nullptr, 0x102},
    {"define",            required_argument, nullptr, 0x103},
    {"parse-option",      required_argument, nullptr, 0x104},
    {"dump-info",         no_argument,       nullptr, 0x105},
    {"dump-symbols",      no_argument,       nullptr, 0x106},
    {"dump-sections",     no_argument,       nullptr, 0x107},
    {"save-temps",        no_argument,       nullptr, 0x108},
    {"aot-metadata-compression", required_argument, nullptr, 0x109},
    {"script-aggregate",  required_argument, nullptr, 0x10a},
    {"depfile-dir",       required_argument, nullptr, 0x10b},
    {"dump-index-json",   no_argument,       nullptr, 0x10c},
    {"link-qo",           no_argument,       nullptr, 0x10d},
    {"aggregate-symbol",  required_argument, nullptr, 0x10e},
    {"qolink-map",        required_argument, nullptr, 0x10f},
    {"strict-call-relocations", no_argument, nullptr, 0x110},
    {"script-aggregate-native-registers", no_argument, nullptr, 0x111},
    {"allow-unresolved-imports", no_argument, nullptr, 0x112},
    {"write-index-json",  required_argument, nullptr, 0x113},
    {"write-manifest",    required_argument, nullptr, 0x114},
    {"skip-if-manifest-current", no_argument, nullptr, 0x115},
    {"manifest-input",    required_argument, nullptr, 0x116},
    {"write-status-json", required_argument, nullptr, 0x117},
    {"success-stamp",     required_argument, nullptr, 0x118},
    {"content-stamp",     required_argument, nullptr, 0x119},
    {"manifest-skip-qo-library-inputs", no_argument, nullptr, 0x11a},
    {"depfile-target",    required_argument, nullptr, 0x11b},
    {"depfile-qo-input-content-stamps", no_argument, nullptr, 0x11c},
    {"source-symbol-manifest", required_argument, nullptr, 0x11d},
    {"jobs",              required_argument, nullptr, 0x11e},
    {"from-objects",      no_argument,       nullptr, 'F'},
    {"archive",           no_argument,       nullptr, 'a'},
    {"entry",             required_argument, nullptr, 'e'},
    {"warnings-are-errors", no_argument,     nullptr, 'r'},
    {"verbose",           no_argument,       nullptr, 'v'},
    {"help",              no_argument,       nullptr, 'h'},
    {"version",           no_argument,       nullptr, 'V'},
    {nullptr,             0,                 nullptr, 0}
};

static int parse_options_cmdline(int argc, char** argv) {
    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:mcSt:TL:l:age:rvhV", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'o':
                output_path = optarg;
                break;
            case 'O':
                opt_level = atoi(optarg);
                if (opt_level < 0 || opt_level > 3) {
                    fprintf(stderr, "error: invalid optimization level '%s' (must be 0-3)\n", optarg);
                    return 1;
                }
                break;
            case 'm':
                module_mode = true;
                break;
            case 'c':
                compile_only = true;
                break;
            case 'S':
                static_link = true;
                break;
            case 't':
                target_triple = optarg;
                break;
            case 'T':
                show_targets = true;
                break;
            case 'I':
                include_source = true;
                break;
            case 'P':
                // strip-source is the default; this is a no-op for backward compatibility
                include_source = false;
                break;
            case 'D':
                strip_debug_info = true;
                break;
            case 'g':
                // -g: explicitly request debug info (default, but overrides --strip-debug-info
                // when both given)
                strip_debug_info = false;
                break;
            case 'Y':
                time_trace_path = optarg ? optarg : "qcc.trace.json";
                break;
            case 'B':
                big_fn_threshold = atoi(optarg);
                if (big_fn_threshold < 0) {
                    fprintf(stderr, "error: --big-fn-threshold must be >= 0\n");
                    return 1;
                }
                big_fn_threshold_cli_explicit = true;
                break;
            case 'C':
                context_dir = optarg;
                break;
            case 'L':
                script_lib_dirs.emplace_back(optarg);
                break;
            case 'l':
                load_modules.emplace_back(optarg);
                break;
            case 0x100:  // --output-dir
                batch_output_dir = optarg;
                break;
            case 0x101:  // --stub
                stub_files.emplace_back(optarg);
                break;
            case 0x102:  // --depfile
                depfile_path = optarg;
                break;
            case 0x11b:  // --depfile-target
                depfile_target_path = optarg;
                break;
            case 0x10b:  // --depfile-dir
                depfile_dir = optarg;
                break;
            case 0x103:  // --define
                parse_defines.emplace_back(optarg);
                break;
            case 0x104:  // --parse-option
                parse_option_flags.emplace_back(optarg);
                break;
            case 0x105:  // --dump-info
                dump_info = true;
                break;
            case 0x106:  // --dump-symbols
                dump_symbols = true;
                break;
            case 0x107:  // --dump-sections
                dump_sections = true;
                break;
            case 0x10c:  // --dump-index-json
                dump_index_json = true;
                break;
            case 0x108:  // --save-temps
                save_temps = true;
                break;
            case 0x109:  // --aot-metadata-compression
                if (strcmp(optarg, "auto") && strcmp(optarg, "none")
                        && strcmp(optarg, "zlib") && strcmp(optarg, "zstd")) {
                    fprintf(stderr, "error: invalid --aot-metadata-compression value '%s' "
                        "(must be auto, none, zlib, or zstd)\n", optarg);
                    return 1;
                }
                metadata_compression = optarg;
                break;
            case 0x10a:  // --script-aggregate
                script_aggregate_symbol = optarg;
                break;
            case 0x10d:  // --link-qo
                link_qo = true;
                break;
            case 0x10e:  // --aggregate-symbol
                link_aggregate_symbol = optarg;
                break;
            case 0x10f:  // --qolink-map
                qolink_map_path = optarg;
                break;
            case 0x110:  // --strict-call-relocations
                strict_call_relocations = true;
                break;
            case 0x111:  // --script-aggregate-native-registers
                script_aggregate_native_registers = true;
                break;
            case 0x112:  // --allow-unresolved-imports
                allow_unresolved_qo_imports = true;
                break;
            case 0x113:  // --write-index-json
                write_index_json_path = optarg;
                break;
            case 0x114:  // --write-manifest
                write_manifest_path = optarg;
                break;
            case 0x115:  // --skip-if-manifest-current
                skip_if_manifest_current = true;
                break;
            case 0x116:  // --manifest-input
                manifest_inputs.emplace_back(optarg);
                break;
            case 0x117:  // --write-status-json
                write_status_json_path = optarg;
                break;
            case 0x118:  // --success-stamp
                success_stamp_path = optarg;
                break;
            case 0x119:  // --content-stamp
                content_stamp_path = optarg;
                break;
            case 0x11a:  // --manifest-skip-qo-library-inputs
                manifest_skip_qo_library_inputs = true;
                break;
            case 0x11c:  // --depfile-qo-input-content-stamps
                depfile_qo_input_content_stamps = true;
                break;
            case 0x11d:  // --source-symbol-manifest
                source_symbol_manifest_path = optarg;
                manifest_inputs.emplace_back(optarg);
                break;
            case 0x11e:  // --jobs
                aot_jobs = atoi(optarg);
                if (aot_jobs < 1) {
                    fprintf(stderr, "error: --jobs must be >= 1\n");
                    return -1;
                }
                break;
            case 'F':
                from_objects = true;
                break;
            case 'a':
                archive_mode = true;
                break;
            case 'e':
                entry_fn = optarg;
                break;
            case 'r':
                warnings_are_errors = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                show_help = true;
                break;
            case 'V':
                show_version = true;
                break;
            default:
                return 1;
        }
    }
    return 0;
}

static bool read_file(const char* path, std::string& content) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: cannot seek '%s': %s\n", path, strerror(errno));
        fclose(f);
        return false;
    }

    long fsize = ftell(f);
    if (fsize < 0) {
        fprintf(stderr, "error: cannot get size of '%s': %s\n", path, strerror(errno));
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: cannot seek '%s': %s\n", path, strerror(errno));
        fclose(f);
        return false;
    }

    if (!fsize) {
        content.clear();
        if (fclose(f) != 0) {
            fprintf(stderr, "error: cannot close '%s': %s\n", path, strerror(errno));
            return false;
        }
        return true;
    }

    size_t expected = static_cast<size_t>(fsize);
    content.resize(expected);
    size_t nread = fread(&content[0], 1, expected, f);
    if (nread != expected) {
        if (ferror(f)) {
            fprintf(stderr, "error: cannot read '%s': %s\n", path, strerror(errno));
        } else {
            fprintf(stderr, "error: short read from '%s': expected %zu bytes, got %zu\n",
                path, expected, nread);
        }
        fclose(f);
        content.clear();
        return false;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "error: cannot close '%s': %s\n", path, strerror(errno));
        content.clear();
        return false;
    }

    return true;
}

struct AOTDumpMetadataBlob {
    std::string source;
    std::vector<uint8_t> bytes;
};

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0])
        | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

static bool find_aot_metadata_length(const uint8_t* data, size_t avail, size_t& len) {
    len = 0;
    if (avail < QORE_AOT_HEADER_SIZE) {
        return false;
    }
    if (read_u32_le(data) != QORE_AOT_BINARY_MAGIC) {
        return false;
    }
    uint16_t version = read_u16_le(data + 4);
    if (version == 0 || version > QORE_AOT_BINARY_VERSION) {
        return false;
    }

    uint8_t compression = data[34];
    if (compression == QORE_AOT_COMPRESSION_NONE) {
        uint32_t section_count = read_u32_le(data + 16);
        if (section_count > 100000) {
            return false;
        }
        constexpr size_t section_header_size = 12;
        size_t section_dir_size = static_cast<size_t>(section_count) * section_header_size;
        size_t string_pool_size_pos = QORE_AOT_HEADER_SIZE + section_dir_size;
        if (string_pool_size_pos + 4 > avail) {
            return false;
        }
        uint32_t string_pool_size = read_u32_le(data + string_pool_size_pos);
        size_t data_area_offset = string_pool_size_pos + 4 + string_pool_size;
        if (data_area_offset > avail) {
            return false;
        }

        uint64_t data_area_size = 0;
        const uint8_t* sec = data + QORE_AOT_HEADER_SIZE;
        for (uint32_t i = 0; i < section_count; ++i, sec += section_header_size) {
            uint32_t offset = read_u32_le(sec + 4);
            uint32_t size = read_u32_le(sec + 8);
            uint64_t end = static_cast<uint64_t>(offset) + size;
            if (end > data_area_size) {
                data_area_size = end;
            }
        }
        uint64_t total = static_cast<uint64_t>(data_area_offset) + data_area_size;
        if (total > avail) {
            return false;
        }
        len = static_cast<size_t>(total);
        return true;
    }

    if (compression == QORE_AOT_COMPRESSION_ZLIB) {
        if (avail < QORE_AOT_HEADER_SIZE + 4) {
            return false;
        }
        uint32_t uncompressed_size = read_u32_le(data + QORE_AOT_HEADER_SIZE);
        if (uncompressed_size > 100 * 1024 * 1024) {
            return false;
        }
        if (uncompressed_size == 0) {
            len = QORE_AOT_HEADER_SIZE + 4;
            return true;
        }

        std::vector<uint8_t> out(uncompressed_size);
        z_stream strm{};
        int rc = inflateInit(&strm);
        if (rc != Z_OK) {
            return false;
        }
        strm.next_in = const_cast<Bytef*>(data + QORE_AOT_HEADER_SIZE + 4);
        strm.avail_in = static_cast<uInt>(std::min<size_t>(
            avail - QORE_AOT_HEADER_SIZE - 4, std::numeric_limits<uInt>::max()));
        strm.next_out = out.data();
        strm.avail_out = static_cast<uInt>(std::min<uint32_t>(
            uncompressed_size, std::numeric_limits<uInt>::max()));
        rc = inflate(&strm, Z_FINISH);
        bool ok = rc == Z_STREAM_END && strm.total_out == uncompressed_size;
        size_t consumed = static_cast<size_t>(strm.total_in);
        inflateEnd(&strm);
        if (!ok) {
            return false;
        }
        len = QORE_AOT_HEADER_SIZE + 4 + consumed;
        return len <= avail;
    }

    if (compression == QORE_AOT_COMPRESSION_ZSTD) {
        if (avail < QORE_AOT_HEADER_SIZE + 4) {
            return false;
        }
        uint32_t uncompressed_size = read_u32_le(data + QORE_AOT_HEADER_SIZE);
        if (uncompressed_size > 100 * 1024 * 1024) {
            return false;
        }
        if (!uncompressed_size) {
            len = QORE_AOT_HEADER_SIZE + 4;
            return true;
        }
        const uint8_t* frame = data + QORE_AOT_HEADER_SIZE + 4;
        size_t frame_size = ZSTD_findFrameCompressedSize(frame,
            avail - QORE_AOT_HEADER_SIZE - 4);
        if (ZSTD_isError(frame_size)) {
            return false;
        }
        len = QORE_AOT_HEADER_SIZE + 4 + frame_size;
        return len <= avail;
    }

    return false;
}

static const char* aot_section_type_name(uint16_t type) {
    switch (static_cast<QoreAOTSectionType>(type)) {
        case QoreAOTSectionType::STRINGS: return "STRINGS";
        case QoreAOTSectionType::NAMESPACES: return "NAMESPACES";
        case QoreAOTSectionType::CLASSES: return "CLASSES";
        case QoreAOTSectionType::HASHDECLS: return "HASHDECLS";
        case QoreAOTSectionType::ENUMS: return "ENUMS";
        case QoreAOTSectionType::TYPEDEFS: return "TYPEDEFS";
        case QoreAOTSectionType::CONSTANTS: return "CONSTANTS";
        case QoreAOTSectionType::GLOBALS: return "GLOBALS";
        case QoreAOTSectionType::FUNCTIONS: return "FUNCTIONS";
        case QoreAOTSectionType::METHODS: return "METHODS";
        case QoreAOTSectionType::SLOT_MAPS: return "SLOT_MAPS";
        case QoreAOTSectionType::TOPLEVEL: return "TOPLEVEL";
        case QoreAOTSectionType::FUNC_SOURCES: return "FUNC_SOURCES";
        case QoreAOTSectionType::DEPENDENCIES: return "DEPENDENCIES";
        case QoreAOTSectionType::REEXPORT_MODULES: return "REEXPORT_MODULES";
        case QoreAOTSectionType::PROGRAM_METADATA: return "PROGRAM_METADATA";
        case QoreAOTSectionType::INIT_FUNCS: return "INIT_FUNCS";
        case QoreAOTSectionType::TYPE_TABLE: return "TYPE_TABLE";
        case QoreAOTSectionType::MODULE_PATH_PREPEND: return "MODULE_PATH_PREPEND";
        case QoreAOTSectionType::MODULE_PATH_APPEND: return "MODULE_PATH_APPEND";
        case QoreAOTSectionType::BUILD_INFO: return "BUILD_INFO";
        case QoreAOTSectionType::MODULE_COMMANDS: return "MODULE_COMMANDS";
        case QoreAOTSectionType::PLUGIN_TYPE_REGISTRY: return "PLUGIN_TYPE_REGISTRY";
        case QoreAOTSectionType::PLUGIN_IMPORTS: return "PLUGIN_IMPORTS";
        case QoreAOTSectionType::PLUGIN_HELPER_REFS: return "PLUGIN_HELPER_REFS";
        case QoreAOTSectionType::SYMBOL_INDEX: return "SYMBOL_INDEX";
        case QoreAOTSectionType::CALL_RELOCATIONS: return "CALL_RELOCATIONS";
    }
    return "UNKNOWN";
}

static std::string aot_metadata_key(const QoreAOTBinaryReader& reader, size_t size) {
    const QoreAOTBinaryHeader& hdr = reader.getHeader();
    const char* label = reader.getLabel();
    return std::string(label ? label : "") + "|" + std::to_string(hdr.source_hash)
        + "|" + std::to_string(hdr.flags) + "|" + std::to_string(hdr.feature_flags)
        + "|" + std::to_string(size);
}

static bool add_aot_metadata_blob(std::vector<AOTDumpMetadataBlob>& blobs,
        std::set<std::string>& seen, std::vector<uint8_t>&& bytes,
        const std::string& source) {
    if (bytes.size() < QORE_AOT_HEADER_SIZE) {
        return false;
    }
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(bytes.data(), static_cast<uint32_t>(bytes.size()), error)) {
        return false;
    }
    std::string key = aot_metadata_key(reader, bytes.size());
    if (!seen.insert(key).second) {
        return true;
    }

    AOTDumpMetadataBlob blob;
    blob.source = source;
    blob.bytes = std::move(bytes);
    blobs.push_back(std::move(blob));
    return true;
}

static bool is_aot_metadata_symbol(llvm::StringRef name) {
    return name == "qore_aot_metadata"
        || name == "qore_aot_mod_metadata"
        || name.ends_with("_fragment_blob");
}

static bool is_qore_relevant_symbol(const std::string& name) {
    return name.rfind("qore_", 0) == 0
        || name.rfind("_qore_", 0) == 0
        || name.rfind("_qaot_", 0) == 0
        || name.find("qore_aot") != std::string::npos
        || name.find("qore_module") != std::string::npos;
}

static std::string symbol_section_name(const llvm::object::ObjectFile& obj,
        const llvm::object::SymbolRef& sym) {
    auto sec_or = sym.getSection();
    if (!sec_or) {
        llvm::consumeError(sec_or.takeError());
        return "";
    }
    llvm::object::section_iterator sec = *sec_or;
    if (sec == obj.section_end()) {
        return "";
    }
    auto name_or = sec->getName();
    if (!name_or) {
        llvm::consumeError(name_or.takeError());
        return "";
    }
    return name_or->str();
}

static uint64_t symbol_value(const llvm::object::SymbolRef& sym) {
    auto value_or = sym.getValue();
    if (!value_or) {
        llvm::consumeError(value_or.takeError());
        return 0;
    }
    return *value_or;
}

static uint32_t symbol_flags(const llvm::object::SymbolRef& sym) {
    auto flags_or = sym.getFlags();
    if (!flags_or) {
        llvm::consumeError(flags_or.takeError());
        return 0;
    }
    return *flags_or;
}

static std::string symbol_key(const llvm::object::ObjectFile& obj,
        const llvm::object::SymbolRef& sym) {
    auto name_or = sym.getName();
    std::string name;
    if (name_or) {
        name = name_or->str();
    } else {
        llvm::consumeError(name_or.takeError());
    }
    return name + "|" + std::to_string(symbol_value(sym)) + "|" + symbol_section_name(obj, sym);
}

static char symbol_kind(const llvm::object::SymbolRef& sym) {
    uint32_t flags = symbol_flags(sym);
    if (flags & llvm::object::SymbolRef::SF_Undefined) {
        return 'U';
    }
    if (flags & llvm::object::SymbolRef::SF_Common) {
        return 'C';
    }

    char c = '?';
    auto type_or = sym.getType();
    if (type_or) {
        switch (*type_or) {
            case llvm::object::SymbolRef::ST_Function:
                c = 'T';
                break;
            case llvm::object::SymbolRef::ST_Data:
                c = 'D';
                break;
            case llvm::object::SymbolRef::ST_Debug:
            case llvm::object::SymbolRef::ST_File:
                c = 'N';
                break;
            default:
                break;
        }
    } else {
        llvm::consumeError(type_or.takeError());
    }
    if (c == '?' && (flags & llvm::object::SymbolRef::SF_Executable)) {
        c = 'T';
    }
    if (c == '?' && (flags & llvm::object::SymbolRef::SF_Const)) {
        c = 'R';
    }
    if (!(flags & llvm::object::SymbolRef::SF_Global) && c >= 'A' && c <= 'Z') {
        c = static_cast<char>(std::tolower(c));
    }
    return c;
}

static void extract_aot_metadata_from_object(const llvm::object::ObjectFile& obj,
        std::vector<AOTDumpMetadataBlob>& blobs, std::set<std::string>& seen) {
    auto symbol_sizes = llvm::object::computeSymbolSizes(obj);
    for (const auto& [sym, size] : symbol_sizes) {
        if (!size) {
            continue;
        }
        auto name_or = sym.getName();
        if (!name_or) {
            llvm::consumeError(name_or.takeError());
            continue;
        }
        llvm::StringRef name = *name_or;
        if (!is_aot_metadata_symbol(name)) {
            continue;
        }

        auto sec_or = sym.getSection();
        if (!sec_or) {
            llvm::consumeError(sec_or.takeError());
            continue;
        }
        llvm::object::section_iterator sec = *sec_or;
        if (sec == obj.section_end()) {
            continue;
        }
        auto addr_or = sym.getAddress();
        if (!addr_or) {
            llvm::consumeError(addr_or.takeError());
            continue;
        }
        auto contents_or = sec->getContents();
        if (!contents_or) {
            llvm::consumeError(contents_or.takeError());
            continue;
        }

        uint64_t section_address = sec->getAddress();
        if (*addr_or < section_address) {
            continue;
        }
        uint64_t offset = *addr_or - section_address;
        llvm::StringRef contents = *contents_or;
        uint64_t contents_size = static_cast<uint64_t>(contents.size());
        if (offset > contents_size || size > contents_size - offset) {
            continue;
        }
        const uint8_t* p = reinterpret_cast<const uint8_t*>(contents.data() + offset);
        std::vector<uint8_t> bytes(p, p + size);
        add_aot_metadata_blob(blobs, seen, std::move(bytes), "symbol " + name.str());
    }
}

static void scan_aot_metadata_blobs(const std::string& contents,
        std::vector<AOTDumpMetadataBlob>& blobs, std::set<std::string>& seen) {
    size_t pos = 0;
    while ((pos = contents.find("QORD", pos)) != std::string::npos) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(contents.data() + pos);
        size_t len = 0;
        if (find_aot_metadata_length(data, contents.size() - pos, len) && len > 0) {
            std::vector<uint8_t> bytes(data, data + len);
            add_aot_metadata_blob(blobs, seen, std::move(bytes),
                "embedded metadata at file offset " + std::to_string(pos));
            pos += len;
        } else {
            ++pos;
        }
    }
}

static void print_string_list(const char* label, const std::vector<std::string>& values) {
    if (values.empty()) {
        printf("    %s: (none)\n", label);
        return;
    }
    printf("    %s:\n", label);
    for (const std::string& value : values) {
        printf("      %s\n", value.c_str());
    }
}

static void print_module_commands(const std::vector<AOTModuleCommand>& commands) {
    if (commands.empty()) {
        printf("    module commands: (none)\n");
        return;
    }
    printf("    module commands:\n");
    for (const AOTModuleCommand& command : commands) {
        printf("      %%module-cmd(%s) %s\n", command.module.c_str(), command.command.c_str());
    }
}

static void print_aot_feature_flags(uint64_t flags) {
    static const struct {
        uint64_t bit;
        const char* name;
    } feature_names[] = {
        {QORE_AOT_FEAT_FOREACH_REF, "foreach-ref"},
        {QORE_AOT_FEAT_NATIVE_CAST, "native-cast"},
        {QORE_AOT_FEAT_BLOCK_EXIT, "block-exit"},
        {QORE_AOT_FEAT_DIRECT_INDEX, "direct-index"},
        {QORE_AOT_FEAT_HASH_KEY_ACCESS, "hash-key-access"},
        {QORE_AOT_FEAT_FAST_CALL, "fast-call"},
        {QORE_AOT_FEAT_COMPLEX_RETURN, "complex-return"},
        {QORE_AOT_FEAT_HASH_KEY_STORE, "hash-key-store"},
        {QORE_AOT_FEAT_LIST_INDEX_STORE, "list-index-store"},
        {QORE_AOT_FEAT_TYPE_TABLE, "type-table"},
        {QORE_AOT_FEAT_CONST_PENDING, "const-pending"},
        {QORE_AOT_FEAT_SIG_LINES, "sig-lines"},
        {QORE_AOT_FEAT_CONTEXT_IR, "context-ir"},
        {QORE_AOT_FEAT_LVPATH_SLICE, "lvpath-slice"},
        {QORE_AOT_FEAT_MODULE_PATH_LISTS, "module-path-lists"},
        {QORE_AOT_FEAT_LVPATH_DELETE_EXPR, "lvpath-delete-expr"},
        {QORE_AOT_FEAT_LVPATH_PATTERN, "lvpath-pattern"},
        {QORE_AOT_FEAT_FUNC_CALL_VARIANT, "func-call-variant"},
        {QORE_AOT_FEAT_BACKQUOTE, "backquote"},
        {QORE_AOT_FEAT_FIND, "find"},
        {QORE_AOT_FEAT_BACKGROUND_IR, "background-ir"},
        {QORE_AOT_FEAT_INLINE_CALL_ARGS, "inline-call-args"},
        {QORE_AOT_FEAT_LIST_SELECTOR_RANGE, "list-selector-range"},
        {QORE_AOT_FEAT_ENTRY_STMT_LINES, "entry-stmt-lines"},
        {QORE_AOT_FEAT_PARSE_REF_TYPE, "parse-ref-type"},
        {QORE_AOT_FEAT_STMT_LOC_TABLE, "stmt-loc-table"},
        {QORE_AOT_FEAT_DEBUG_IR, "debug-ir"},
        {QORE_AOT_FEAT_SELF_CALL_ARGS, "self-call-args"},
        {QORE_AOT_FEAT_BODY_LOCAL_SLOT, "body-local-slot"},
        {QORE_AOT_FEAT_BCA_LINES, "bca-lines"},
        {QORE_AOT_FEAT_BCA_NATIVE_ARGS, "bca-native-args"},
        {QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS, "closure-varargs-flags"},
        {QORE_AOT_FEAT_CONTAINER_TYPEINFO, "container-typeinfo"},
        {QORE_AOT_FEAT_CLASS_HASH, "class-hash"},
        {QORE_AOT_FEAT_METHOD_SYNC, "method-sync"},
        {QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS, "typed-value-containers"},
        {QORE_AOT_FEAT_MODULE_COMMANDS, "module-commands"},
        {QORE_AOT_FEAT_WIDE_IR_OPERANDS, "wide-ir-operands"},
        {QORE_AOT_FEAT_WIDE_LOC_TABLES, "wide-loc-tables"},
        {QORE_AOT_FEAT_LOCAL_DECL_ORDINAL, "local-decl-ordinal"},
        {QORE_AOT_FEAT_CLASS_INJECTION, "class-injection"},
        {QORE_AOT_FEAT_VARIANT_PARSE_OPTIONS, "variant-parse-options"},
        {QORE_AOT_FEAT_BCA_NAMED_ARG_MAP, "bca-named-arg-map"},
        {QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO, "new-object-typeinfo"},
        {QORE_AOT_FEAT_CLASS_TYPE_PARAMS, "class-type-params"},
        {QORE_AOT_FEAT_CLASS_PARAM_BASES, "class-param-bases"},
        {QORE_AOT_FEAT_CLASS_RAW_GENERIC, "class-raw-generic"},
        {QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE, "static-call-receiver-type"},
        {QORE_AOT_FEAT_HASHDECL_TYPE_PARAMS, "hashdecl-type-params"},
        {QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS, "type-param-defaults"},
        {QORE_AOT_FEAT_HASHDECL_PARAM_PARENTS, "hashdecl-param-parents"},
        {QORE_AOT_FEAT_TYPE_PARAM_BOUNDS, "type-param-bounds"},
        {QORE_AOT_FEAT_PLUGIN_DISPATCH, "plugin-dispatch"},
        {QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND, "complex-buffer-init-kind"},
        {QORE_AOT_FEAT_READONLY_LOCALS, "readonly-locals"},
        {QORE_AOT_FEAT_CONST_METHODS, "const-methods"},
        {QORE_AOT_FEAT_CALL_CLOSURE_REF_ARGS, "call-closure-ref-args"},
        {QORE_AOT_FEAT_TYPED_PHI, "typed-phi"},
        {QORE_AOT_FEAT_CALL_RELOCATIONS, "call-relocations"},
        {QORE_AOT_FEAT_HASH_DEREF_TYPEINFO, "hash-deref-typeinfo"},
    };

    printf("    features: 0x%016llx", static_cast<unsigned long long>(flags));
    bool first = true;
    for (const auto& feature : feature_names) {
        if (flags & feature.bit) {
            printf("%s%s", first ? " (" : ", ", feature.name);
            first = false;
        }
    }
    if (!first) {
        printf(")");
    }
    printf("\n");

    uint64_t unsupported = flags & ~QORE_AOT_SUPPORTED_FEATURES;
    if (unsupported) {
        printf("    unsupported features in this runtime: 0x%016llx\n",
            static_cast<unsigned long long>(unsupported));
    }
}

struct AOTSlotMapDumpSummary {
    struct ExprTreeDetail {
        std::string function;
        uint8_t root_kind = 0;
        uint8_t top_kind = 0;
        uint8_t container_kind = 0;
        uint16_t slot = 0xffff;
        bool top_level = false;
    };

    uint32_t functions = 0;
    uint64_t local_slots = 0;
    uint64_t global_slots = 0;
    uint64_t expr_slots = 0;
    uint64_t stmt_slots = 0;
    uint64_t regex_slots = 0;
    uint64_t body_locals = 0;
    uint64_t lv_path_slots = 0;
    uint32_t unsupported_functions = 0;
    uint32_t malformed_entries = 0;
    std::map<uint8_t, uint64_t> top_expr_kinds;
    std::map<uint8_t, uint64_t> all_expr_kinds;
    std::map<uint8_t, uint64_t> expr_tree_root_kinds;
    std::vector<ExprTreeDetail> expr_tree_details;
    std::vector<std::string> expr_tree_functions;
    std::vector<std::string> generic_eval_functions;
};

struct AOTDumpExprContext {
    uint8_t top_kind = 0;
    uint8_t container_kind = 0;
    uint16_t slot = 0xffff;
};

static bool dump_need(const uint8_t* p, const uint8_t* end, size_t n) {
    return p && end && p <= end && static_cast<size_t>(end - p) >= n;
}

static bool dump_skip_bytes(const uint8_t*& p, const uint8_t* end, size_t n) {
    if (!dump_need(p, end, n)) {
        return false;
    }
    p += n;
    return true;
}

static bool dump_read_u8(const uint8_t*& p, const uint8_t* end, uint8_t& v) {
    if (!dump_need(p, end, 1)) {
        return false;
    }
    v = QoreAOTBinaryReader::readU8(p);
    return true;
}

static bool dump_read_u16(const uint8_t*& p, const uint8_t* end, uint16_t& v) {
    if (!dump_need(p, end, 2)) {
        return false;
    }
    v = QoreAOTBinaryReader::readU16(p);
    return true;
}

static bool dump_read_u32(const uint8_t*& p, const uint8_t* end, uint32_t& v) {
    if (!dump_need(p, end, 4)) {
        return false;
    }
    v = QoreAOTBinaryReader::readU32(p);
    return true;
}

static bool dump_skip_string_ref(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, const char** value = nullptr) {
    if (!dump_need(p, end, 4)) {
        return false;
    }
    const char* s = reader.readStringRef(p);
    if (value) {
        *value = s;
    }
    return true;
}

static bool dump_skip_len_string_ref(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end) {
    uint32_t len = 0;
    return dump_read_u32(p, end, len) && dump_skip_string_ref(reader, p, end);
}

static const char* dump_aot_expr_kind_name(uint8_t kind) {
    const auto* info = getAOTExprSlotKindInfo(kind);
    if (info && info->name) {
        return info->name;
    }
    switch (static_cast<AOTExprKind>(kind)) {
        case AOTExprKind::HASH_LITERAL: return "HASH_LITERAL";
        case AOTExprKind::HASH_DEREF: return "HASH_DEREF";
        case AOTExprKind::PARSE_REF: return "PARSE_REF";
        case AOTExprKind::LIST_LITERAL: return "LIST_LITERAL";
        case AOTExprKind::PLUS: return "PLUS";
        case AOTExprKind::SQUARE_BRACKET: return "SQUARE_BRACKET";
        case AOTExprKind::PARSE_HASH: return "PARSE_HASH";
        case AOTExprKind::EXISTS: return "EXISTS";
        case AOTExprKind::IMPLICIT_ARG: return "IMPLICIT_ARG";
        case AOTExprKind::MINUS: return "MINUS";
        case AOTExprKind::KEYS: return "KEYS";
        case AOTExprKind::MULTIPLY: return "MULTIPLY";
        case AOTExprKind::DIVIDE: return "DIVIDE";
        case AOTExprKind::MODULO: return "MODULO";
        case AOTExprKind::IMPLICIT_ELEM: return "IMPLICIT_ELEM";
        case AOTExprKind::INSTANCEOF: return "INSTANCEOF";
        case AOTExprKind::REGEX_MATCH: return "REGEX_MATCH";
        case AOTExprKind::REGEX_NMATCH: return "REGEX_NMATCH";
        case AOTExprKind::REGEX_EXTRACT: return "REGEX_EXTRACT";
        case AOTExprKind::PRE_INC: return "PRE_INC";
        case AOTExprKind::PRE_DEC: return "PRE_DEC";
        case AOTExprKind::POST_INC: return "POST_INC";
        case AOTExprKind::POST_DEC: return "POST_DEC";
        case AOTExprKind::LOG_EQ: return "LOG_EQ";
        case AOTExprKind::LOG_NE: return "LOG_NE";
        case AOTExprKind::LOG_LT: return "LOG_LT";
        case AOTExprKind::LOG_GT: return "LOG_GT";
        case AOTExprKind::LOG_LE: return "LOG_LE";
        case AOTExprKind::LOG_GE: return "LOG_GE";
        case AOTExprKind::LOG_AND: return "LOG_AND";
        case AOTExprKind::LOG_OR: return "LOG_OR";
        case AOTExprKind::CALLREF_CALL: return "CALLREF_CALL";
        case AOTExprKind::LOG_NOT: return "LOG_NOT";
        case AOTExprKind::TRIM: return "TRIM";
        case AOTExprKind::CHOMP: return "CHOMP";
        case AOTExprKind::POP: return "POP";
        case AOTExprKind::SHIFT: return "SHIFT";
        case AOTExprKind::PUSH: return "PUSH";
        case AOTExprKind::UNSHIFT: return "UNSHIFT";
        case AOTExprKind::ELEMENTS: return "ELEMENTS";
        case AOTExprKind::DELETE: return "DELETE";
        case AOTExprKind::REMOVE: return "REMOVE";
        case AOTExprKind::BACKGROUND: return "BACKGROUND";
        case AOTExprKind::CONTEXT_REF: return "CONTEXT_REF";
        case AOTExprKind::CONTEXT_ROW: return "CONTEXT_ROW";
        case AOTExprKind::COMPLEX_CONTEXT_REF: return "COMPLEX_CONTEXT_REF";
        case AOTExprKind::NULL_COAL: return "NULL_COAL";
        case AOTExprKind::VALUE_COAL: return "VALUE_COAL";
        case AOTExprKind::QUESTION: return "QUESTION";
        case AOTExprKind::FOLDL: return "FOLDL";
        case AOTExprKind::FOLDR: return "FOLDR";
        case AOTExprKind::MAP: return "MAP";
        case AOTExprKind::MAP_SELECT: return "MAP_SELECT";
        case AOTExprKind::HASH_MAP_OP: return "HASH_MAP";
        case AOTExprKind::HASH_MAP_SELECT_OP: return "HASH_MAP_SELECT";
        case AOTExprKind::SELECT: return "SELECT";
        default: break;
    }
    return "UNKNOWN";
}

static const char* dump_aot_expr_node_kind_name(uint8_t kind) {
    const auto* info = getAOTExprNodeKindInfo(kind);
    return info && info->name ? info->name : "UNKNOWN";
}

static void dump_record_expr_tree_root(AOTSlotMapDumpSummary& summary, uint8_t root_kind,
        const std::string& func_name, bool top_level, const AOTDumpExprContext& ctx) {
    ++summary.expr_tree_root_kinds[root_kind];
    summary.expr_tree_details.push_back({
        func_name,
        root_kind,
        ctx.top_kind,
        ctx.container_kind,
        ctx.slot,
        top_level,
    });
}

static void dump_record_expr_kind(AOTSlotMapDumpSummary& summary, uint8_t kind,
        bool top_level, const std::string& func_name) {
    ++summary.all_expr_kinds[kind];
    if (top_level) {
        ++summary.top_expr_kinds[kind];
    }
    if (kind == static_cast<uint8_t>(AOTExprKind::EXPR_TREE)) {
        if (std::find(summary.expr_tree_functions.begin(), summary.expr_tree_functions.end(),
                func_name) == summary.expr_tree_functions.end()) {
            summary.expr_tree_functions.push_back(func_name);
        }
    } else if (kind == static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL)) {
        if (std::find(summary.generic_eval_functions.begin(), summary.generic_eval_functions.end(),
                func_name) == summary.generic_eval_functions.end()) {
            summary.generic_eval_functions.push_back(func_name);
        }
    }
}

static bool dump_skip_inline_expr(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, AOTSlotMapDumpSummary& summary,
        const std::string& func_name, const AOTDumpExprContext& ctx);

static bool dump_skip_inline_expr_list(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, uint8_t count,
        AOTSlotMapDumpSummary& summary, const std::string& func_name,
        const AOTDumpExprContext& ctx) {
    for (uint8_t i = 0; i < count; ++i) {
        if (!dump_skip_inline_expr(reader, p, end, summary, func_name, ctx)) {
            return false;
        }
    }
    return true;
}

static bool dump_skip_serialized_value_no_summary(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end);

static bool dump_skip_expr_payload(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, uint8_t kind, bool slot_form,
        AOTSlotMapDumpSummary& summary, const std::string& func_name,
        const AOTDumpExprContext& ctx) {
    AOTDumpExprContext child_ctx = ctx;
    child_ctx.container_kind = kind;
    auto skip_n_args = [&](uint8_t count) {
        return dump_skip_inline_expr_list(reader, p, end, count, summary, func_name,
            child_ctx);
    };

    switch (static_cast<AOTExprKind>(kind)) {
        case AOTExprKind::FUNC_CALL:
            if (!dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            if (!slot_form || (reader.getHeader().feature_flags & QORE_AOT_FEAT_FUNC_CALL_VARIANT) != 0) {
                if (!dump_skip_string_ref(reader, p, end)) {
                    return false;
                }
            }
            if (!slot_form && (reader.getHeader().feature_flags & QORE_AOT_FEAT_INLINE_CALL_ARGS) != 0) {
                uint8_t nargs = 0;
                return dump_read_u8(p, end, nargs) && skip_n_args(nargs);
            }
            return true;

        case AOTExprKind::SELF_METHOD_CALL:
            if (!dump_skip_string_ref(reader, p, end) || !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            if (!slot_form && (reader.getHeader().feature_flags & QORE_AOT_FEAT_SELF_CALL_ARGS) != 0) {
                uint8_t nargs = 0;
                return dump_read_u8(p, end, nargs) && skip_n_args(nargs);
            }
            return true;

        case AOTExprKind::STATIC_VARREF:
        case AOTExprKind::CONST_ENUM:
        case AOTExprKind::BOUND_METHOD_REF:
        case AOTExprKind::STATIC_METHOD_REF:
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_string_ref(reader, p, end);

        case AOTExprKind::STATIC_METHOD_CALL: {
            uint8_t nargs = 0;
            if (!dump_skip_string_ref(reader, p, end)
                    || !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE) != 0
                    && !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            return dump_read_u8(p, end, nargs) && skip_n_args(nargs);
        }

        case AOTExprKind::NEW_OBJECT:
        case AOTExprKind::SCOPED_NEW_OBJECT:
            if (slot_form) {
                if (!dump_skip_string_ref(reader, p, end)
                        || !dump_skip_string_ref(reader, p, end)) {
                    return false;
                }
                return (reader.getHeader().feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) == 0
                    || dump_skip_string_ref(reader, p, end);
            } else {
                uint8_t nargs = 0;
                return dump_skip_string_ref(reader, p, end)
                    && dump_read_u8(p, end, nargs)
                    && skip_n_args(nargs);
            }

        case AOTExprKind::CALLREF_CALL: {
            uint8_t nargs = 0;
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_read_u8(p, end, nargs)
                && skip_n_args(nargs);
        }

        case AOTExprKind::RUNTIME_CONST_REF:
        case AOTExprKind::SELF_VARREF:
        case AOTExprKind::LOCAL_VARREF:
        case AOTExprKind::GLOBAL_VARREF:
        case AOTExprKind::CONST_NUMBER:
        case AOTExprKind::CONST_BINARY:
        case AOTExprKind::CONST_STRING:
        case AOTExprKind::FUNC_CALL_REF:
        case AOTExprKind::SELF_METHOD_REF:
            return dump_skip_string_ref(reader, p, end);

        case AOTExprKind::PLUS:
        case AOTExprKind::SQUARE_BRACKET:
        case AOTExprKind::MINUS:
        case AOTExprKind::MULTIPLY:
        case AOTExprKind::DIVIDE:
        case AOTExprKind::MODULO:
        case AOTExprKind::BIT_AND:
        case AOTExprKind::BIT_OR:
        case AOTExprKind::BIT_XOR:
        case AOTExprKind::SHIFT_LEFT:
        case AOTExprKind::SHIFT_RIGHT:
        case AOTExprKind::NULL_COAL:
        case AOTExprKind::VALUE_COAL:
        case AOTExprKind::FOLDL:
        case AOTExprKind::FOLDR:
        case AOTExprKind::MAP:
        case AOTExprKind::SELECT:
        case AOTExprKind::LOG_AND:
        case AOTExprKind::LOG_OR:
        case AOTExprKind::RANGE:
        case AOTExprKind::ASSIGN:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::MAP_SELECT:
        case AOTExprKind::HASH_MAP_OP:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::HASH_MAP_SELECT_OP:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::EXISTS:
        case AOTExprKind::KEYS:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::IMPLICIT_ARG:
            return dump_skip_bytes(p, end, 8);

        case AOTExprKind::IMPLICIT_ELEM:
            return true;

        case AOTExprKind::INSTANCEOF:
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::REGEX_MATCH:
        case AOTExprKind::REGEX_NMATCH:
        case AOTExprKind::REGEX_EXTRACT:
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_bytes(p, end, 8)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::CONTEXT_REF:
            return dump_skip_string_ref(reader, p, end);

        case AOTExprKind::CONTEXT_ROW:
            return true;

        case AOTExprKind::COMPLEX_CONTEXT_REF:
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_string_ref(reader, p, end)
                && dump_skip_bytes(p, end, 8);

        case AOTExprKind::PRE_INC:
        case AOTExprKind::PRE_DEC:
        case AOTExprKind::POST_INC:
        case AOTExprKind::POST_DEC:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::LOG_EQ:
        case AOTExprKind::LOG_NE:
        case AOTExprKind::LOG_LT:
        case AOTExprKind::LOG_GT:
        case AOTExprKind::LOG_LE:
        case AOTExprKind::LOG_GE:
        case AOTExprKind::PUSH:
        case AOTExprKind::UNSHIFT:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::LOG_NOT:
        case AOTExprKind::TRIM:
        case AOTExprKind::CHOMP:
        case AOTExprKind::POP:
        case AOTExprKind::SHIFT:
        case AOTExprKind::ELEMENTS:
        case AOTExprKind::DELETE:
        case AOTExprKind::REMOVE:
        case AOTExprKind::BACKGROUND:
        case AOTExprKind::UNARY_MINUS:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::QUESTION:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::SQUARE_BRACKET_RANGE:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::CALL_REF:
        case AOTExprKind::OBJ_METHOD_REF:
        case AOTExprKind::CONST_NOTHING:
        case AOTExprKind::CONST_NULL:
        case AOTExprKind::GENERIC_EVAL:
            return true;

        case AOTExprKind::CONST_INT:
        case AOTExprKind::CONST_FLOAT:
            return slot_form ? dump_skip_string_ref(reader, p, end)
                : dump_skip_bytes(p, end, 8);

        case AOTExprKind::CONST_BOOL:
            return slot_form ? dump_skip_string_ref(reader, p, end)
                : dump_skip_bytes(p, end, 1);

        case AOTExprKind::CONST_VALUE: {
            return dump_skip_serialized_value_no_summary(reader, p, end);
        }

        case AOTExprKind::HASHDECL_NEW:
        case AOTExprKind::COMPLEX_HASH_NEW: {
            uint8_t nargs = 0;
            return dump_skip_string_ref(reader, p, end)
                && dump_read_u8(p, end, nargs)
                && skip_n_args(nargs);
        }

        case AOTExprKind::COMPLEX_LIST_NEW: {
            uint8_t has_arg = 0;
            if (!dump_skip_string_ref(reader, p, end)
                    || !dump_read_u8(p, end, has_arg)) {
                return false;
            }
            return !has_arg
                || dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);
        }

        case AOTExprKind::COMPLEX_BUFFER_NEW: {
            uint8_t has_arg = 0;
            if (!dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0
                    && !dump_skip_bytes(p, end, 1)) {
                return false;
            }
            if (!dump_read_u8(p, end, has_arg)) {
                return false;
            }
            return !has_arg
                || dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);
        }

        case AOTExprKind::HASH_LITERAL: {
            uint8_t count = 0;
            if (!dump_read_u8(p, end, count)) {
                return false;
            }
            for (uint8_t i = 0; i < count; ++i) {
                if (!dump_skip_string_ref(reader, p, end)
                        || !dump_skip_inline_expr(reader, p, end, summary, func_name,
                            child_ctx)) {
                    return false;
                }
            }
            return true;
        }

        case AOTExprKind::PARSE_HASH: {
            uint8_t count = 0;
            if (!dump_read_u8(p, end, count)) {
                return false;
            }
            for (uint8_t i = 0; i < count; ++i) {
                if (!dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                        || !dump_skip_inline_expr(reader, p, end, summary, func_name,
                            child_ctx)) {
                    return false;
                }
            }
            return true;
        }

        case AOTExprKind::LIST_LITERAL: {
            uint8_t count = 0;
            return dump_read_u8(p, end, count) && skip_n_args(count);
        }

        case AOTExprKind::HASH_DEREF:
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_HASH_DEREF_TYPEINFO) != 0
                    && !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::PARSE_REF:
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_PARSE_REF_TYPE) != 0
                    && !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::CAST_HASHDECL:
        case AOTExprKind::CAST_COMPLEX_HASH:
        case AOTExprKind::CAST_COMPLEX_LIST:
        case AOTExprKind::CAST_CLASS:
        case AOTExprKind::CAST_ENUM:
        case AOTExprKind::CAST_SCALAR: {
            if (!dump_skip_string_ref(reader, p, end) || !dump_skip_bytes(p, end, 1)) {
                return false;
            }
            if (slot_form) {
                return true;
            }
            uint8_t has_inner = 0;
            if (!dump_read_u8(p, end, has_inner)) {
                return false;
            }
            return !has_inner
                || dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);
        }

        case AOTExprKind::DOT_EVAL_TARGET: {
            if (!dump_skip_string_ref(reader, p, end)
                    || !dump_skip_string_ref(reader, p, end)
                    || !dump_skip_bytes(p, end, 1)) {
                return false;
            }
            if (!slot_form) {
                uint8_t nargs = 0;
                return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                    && dump_read_u8(p, end, nargs)
                    && skip_n_args(nargs);
            }
            return true;
        }

        case AOTExprKind::OBJ_METHOD_REF_EXPR:
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::DOT_EVAL_EXPR:
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::CLOSURE_CREATE: {
            if (!dump_skip_string_ref(reader, p, end)
                    || !dump_skip_string_ref(reader, p, end)
                    || !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            uint16_t nparams = 0;
            if (!dump_read_u16(p, end, nparams)) {
                return false;
            }
            for (uint16_t i = 0; i < nparams; ++i) {
                if (!dump_skip_string_ref(reader, p, end)
                        || !dump_skip_string_ref(reader, p, end)) {
                    return false;
                }
                uint8_t has_default = 0;
                if (!dump_read_u8(p, end, has_default)) {
                    return false;
                }
                if (has_default && !dump_skip_serialized_value_no_summary(reader, p, end)) {
                    return false;
                }
            }
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS) != 0) {
                if (!dump_skip_bytes(p, end, 2)) {
                    return false;
                }
            } else {
                if (!dump_skip_bytes(p, end, 1)) {
                    return false;
                }
            }
            uint16_t ncaptured = 0;
            if (!dump_read_u16(p, end, ncaptured)) {
                return false;
            }
            for (uint16_t i = 0; i < ncaptured; ++i) {
                if (!dump_skip_string_ref(reader, p, end) || !dump_skip_bytes(p, end, 4)) {
                    return false;
                }
            }
            uint8_t has_ir = 0;
            if (!dump_read_u8(p, end, has_ir)) {
                return false;
            }
            if (has_ir) {
                uint32_t ir_size = 0;
                if (!dump_read_u32(p, end, ir_size) || !dump_skip_bytes(p, end, ir_size)) {
                    return false;
                }
            }
            return true;
        }

        case AOTExprKind::EXPR_TREE: {
            uint32_t blob_size = 0;
            if (!dump_read_u32(p, end, blob_size)) {
                return false;
            }
            if (blob_size > 0 && dump_need(p, end, blob_size)) {
                dump_record_expr_tree_root(summary, *p, func_name, slot_form, ctx);
            }
            return dump_skip_bytes(p, end, blob_size);
        }
    }

    return false;
}

static bool dump_skip_inline_expr(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, AOTSlotMapDumpSummary& summary,
        const std::string& func_name, const AOTDumpExprContext& ctx) {
    uint8_t kind = 0;
    if (!dump_read_u8(p, end, kind)) {
        return false;
    }
    dump_record_expr_kind(summary, kind, false, func_name);
    return dump_skip_expr_payload(reader, p, end, kind, false, summary, func_name, ctx);
}

static bool summarize_aot_slot_maps(const QoreAOTBinaryReader& reader,
        AOTSlotMapDumpSummary& summary, std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    if (!sec) {
        return true;
    }
    const uint8_t* p = reader.getSectionData(*sec);
    if (!p) {
        error = "invalid SLOT_MAPS section";
        return false;
    }
    const uint8_t* end = p + sec->size;
    uint32_t num_funcs = 0;
    if (!dump_read_u32(p, end, num_funcs)) {
        error = "truncated SLOT_MAPS header";
        return false;
    }
    summary.functions = num_funcs;

    for (uint32_t f = 0; f < num_funcs; ++f) {
        const uint8_t* entry_start = p;
        uint32_t entry_size = 0;
        if (!dump_read_u32(p, end, entry_size)) {
            error = "truncated SLOT_MAPS entry header";
            return false;
        }
        if (static_cast<size_t>(end - p) < entry_size) {
            error = "SLOT_MAPS entry overruns section";
            return false;
        }
        const uint8_t* entry_end = p + entry_size;
        const char* fname = nullptr;
        if (!dump_skip_string_ref(reader, p, entry_end, &fname)) {
            ++summary.malformed_entries;
            p = entry_start + 4 + entry_size;
            continue;
        }
        std::string func_name = fname && *fname ? fname : "(unnamed)";

        uint16_t num_locals = 0, num_globals = 0, num_exprs = 0, num_stmts = 0;
        uint16_t num_regex = 0, num_body_locals = 0;
        uint8_t has_unsupported = 0, num_lv_paths = 0;
        if (!dump_read_u16(p, entry_end, num_locals)
                || !dump_read_u16(p, entry_end, num_globals)
                || !dump_read_u16(p, entry_end, num_exprs)
                || !dump_read_u16(p, entry_end, num_stmts)
                || !dump_read_u16(p, entry_end, num_regex)
                || !dump_read_u16(p, entry_end, num_body_locals)
                || !dump_read_u8(p, entry_end, has_unsupported)
                || !dump_read_u8(p, entry_end, num_lv_paths)) {
            ++summary.malformed_entries;
            p = entry_start + 4 + entry_size;
            continue;
        }

        summary.local_slots += num_locals;
        summary.global_slots += num_globals;
        summary.expr_slots += num_exprs;
        summary.stmt_slots += num_stmts;
        summary.regex_slots += num_regex;
        summary.body_locals += num_body_locals;
        summary.lv_path_slots += num_lv_paths;
        if (has_unsupported) {
            ++summary.unsupported_functions;
        }

        const size_t local_slot_extra_bytes =
            (reader.getHeader().feature_flags & QORE_AOT_FEAT_LOCAL_DECL_ORDINAL) != 0 ? 7 : 3;
        const size_t global_slot_extra_bytes =
            (reader.getHeader().feature_flags & QORE_AOT_FEAT_GLOBAL_SLOT_FLAGS) != 0 ? 2 : 1;
        bool malformed = false;
        for (uint16_t i = 0; i < num_locals && !malformed; ++i) {
            malformed = !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_bytes(p, entry_end, local_slot_extra_bytes);
        }
        for (uint16_t i = 0; i < num_globals && !malformed; ++i) {
            malformed = !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_bytes(p, entry_end, global_slot_extra_bytes);
        }
        for (uint16_t i = 0; i < num_exprs && !malformed; ++i) {
            uint8_t kind = 0;
            if (!dump_read_u8(p, entry_end, kind)) {
                malformed = true;
                break;
            }
            dump_record_expr_kind(summary, kind, true, func_name);
            AOTDumpExprContext ctx;
            ctx.top_kind = kind;
            ctx.container_kind = kind;
            ctx.slot = i;
            if (!dump_skip_expr_payload(reader, p, entry_end, kind, true, summary, func_name, ctx)) {
                malformed = true;
                break;
            }
        }
        if (malformed) {
            ++summary.malformed_entries;
        }
        p = entry_start + 4 + entry_size;
    }

    return true;
}

static void print_aot_slot_map_summary(const QoreAOTBinaryReader& reader) {
    AOTSlotMapDumpSummary summary;
    std::string error;
    if (!summarize_aot_slot_maps(reader, summary, error)) {
        printf("    slot maps: error: %s\n", error.c_str());
        return;
    }
    if (!summary.functions) {
        printf("    slot maps: (none)\n");
        return;
    }

    uint64_t top_expr_tree = summary.top_expr_kinds[static_cast<uint8_t>(AOTExprKind::EXPR_TREE)];
    uint64_t all_expr_tree = summary.all_expr_kinds[static_cast<uint8_t>(AOTExprKind::EXPR_TREE)];
    uint64_t top_generic = summary.top_expr_kinds[static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL)];
    uint64_t all_generic = summary.all_expr_kinds[static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL)];

    printf("    slot maps: functions=%u locals=%llu globals=%llu expr-slots=%llu stmts=%llu "
           "regex=%llu body-locals=%llu lvpaths=%llu unsupported-functions=%u malformed=%u\n",
        summary.functions,
        static_cast<unsigned long long>(summary.local_slots),
        static_cast<unsigned long long>(summary.global_slots),
        static_cast<unsigned long long>(summary.expr_slots),
        static_cast<unsigned long long>(summary.stmt_slots),
        static_cast<unsigned long long>(summary.regex_slots),
        static_cast<unsigned long long>(summary.body_locals),
        static_cast<unsigned long long>(summary.lv_path_slots),
        summary.unsupported_functions,
        summary.malformed_entries);
    printf("      fallback exprs: EXPR_TREE=%llu top/%llu total, GENERIC_EVAL=%llu top/%llu total\n",
        static_cast<unsigned long long>(top_expr_tree),
        static_cast<unsigned long long>(all_expr_tree),
        static_cast<unsigned long long>(top_generic),
        static_cast<unsigned long long>(all_generic));

    auto print_limited_names = [](const char* label, const std::vector<std::string>& values) {
        if (values.empty()) {
            return;
        }
        printf("      %s:", label);
        size_t limit = std::min<size_t>(values.size(), 8);
        for (size_t i = 0; i < limit; ++i) {
            printf("%s%s", i ? ", " : " ", values[i].c_str());
        }
        if (values.size() > limit) {
            printf(", ... +%zu", values.size() - limit);
        }
        printf("\n");
    };
    print_limited_names("EXPR_TREE functions", summary.expr_tree_functions);
    print_limited_names("GENERIC_EVAL functions", summary.generic_eval_functions);

    if (!summary.expr_tree_root_kinds.empty()) {
        printf("      EXPR_TREE root nodes:\n");
        for (const auto& [kind, count] : summary.expr_tree_root_kinds) {
            printf("        %-20s (%3u): %llu\n", dump_aot_expr_node_kind_name(kind), kind,
                static_cast<unsigned long long>(count));
        }
    }

    if (dump_sections && !summary.top_expr_kinds.empty()) {
        if (!summary.expr_tree_details.empty()) {
            printf("      EXPR_TREE details:\n");
            for (const auto& detail : summary.expr_tree_details) {
                printf("        %s %-20s (%3u) top=%s slot=%u container=%s: %s\n",
                    detail.top_level ? "top   " : "nested",
                    dump_aot_expr_node_kind_name(detail.root_kind),
                    detail.root_kind,
                    dump_aot_expr_kind_name(detail.top_kind),
                    detail.slot == 0xffff ? 0 : detail.slot,
                    dump_aot_expr_kind_name(detail.container_kind),
                detail.function.c_str());
            }
        }
        printf("      top expr slot kinds:\n");
        for (const auto& [kind, count] : summary.top_expr_kinds) {
            printf("        %-20s (%3u): %llu\n", dump_aot_expr_kind_name(kind), kind,
                static_cast<unsigned long long>(count));
        }
    }
    if (dump_sections && summary.all_expr_kinds.size() != summary.top_expr_kinds.size()) {
        printf("      all encoded expr kinds:\n");
        for (const auto& [kind, count] : summary.all_expr_kinds) {
            printf("        %-20s (%3u): %llu\n", dump_aot_expr_kind_name(kind), kind,
                static_cast<unsigned long long>(count));
        }
    }
}

static const char* dump_aot_value_tag_name(uint8_t tag) {
    switch (static_cast<QoreAOTValueTag>(tag)) {
        case QoreAOTValueTag::VT_NOTHING: return "NOTHING";
        case QoreAOTValueTag::VT_NULL: return "NULL";
        case QoreAOTValueTag::VT_BOOL: return "BOOL";
        case QoreAOTValueTag::VT_INT64: return "INT64";
        case QoreAOTValueTag::VT_FLOAT64: return "FLOAT64";
        case QoreAOTValueTag::VT_STRING: return "STRING";
        case QoreAOTValueTag::VT_ABS_DATE: return "ABS_DATE";
        case QoreAOTValueTag::VT_REL_DATE: return "REL_DATE";
        case QoreAOTValueTag::VT_LIST: return "LIST";
        case QoreAOTValueTag::VT_HASH: return "HASH";
        case QoreAOTValueTag::VT_NUMBER: return "NUMBER";
        case QoreAOTValueTag::VT_BINARY: return "BINARY";
        case QoreAOTValueTag::VT_OPAQUE_DEFAULT: return "OPAQUE_DEFAULT";
        case QoreAOTValueTag::VT_ABS_DATE_REGION: return "ABS_DATE_REGION";
        case QoreAOTValueTag::VT_ENUM: return "ENUM";
        case QoreAOTValueTag::VT_NEW_OBJECT: return "NEW_OBJECT";
        case QoreAOTValueTag::VT_CONST_REF: return "CONST_REF";
        case QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT: return "NEW_COMPLEX_DEFAULT";
        case QoreAOTValueTag::VT_EXPR_TREE: return "EXPR_TREE";
        case QoreAOTValueTag::VT_EXPR_NATIVE: return "EXPR_NATIVE";
        case QoreAOTValueTag::VT_PLUGIN_INSTANCE: return "PLUGIN_INSTANCE";
        case QoreAOTValueTag::VT_CHAR: return "CHAR";
    }
    return "UNKNOWN";
}

struct AOTDefaultDumpSummary {
    struct Detail {
        std::string category;
        std::string owner;
        std::string name;
        uint8_t tag = 0;
        uint8_t root_kind = 0;
        uint32_t blob_size = 0;
    };

    uint64_t defaults = 0;
    std::map<uint8_t, uint64_t> top_value_tags;
    std::map<uint8_t, uint64_t> all_value_tags;
    std::vector<Detail> expr_tree_details;
};

static bool dump_skip_serialized_value(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, AOTDefaultDumpSummary& summary,
        std::string& error, bool top_level, const char* category,
        const std::string& owner, const std::string& name);

static bool dump_skip_serialized_value_args(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, uint32_t count,
        AOTDefaultDumpSummary& summary, std::string& error) {
    for (uint32_t i = 0; i < count; ++i) {
        if (!dump_skip_serialized_value(reader, p, end, summary, error, false,
                "", "", "")) {
            return false;
        }
    }
    return true;
}

static bool dump_skip_container_value_type(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end) {
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPED_VALUE_CONTAINERS) == 0) {
        return true;
    }
    uint8_t kind = 0;
    if (!dump_read_u8(p, end, kind)) {
        return false;
    }
    return kind == static_cast<uint8_t>(QoreAOTContainerValueType::Plain)
        || dump_skip_len_string_ref(reader, p, end);
}

static bool dump_skip_serialized_value(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end, AOTDefaultDumpSummary& summary,
        std::string& error, bool top_level, const char* category,
        const std::string& owner, const std::string& name) {
    uint8_t tag = 0;
    if (!dump_read_u8(p, end, tag)) {
        error = "truncated serialized value tag";
        return false;
    }

    ++summary.all_value_tags[tag];
    if (top_level) {
        ++summary.defaults;
        ++summary.top_value_tags[tag];
    }

    auto fail = [&](const char* msg) {
        error = msg;
        return false;
    };

    switch (static_cast<QoreAOTValueTag>(tag)) {
        case QoreAOTValueTag::VT_NOTHING:
        case QoreAOTValueTag::VT_NULL:
        case QoreAOTValueTag::VT_OPAQUE_DEFAULT:
            return true;

        case QoreAOTValueTag::VT_BOOL:
            return dump_skip_bytes(p, end, 1) || fail("truncated bool value");

        case QoreAOTValueTag::VT_CHAR:
            return dump_skip_bytes(p, end, 4) || fail("truncated char value");

        case QoreAOTValueTag::VT_INT64:
        case QoreAOTValueTag::VT_FLOAT64:
            return dump_skip_bytes(p, end, 8) || fail("truncated 64-bit value");

        case QoreAOTValueTag::VT_STRING:
        case QoreAOTValueTag::VT_NUMBER:
        case QoreAOTValueTag::VT_CONST_REF:
            return dump_skip_len_string_ref(reader, p, end)
                || fail("truncated string reference value");

        case QoreAOTValueTag::VT_ABS_DATE:
            return dump_skip_bytes(p, end, 16) || fail("truncated absolute date value");

        case QoreAOTValueTag::VT_REL_DATE:
            return dump_skip_bytes(p, end, 56) || fail("truncated relative date value");

        case QoreAOTValueTag::VT_ABS_DATE_REGION:
            return (dump_skip_bytes(p, end, 8) && dump_skip_len_string_ref(reader, p, end))
                || fail("truncated regional absolute date value");

        case QoreAOTValueTag::VT_BINARY: {
            uint32_t size = 0;
            return (dump_read_u32(p, end, size) && dump_skip_bytes(p, end, size))
                || fail("truncated binary value");
        }

        case QoreAOTValueTag::VT_PLUGIN_INSTANCE: {
            if (!dump_skip_bytes(p, end, 8)) {
                return fail("truncated plugin value instance header");
            }
            uint32_t payload_size = 0;
            return (dump_read_u32(p, end, payload_size)
                    && dump_skip_bytes(p, end, payload_size))
                || fail("truncated plugin value payload");
        }

        case QoreAOTValueTag::VT_LIST: {
            uint32_t count = 0;
            return dump_skip_container_value_type(reader, p, end)
                && dump_read_u32(p, end, count)
                && dump_skip_serialized_value_args(reader, p, end, count, summary, error);
        }

        case QoreAOTValueTag::VT_HASH: {
            uint32_t count = 0;
            if (!dump_skip_container_value_type(reader, p, end)
                    || !dump_read_u32(p, end, count)) {
                return fail("truncated hash value");
            }
            for (uint32_t i = 0; i < count; ++i) {
                if (!dump_skip_string_ref(reader, p, end)
                        || !dump_skip_serialized_value(reader, p, end, summary, error,
                            false, "", "", "")) {
                    if (error.empty()) {
                        error = "truncated hash entry";
                    }
                    return false;
                }
            }
            return true;
        }

        case QoreAOTValueTag::VT_ENUM:
            return (dump_skip_len_string_ref(reader, p, end)
                    && dump_skip_len_string_ref(reader, p, end))
                || fail("truncated enum value");

        case QoreAOTValueTag::VT_NEW_OBJECT: {
            uint32_t nargs = 0;
            return dump_skip_len_string_ref(reader, p, end)
                && dump_read_u32(p, end, nargs)
                && dump_skip_serialized_value_args(reader, p, end, nargs, summary, error);
        }

        case QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT: {
            uint32_t nargs = 0;
            uint8_t kind = 0;
            return dump_read_u8(p, end, kind)
                && (kind != 3
                    || (reader.getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) == 0
                    || dump_skip_bytes(p, end, 1))
                && dump_skip_len_string_ref(reader, p, end)
                && dump_read_u32(p, end, nargs)
                && dump_skip_serialized_value_args(reader, p, end, nargs, summary, error);
        }

        case QoreAOTValueTag::VT_EXPR_TREE: {
            uint32_t blob_size = 0;
            if (!dump_read_u32(p, end, blob_size)) {
                return fail("truncated expression tree size");
            }
            if (!dump_need(p, end, blob_size)) {
                return fail("expression tree blob exceeds section bounds");
            }
            if (top_level) {
                summary.expr_tree_details.push_back({
                    category ? category : "",
                    owner,
                    name,
                    tag,
                    blob_size ? static_cast<uint8_t>(*p) : static_cast<uint8_t>(0),
                    blob_size,
                });
            }
            p += blob_size;
            return true;
        }

        case QoreAOTValueTag::VT_EXPR_NATIVE: {
            uint32_t blob_size = 0;
            if (!dump_read_u32(p, end, blob_size)) {
                return fail("truncated native expression size");
            }
            if (!dump_need(p, end, blob_size)) {
                return fail("native expression blob exceeds section bounds");
            }
            const uint8_t* blob_end = p + blob_size;
            AOTSlotMapDumpSummary expr_summary;
            AOTDumpExprContext expr_ctx;
            bool ok = dump_skip_inline_expr(reader, p, blob_end, expr_summary,
                "default", expr_ctx);
            if (!ok || p != blob_end) {
                p = blob_end;
                return fail("malformed native default expression");
            }
            return true;
        }
    }

    error = "unknown serialized value tag " + std::to_string(static_cast<unsigned>(tag));
    return false;
}

static bool dump_skip_serialized_value_no_summary(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end) {
    AOTDefaultDumpSummary summary;
    std::string error;
    return dump_skip_serialized_value(reader, p, end, summary, error, false,
        "", "", "");
}

static std::string dump_owner_name(const char* preferred, const char* fallback) {
    if (preferred && *preferred) {
        return preferred;
    }
    if (fallback && *fallback) {
        return fallback;
    }
    return "(unnamed)";
}

static bool dump_scan_class_defaults(const QoreAOTBinaryReader& reader,
        AOTDefaultDumpSummary& summary, std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CLASSES);
    if (!sec) {
        return true;
    }
    const uint8_t* p = reader.getSectionData(*sec);
    if (!p) {
        error = "invalid CLASSES section";
        return false;
    }
    const uint8_t* end = p + sec->size;

    uint32_t count = 0;
    if (!dump_read_u32(p, end, count)) {
        error = "truncated CLASSES header";
        return false;
    }
    const bool has_const_pending_flag =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CONST_PENDING) != 0;
    const bool has_class_hash =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_HASH) != 0;
    const bool has_class_injection =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_INJECTION) != 0;
    const bool has_class_type_params =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_TYPE_PARAMS) != 0;
    const bool has_type_param_defaults =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0;
    const bool has_type_param_bounds =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0;
    const bool has_class_param_bases =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_CLASS_PARAM_BASES) != 0;

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = nullptr;
        const char* path = nullptr;
        if (!dump_skip_string_ref(reader, p, end, &name)
                || !dump_skip_string_ref(reader, p, end, &path)
                || !dump_skip_bytes(p, end, 4 + 2 + 8)) {
            error = "truncated class header";
            return false;
        }
        std::string owner = dump_owner_name(path, name);
        if (has_class_hash && !dump_skip_bytes(p, end, 1 + SH_SIZE)) {
            error = "truncated class signature hash";
            return false;
        }
        if (has_class_injection && !dump_skip_string_ref(reader, p, end)) {
            error = "truncated class injection target";
            return false;
        }
        if (has_class_type_params) {
            uint32_t type_param_count = 0;
            if (!dump_read_u32(p, end, type_param_count)) {
                error = "truncated class type parameter count";
                return false;
            }
            for (uint32_t j = 0; j < type_param_count; ++j) {
                if (j && !(j % 100) && qcc_check_cancel("AOT class type parameter dump")) {
                    error = "operation cancelled during AOT class type parameter dump";
                    return false;
                }
                if (!dump_skip_string_ref(reader, p, end)) {
                    error = "truncated class type parameter entry";
                    return false;
                }
                if (has_type_param_defaults) {
                    uint8_t has_default = 0;
                    if (!dump_read_u8(p, end, has_default)
                            || (has_default && !dump_skip_string_ref(reader, p, end))) {
                        error = "truncated class type parameter default";
                        return false;
                    }
                }
                if (has_type_param_bounds) {
                    uint8_t has_bound = 0;
                    if (!dump_read_u8(p, end, has_bound)
                            || (has_bound && !dump_skip_string_ref(reader, p, end))) {
                        error = "truncated class type parameter bound";
                        return false;
                    }
                }
            }
        }

        uint32_t num_bases = 0;
        if (!dump_read_u32(p, end, num_bases)) {
            error = "truncated class base count";
            return false;
        }
        for (uint32_t j = 0; j < num_bases; ++j) {
            if (!dump_skip_string_ref(reader, p, end) || !dump_skip_bytes(p, end, 2)) {
                error = "truncated class base entry";
                return false;
            }
            if (has_class_param_bases && !dump_skip_string_ref(reader, p, end)) {
                error = "truncated class parameterized base type";
                return false;
            }
        }

        uint32_t num_members = 0;
        if (!dump_read_u32(p, end, num_members)) {
            error = "truncated class member count";
            return false;
        }
        for (uint32_t j = 0; j < num_members; ++j) {
            const char* mname = nullptr;
            if (!dump_skip_string_ref(reader, p, end, &mname)
                    || !dump_skip_string_ref(reader, p, end)
                    || !dump_skip_bytes(p, end, 3)) {
                error = "truncated class member entry";
                return false;
            }
            uint8_t has_default = *(p - 1);
            if (has_default && !dump_skip_serialized_value(reader, p, end, summary,
                    error, true, "instance member", owner, dump_owner_name(mname, nullptr))) {
                return false;
            }
        }

        uint32_t num_static = 0;
        if (!dump_read_u32(p, end, num_static)) {
            error = "truncated static member count";
            return false;
        }
        for (uint32_t j = 0; j < num_static; ++j) {
            const char* mname = nullptr;
            if (!dump_skip_string_ref(reader, p, end, &mname)
                    || !dump_skip_string_ref(reader, p, end)
                    || !dump_skip_bytes(p, end, 2)) {
                error = "truncated static member entry";
                return false;
            }
            uint8_t has_default = *(p - 1);
            if (has_default && !dump_skip_serialized_value(reader, p, end, summary,
                    error, true, "static member", owner, dump_owner_name(mname, nullptr))) {
                return false;
            }
        }

        uint32_t num_consts = 0;
        if (!dump_read_u32(p, end, num_consts)) {
            error = "truncated class constant count";
            return false;
        }
        for (uint32_t j = 0; j < num_consts; ++j) {
            const char* cname = nullptr;
            if (!dump_skip_string_ref(reader, p, end, &cname)
                    || !dump_skip_string_ref(reader, p, end)
                    || !dump_skip_bytes(p, end, has_const_pending_flag ? 2 : 1)) {
                error = "truncated class constant entry";
                return false;
            }
            if (!dump_skip_serialized_value(reader, p, end, summary, error, true,
                    "class constant", owner, dump_owner_name(cname, nullptr))) {
                return false;
            }
        }
    }

    return true;
}

static bool dump_scan_hashdecl_defaults(const QoreAOTBinaryReader& reader,
        AOTDefaultDumpSummary& summary, std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::HASHDECLS);
    if (!sec) {
        return true;
    }
    const uint8_t* p = reader.getSectionData(*sec);
    if (!p) {
        error = "invalid HASHDECLS section";
        return false;
    }
    const uint8_t* end = p + sec->size;

    uint32_t count = 0;
    if (!dump_read_u32(p, end, count)) {
        error = "truncated HASHDECLS header";
        return false;
    }
    const bool has_hashdecl_type_params =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_HASHDECL_TYPE_PARAMS) != 0;
    const bool has_type_param_defaults =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_DEFAULTS) != 0;
    const bool has_type_param_bounds =
        (reader.getHeader().feature_flags & QORE_AOT_FEAT_TYPE_PARAM_BOUNDS) != 0;

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = nullptr;
        const char* path = nullptr;
        if (!dump_skip_string_ref(reader, p, end, &name)
                || !dump_skip_string_ref(reader, p, end, &path)
                || !dump_skip_bytes(p, end, 4 + 2)
                || !dump_skip_string_ref(reader, p, end)) {
            error = "truncated hashdecl header";
            return false;
        }
        std::string owner = dump_owner_name(path, name);

        if (has_hashdecl_type_params) {
            uint16_t type_param_count = 0;
            if (!dump_read_u16(p, end, type_param_count)) {
                error = "truncated hashdecl type parameter count";
                return false;
            }
            for (uint16_t j = 0; j < type_param_count; ++j) {
                if (j && !(j % 100) && qcc_check_cancel("AOT hashdecl type parameter dump")) {
                    error = "operation cancelled during AOT hashdecl type parameter dump";
                    return false;
                }
                if (!dump_skip_string_ref(reader, p, end)) {
                    error = "truncated hashdecl type parameter entry";
                    return false;
                }
                if (has_type_param_defaults) {
                    uint8_t has_default = 0;
                    if (!dump_read_u8(p, end, has_default)
                            || (has_default && !dump_skip_string_ref(reader, p, end))) {
                        error = "truncated hashdecl type parameter default";
                        return false;
                    }
                }
                if (has_type_param_bounds) {
                    uint8_t has_bound = 0;
                    if (!dump_read_u8(p, end, has_bound)
                            || (has_bound && !dump_skip_string_ref(reader, p, end))) {
                        error = "truncated hashdecl type parameter bound";
                        return false;
                    }
                }
            }
        }

        uint32_t num_members = 0;
        if (!dump_read_u32(p, end, num_members)) {
            error = "truncated hashdecl member count";
            return false;
        }
        for (uint32_t j = 0; j < num_members; ++j) {
            const char* mname = nullptr;
            if (!dump_skip_string_ref(reader, p, end, &mname)
                    || !dump_skip_string_ref(reader, p, end)) {
                error = "truncated hashdecl member entry";
                return false;
            }
            uint8_t has_default = 0;
            if (!dump_read_u8(p, end, has_default)) {
                error = "truncated hashdecl member default flag";
                return false;
            }
            if (has_default && !dump_skip_serialized_value(reader, p, end, summary,
                    error, true, "hashdecl member", owner, dump_owner_name(mname, nullptr))) {
                return false;
            }
        }
    }

    return true;
}

static bool dump_scan_constant_defaults(const QoreAOTBinaryReader& reader,
        AOTDefaultDumpSummary& summary, std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CONSTANTS);
    if (!sec) {
        return true;
    }
    const uint8_t* p = reader.getSectionData(*sec);
    if (!p) {
        error = "invalid CONSTANTS section";
        return false;
    }
    const uint8_t* end = p + sec->size;

    uint32_t count = 0;
    if (!dump_read_u32(p, end, count)) {
        error = "truncated CONSTANTS header";
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = nullptr;
        if (!dump_skip_string_ref(reader, p, end, &name)
                || !dump_skip_string_ref(reader, p, end)
                || !dump_skip_bytes(p, end, 4 + 3)) {
            error = "truncated constant entry";
            return false;
        }
        if (!dump_skip_serialized_value(reader, p, end, summary, error, true,
                "constant", "", dump_owner_name(name, nullptr))) {
            return false;
        }
    }

    return true;
}

static void print_aot_default_summary(const QoreAOTBinaryReader& reader) {
    AOTDefaultDumpSummary summary;
    std::string error;
    if (!dump_scan_class_defaults(reader, summary, error)
            || !dump_scan_hashdecl_defaults(reader, summary, error)
            || !dump_scan_constant_defaults(reader, summary, error)) {
        printf("    serialized defaults: error: %s\n", error.c_str());
        return;
    }

    if (!summary.defaults) {
        printf("    serialized defaults: (none)\n");
        return;
    }

    auto count_tag = [&](QoreAOTValueTag tag) -> uint64_t {
        auto i = summary.top_value_tags.find(static_cast<uint8_t>(tag));
        return i == summary.top_value_tags.end() ? 0 : i->second;
    };

    printf("    serialized defaults: total=%llu expr-tree=%llu native-expr=%llu "
           "new-object=%llu complex-default=%llu enum=%llu const-ref=%llu\n",
        static_cast<unsigned long long>(summary.defaults),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_EXPR_TREE)),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_EXPR_NATIVE)),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_NEW_OBJECT)),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_NEW_COMPLEX_DEFAULT)),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_ENUM)),
        static_cast<unsigned long long>(count_tag(QoreAOTValueTag::VT_CONST_REF)));

    if (dump_sections) {
        printf("      top serialized default tags:\n");
        for (const auto& [tag, count] : summary.top_value_tags) {
            printf("        %-20s (%3u): %llu\n", dump_aot_value_tag_name(tag), tag,
                static_cast<unsigned long long>(count));
        }
    }

    if (!summary.expr_tree_details.empty()) {
        printf("      EXPR_TREE defaults:\n");
        size_t limit = dump_sections ? summary.expr_tree_details.size()
            : std::min<size_t>(summary.expr_tree_details.size(), 8);
        for (size_t i = 0; i < limit; ++i) {
            const auto& detail = summary.expr_tree_details[i];
            printf("        %s %s::%s root=%s (%u) size=%u\n",
                detail.category.c_str(),
                detail.owner.c_str(),
                detail.name.c_str(),
                dump_aot_expr_node_kind_name(detail.root_kind),
                detail.root_kind,
                detail.blob_size);
        }
        if (summary.expr_tree_details.size() > limit) {
            printf("        ... +%zu\n", summary.expr_tree_details.size() - limit);
        }
    }
}

static void print_aot_symbol_record(const QoreAOTSymbolIndexRecord& rec, bool native_record = false) {
    printf("      %-13s %s", qoreAOTSymbolKindName(rec.kind),
        rec.qore_path.empty() ? "(none)" : rec.qore_path.c_str());
    if (!rec.visibility.empty()) {
        printf(" visibility=%s", rec.visibility.c_str());
    }
    if (!rec.source_file.empty()) {
        printf(" source=%s", rec.source_file.c_str());
    }
    if (!rec.signature_hash.empty()) {
        printf(" sig=%s", rec.signature_hash.c_str());
    }
    if (!rec.declaration_hash.empty()) {
        printf(" decl=%s", rec.declaration_hash.c_str());
    }
    if (!rec.value_hash.empty()) {
        printf(" value=%s", rec.value_hash.c_str());
    }
    if (!rec.native_symbol.empty()) {
        if (native_record) {
            printf(" %s->%s",
                (rec.flags & QORE_AOT_SYMBOL_FLAG_NATIVE_DEFINED) ? "defined" : "undefined",
                rec.native_symbol.c_str());
        } else {
            printf(" native=%s", rec.native_symbol.c_str());
        }
    }
    if (!rec.abi_kind.empty()) {
        printf(" abi=%s", rec.abi_kind.c_str());
    }
    if (rec.fast_entry_flags) {
        printf(" fast_flags=0x%x fast_params=%u fast_return=%u", rec.fast_entry_flags,
            rec.fast_entry_num_params, rec.fast_return_kind);
        if (rec.scalar_leaf_kind) {
            printf(" scalar_leaf=%u:%u", rec.scalar_leaf_kind, rec.scalar_leaf_opcode);
        }
    }
    if (rec.dependency_class != QoreAOTDependencyClass::UNKNOWN) {
        printf(" dep=%s", qoreAOTDependencyClassName(rec.dependency_class));
    }
    if (!rec.consumer_source_file.empty()) {
        printf(" consumer=%s", rec.consumer_source_file.c_str());
    }
    if (!rec.provider_source_file.empty()) {
        printf(" provider=%s", rec.provider_source_file.c_str());
    }
    if (rec.metadata_slot != UINT32_MAX) {
        printf(" slot=%u", rec.metadata_slot);
    }
    printf("\n");
}

static bool dump_aot_symbol_index_check_cancel(size_t ordinal, const char* operation) {
    if (ordinal && !(ordinal % 100) && qcc_check_cancel(operation)) {
        printf("      cancelled during %s\n", operation ? operation : "AOT symbol-index dump");
        return false;
    }
    return true;
}

static void print_aot_symbol_index(const QoreAOTBinaryReader& reader) {
    QoreAOTSymbolIndex index;
    std::string error;
    if (!readSymbolIndex(reader, index, error)) {
        printf("    symbol index: error: %s\n", error.c_str());
        return;
    }

    if (!index.version) {
        printf("    symbol index: not present\n");
        return;
    }

    printf("    symbol index: version=%u defined=%zu imported=%zu native=%zu\n",
        index.version, index.defined.size(), index.imported.size(), index.native.size());
    if (!dump_symbols) {
        return;
    }

    if (!index.context.empty()) {
        printf("    AOT symbol context:\n");
        for (size_t i = 0; i < index.context.size(); ++i) {
            if (!dump_aot_symbol_index_check_cancel(i, "AOT symbol-index context dump")) {
                return;
            }
            const auto& [key, value] = index.context[i];
            printf("      %s: %s\n", key.c_str(), value.c_str());
        }
    }

    printf("    AOT defined Qore symbols:%s\n", index.defined.empty() ? " (none)" : "");
    for (size_t i = 0; i < index.defined.size(); ++i) {
        if (!dump_aot_symbol_index_check_cancel(i, "AOT symbol-index definition dump")) {
            return;
        }
        print_aot_symbol_record(index.defined[i]);
    }

    printf("    AOT imported Qore symbols:%s\n", index.imported.empty() ? " (none)" : "");
    for (size_t i = 0; i < index.imported.size(); ++i) {
        if (!dump_aot_symbol_index_check_cancel(i, "AOT symbol-index import dump")) {
            return;
        }
        print_aot_symbol_record(index.imported[i]);
    }

    printf("    AOT native symbols:%s\n", index.native.empty() ? " (none)" : "");
    for (size_t i = 0; i < index.native.size(); ++i) {
        if (!dump_aot_symbol_index_check_cancel(i, "AOT symbol-index native dump")) {
            return;
        }
        print_aot_symbol_record(index.native[i], true);
    }
}

static bool dump_aot_call_relocations_check_cancel(size_t ordinal, const char* operation) {
    if (ordinal && !(ordinal % 100) && qcc_check_cancel(operation)) {
        printf("      cancelled during %s\n", operation ? operation : "AOT call-relocation dump");
        return false;
    }
    return true;
}

static void print_aot_call_relocations(const QoreAOTBinaryReader& reader) {
    QoreAOTCallRelocations relocs;
    std::string error;
    if (!readCallRelocations(reader, relocs, error)) {
        printf("    call relocations: error: %s\n", error.c_str());
        return;
    }

    if (!relocs.version) {
        printf("    call relocations: not present\n");
        return;
    }

    printf("    call relocations: version=%u records=%zu\n",
        relocs.version, relocs.records.size());
    if (!dump_symbols) {
        return;
    }

    for (size_t i = 0; i < relocs.records.size(); ++i) {
        if (!dump_aot_call_relocations_check_cancel(i, "AOT call-relocation dump")) {
            return;
        }
        const QoreAOTCallRelocationRecord& rec = relocs.records[i];
        printf("      %-13s %s func=%s slot=%u strict=%s",
            qoreAOTCallRelocationTargetKindName(rec.target_kind),
            rec.qore_path.empty() ? "(none)" : rec.qore_path.c_str(),
            rec.function_name.c_str(), rec.expr_slot,
            rec.strictness == QoreAOTCallRelocationStrictness::REQUIRED ? "required" : "optional");
        if (!rec.native_symbol.empty()) {
            printf(" native=%s", rec.native_symbol.c_str());
        }
        if (!rec.signature_hash.empty()) {
            printf(" sig=%s", rec.signature_hash.c_str());
        }
        if (!rec.declaration_hash.empty()) {
            printf(" decl=%s", rec.declaration_hash.c_str());
        }
        if (!rec.fallback_descriptor.empty()) {
            printf(" fallback=%s", rec.fallback_descriptor.c_str());
        }
        printf("\n");
    }
}

static void dump_aot_metadata_blob(const AOTDumpMetadataBlob& blob, size_t index) {
    QoreAOTBinaryReader reader;
    std::string error;
    if (!reader.open(blob.bytes.data(), static_cast<uint32_t>(blob.bytes.size()), error)) {
        printf("  AOT metadata #%zu: invalid: %s\n", index, error.c_str());
        return;
    }

    const QoreAOTBinaryHeader& hdr = reader.getHeader();
    const char* label = reader.getLabel();
    printf("  AOT metadata #%zu (%s):\n", index, blob.source.c_str());
    const char* compression_name = hdr.compression == QORE_AOT_COMPRESSION_ZSTD ? "zstd"
        : (hdr.compression == QORE_AOT_COMPRESSION_ZLIB ? "zlib" : nullptr);
    printf("    size: %zu bytes%s", blob.bytes.size(), compression_name ? " compressed" : "");
    if (compression_name) {
        printf(" (%s)", compression_name);
    }
    printf("\n");
    printf("    label: %s\n", label ? label : "");
    printf("    kind:%s%s\n",
        (hdr.flags & QORE_AOT_FLAG_IS_MODULE) ? " module" : "",
        (hdr.flags & QORE_AOT_FLAG_HAS_TOPLEVEL) ? " toplevel" : "");
    printf("    format: %u (current %u)%s\n", hdr.version, QORE_AOT_BINARY_VERSION,
        hdr.version == QORE_AOT_BINARY_VERSION ? "" : " MISMATCH");
    printf("    compiled qore: %u.%u.%u (runtime %s)\n",
        hdr.qore_version_major, hdr.qore_version_minor, hdr.qore_version_patch,
        qore_version_string);
    printf("    parse options: lo=0x%016llx hi=0x%016llx\n",
        static_cast<unsigned long long>(hdr.parse_options_lo),
        static_cast<unsigned long long>(hdr.parse_options_hi));
    printf("    source hash: 0x%016llx\n",
        static_cast<unsigned long long>(hdr.source_hash));
    printf("    max opcode: %u (runtime %u)%s\n", hdr.max_opcode_id,
        QORE_IR_MAX_OPCODE, hdr.max_opcode_id <= QORE_IR_MAX_OPCODE ? "" : " MISMATCH");
    print_aot_feature_flags(hdr.feature_flags);

    std::vector<std::string> deps;
    if (readDependencies(blob.bytes.data(), static_cast<uint32_t>(blob.bytes.size()), deps, error)) {
        print_string_list("dependencies", deps);
    } else {
        printf("    dependencies: error: %s\n", error.c_str());
        error.clear();
    }

    std::vector<std::string> reexports;
    if (readReexportModules(blob.bytes.data(), static_cast<uint32_t>(blob.bytes.size()), reexports, error)) {
        print_string_list("reexports", reexports);
    } else {
        printf("    reexports: error: %s\n", error.c_str());
        error.clear();
    }

    std::vector<std::string> prepended;
    std::vector<std::string> appended;
    if (readModulePathLists(reader, prepended, appended, error)) {
        print_string_list("prepend module paths", prepended);
        print_string_list("append module paths", appended);
    } else {
        printf("    module paths: error: %s\n", error.c_str());
        error.clear();
    }

    std::vector<AOTModuleCommand> module_commands;
    if (readModuleCommands(reader, module_commands, error)) {
        print_module_commands(module_commands);
    } else {
        printf("    module commands: error: %s\n", error.c_str());
        error.clear();
    }

    std::string exec_class;
    if (readProgramMetadata(blob.bytes.data(), static_cast<uint32_t>(blob.bytes.size()), exec_class, error)
            && !exec_class.empty()) {
        printf("    exec class: %s\n", exec_class.c_str());
    } else if (!error.empty()) {
        printf("    program metadata: error: %s\n", error.c_str());
        error.clear();
    }

    std::vector<std::pair<std::string, std::string>> build_info;
    if (readBuildInfo(reader, build_info, error)) {
        if (!build_info.empty()) {
            printf("    build info:\n");
            for (const auto& [key, value] : build_info) {
                printf("      %s: %s\n", key.c_str(), value.c_str());
            }
        }
    } else {
        printf("    build info: error: %s\n", error.c_str());
        error.clear();
    }

    print_aot_slot_map_summary(reader);
    print_aot_default_summary(reader);
    print_aot_symbol_index(reader);
    print_aot_call_relocations(reader);

    printf("    sections: %u\n", reader.getSectionCount());
    if (dump_sections) {
        for (uint32_t i = 0; i < reader.getSectionCount(); ++i) {
            const QoreAOTSectionHeader* sec = reader.getSection(i);
            if (!sec) {
                continue;
            }
            printf("      %-20s type=%u offset=%u size=%u\n",
                aot_section_type_name(sec->type), sec->type, sec->offset, sec->size);
        }
    }
}

struct AOTDumpSymbolRow {
    std::string name;
    std::string section;
    uint64_t value = 0;
    uint64_t size = 0;
    char kind = '?';
    bool qore_relevant = false;
};

static std::string hex64_string(uint64_t value);

static std::vector<AOTDumpSymbolRow> collect_symbol_rows(const llvm::object::ObjectFile& obj) {
    std::map<std::string, uint64_t> size_by_key;
    for (const auto& [sym, size] : llvm::object::computeSymbolSizes(obj)) {
        size_by_key[symbol_key(obj, sym)] = size;
    }

    std::vector<AOTDumpSymbolRow> rows;
    for (const llvm::object::SymbolRef& sym : obj.symbols()) {
        auto name_or = sym.getName();
        if (!name_or) {
            llvm::consumeError(name_or.takeError());
            continue;
        }
        AOTDumpSymbolRow row;
        row.name = name_or->str();
        row.section = symbol_section_name(obj, sym);
        row.value = symbol_value(sym);
        row.kind = symbol_kind(sym);
        row.qore_relevant = is_qore_relevant_symbol(row.name);
        auto size_it = size_by_key.find(symbol_key(obj, sym));
        if (size_it != size_by_key.end()) {
            row.size = size_it->second;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

static bool string_has_suffix(const std::string& s, const char* suffix) {
    size_t n = s.size();
    size_t e = std::strlen(suffix);
    return n >= e && std::strcmp(s.c_str() + n - e, suffix) == 0;
}

static bool string_has_prefix(const std::string& s, const char* prefix) {
    size_t n = std::strlen(prefix);
    return s.size() >= n && std::memcmp(s.data(), prefix, n) == 0;
}

struct QOLinkInputInfo {
    std::string path;
    std::string object_hash;
    uint64_t object_size = 0;
    std::vector<std::string> source_text;
    std::string register_symbol;
    QoreAOTSymbolIndex index;
    std::vector<std::string> dependencies;
    std::vector<std::string> module_path_prepend;
    std::vector<std::string> module_path_append;
    std::vector<AOTModuleCommand> module_commands;
    std::vector<std::string> native_symbols;
    std::vector<QoreAOTCallRelocationRecord> call_relocations;
};

struct QOLinkIssue {
    std::string consumer;
    std::string path;
    std::string dependency_class;
    std::string expected;
    std::vector<std::string> providers;
};

struct QOLinkCallRelocation {
    std::string consumer;
    std::string function_name;
    uint32_t expr_slot = UINT32_MAX;
    std::string target_kind;
    std::string path;
    std::string path_scope;
    std::string dependency_class;
    std::string resolution;
    std::string reason;
    std::string expected;
    std::string provider_kind;
    std::string provider_source;
    std::string native_symbol;
    std::string fallback_descriptor;
    std::vector<std::string> providers;
};

struct QOLinkPlan {
    std::vector<std::string> provided_qore_symbols;
    std::vector<std::string> native_symbols;
    std::vector<std::string> external_dependencies;
    std::vector<std::string> module_path_prepend;
    std::vector<std::string> module_path_append;
    std::vector<AOTModuleCommand> module_commands;
    std::vector<QOLinkIssue> resolved_imports;
    std::vector<QOLinkIssue> unresolved_imports;
    std::vector<QOLinkIssue> ambiguous_imports;
    std::vector<QOLinkIssue> hash_mismatches;
    std::vector<QOLinkCallRelocation> resolved_call_relocations;
    std::vector<QOLinkCallRelocation> unresolved_call_relocations;
    std::vector<QOLinkCallRelocation> ambiguous_call_relocations;
    std::vector<QOLinkCallRelocation> call_relocation_hash_mismatches;
};

using QOLinkProviderMap = std::map<std::string, std::vector<const QoreAOTSymbolIndexRecord*>>;

struct QOLinkProviderIndex {
    QOLinkProviderMap exact;
    QOLinkProviderMap raw_suffix;
    QOLinkProviderMap callable_suffix;
    QOLinkProviderMap callable_base_exact;
};

static void add_unique_string(std::vector<std::string>& out, std::set<std::string>& seen,
        const std::string& value) {
    if (!value.empty() && seen.insert(value).second) {
        out.push_back(value);
    }
}

static bool qo_link_check_cancel(size_t ordinal, const char* operation, std::string& error) {
    if (ordinal && !(ordinal % 100) && qcc_check_cancel(operation)) {
        error = "operation cancelled during ";
        error += operation ? operation : "AOT qo-link processing";
        return false;
    }
    return true;
}

static bool collect_qo_register_symbols(const llvm::object::ObjectFile& obj,
        std::vector<std::string>& register_symbols, std::string& error) {
    size_t i = 0;
    for (const llvm::object::SymbolRef& sym : obj.symbols()) {
        if (!qo_link_check_cancel(i++, "AOT qo-link register-symbol scan", error)) {
            return false;
        }
        auto name_or = sym.getName();
        if (!name_or) {
            llvm::consumeError(name_or.takeError());
            continue;
        }
        std::string name = name_or->str();
        if (string_has_suffix(name, "_script_register") && symbol_kind(sym) != 'U') {
            register_symbols.push_back(std::move(name));
        }
    }
    return true;
}

static bool collect_qo_link_input(const char* path, QOLinkInputInfo& input,
        std::string& error) {
    input = QOLinkInputInfo();
    input.path = path;

    std::string contents;
    if (!read_file(path, contents)) {
        error = std::string("cannot read input object: ") + path;
        return false;
    }
    input.object_size = contents.size();
    input.object_hash = "xxh64:" + hex64_string(XXH64(contents.data(), contents.size(), 0));

    auto binary_or = llvm::object::createBinary(path);
    if (!binary_or) {
        error = "cannot read object file '" + std::string(path) + "': "
            + llvm::toString(binary_or.takeError());
        return false;
    }
    llvm::object::Binary* binary = binary_or->getBinary();
    auto* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
    if (!obj) {
        error = "input is not a relocatable object file: " + std::string(path);
        return false;
    }

    std::vector<std::string> register_symbols;
    if (!collect_qo_register_symbols(*obj, register_symbols, error)) {
        return false;
    }
    if (register_symbols.empty()) {
        error = "input has no exported *_script_register symbol: " + std::string(path);
        return false;
    }
    if (register_symbols.size() > 1) {
        error = "input has multiple exported *_script_register symbols: " + std::string(path);
        for (const std::string& sym : register_symbols) {
            error += " " + sym;
        }
        return false;
    }
    input.register_symbol = register_symbols.front();

    std::vector<AOTDumpMetadataBlob> blobs;
    std::set<std::string> seen_blobs;
    extract_aot_metadata_from_object(*obj, blobs, seen_blobs);
    scan_aot_metadata_blobs(contents, blobs, seen_blobs);
    if (blobs.empty()) {
        error = "input has no AOT metadata blob: " + std::string(path);
        return false;
    }

    bool have_index = false;
    std::set<std::string> source_seen;
    std::set<std::string> dep_seen;
    std::set<std::string> prepend_seen;
    std::set<std::string> append_seen;
    std::set<std::string> module_cmd_seen;
    std::set<std::string> native_seen;
    input.index.version = QORE_AOT_SYMBOL_INDEX_VERSION;

    for (size_t i = 0; i < blobs.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("AOT qo-link metadata scan")) {
            error = "operation cancelled during AOT qo-link metadata scan";
            return false;
        }
        QoreAOTBinaryReader reader;
        std::string read_error;
        if (!reader.open(blobs[i].bytes.data(), static_cast<uint32_t>(blobs[i].bytes.size()), read_error)) {
            error = "invalid AOT metadata in '" + std::string(path) + "': " + read_error;
            return false;
        }
        const char* label = reader.getLabel();
        if (label && *label) {
            add_unique_string(input.source_text, source_seen, label);
        }

        QoreAOTSymbolIndex index;
        if (!readSymbolIndex(reader, index, read_error)) {
            error = "invalid SYMBOL_INDEX in '" + std::string(path) + "': " + read_error;
            return false;
        }
        if (index.version) {
            have_index = true;
            input.index.context.insert(input.index.context.end(), index.context.begin(), index.context.end());
            input.index.defined.insert(input.index.defined.end(), index.defined.begin(), index.defined.end());
            input.index.imported.insert(input.index.imported.end(), index.imported.begin(), index.imported.end());
            input.index.native.insert(input.index.native.end(), index.native.begin(), index.native.end());
            for (size_t j = 0; j < index.native.size(); ++j) {
                if (!qo_link_check_cancel(j, "AOT qo-link native symbol collection", error)) {
                    return false;
                }
                const QoreAOTSymbolIndexRecord& rec = index.native[j];
                if (!rec.native_symbol.empty()) {
                    add_unique_string(input.native_symbols, native_seen, rec.native_symbol);
                }
            }
        }

        QoreAOTCallRelocations relocs;
        if (!readCallRelocations(reader, relocs, read_error)) {
            error = "invalid CALL_RELOCATIONS in '" + std::string(path) + "': " + read_error;
            return false;
        }
        if (relocs.version) {
            input.call_relocations.insert(input.call_relocations.end(),
                relocs.records.begin(), relocs.records.end());
        }

        std::vector<std::string> deps;
        if (!readDependencies(reader, deps, read_error)) {
            error = "invalid dependency metadata in '" + std::string(path) + "': " + read_error;
            return false;
        }
        for (size_t j = 0; j < deps.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link dependency collection", error)) {
                return false;
            }
            add_unique_string(input.dependencies, dep_seen, deps[j]);
        }

        std::vector<std::string> prepended;
        std::vector<std::string> appended;
        if (!readModulePathLists(reader, prepended, appended, read_error)) {
            error = "invalid module path metadata in '" + std::string(path) + "': " + read_error;
            return false;
        }
        for (size_t j = 0; j < prepended.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module path collection", error)) {
                return false;
            }
            add_unique_string(input.module_path_prepend, prepend_seen, prepended[j]);
        }
        for (size_t j = 0; j < appended.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module path collection", error)) {
                return false;
            }
            add_unique_string(input.module_path_append, append_seen, appended[j]);
        }

        std::vector<AOTModuleCommand> commands;
        if (!readModuleCommands(reader, commands, read_error)) {
            error = "invalid module command metadata in '" + std::string(path) + "': " + read_error;
            return false;
        }
        for (size_t j = 0; j < commands.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module command collection", error)) {
                return false;
            }
            const AOTModuleCommand& cmd = commands[j];
            std::string key = cmd.module + "\n" + cmd.command;
            if (module_cmd_seen.insert(key).second) {
                input.module_commands.push_back(cmd);
            }
        }
    }

    if (!have_index) {
        error = "input has no SYMBOL_INDEX section: " + std::string(path);
        return false;
    }
    return true;
}

static bool is_owned_qore_provider(const QoreAOTSymbolIndexRecord& rec) {
    return !rec.qore_path.empty() && !rec.source_file.empty();
}

static bool is_optional_qo_import(const QoreAOTSymbolIndexRecord& rec) {
    return (rec.flags & QORE_AOT_SYMBOL_FLAG_OPTIONAL_IMPORT) != 0;
}

static bool is_deferred_callable_qo_import(const QoreAOTSymbolIndexRecord& rec) {
    return (rec.kind == QoreAOTSymbolKind::FUNCTION
            || rec.kind == QoreAOTSymbolKind::METHOD
            || rec.kind == QoreAOTSymbolKind::STATIC_METHOD
            || rec.kind == QoreAOTSymbolKind::CONSTRUCTOR)
        && rec.qore_path.find('(') == std::string::npos;
}

static std::string qo_link_strip_leading_colons(const std::string& path) {
    return string_has_prefix(path, "::") ? path.substr(2) : path;
}

static void qo_link_add_provider_candidate(QOLinkProviderMap& providers,
        const std::string& key, const QoreAOTSymbolIndexRecord* rec) {
    if (!key.empty()) {
        providers[key].push_back(rec);
    }
}

static void qo_link_add_suffix_keys(std::set<std::string>& keys, const std::string& path) {
    if (path.empty()) {
        return;
    }
    keys.insert(path);
    size_t pos = 0;
    while ((pos = path.find("::", pos)) != std::string::npos) {
        pos += 2;
        if (pos < path.size()) {
            keys.insert(path.substr(pos));
        }
    }
}

static void qo_link_index_raw_suffix(QOLinkProviderMap& providers,
        const QoreAOTSymbolIndexRecord* rec) {
    std::string stripped = qo_link_strip_leading_colons(rec->qore_path);
    size_t sig_pos = stripped.find('(');
    std::set<std::string> keys;
    if (sig_pos == std::string::npos) {
        qo_link_add_suffix_keys(keys, stripped);
    } else {
        std::string base = stripped.substr(0, sig_pos);
        std::string signature = stripped.substr(sig_pos);
        std::set<std::string> base_keys;
        qo_link_add_suffix_keys(base_keys, base);
        for (const std::string& key : base_keys) {
            keys.insert(key + signature);
        }
    }
    for (const std::string& key : keys) {
        qo_link_add_provider_candidate(providers, key, rec);
    }
}

static void qo_link_index_callable_suffix(QOLinkProviderMap& providers,
        const QoreAOTSymbolIndexRecord* rec) {
    std::string stripped = qo_link_strip_leading_colons(rec->qore_path);
    size_t sig_pos = stripped.find('(');
    if (sig_pos == std::string::npos) {
        qo_link_index_raw_suffix(providers, rec);
        return;
    }

    std::string base = stripped.substr(0, sig_pos);
    std::string signature = stripped.substr(sig_pos);
    std::set<std::string> base_keys;
    qo_link_add_suffix_keys(base_keys, base);
    for (const std::string& key : base_keys) {
        qo_link_add_provider_candidate(providers, key, rec);
        qo_link_add_provider_candidate(providers, key + signature, rec);
    }
}

static void qo_link_index_callable_base_exact(QOLinkProviderMap& providers,
        const QoreAOTSymbolIndexRecord* rec) {
    std::string stripped = qo_link_strip_leading_colons(rec->qore_path);
    size_t sig_pos = stripped.find('(');
    if (sig_pos != std::string::npos) {
        qo_link_add_provider_candidate(providers, stripped.substr(0, sig_pos), rec);
    }
}

static void qo_link_index_provider(QOLinkProviderIndex& index,
        const QoreAOTSymbolIndexRecord* rec) {
    index.exact[rec->qore_path].push_back(rec);
    qo_link_index_raw_suffix(index.raw_suffix, rec);
    qo_link_index_callable_suffix(index.callable_suffix, rec);
    qo_link_index_callable_base_exact(index.callable_base_exact, rec);
}

static void find_deferred_callable_qo_providers(
        const QOLinkProviderMap& providers,
        const std::string& name, std::vector<const QoreAOTSymbolIndexRecord*>& matches) {
    std::string key = qo_link_strip_leading_colons(name);
    auto it = providers.find(key);
    if (it != providers.end()) {
        matches.insert(matches.end(), it->second.begin(), it->second.end());
    }
}

static std::string qo_link_callable_base_path(const std::string& path) {
    size_t pos = path.find('(');
    return pos == std::string::npos ? path : path.substr(0, pos);
}

static bool qo_link_path_is_bare_name(const std::string& path) {
    std::string stripped = qo_link_strip_leading_colons(path);
    return !stripped.empty()
        && stripped.find("::") == std::string::npos
        && stripped.find('(') == std::string::npos;
}

static size_t qo_link_path_namespace_depth(const std::string& path) {
    std::string stripped = qo_link_strip_leading_colons(qo_link_callable_base_path(path));
    size_t depth = 0;
    size_t pos = 0;
    while ((pos = stripped.find("::", pos)) != std::string::npos) {
        ++depth;
        pos += 2;
    }
    return depth;
}

static void find_suffix_qo_providers(
        const QOLinkProviderIndex& providers,
        const std::string& name, bool callable, std::vector<const QoreAOTSymbolIndexRecord*>& matches) {
    std::string key = qo_link_strip_leading_colons(name);
    const QOLinkProviderMap& suffix_providers = callable ? providers.callable_suffix : providers.raw_suffix;
    auto it = suffix_providers.find(key);
    if (it != suffix_providers.end()) {
        matches.insert(matches.end(), it->second.begin(), it->second.end());
    }
}

static bool qo_link_call_relocation_is_callable(const QoreAOTCallRelocationRecord& rec) {
    return rec.target_kind == QoreAOTCallRelocationTargetKind::FUNCTION
        || rec.target_kind == QoreAOTCallRelocationTargetKind::METHOD
        || rec.target_kind == QoreAOTCallRelocationTargetKind::STATIC_METHOD
        || rec.target_kind == QoreAOTCallRelocationTargetKind::CONSTRUCTOR;
}

static bool qo_link_provider_matches_hashes(const QoreAOTSymbolIndexRecord& rec,
        const QoreAOTSymbolIndexRecord& provider) {
    return (rec.signature_hash.empty() || rec.signature_hash == provider.signature_hash)
        && (rec.declaration_hash.empty() || rec.declaration_hash == provider.declaration_hash)
        && (rec.value_hash.empty() || rec.value_hash == provider.value_hash);
}

static bool qo_link_call_provider_matches_hashes(const QoreAOTCallRelocationRecord& rec,
        const QoreAOTSymbolIndexRecord& provider) {
    return (rec.signature_hash.empty() || rec.signature_hash == provider.signature_hash)
        && (rec.declaration_hash.empty() || rec.declaration_hash == provider.declaration_hash);
}

static std::vector<const QoreAOTSymbolIndexRecord*> dedupe_qo_provider_candidates(
        const QoreAOTSymbolIndexRecord& rec,
        const std::vector<const QoreAOTSymbolIndexRecord*>& candidates) {
    std::vector<const QoreAOTSymbolIndexRecord*> out;
    std::map<std::string, size_t> by_source;
    for (const QoreAOTSymbolIndexRecord* candidate : candidates) {
        std::string key = candidate->source_file;
        auto [it, inserted] = by_source.emplace(key, out.size());
        if (inserted) {
            out.push_back(candidate);
            continue;
        }
        const QoreAOTSymbolIndexRecord* current = out[it->second];
        if (!qo_link_provider_matches_hashes(rec, *current)
                && qo_link_provider_matches_hashes(rec, *candidate)) {
            out[it->second] = candidate;
        }
    }
    return out;
}

static void qo_link_prefer_shallowest_provider_for_bare_import(const QoreAOTSymbolIndexRecord& rec,
        std::vector<const QoreAOTSymbolIndexRecord*>& candidates) {
    if (!qo_link_path_is_bare_name(rec.qore_path) || is_deferred_callable_qo_import(rec) || candidates.size() <= 1) {
        return;
    }

    size_t best_depth = std::numeric_limits<size_t>::max();
    for (const QoreAOTSymbolIndexRecord* candidate : candidates) {
        best_depth = std::min(best_depth, qo_link_path_namespace_depth(candidate->qore_path));
    }

    std::vector<const QoreAOTSymbolIndexRecord*> filtered;
    for (const QoreAOTSymbolIndexRecord* candidate : candidates) {
        if (qo_link_path_namespace_depth(candidate->qore_path) == best_depth) {
            filtered.push_back(candidate);
        }
    }
    candidates.swap(filtered);
}

static std::vector<const QoreAOTSymbolIndexRecord*> dedupe_qo_call_provider_candidates(
        const QoreAOTCallRelocationRecord& rec,
        const std::vector<const QoreAOTSymbolIndexRecord*>& candidates) {
    std::vector<const QoreAOTSymbolIndexRecord*> out;
    std::map<std::string, size_t> by_source;
    for (const QoreAOTSymbolIndexRecord* candidate : candidates) {
        std::string key = candidate->source_file;
        auto [it, inserted] = by_source.emplace(key, out.size());
        if (inserted) {
            out.push_back(candidate);
            continue;
        }
        const QoreAOTSymbolIndexRecord* current = out[it->second];
        if (!qo_link_call_provider_matches_hashes(rec, *current)
                && qo_link_call_provider_matches_hashes(rec, *candidate)) {
            out[it->second] = candidate;
        }
    }
    return out;
}

static const char* qo_link_call_path_scope(const std::string& path) {
    if (path.empty()) {
        return "unknown";
    }
    if (string_has_prefix(path, "Qore::")) {
        return "qore_runtime";
    }
    if (string_has_prefix(path, "Qorus::") || string_has_prefix(path, "OMQ::")) {
        return "local";
    }
    if (path.find("::") == std::string::npos) {
        return "global";
    }
    return "external_namespace";
}

static const char* qo_link_unresolved_call_reason(const QOLinkCallRelocation& reloc) {
    if (reloc.path_scope == "qore_runtime") {
        return "qore_runtime_provider";
    }
    if (reloc.dependency_class == "module_api") {
        return "external_module_api";
    }
    if (reloc.dependency_class == "module_runtime") {
        return "external_module_runtime";
    }
    if (reloc.dependency_class == "native_body") {
        return "external_native_body";
    }
    if (reloc.dependency_class == "dynamic") {
        return "dynamic_dispatch";
    }
    if (reloc.path_scope == "external_namespace") {
        return "external_namespace_provider";
    }
    if (reloc.path_scope == "global") {
        return "external_global_provider";
    }
    if (reloc.path_scope == "local") {
        return "local_provider_not_in_aggregate";
    }
    return "provider_not_found";
}

static bool validate_qo_link_inputs(const std::vector<QOLinkInputInfo>& inputs,
        QOLinkPlan& plan, std::string& error) {
    QOLinkProviderIndex providers;
    std::map<std::string, QoreAOTDependencyClass> import_classes;
    std::set<std::string> provided_seen;
    std::set<std::string> native_seen;
    std::set<std::string> dep_seen;
    std::set<std::string> prepend_seen;
    std::set<std::string> append_seen;
    std::set<std::string> module_cmd_seen;

    for (size_t i = 0; i < inputs.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("AOT qo-link provider collection")) {
            error = "operation cancelled during AOT qo-link provider collection";
            return false;
        }
        const QOLinkInputInfo& input = inputs[i];
        for (size_t j = 0; j < input.index.imported.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link import-class collection", error)) {
                return false;
            }
            const QoreAOTSymbolIndexRecord& rec = input.index.imported[j];
            if (!rec.qore_path.empty() && rec.dependency_class != QoreAOTDependencyClass::UNKNOWN) {
                auto [it, inserted] = import_classes.emplace(rec.qore_path, rec.dependency_class);
                if (!inserted && it->second == QoreAOTDependencyClass::UNKNOWN) {
                    it->second = rec.dependency_class;
                }
            }
        }
        for (size_t j = 0; j < input.index.defined.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link provider collection", error)) {
                return false;
            }
            const QoreAOTSymbolIndexRecord& rec = input.index.defined[j];
            if (is_owned_qore_provider(rec)) {
                qo_link_index_provider(providers, &rec);
                add_unique_string(plan.provided_qore_symbols, provided_seen, rec.qore_path);
            }
        }
        for (size_t j = 0; j < input.native_symbols.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link native symbol plan", error)) {
                return false;
            }
            add_unique_string(plan.native_symbols, native_seen, input.native_symbols[j]);
        }
        for (size_t j = 0; j < input.dependencies.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link dependency plan", error)) {
                return false;
            }
            add_unique_string(plan.external_dependencies, dep_seen, input.dependencies[j]);
        }
        for (size_t j = 0; j < input.module_path_prepend.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module path plan", error)) {
                return false;
            }
            add_unique_string(plan.module_path_prepend, prepend_seen, input.module_path_prepend[j]);
        }
        for (size_t j = 0; j < input.module_path_append.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module path plan", error)) {
                return false;
            }
            add_unique_string(plan.module_path_append, append_seen, input.module_path_append[j]);
        }
        for (size_t j = 0; j < input.module_commands.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link module command plan", error)) {
                return false;
            }
            const AOTModuleCommand& cmd = input.module_commands[j];
            std::string key = cmd.module + "\n" + cmd.command;
            if (module_cmd_seen.insert(key).second) {
                plan.module_commands.push_back(cmd);
            }
        }
    }

    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link import validation", error)) {
            return false;
        }
        const QOLinkInputInfo& input = inputs[i];
        for (size_t j = 0; j < input.index.imported.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link import validation", error)) {
                return false;
            }
            const QoreAOTSymbolIndexRecord& rec = input.index.imported[j];
            if (rec.qore_path.empty()) {
                continue;
            }
            QoreAOTDependencyClass dep_class = rec.dependency_class;
            if (dep_class == QoreAOTDependencyClass::MODULE_API
                    || dep_class == QoreAOTDependencyClass::MODULE_RUNTIME
                    || dep_class == QoreAOTDependencyClass::NATIVE_BODY
                    || dep_class == QoreAOTDependencyClass::DYNAMIC) {
                continue;
            }
            bool optional_import = is_optional_qo_import(rec);

            QOLinkIssue issue;
            issue.consumer = input.path;
            issue.path = rec.qore_path;
            issue.dependency_class = qoreAOTDependencyClassName(dep_class);
            auto provider_it = providers.exact.find(rec.qore_path);
            std::vector<const QoreAOTSymbolIndexRecord*> deferred_callable_candidates;
            std::vector<const QoreAOTSymbolIndexRecord*> suffix_candidates;
            const std::vector<const QoreAOTSymbolIndexRecord*>* candidates_ptr =
                provider_it == providers.exact.end() ? nullptr : &provider_it->second;
            if (!candidates_ptr && is_deferred_callable_qo_import(rec)) {
                find_deferred_callable_qo_providers(providers.callable_base_exact, rec.qore_path,
                    deferred_callable_candidates);
                if (!deferred_callable_candidates.empty()) {
                    candidates_ptr = &deferred_callable_candidates;
                }
            }
            if (!candidates_ptr) {
                find_suffix_qo_providers(providers, rec.qore_path, is_deferred_callable_qo_import(rec),
                    suffix_candidates);
                if (!suffix_candidates.empty()) {
                    candidates_ptr = &suffix_candidates;
                }
            }
            if (!candidates_ptr) {
                if (!optional_import) {
                    plan.unresolved_imports.push_back(std::move(issue));
                }
                continue;
            }

            std::vector<const QoreAOTSymbolIndexRecord*> candidates =
                dedupe_qo_provider_candidates(rec, *candidates_ptr);
            qo_link_prefer_shallowest_provider_for_bare_import(rec, candidates);
            for (size_t k = 0; k < candidates.size(); ++k) {
                if (!qo_link_check_cancel(k, "AOT qo-link provider validation", error)) {
                    return false;
                }
                issue.providers.push_back(candidates[k]->source_file);
            }
            if (candidates.size() > 1) {
                if (!optional_import) {
                    plan.ambiguous_imports.push_back(std::move(issue));
                }
                continue;
            }

            const QoreAOTSymbolIndexRecord* provider = candidates.front();
            bool mismatch = false;
            if (!rec.signature_hash.empty() && rec.signature_hash != provider->signature_hash) {
                issue.expected = "signature=" + rec.signature_hash
                    + " actual=" + provider->signature_hash;
                mismatch = true;
            } else if (!rec.declaration_hash.empty() && rec.declaration_hash != provider->declaration_hash) {
                issue.expected = "declaration=" + rec.declaration_hash
                    + " actual=" + provider->declaration_hash;
                mismatch = true;
            } else if (!rec.value_hash.empty() && rec.value_hash != provider->value_hash) {
                issue.expected = "value=" + rec.value_hash + " actual=" + provider->value_hash;
                mismatch = true;
            }
            if (mismatch) {
                if (!optional_import) {
                    plan.hash_mismatches.push_back(std::move(issue));
                }
            } else {
                plan.resolved_imports.push_back(std::move(issue));
            }
        }

        for (size_t j = 0; j < input.call_relocations.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link call-relocation validation", error)) {
                return false;
            }
            const QoreAOTCallRelocationRecord& rec = input.call_relocations[j];
            if (rec.qore_path.empty()) {
                continue;
            }
            QOLinkCallRelocation reloc;
            reloc.consumer = input.path;
            reloc.function_name = rec.function_name;
            reloc.expr_slot = rec.expr_slot;
            reloc.target_kind = qoreAOTCallRelocationTargetKindName(rec.target_kind);
            reloc.path = rec.qore_path;
            reloc.path_scope = qo_link_call_path_scope(rec.qore_path);
            auto import_it = import_classes.find(rec.qore_path);
            if (import_it != import_classes.end()) {
                reloc.dependency_class = qoreAOTDependencyClassName(import_it->second);
            }
            reloc.fallback_descriptor = rec.fallback_descriptor;
            auto provider_it = providers.exact.find(rec.qore_path);
            std::vector<const QoreAOTSymbolIndexRecord*> suffix_candidates;
            const std::vector<const QoreAOTSymbolIndexRecord*>* candidates_ptr =
                provider_it == providers.exact.end() ? nullptr : &provider_it->second;
            if (!candidates_ptr) {
                find_suffix_qo_providers(providers, rec.qore_path, qo_link_call_relocation_is_callable(rec),
                    suffix_candidates);
                if (!suffix_candidates.empty()) {
                    candidates_ptr = &suffix_candidates;
                }
            }
            if (!candidates_ptr) {
                reloc.resolution = "unresolved";
                reloc.reason = qo_link_unresolved_call_reason(reloc);
                plan.unresolved_call_relocations.push_back(std::move(reloc));
                continue;
            }

            std::vector<const QoreAOTSymbolIndexRecord*> candidates =
                dedupe_qo_call_provider_candidates(rec, *candidates_ptr);
            for (size_t k = 0; k < candidates.size(); ++k) {
                if (!qo_link_check_cancel(k, "AOT qo-link call-relocation provider validation", error)) {
                    return false;
                }
                reloc.providers.push_back(candidates[k]->source_file);
            }
            if (candidates.size() > 1) {
                reloc.resolution = "ambiguous";
                reloc.reason = "ambiguous_provider";
                plan.ambiguous_call_relocations.push_back(std::move(reloc));
                continue;
            }

            const QoreAOTSymbolIndexRecord* provider = candidates.front();
            reloc.resolution = "resolved";
            reloc.reason = "resolved";
            reloc.dependency_class = qoreAOTDependencyClassName(provider->dependency_class);
            reloc.provider_kind = qoreAOTSymbolKindName(provider->kind);
            reloc.provider_source = provider->source_file;
            bool mismatch = false;
            if (!rec.signature_hash.empty() && rec.signature_hash != provider->signature_hash) {
                reloc.expected = "signature=" + rec.signature_hash
                    + " actual=" + provider->signature_hash;
                reloc.resolution = "hash_mismatch";
                reloc.reason = "signature_hash_mismatch";
                mismatch = true;
            } else if (!rec.declaration_hash.empty() && rec.declaration_hash != provider->declaration_hash) {
                reloc.expected = "declaration=" + rec.declaration_hash
                    + " actual=" + provider->declaration_hash;
                reloc.resolution = "hash_mismatch";
                reloc.reason = "declaration_hash_mismatch";
                mismatch = true;
            }
            reloc.native_symbol = provider->native_symbol;
            if (mismatch) {
                plan.call_relocation_hash_mismatches.push_back(std::move(reloc));
            } else {
                plan.resolved_call_relocations.push_back(std::move(reloc));
            }
        }
    }

    std::sort(plan.provided_qore_symbols.begin(), plan.provided_qore_symbols.end());
    std::sort(plan.native_symbols.begin(), plan.native_symbols.end());
    std::sort(plan.external_dependencies.begin(), plan.external_dependencies.end());
    std::sort(plan.module_path_prepend.begin(), plan.module_path_prepend.end());
    std::sort(plan.module_path_append.begin(), plan.module_path_append.end());
    std::sort(plan.module_commands.begin(), plan.module_commands.end(),
        [](const AOTModuleCommand& a, const AOTModuleCommand& b) {
            return std::tie(a.module, a.command) < std::tie(b.module, b.command);
        });
    auto call_reloc_less = [](const QOLinkCallRelocation& a, const QOLinkCallRelocation& b) {
        return std::tie(a.consumer, a.function_name, a.expr_slot, a.target_kind, a.path)
            < std::tie(b.consumer, b.function_name, b.expr_slot, b.target_kind, b.path);
    };
    std::sort(plan.resolved_call_relocations.begin(), plan.resolved_call_relocations.end(), call_reloc_less);
    std::sort(plan.unresolved_call_relocations.begin(), plan.unresolved_call_relocations.end(), call_reloc_less);
    std::sort(plan.ambiguous_call_relocations.begin(), plan.ambiguous_call_relocations.end(), call_reloc_less);
    std::sort(plan.call_relocation_hash_mismatches.begin(), plan.call_relocation_hash_mismatches.end(),
        call_reloc_less);
    return true;
}

static void json_file_string(FILE* f, const std::string& value) {
    fputc('"', f);
    for (unsigned char c : value) {
        switch (c) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (c < 0x20) {
                    fprintf(f, "\\u%04x", c);
                } else {
                    fputc(c, f);
                }
                break;
        }
    }
    fputc('"', f);
}

static bool json_file_string_array(FILE* f, const char* key,
        const std::vector<std::string>& values, std::string& error, const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < values.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map string array write", error)) {
            return false;
        }
        if (i) {
            fputc(',', f);
        }
        fputc('\n', f);
        fputs("    ", f);
        json_file_string(f, values[i]);
    }
    if (!values.empty()) {
        fputc('\n', f);
        fputs("  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool json_file_issue_array(FILE* f, const char* key,
        const std::vector<QOLinkIssue>& issues, std::string& error, const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < issues.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map issue write", error)) {
            return false;
        }
        const QOLinkIssue& issue = issues[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"consumer\": ", f);
        json_file_string(f, issue.consumer);
        fputs(", \"path\": ", f);
        json_file_string(f, issue.path);
        fputs(", \"dependency_class\": ", f);
        json_file_string(f, issue.dependency_class);
        fputs(", \"expected\": ", f);
        json_file_string(f, issue.expected);
        fputs(", \"providers\": [", f);
        for (size_t j = 0; j < issue.providers.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link map issue provider write", error)) {
                return false;
            }
            if (j) {
                fputs(", ", f);
            }
            json_file_string(f, issue.providers[j]);
        }
        fputs("]}", f);
    }
    if (!issues.empty()) {
        fputc('\n', f);
        fputs("  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool json_file_call_relocation_array(FILE* f, const char* key,
        const std::vector<QOLinkCallRelocation>& relocs, std::string& error,
        const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < relocs.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map call-relocation write", error)) {
            return false;
        }
        const QOLinkCallRelocation& reloc = relocs[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"consumer\": ", f);
        json_file_string(f, reloc.consumer);
        fputs(", \"function\": ", f);
        json_file_string(f, reloc.function_name);
        fprintf(f, ", \"expr_slot\": %u", reloc.expr_slot);
        fputs(", \"target_kind\": ", f);
        json_file_string(f, reloc.target_kind);
        fputs(", \"path\": ", f);
        json_file_string(f, reloc.path);
        fputs(", \"path_scope\": ", f);
        json_file_string(f, reloc.path_scope);
        fputs(", \"dependency_class\": ", f);
        json_file_string(f, reloc.dependency_class);
        fputs(", \"resolution\": ", f);
        json_file_string(f, reloc.resolution);
        fputs(", \"reason\": ", f);
        json_file_string(f, reloc.reason);
        fputs(", \"expected\": ", f);
        json_file_string(f, reloc.expected);
        fputs(", \"provider_kind\": ", f);
        json_file_string(f, reloc.provider_kind);
        fputs(", \"provider_source\": ", f);
        json_file_string(f, reloc.provider_source);
        fputs(", \"native_symbol\": ", f);
        json_file_string(f, reloc.native_symbol);
        fputs(", \"fallback_descriptor\": ", f);
        json_file_string(f, reloc.fallback_descriptor);
        fputs(", \"providers\": [", f);
        for (size_t j = 0; j < reloc.providers.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link map call-relocation provider write", error)) {
                return false;
            }
            if (j) {
                fputs(", ", f);
            }
            json_file_string(f, reloc.providers[j]);
        }
        fputs("]}", f);
    }
    if (!relocs.empty()) {
        fputc('\n', f);
        fputs("  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool add_qo_link_call_relocation_reason_counts(
        std::map<std::string, size_t>& reasons,
        const std::vector<QOLinkCallRelocation>& relocs,
        const char* operation,
        std::string& error) {
    for (size_t i = 0; i < relocs.size(); ++i) {
        if (!qo_link_check_cancel(i, operation, error)) {
            return false;
        }
        const std::string& reason = relocs[i].reason.empty() ? relocs[i].resolution : relocs[i].reason;
        ++reasons[reason.empty() ? "unknown" : reason];
    }
    return true;
}

static bool json_file_call_relocation_summary(FILE* f, const QOLinkPlan& plan,
        std::string& error, const char* comma = ",") {
    std::map<std::string, size_t> reasons;
    if (!add_qo_link_call_relocation_reason_counts(reasons, plan.resolved_call_relocations,
            "AOT qo-link map call-relocation summary write", error)
            || !add_qo_link_call_relocation_reason_counts(reasons, plan.unresolved_call_relocations,
                "AOT qo-link map call-relocation summary write", error)
            || !add_qo_link_call_relocation_reason_counts(reasons, plan.ambiguous_call_relocations,
                "AOT qo-link map call-relocation summary write", error)
            || !add_qo_link_call_relocation_reason_counts(reasons, plan.call_relocation_hash_mismatches,
                "AOT qo-link map call-relocation summary write", error)) {
        return false;
    }

    const size_t resolved = plan.resolved_call_relocations.size();
    const size_t unresolved = plan.unresolved_call_relocations.size();
    const size_t ambiguous = plan.ambiguous_call_relocations.size();
    const size_t hash_mismatches = plan.call_relocation_hash_mismatches.size();
    const size_t total = resolved + unresolved + ambiguous + hash_mismatches;
    fprintf(f,
        "  \"call_relocation_summary\": {\n"
        "    \"total\": %llu,\n"
        "    \"resolved\": %llu,\n"
        "    \"unresolved\": %llu,\n"
        "    \"ambiguous\": %llu,\n"
        "    \"hash_mismatches\": %llu,\n"
        "    \"reasons\": [",
        static_cast<unsigned long long>(total),
        static_cast<unsigned long long>(resolved),
        static_cast<unsigned long long>(unresolved),
        static_cast<unsigned long long>(ambiguous),
        static_cast<unsigned long long>(hash_mismatches));
    size_t i = 0;
    for (const auto& entry : reasons) {
        if (!qo_link_check_cancel(i, "AOT qo-link map call-relocation reason write", error)) {
            return false;
        }
        if (i) {
            fputc(',', f);
        }
        fputs("\n      {\"reason\": ", f);
        json_file_string(f, entry.first);
        fprintf(f, ", \"count\": %llu}", static_cast<unsigned long long>(entry.second));
        ++i;
    }
    if (!reasons.empty()) {
        fputc('\n', f);
        fputs("    ", f);
    }
    fprintf(f, "]\n  }%s\n", comma);
    return true;
}

static bool write_qo_link_map(const std::string& path, const std::string& output,
        const std::string& aggregate_symbol, const std::vector<QOLinkInputInfo>& inputs,
        const QOLinkPlan& plan, std::string& error) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        error = "cannot open qo link map '" + path + "': " + strerror(errno);
        return false;
    }

    fprintf(f, "{\n");
    fputs("  \"format\": 1,\n", f);
    fputs("  \"output\": ", f);
    json_file_string(f, output);
    fputs(",\n  \"aggregate_symbol\": ", f);
    json_file_string(f, aggregate_symbol);
    fprintf(f, ",\n  \"allow_unresolved_imports\": %s",
        allow_unresolved_qo_imports ? "true" : "false");
    fputs(",\n  \"inputs\": [", f);
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map input write", error)) {
            fclose(f);
            return false;
        }
        const QOLinkInputInfo& input = inputs[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"path\": ", f);
        json_file_string(f, input.path);
        fputs(", \"object_hash\": ", f);
        json_file_string(f, input.object_hash);
        fprintf(f, ", \"object_size\": %llu, \"register_symbol\": ",
            static_cast<unsigned long long>(input.object_size));
        json_file_string(f, input.register_symbol);
        fputs(", \"source_text\": [", f);
        for (size_t j = 0; j < input.source_text.size(); ++j) {
            if (!qo_link_check_cancel(j, "AOT qo-link map source-text write", error)) {
                fclose(f);
                return false;
            }
            if (j) {
                fputs(", ", f);
            }
            json_file_string(f, input.source_text[j]);
        }
        fputs("]}", f);
    }
    if (!inputs.empty()) {
        fputc('\n', f);
        fputs("  ", f);
    }
    fputs("],\n", f);

    std::vector<std::string> register_symbols;
    register_symbols.reserve(inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map register-symbol collection", error)) {
            fclose(f);
            return false;
        }
        register_symbols.push_back(inputs[i].register_symbol);
    }
    if (!json_file_string_array(f, "register_symbols", register_symbols, error)
            || !json_file_string_array(f, "provided_qore_symbols", plan.provided_qore_symbols, error)
            || !json_file_string_array(f, "native_symbols", plan.native_symbols, error)
            || !json_file_string_array(f, "external_dependencies", plan.external_dependencies, error)
            || !json_file_string_array(f, "module_path_prepend", plan.module_path_prepend, error)
            || !json_file_string_array(f, "module_path_append", plan.module_path_append, error)) {
        fclose(f);
        return false;
    }

    fputs("  \"module_commands\": [", f);
    for (size_t i = 0; i < plan.module_commands.size(); ++i) {
        if (!qo_link_check_cancel(i, "AOT qo-link map module-command write", error)) {
            fclose(f);
            return false;
        }
        const AOTModuleCommand& cmd = plan.module_commands[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"module\": ", f);
        json_file_string(f, cmd.module);
        fputs(", \"command\": ", f);
        json_file_string(f, cmd.command);
        fputs("}", f);
    }
    if (!plan.module_commands.empty()) {
        fputc('\n', f);
        fputs("  ", f);
    }
    fputs("],\n", f);

    if (!json_file_issue_array(f, "resolved_imports", plan.resolved_imports, error)
            || !json_file_issue_array(f, "unresolved_imports", plan.unresolved_imports, error)
            || !json_file_issue_array(f, "ambiguous_imports", plan.ambiguous_imports, error)
            || !json_file_issue_array(f, "hash_mismatches", plan.hash_mismatches, error)
            || !json_file_call_relocation_summary(f, plan, error)
            || !json_file_call_relocation_array(f, "resolved_call_relocations",
                plan.resolved_call_relocations, error)
            || !json_file_call_relocation_array(f, "unresolved_call_relocations",
                plan.unresolved_call_relocations, error)
            || !json_file_call_relocation_array(f, "ambiguous_call_relocations",
                plan.ambiguous_call_relocations, error)
            || !json_file_call_relocation_array(f, "call_relocation_hash_mismatches",
                plan.call_relocation_hash_mismatches, error, "")) {
        fclose(f);
        return false;
    }
    fputs("}\n", f);

    if (fclose(f) != 0) {
        error = "cannot close qo link map '" + path + "': " + strerror(errno);
        return false;
    }
    return true;
}

static void print_qo_link_issues(const char* label, const std::vector<QOLinkIssue>& issues) {
    for (size_t i = 0; i < issues.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("AOT qo-link issue reporting")) {
            fprintf(stderr, "error: operation cancelled during AOT qo-link issue reporting\n");
            return;
        }
        const QOLinkIssue& issue = issues[i];
        fprintf(stderr, "error: qo-link %s: consumer='%s' symbol='%s' class='%s'",
            label, issue.consumer.c_str(), issue.path.c_str(), issue.dependency_class.c_str());
        if (!issue.expected.empty()) {
            fprintf(stderr, " %s", issue.expected.c_str());
        }
        if (!issue.providers.empty()) {
            fprintf(stderr, " providers=");
            for (size_t j = 0; j < issue.providers.size(); ++j) {
                if (j && !(j % 100) && qcc_check_cancel("AOT qo-link issue provider reporting")) {
                    fprintf(stderr, "\nerror: operation cancelled during AOT qo-link issue provider reporting\n");
                    return;
                }
                fprintf(stderr, "%s%s", j ? "," : "", issue.providers[j].c_str());
            }
        }
        fputc('\n', stderr);
    }
}

static void print_qo_link_call_relocation_issues(const char* label,
        const std::vector<QOLinkCallRelocation>& relocs) {
    for (size_t i = 0; i < relocs.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("AOT qo-link call-relocation issue reporting")) {
            fprintf(stderr, "error: operation cancelled during AOT qo-link call-relocation issue reporting\n");
            return;
        }
        const QOLinkCallRelocation& reloc = relocs[i];
        fprintf(stderr, "error: qo-link %s call relocation: consumer='%s' function='%s' slot=%u "
            "target='%s' symbol='%s' reason='%s'",
            label, reloc.consumer.c_str(), reloc.function_name.c_str(), reloc.expr_slot,
            reloc.target_kind.c_str(), reloc.path.c_str(),
            reloc.reason.empty() ? reloc.resolution.c_str() : reloc.reason.c_str());
        if (!reloc.expected.empty()) {
            fprintf(stderr, " %s", reloc.expected.c_str());
        }
        if (!reloc.providers.empty()) {
            fprintf(stderr, " providers=");
            for (size_t j = 0; j < reloc.providers.size(); ++j) {
                if (j && !(j % 100)
                        && qcc_check_cancel("AOT qo-link call-relocation provider reporting")) {
                    fprintf(stderr,
                        "\nerror: operation cancelled during AOT qo-link call-relocation provider reporting\n");
                    return;
                }
                fprintf(stderr, "%s%s", j ? "," : "", reloc.providers[j].c_str());
            }
        }
        fputc('\n', stderr);
    }
}

static void dump_object_sections(const llvm::object::ObjectFile& obj) {
    printf("  object sections:\n");
    for (const llvm::object::SectionRef& sec : obj.sections()) {
        auto name_or = sec.getName();
        std::string name;
        if (name_or) {
            name = name_or->str();
        } else {
            llvm::consumeError(name_or.takeError());
        }
        printf("    0x%016llx %10llu %s\n",
            static_cast<unsigned long long>(sec.getAddress()),
            static_cast<unsigned long long>(sec.getSize()),
            name.c_str());
    }
}

static void dump_object_symbols(const llvm::object::ObjectFile& obj, bool all_symbols) {
    std::vector<AOTDumpSymbolRow> rows = collect_symbol_rows(obj);
    size_t qore_count = 0;
    for (const AOTDumpSymbolRow& row : rows) {
        if (row.qore_relevant) {
            ++qore_count;
        }
    }

    printf("  symbols: %zu total, %zu qore/aot%s\n", rows.size(), qore_count,
        all_symbols ? "" : " (important symbols shown)");
    for (const AOTDumpSymbolRow& row : rows) {
        if (!all_symbols && !row.qore_relevant) {
            continue;
        }
        printf("    %016llx %8llu %c %-18s %s\n",
            static_cast<unsigned long long>(row.value),
            static_cast<unsigned long long>(row.size),
            row.kind, row.section.c_str(), row.name.c_str());
    }
}

static void dump_object_symbol_summary(const llvm::object::ObjectFile& obj) {
    std::vector<AOTDumpSymbolRow> rows = collect_symbol_rows(obj);
    size_t qore_count = 0;
    for (const AOTDumpSymbolRow& row : rows) {
        if (row.qore_relevant) {
            ++qore_count;
        }
    }

    printf("  symbols: %zu total, %zu qore/aot (use --dump-symbols to list)\n",
        rows.size(), qore_count);
}

static void inspect_object_file(const char* path,
        std::vector<AOTDumpMetadataBlob>& blobs, std::set<std::string>& seen) {
    auto binary_or = llvm::object::createBinary(path);
    if (!binary_or) {
        if (dump_symbols || dump_sections) {
            std::string msg = llvm::toString(binary_or.takeError());
            printf("  object: not recognized: %s\n", msg.c_str());
        } else {
            llvm::consumeError(binary_or.takeError());
        }
        return;
    }

    llvm::object::Binary* binary = binary_or->getBinary();
    auto* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
    if (!obj) {
        printf("  object: %s\n", binary->getFileName().str().c_str());
        printf("  object symbols/sections: unsupported container type\n");
        return;
    }

    printf("  object: %s\n", obj->getFileFormatName().str().c_str());
    extract_aot_metadata_from_object(*obj, blobs, seen);
    if (dump_sections) {
        dump_object_sections(*obj);
    }
    if (dump_symbols) {
        dump_object_symbols(*obj, true);
    } else if (dump_info) {
        dump_object_symbol_summary(*obj);
    }
}

static void collect_object_metadata_quiet(const char* path,
        std::vector<AOTDumpMetadataBlob>& blobs, std::set<std::string>& seen) {
    auto binary_or = llvm::object::createBinary(path);
    if (!binary_or) {
        llvm::consumeError(binary_or.takeError());
        return;
    }

    llvm::object::Binary* binary = binary_or->getBinary();
    auto* obj = llvm::dyn_cast<llvm::object::ObjectFile>(binary);
    if (!obj) {
        return;
    }

    extract_aot_metadata_from_object(*obj, blobs, seen);
}

static std::string hex64_string(uint64_t value) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(value));
    return buf;
}

static bool json_output_cancelled = false;

static void json_print_string(const std::string& value) {
    putchar('"');
    size_t i = 0;
    for (unsigned char c : value) {
        if (i && !(i % 100) && qcc_check_cancel("AOT symbol-index JSON string dump")) {
            fprintf(stderr, "error: operation cancelled during AOT symbol-index JSON string dump\n");
            json_output_cancelled = true;
            return;
        }
        ++i;
        switch (c) {
            case '"':
                printf("\\\"");
                break;
            case '\\':
                printf("\\\\");
                break;
            case '\b':
                printf("\\b");
                break;
            case '\f':
                printf("\\f");
                break;
            case '\n':
                printf("\\n");
                break;
            case '\r':
                printf("\\r");
                break;
            case '\t':
                printf("\\t");
                break;
            default:
                if (c < 0x20) {
                    printf("\\u%04x", static_cast<unsigned>(c));
                } else {
                    putchar(c);
                }
                break;
        }
    }
    putchar('"');
}

static void json_print_key(const char* key, unsigned indent) {
    printf("%*s", static_cast<int>(indent), "");
    json_print_string(key ? key : "");
    printf(": ");
}

static void json_print_string_field(const char* key, const std::string& value,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    json_print_string(value);
    printf("%s\n", comma ? "," : "");
}

static void json_print_u64_field(const char* key, uint64_t value,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    printf("%llu%s\n", static_cast<unsigned long long>(value), comma ? "," : "");
}

static bool json_dump_check_cancel(size_t ordinal, const char* operation) {
    if (ordinal && !(ordinal % 100) && qcc_check_cancel(operation)) {
        fprintf(stderr, "error: operation cancelled during %s\n",
            operation ? operation : "AOT symbol-index JSON dump");
        return false;
    }
    return true;
}

static bool json_print_string_array(const char* key, const std::vector<std::string>& values,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    printf("[");
    for (size_t i = 0; i < values.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON string-array dump")) {
            return false;
        }
        if (i) {
            printf(", ");
        }
        json_print_string(values[i]);
    }
    printf("]%s\n", comma ? "," : "");
    return true;
}

static bool json_print_context_array(const char* key,
        const std::vector<std::pair<std::string, std::string>>& values,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    printf("[");
    for (size_t i = 0; i < values.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON context dump")) {
            return false;
        }
        if (i) {
            printf(",");
        }
        printf("\n%*s{", static_cast<int>(indent + 2), "");
        json_print_string("key");
        printf(": ");
        json_print_string(values[i].first);
        printf(", ");
        json_print_string("value");
        printf(": ");
        json_print_string(values[i].second);
        printf("}");
    }
    if (!values.empty()) {
        printf("\n%*s", static_cast<int>(indent), "");
    }
    printf("]%s\n", comma ? "," : "");
    return true;
}

static bool json_collect_context_values(const std::vector<std::pair<std::string, std::string>>& values,
        const char* key, std::vector<std::string>& out) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON context value collection")) {
            return false;
        }
        if (values[i].first == key) {
            out.push_back(values[i].second);
        }
    }
    return true;
}

static void json_print_symbol_record(const QoreAOTSymbolIndexRecord& rec, unsigned indent) {
    printf("%*s{", static_cast<int>(indent), "");
    json_print_string("kind");
    printf(": ");
    json_print_string(qoreAOTSymbolKindName(rec.kind));
    printf(", ");
    json_print_string("dependency_class");
    printf(": ");
    json_print_string(qoreAOTDependencyClassName(rec.dependency_class));
    printf(", ");
    json_print_string("flags");
    printf(": %u, ", rec.flags);
    json_print_string("metadata_slot");
    if (rec.metadata_slot == UINT32_MAX) {
        printf(": null");
    } else {
        printf(": %u", rec.metadata_slot);
    }
    printf(", ");
    json_print_string("qore_path");
    printf(": ");
    json_print_string(rec.qore_path);
    printf(", ");
    json_print_string("source_file");
    printf(": ");
    json_print_string(rec.source_file);
    printf(", ");
    json_print_string("visibility");
    printf(": ");
    json_print_string(rec.visibility);
    printf(", ");
    json_print_string("signature_hash");
    printf(": ");
    json_print_string(rec.signature_hash);
    printf(", ");
    json_print_string("declaration_hash");
    printf(": ");
    json_print_string(rec.declaration_hash);
    printf(", ");
    json_print_string("value_hash");
    printf(": ");
    json_print_string(rec.value_hash);
    printf(", ");
    json_print_string("native_symbol");
    printf(": ");
    json_print_string(rec.native_symbol);
    printf(", ");
    json_print_string("abi_kind");
    printf(": ");
    json_print_string(rec.abi_kind);
    printf(", ");
    json_print_string("consumer_source_file");
    printf(": ");
    json_print_string(rec.consumer_source_file);
    printf(", ");
    json_print_string("provider_source_file");
    printf(": ");
    json_print_string(rec.provider_source_file);
    printf(", \"fast_entry_flags\": %u, \"fast_entry_num_params\": %u, \"fast_return_kind\": %u",
        rec.fast_entry_flags, rec.fast_entry_num_params, rec.fast_return_kind);
    auto print_bytes = [](const char* name, const std::vector<uint8_t>& values) {
        printf(", \"%s\": [", name);
        for (size_t i = 0; i < values.size(); ++i) {
            if (i && !(i % 100)
                    && qcc_check_cancel("AOT fast-entry parameter JSON dump")) {
                break;
            }
            printf("%s%u", i ? ", " : "", values[i]);
        }
        printf("]");
    };
    print_bytes("fast_param_kinds", rec.fast_param_kinds);
    print_bytes("fast_param_rejects_nothing", rec.fast_param_rejects_nothing);
    print_bytes("fast_param_noescape", rec.fast_param_noescape);
    printf(", \"scalar_leaf_kind\": %u, \"scalar_leaf_opcode\": %u"
        ", \"scalar_leaf_lhs_param\": %d, \"scalar_leaf_rhs_param\": %d"
        ", \"scalar_leaf_lhs_int\": " QLLD ", \"scalar_leaf_rhs_int\": " QLLD
        ", \"scalar_leaf_lhs_float\": %.17g, \"scalar_leaf_rhs_float\": %.17g"
        ", \"scalar_leaf_true_scale\": " QLLD ", \"scalar_leaf_true_offset\": " QLLD
        ", \"scalar_leaf_false_scale\": " QLLD ", \"scalar_leaf_false_offset\": " QLLD
        ", \"object_getter_member\": ",
        rec.scalar_leaf_kind, rec.scalar_leaf_opcode,
        rec.scalar_leaf_lhs_param, rec.scalar_leaf_rhs_param,
        static_cast<long long>(rec.scalar_leaf_lhs_int),
        static_cast<long long>(rec.scalar_leaf_rhs_int),
        rec.scalar_leaf_lhs_float, rec.scalar_leaf_rhs_float,
        static_cast<long long>(rec.scalar_leaf_true_scale),
        static_cast<long long>(rec.scalar_leaf_true_offset),
        static_cast<long long>(rec.scalar_leaf_false_scale),
        static_cast<long long>(rec.scalar_leaf_false_offset));
    json_print_string(rec.object_getter_member);
    printf(", \"string_op_kind\": %u, \"string_op_base_param\": %d"
        ", \"string_op_arg0_param\": %d, \"string_op_arg1_param\": %d",
        rec.string_op_kind, rec.string_op_base_param,
        rec.string_op_arg0_param, rec.string_op_arg1_param);
    printf(", \"collection_op_kind\": %u, \"collection_op_base_param\": %d"
        ", \"collection_op_index_param\": %d, \"collection_op_string_index_char\": %s"
        ", \"collection_op_key\": ", rec.collection_op_kind,
        rec.collection_op_base_param, rec.collection_op_index_param,
        rec.collection_op_string_index_char ? "true" : "false");
    json_print_string(rec.collection_op_key);
    printf(", \"composed_int_source_kind\": %u, \"composed_int_base_param\": %d"
        ", \"composed_int_value_param\": %d, \"composed_int_source_scale\": " QLLD
        ", \"composed_int_value_scale\": " QLLD ", \"composed_int_offset\": " QLLD,
        rec.composed_int_source_kind, rec.composed_int_base_param,
        rec.composed_int_value_param,
        static_cast<long long>(rec.composed_int_source_scale),
        static_cast<long long>(rec.composed_int_value_scale),
        static_cast<long long>(rec.composed_int_offset));
    printf(", \"global_int_value_param\": %d, \"global_int_slot\": %d"
        ", \"global_int_value_scale\": " QLLD ", \"global_int_global_scale\": " QLLD
        ", \"global_int_offset\": " QLLD,
        rec.global_int_value_param, rec.global_int_slot,
        static_cast<long long>(rec.global_int_value_scale),
        static_cast<long long>(rec.global_int_global_scale),
        static_cast<long long>(rec.global_int_offset));
    printf(", \"int_expression_nodes\": [");
    for (size_t i = 0; i < rec.int_expression_nodes.size(); ++i) {
        const auto& node = rec.int_expression_nodes[i];
        printf("%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"third\": %u, \"param\": %d"
            ", \"constant\": " QLLD "}", i ? ", " : "", node.kind,
            node.lhs, node.rhs, node.third, node.param,
            static_cast<long long>(node.constant));
    }
    printf("]");
    printf(", \"float_expression_nodes\": [");
    for (size_t i = 0; i < rec.float_expression_nodes.size(); ++i) {
        const auto& node = rec.float_expression_nodes[i];
        printf("%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"param\": %d"
            ", \"constant\": %.17g}", i ? ", " : "", node.kind,
            node.lhs, node.rhs, node.param, node.constant);
    }
    printf("]");
    printf(", \"string_expression_nodes\": [");
    for (size_t i = 0; i < rec.string_expression_nodes.size(); ++i) {
        const auto& node = rec.string_expression_nodes[i];
        printf("%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"third\": %u"
            ", \"param\": %d, \"int_constant\": " QLLD
            ", \"string_constant\": ", i ? ", " : "", node.kind,
            node.lhs, node.rhs, node.third, node.param,
            static_cast<long long>(node.int_constant));
        json_print_string(node.string_constant);
        printf("}");
    }
    printf("]");
    printf("}");
}

static bool json_print_symbol_array(const char* key,
        const std::vector<QoreAOTSymbolIndexRecord>& records,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    printf("[");
    for (size_t i = 0; i < records.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON record dump")) {
            return false;
        }
        if (i) {
            printf(",");
        }
        printf("\n");
        json_print_symbol_record(records[i], indent + 2);
    }
    if (!records.empty()) {
        printf("\n%*s", static_cast<int>(indent), "");
    }
    printf("]%s\n", comma ? "," : "");
    return true;
}

static void json_print_call_relocation_record(const QoreAOTCallRelocationRecord& rec,
        unsigned indent) {
    printf("%*s{", static_cast<int>(indent), "");
    json_print_string("function");
    printf(": ");
    json_print_string(rec.function_name);
    printf(", ");
    json_print_string("expr_slot");
    printf(": %u, ", rec.expr_slot);
    json_print_string("target_kind");
    printf(": ");
    json_print_string(qoreAOTCallRelocationTargetKindName(rec.target_kind));
    printf(", ");
    json_print_string("strictness");
    printf(": ");
    json_print_string(rec.strictness == QoreAOTCallRelocationStrictness::REQUIRED ? "required" : "optional");
    printf(", ");
    json_print_string("qore_path");
    printf(": ");
    json_print_string(rec.qore_path);
    printf(", ");
    json_print_string("path");
    printf(": ");
    json_print_string(rec.qore_path);
    printf(", ");
    json_print_string("signature_hash");
    printf(": ");
    json_print_string(rec.signature_hash);
    printf(", ");
    json_print_string("declaration_hash");
    printf(": ");
    json_print_string(rec.declaration_hash);
    printf(", ");
    json_print_string("native_symbol");
    printf(": ");
    json_print_string(rec.native_symbol);
    printf(", ");
    json_print_string("fallback_descriptor");
    printf(": ");
    json_print_string(rec.fallback_descriptor);
    printf("}");
}

static bool json_print_call_relocation_array(const char* key,
        const std::vector<QoreAOTCallRelocationRecord>& records,
        unsigned indent, bool comma = true) {
    json_print_key(key, indent);
    printf("[");
    for (size_t i = 0; i < records.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT call-relocation JSON record dump")) {
            return false;
        }
        if (i) {
            printf(",");
        }
        printf("\n");
        json_print_call_relocation_record(records[i], indent + 2);
    }
    if (!records.empty()) {
        printf("\n%*s", static_cast<int>(indent), "");
    }
    printf("]%s\n", comma ? "," : "");
    return true;
}

static int dump_aot_index_json_for_file(const char* path) {
    json_output_cancelled = false;
    std::vector<AOTDumpMetadataBlob> blobs;
    std::set<std::string> seen;
    collect_object_metadata_quiet(path, blobs, seen);

    std::string contents;
    if (!read_file(path, contents)) {
        return 1;
    }
    scan_aot_metadata_blobs(contents, blobs, seen);

    QoreAOTSymbolIndex combined;
    combined.version = QORE_AOT_SYMBOL_INDEX_VERSION;
    std::vector<QoreAOTCallRelocationRecord> call_relocations;
    std::vector<std::string> source_text;
    std::set<std::string> source_seen;
    for (size_t i = 0; i < blobs.size(); ++i) {
        if (i && !(i % 100) && qcc_check_cancel("AOT symbol-index JSON dump")) {
            fprintf(stderr, "error: operation cancelled during AOT symbol-index JSON dump\n");
            return 1;
        }
        QoreAOTBinaryReader reader;
        std::string error;
        if (!reader.open(blobs[i].bytes.data(), static_cast<uint32_t>(blobs[i].bytes.size()), error)) {
            fprintf(stderr, "error: invalid AOT metadata in '%s': %s\n", path, error.c_str());
            return 1;
        }
        const char* label = reader.getLabel();
        if (label && source_seen.insert(label).second) {
            source_text.emplace_back(label);
        }
        QoreAOTCallRelocations relocs;
        if (!readCallRelocations(reader, relocs, error)) {
            fprintf(stderr, "error: invalid CALL_RELOCATIONS in '%s': %s\n", path, error.c_str());
            return 1;
        }
        if (relocs.version) {
            call_relocations.insert(call_relocations.end(), relocs.records.begin(), relocs.records.end());
        }
        QoreAOTSymbolIndex index;
        if (!readSymbolIndex(reader, index, error)) {
            fprintf(stderr, "error: invalid SYMBOL_INDEX in '%s': %s\n", path, error.c_str());
            return 1;
        }
        if (!index.version) {
            continue;
        }
        combined.context.insert(combined.context.end(), index.context.begin(), index.context.end());
        combined.defined.insert(combined.defined.end(), index.defined.begin(), index.defined.end());
        combined.imported.insert(combined.imported.end(), index.imported.begin(), index.imported.end());
        combined.native.insert(combined.native.end(), index.native.begin(), index.native.end());
    }

    uint64_t object_hash = XXH64(contents.data(), contents.size(), 0);
    std::string hash = "xxh64:" + hex64_string(object_hash);
    std::vector<std::string> source_parse_defines;
    if (!json_collect_context_values(combined.context, "source_parse_define", source_parse_defines)) {
        return 1;
    }

    printf("{\n");
    json_print_u64_field("format", 1, 2);
    json_print_string_field("output", path, 2);
    json_print_string_field("object_hash", hash, 2);
    json_print_u64_field("object_size", contents.size(), 2);
    json_print_string_field("source", source_text.empty() ? "" : source_text.front(), 2);
    if (!json_print_string_array("source_text", source_text, 2)
            || !json_print_string_array("source_parse_defines", source_parse_defines, 2)
            || !json_print_context_array("context", combined.context, 2)
            || !json_print_symbol_array("defines", combined.defined, 2)
            || !json_print_symbol_array("provides", combined.defined, 2)
            || !json_print_symbol_array("requires", combined.imported, 2)
            || !json_print_symbol_array("native", combined.native, 2)
            || !json_print_call_relocation_array("call_relocations", call_relocations, 2)) {
        return 1;
    }
    json_print_string_field("native_body_hash", hash, 2, false);
    if (json_output_cancelled) {
        return 1;
    }
    printf("}\n");
    return blobs.empty() ? 1 : 0;
}

static bool write_generated_file_if_changed(const std::string& path,
        const std::string& content, std::string& error) {
    std::string old;
    if (is_file(path) && read_file(path.c_str(), old) && old == content) {
        return true;
    }

    std::string dir = dirname_of(path);
    if (dir.empty()) {
        dir = ".";
    }
    std::string pattern = dir + "/." + basename_no_ext(path) + ".tmp.XXXXXX";
    std::vector<char> tmp(pattern.begin(), pattern.end());
    tmp.push_back('\0');
    int fd = mkstemp(tmp.data());
    if (fd < 0) {
        error = "cannot create temporary file for '" + path + "': " + strerror(errno);
        return false;
    }
    FILE* f = fdopen(fd, "wb");
    if (!f) {
        int e = errno;
        close(fd);
        unlink(tmp.data());
        error = "cannot open temporary file for '" + path + "': " + strerror(e);
        return false;
    }
    if (!content.empty()
            && fwrite(content.data(), 1, content.size(), f) != content.size()) {
        int e = errno;
        fclose(f);
        unlink(tmp.data());
        error = "cannot write temporary file for '" + path + "': " + strerror(e);
        return false;
    }
    if (fclose(f) != 0) {
        int e = errno;
        unlink(tmp.data());
        error = "cannot close temporary file for '" + path + "': " + strerror(e);
        return false;
    }
    if (rename(tmp.data(), path.c_str()) != 0) {
        int e = errno;
        unlink(tmp.data());
        error = "cannot replace '" + path + "': " + strerror(e);
        return false;
    }
    return true;
}

static bool read_file_required(const std::string& path, std::string& content,
        std::string& error) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        error = "cannot open '" + path + "': " + strerror(errno);
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        error = "cannot seek '" + path + "': " + strerror(errno);
        fclose(f);
        return false;
    }
    long fsize = ftell(f);
    if (fsize < 0) {
        error = "cannot get size of '" + path + "': " + strerror(errno);
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        error = "cannot seek '" + path + "': " + strerror(errno);
        fclose(f);
        return false;
    }
    content.resize(static_cast<size_t>(fsize));
    if (fsize > 0
            && fread(&content[0], 1, static_cast<size_t>(fsize), f) != static_cast<size_t>(fsize)) {
        error = "cannot read '" + path + "': " + strerror(errno);
        fclose(f);
        content.clear();
        return false;
    }
    if (fclose(f) != 0) {
        error = "cannot close '" + path + "': " + strerror(errno);
        content.clear();
        return false;
    }
    return true;
}

static std::string canonical_existing_path(const std::string& path) {
    char* resolved = realpath(path.c_str(), nullptr);
    if (!resolved) {
        return path;
    }
    std::string rv = resolved;
    free(resolved);
    return rv;
}

static QoreAOTSourceSymbolMap* source_symbol_map_for_kind(QoreAOTSourceSymbolManifest& manifest,
        const std::string& kind) {
    if (kind == "class") {
        return &manifest.classes;
    }
    if (kind == "hashdecl") {
        return &manifest.hashdecls;
    }
    if (kind == "function") {
        return &manifest.functions;
    }
    if (kind == "global") {
        return &manifest.globals;
    }
    return nullptr;
}

static bool read_source_symbol_manifest(const char* path, QoreAOTSourceSymbolManifest& manifest,
        std::string& error) {
    if (!path || !*path) {
        return true;
    }
    std::string contents;
    if (!read_file(path, contents)) {
        error = std::string("cannot read source-symbol manifest: ") + path;
        return false;
    }

    size_t line_no = 0;
    size_t pos = 0;
    bool saw_format = false;
    while (pos <= contents.size()) {
        size_t end = contents.find('\n', pos);
        std::string line = end == std::string::npos
            ? contents.substr(pos)
            : contents.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ++line_no;
        if (!(line_no % 100) && qore_check_cancel(nullptr, "qcc source-symbol manifest read")) {
            error = "operation cancelled during qcc source-symbol manifest read";
            return false;
        }
        pos = end == std::string::npos ? contents.size() + 1 : end + 1;

        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!saw_format) {
            if (line != "format=1") {
                error = "invalid source-symbol manifest '" + std::string(path)
                    + "': expected format=1 on line " + std::to_string(line_no);
                return false;
            }
            saw_format = true;
            continue;
        }

        size_t tab1 = line.find('\t');
        size_t tab2 = tab1 == std::string::npos ? std::string::npos : line.find('\t', tab1 + 1);
        if (tab1 == std::string::npos || tab2 == std::string::npos || tab2 + 1 >= line.size()) {
            error = "invalid source-symbol manifest '" + std::string(path)
                + "': malformed line " + std::to_string(line_no);
            return false;
        }
        std::string kind = line.substr(0, tab1);
        std::string symbol = line.substr(tab1 + 1, tab2 - tab1 - 1);
        std::string source = line.substr(tab2 + 1);
        if (symbol.empty() || source.empty()) {
            error = "invalid source-symbol manifest '" + std::string(path)
                + "': empty symbol or source on line " + std::to_string(line_no);
            return false;
        }
        QoreAOTSourceSymbolMap* map = source_symbol_map_for_kind(manifest, kind);
        if (!map) {
            error = "invalid source-symbol manifest '" + std::string(path)
                + "': unknown symbol kind '" + kind + "' on line " + std::to_string(line_no);
            return false;
        }
        (*map)[symbol].insert(canonical_existing_path(source));
    }
    if (!saw_format) {
        error = "invalid source-symbol manifest '" + std::string(path) + "': missing format=1";
        return false;
    }
    return true;
}

static bool write_json_file_string_field(FILE* f, const char* key,
        const std::string& value, unsigned indent, const char* comma = ",") {
    fprintf(f, "%*s", static_cast<int>(indent), "");
    json_file_string(f, key ? key : "");
    fputs(": ", f);
    json_file_string(f, value);
    fprintf(f, "%s\n", comma);
    return true;
}

static bool write_json_file_u64_field(FILE* f, const char* key, uint64_t value,
        unsigned indent, const char* comma = ",") {
    fprintf(f, "%*s", static_cast<int>(indent), "");
    json_file_string(f, key ? key : "");
    fprintf(f, ": %llu%s\n", static_cast<unsigned long long>(value), comma);
    return true;
}

static bool write_json_file_bool_field(FILE* f, const char* key, bool value,
        unsigned indent, const char* comma = ",") {
    fprintf(f, "%*s", static_cast<int>(indent), "");
    json_file_string(f, key ? key : "");
    fprintf(f, ": %s%s\n", value ? "true" : "false", comma);
    return true;
}

static bool write_json_file_record(FILE* f, const char* key, const std::string& path,
        unsigned indent, std::string& error, const char* comma = ",") {
    std::string contents;
    if (!read_file_required(path, contents, error)) {
        return false;
    }
    uint64_t hash = XXH64(contents.data(), contents.size(), 0);
    fprintf(f, "%*s", static_cast<int>(indent), "");
    json_file_string(f, key ? key : "");
    fputs(": {\"path\": ", f);
    json_file_string(f, canonical_existing_path(path));
    fprintf(f, ", \"size\": %llu, \"hash\": ",
        static_cast<unsigned long long>(contents.size()));
    json_file_string(f, "xxh64:" + hex64_string(hash));
    fprintf(f, "}%s\n", comma);
    return true;
}

static bool write_json_file_record_array(FILE* f, const char* key,
        const std::vector<std::string>& paths, unsigned indent, std::string& error,
        const char* comma = ",") {
    fprintf(f, "%*s", static_cast<int>(indent), "");
    json_file_string(f, key ? key : "");
    fputs(": [", f);
    std::set<std::string> seen;
    std::vector<std::string> canonical_paths;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (!json_dump_check_cancel(i, "qcc manifest input write")) {
            error = "operation cancelled during qcc manifest input write";
            return false;
        }
        if (paths[i].empty() || !is_file(paths[i])) {
            continue;
        }
        std::string canon = canonical_existing_path(paths[i]);
        if (!seen.insert(canon).second) {
            continue;
        }
        canonical_paths.push_back(std::move(canon));
    }
    std::sort(canonical_paths.begin(), canonical_paths.end());
    size_t written = 0;
    for (size_t i = 0; i < canonical_paths.size(); ++i) {
        if (!json_dump_check_cancel(i, "qcc manifest canonical input write")) {
            error = "operation cancelled during qcc manifest canonical input write";
            return false;
        }
        std::string contents;
        if (!read_file_required(canonical_paths[i], contents, error)) {
            return false;
        }
        uint64_t hash = XXH64(contents.data(), contents.size(), 0);
        if (written++) {
            fputc(',', f);
        }
        fprintf(f, "\n%*s{\"path\": ", static_cast<int>(indent + 2), "");
        json_file_string(f, canonical_paths[i]);
        fprintf(f, ", \"size\": %llu, \"hash\": ",
            static_cast<unsigned long long>(contents.size()));
        json_file_string(f, "xxh64:" + hex64_string(hash));
        fputc('}', f);
    }
    if (written) {
        fprintf(f, "\n%*s", static_cast<int>(indent), "");
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool json_file_context_array_for_index(FILE* f, const char* key,
        const std::vector<std::pair<std::string, std::string>>& values,
        std::string& error, const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < values.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON context write")) {
            error = "operation cancelled during AOT symbol-index JSON context write";
            return false;
        }
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"key\": ", f);
        json_file_string(f, values[i].first);
        fputs(", \"value\": ", f);
        json_file_string(f, values[i].second);
        fputc('}', f);
    }
    if (!values.empty()) {
        fputs("\n  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool json_file_symbol_array_for_index(FILE* f, const char* key,
        const std::vector<QoreAOTSymbolIndexRecord>& records,
        std::string& error, const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < records.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON record write")) {
            error = "operation cancelled during AOT symbol-index JSON record write";
            return false;
        }
        const QoreAOTSymbolIndexRecord& rec = records[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"kind\": ", f);
        json_file_string(f, qoreAOTSymbolKindName(rec.kind));
        fputs(", \"dependency_class\": ", f);
        json_file_string(f, qoreAOTDependencyClassName(rec.dependency_class));
        fprintf(f, ", \"flags\": %u, \"metadata_slot\": ", rec.flags);
        if (rec.metadata_slot == UINT32_MAX) {
            fputs("null", f);
        } else {
            fprintf(f, "%u", rec.metadata_slot);
        }
        fputs(", \"qore_path\": ", f);
        json_file_string(f, rec.qore_path);
        fputs(", \"source_file\": ", f);
        json_file_string(f, rec.source_file);
        fputs(", \"visibility\": ", f);
        json_file_string(f, rec.visibility);
        fputs(", \"signature_hash\": ", f);
        json_file_string(f, rec.signature_hash);
        fputs(", \"declaration_hash\": ", f);
        json_file_string(f, rec.declaration_hash);
        fputs(", \"value_hash\": ", f);
        json_file_string(f, rec.value_hash);
        fputs(", \"native_symbol\": ", f);
        json_file_string(f, rec.native_symbol);
        fputs(", \"abi_kind\": ", f);
        json_file_string(f, rec.abi_kind);
        fputs(", \"consumer_source_file\": ", f);
        json_file_string(f, rec.consumer_source_file);
        fputs(", \"provider_source_file\": ", f);
        json_file_string(f, rec.provider_source_file);
        fprintf(f, ", \"fast_entry_flags\": %u, \"fast_entry_num_params\": %u, \"fast_return_kind\": %u",
            rec.fast_entry_flags, rec.fast_entry_num_params, rec.fast_return_kind);
        auto write_bytes = [f](const char* name, const std::vector<uint8_t>& values) {
            fprintf(f, ", \"%s\": [", name);
            for (size_t j = 0; j < values.size(); ++j) {
                if (j && !(j % 100)
                        && qcc_check_cancel("AOT fast-entry parameter JSON write")) {
                    break;
                }
                fprintf(f, "%s%u", j ? ", " : "", values[j]);
            }
            fputc(']', f);
        };
        write_bytes("fast_param_kinds", rec.fast_param_kinds);
        write_bytes("fast_param_rejects_nothing", rec.fast_param_rejects_nothing);
        write_bytes("fast_param_noescape", rec.fast_param_noescape);
        fprintf(f, ", \"scalar_leaf_kind\": %u, \"scalar_leaf_opcode\": %u"
            ", \"scalar_leaf_lhs_param\": %d, \"scalar_leaf_rhs_param\": %d"
            ", \"scalar_leaf_lhs_int\": " QLLD ", \"scalar_leaf_rhs_int\": " QLLD
            ", \"scalar_leaf_lhs_float\": %.17g, \"scalar_leaf_rhs_float\": %.17g"
            ", \"scalar_leaf_true_scale\": " QLLD ", \"scalar_leaf_true_offset\": " QLLD
            ", \"scalar_leaf_false_scale\": " QLLD ", \"scalar_leaf_false_offset\": " QLLD
            ", \"object_getter_member\": ",
            rec.scalar_leaf_kind, rec.scalar_leaf_opcode,
            rec.scalar_leaf_lhs_param, rec.scalar_leaf_rhs_param,
            static_cast<long long>(rec.scalar_leaf_lhs_int),
            static_cast<long long>(rec.scalar_leaf_rhs_int),
            rec.scalar_leaf_lhs_float, rec.scalar_leaf_rhs_float,
            static_cast<long long>(rec.scalar_leaf_true_scale),
            static_cast<long long>(rec.scalar_leaf_true_offset),
            static_cast<long long>(rec.scalar_leaf_false_scale),
            static_cast<long long>(rec.scalar_leaf_false_offset));
        json_file_string(f, rec.object_getter_member);
        fprintf(f, ", \"string_op_kind\": %u, \"string_op_base_param\": %d"
            ", \"string_op_arg0_param\": %d, \"string_op_arg1_param\": %d",
            rec.string_op_kind, rec.string_op_base_param,
            rec.string_op_arg0_param, rec.string_op_arg1_param);
        fprintf(f, ", \"collection_op_kind\": %u, \"collection_op_base_param\": %d"
            ", \"collection_op_index_param\": %d, \"collection_op_string_index_char\": %s"
            ", \"collection_op_key\": ", rec.collection_op_kind,
            rec.collection_op_base_param, rec.collection_op_index_param,
            rec.collection_op_string_index_char ? "true" : "false");
        json_file_string(f, rec.collection_op_key);
        fprintf(f, ", \"composed_int_source_kind\": %u, \"composed_int_base_param\": %d"
            ", \"composed_int_value_param\": %d, \"composed_int_source_scale\": " QLLD
            ", \"composed_int_value_scale\": " QLLD ", \"composed_int_offset\": " QLLD,
            rec.composed_int_source_kind, rec.composed_int_base_param,
            rec.composed_int_value_param,
            static_cast<long long>(rec.composed_int_source_scale),
            static_cast<long long>(rec.composed_int_value_scale),
            static_cast<long long>(rec.composed_int_offset));
        fprintf(f, ", \"global_int_value_param\": %d, \"global_int_slot\": %d"
            ", \"global_int_value_scale\": " QLLD ", \"global_int_global_scale\": " QLLD
            ", \"global_int_offset\": " QLLD,
            rec.global_int_value_param, rec.global_int_slot,
            static_cast<long long>(rec.global_int_value_scale),
            static_cast<long long>(rec.global_int_global_scale),
            static_cast<long long>(rec.global_int_offset));
        fputs(", \"int_expression_nodes\": [", f);
        for (size_t j = 0; j < rec.int_expression_nodes.size(); ++j) {
            const auto& node = rec.int_expression_nodes[j];
            fprintf(f, "%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"third\": %u, \"param\": %d"
                ", \"constant\": " QLLD "}", j ? ", " : "", node.kind,
                node.lhs, node.rhs, node.third, node.param,
                static_cast<long long>(node.constant));
        }
        fputc(']', f);
        fputs(", \"float_expression_nodes\": [", f);
        for (size_t j = 0; j < rec.float_expression_nodes.size(); ++j) {
            const auto& node = rec.float_expression_nodes[j];
            fprintf(f, "%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"param\": %d"
                ", \"constant\": %.17g}", j ? ", " : "", node.kind,
                node.lhs, node.rhs, node.param, node.constant);
        }
        fputc(']', f);
        fputs(", \"string_expression_nodes\": [", f);
        for (size_t j = 0; j < rec.string_expression_nodes.size(); ++j) {
            const auto& node = rec.string_expression_nodes[j];
            fprintf(f, "%s{\"kind\": %u, \"lhs\": %u, \"rhs\": %u, \"third\": %u"
                ", \"param\": %d, \"int_constant\": " QLLD
                ", \"string_constant\": ", j ? ", " : "", node.kind,
                node.lhs, node.rhs, node.third, node.param,
                static_cast<long long>(node.int_constant));
            json_file_string(f, node.string_constant);
            fputc('}', f);
        }
        fputc(']', f);
        fputc('}', f);
    }
    if (!records.empty()) {
        fputs("\n  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool json_file_call_relocations_for_index(FILE* f, const char* key,
        const std::vector<QoreAOTCallRelocationRecord>& records,
        std::string& error, const char* comma = ",") {
    fprintf(f, "  \"%s\": [", key);
    for (size_t i = 0; i < records.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT call-relocation JSON record write")) {
            error = "operation cancelled during AOT call-relocation JSON record write";
            return false;
        }
        const QoreAOTCallRelocationRecord& rec = records[i];
        if (i) {
            fputc(',', f);
        }
        fputs("\n    {\"function\": ", f);
        json_file_string(f, rec.function_name);
        fprintf(f, ", \"expr_slot\": %u, \"target_kind\": ", rec.expr_slot);
        json_file_string(f, qoreAOTCallRelocationTargetKindName(rec.target_kind));
        fputs(", \"strictness\": ", f);
        json_file_string(f, rec.strictness == QoreAOTCallRelocationStrictness::REQUIRED
            ? "required" : "optional");
        fputs(", \"qore_path\": ", f);
        json_file_string(f, rec.qore_path);
        fputs(", \"path\": ", f);
        json_file_string(f, rec.qore_path);
        fputs(", \"signature_hash\": ", f);
        json_file_string(f, rec.signature_hash);
        fputs(", \"declaration_hash\": ", f);
        json_file_string(f, rec.declaration_hash);
        fputs(", \"native_symbol\": ", f);
        json_file_string(f, rec.native_symbol);
        fputs(", \"fallback_descriptor\": ", f);
        json_file_string(f, rec.fallback_descriptor);
        fputc('}', f);
    }
    if (!records.empty()) {
        fputs("\n  ", f);
    }
    fprintf(f, "]%s\n", comma);
    return true;
}

static bool write_aot_index_json_stream(FILE* f, const char* path,
        bool allow_empty_metadata, std::string& error) {
    std::vector<AOTDumpMetadataBlob> blobs;
    std::set<std::string> seen;
    collect_object_metadata_quiet(path, blobs, seen);

    std::string contents;
    if (!read_file_required(path, contents, error)) {
        return false;
    }
    scan_aot_metadata_blobs(contents, blobs, seen);

    QoreAOTSymbolIndex combined;
    combined.version = QORE_AOT_SYMBOL_INDEX_VERSION;
    std::vector<QoreAOTCallRelocationRecord> call_relocations;
    std::vector<std::string> source_text;
    std::set<std::string> source_seen;
    for (size_t i = 0; i < blobs.size(); ++i) {
        if (!json_dump_check_cancel(i, "AOT symbol-index JSON write")) {
            error = "operation cancelled during AOT symbol-index JSON write";
            return false;
        }
        QoreAOTBinaryReader reader;
        if (!reader.open(blobs[i].bytes.data(), static_cast<uint32_t>(blobs[i].bytes.size()), error)) {
            error = "invalid AOT metadata in '" + std::string(path) + "': " + error;
            return false;
        }
        const char* label = reader.getLabel();
        if (label && source_seen.insert(label).second) {
            source_text.emplace_back(label);
        }
        QoreAOTCallRelocations relocs;
        if (!readCallRelocations(reader, relocs, error)) {
            error = "invalid CALL_RELOCATIONS in '" + std::string(path) + "': " + error;
            return false;
        }
        if (relocs.version) {
            call_relocations.insert(call_relocations.end(), relocs.records.begin(), relocs.records.end());
        }
        QoreAOTSymbolIndex index;
        if (!readSymbolIndex(reader, index, error)) {
            error = "invalid SYMBOL_INDEX in '" + std::string(path) + "': " + error;
            return false;
        }
        if (!index.version) {
            continue;
        }
        combined.context.insert(combined.context.end(), index.context.begin(), index.context.end());
        combined.defined.insert(combined.defined.end(), index.defined.begin(), index.defined.end());
        combined.imported.insert(combined.imported.end(), index.imported.begin(), index.imported.end());
        combined.native.insert(combined.native.end(), index.native.begin(), index.native.end());
    }

    uint64_t object_hash = XXH64(contents.data(), contents.size(), 0);
    std::string hash = "xxh64:" + hex64_string(object_hash);
    std::vector<std::string> source_parse_defines;
    if (!json_collect_context_values(combined.context, "source_parse_define", source_parse_defines)) {
        error = "operation cancelled during AOT symbol-index parse-define collection";
        return false;
    }

    fputs("{\n", f);
    write_json_file_u64_field(f, "format", 1, 2);
    write_json_file_string_field(f, "output", path, 2);
    write_json_file_string_field(f, "object_hash", hash, 2);
    write_json_file_u64_field(f, "object_size", contents.size(), 2);
    write_json_file_string_field(f, "source", source_text.empty() ? "" : source_text.front(), 2);
    if (!json_file_string_array(f, "source_text", source_text, error)
            || !json_file_string_array(f, "source_parse_defines", source_parse_defines, error)
            || !json_file_context_array_for_index(f, "context", combined.context, error)
            || !json_file_symbol_array_for_index(f, "defines", combined.defined, error)
            || !json_file_symbol_array_for_index(f, "provides", combined.defined, error)
            || !json_file_symbol_array_for_index(f, "requires", combined.imported, error)
            || !json_file_symbol_array_for_index(f, "native", combined.native, error)
            || !json_file_call_relocations_for_index(f, "call_relocations", call_relocations, error)) {
        return false;
    }
    write_json_file_string_field(f, "native_body_hash", hash, 2, "");
    fputs("}\n", f);
    if (json_output_cancelled) {
        error = "operation cancelled during AOT symbol-index JSON write";
        return false;
    }
    if (blobs.empty() && !allow_empty_metadata) {
        error = "no AOT metadata found in '" + std::string(path) + "'";
        return false;
    }
    return true;
}

static bool write_aot_index_json_file(const char* index_path, const char* object_path,
        bool allow_empty_metadata, std::string& error) {
    json_output_cancelled = false;
    char* buf = nullptr;
    size_t size = 0;
    FILE* f = open_memstream(&buf, &size);
    if (!f) {
        error = "cannot allocate AOT index JSON buffer: " + std::string(strerror(errno));
        return false;
    }
    bool ok = write_aot_index_json_stream(f, object_path, allow_empty_metadata, error);
    if (fclose(f) != 0 && ok) {
        error = "cannot close AOT index JSON buffer: " + std::string(strerror(errno));
        ok = false;
    }
    std::string content;
    if (ok) {
        content.assign(buf, size);
    }
    free(buf);
    return ok && write_generated_file_if_changed(index_path, content, error);
}

static void read_make_depfile_inputs(const std::string& depfile,
        std::vector<std::string>& out) {
    std::string text;
    if (!is_file(depfile) || !read_file(depfile.c_str(), text)) {
        return;
    }
    size_t pos = text.find(':');
    if (pos == std::string::npos) {
        return;
    }
    ++pos;
    std::string token;
    bool escaped = false;
    for (; pos < text.size(); ++pos) {
        char c = text[pos];
        if (escaped) {
            if (c != '\n') {
                token.push_back(c);
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) {
                out.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(c);
    }
    if (!token.empty()) {
        out.push_back(token);
    }
}

static void collect_qo_library_inputs(std::vector<std::string>& out,
        const std::vector<std::string>& dirs, const std::string& output) {
    std::string output_canon = output.empty() ? "" : canonical_existing_path(output);
    for (const std::string& dir_path : dirs) {
        DIR* d = opendir(dir_path.c_str());
        if (!d) {
            continue;
        }
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (!has_qo_extension(ent->d_name)) {
                continue;
            }
            std::string path = dir_path + "/" + ent->d_name;
            std::string canon = canonical_existing_path(path);
            if (!output_canon.empty() && canon == output_canon) {
                continue;
            }
            out.push_back(canon);
        }
        closedir(d);
    }
}

static std::string resolve_command_path(const char* argv0) {
    if (!argv0 || !*argv0) {
        return "";
    }
    if (strchr(argv0, '/')) {
        return canonical_existing_path(argv0);
    }
    const char* path_env = getenv("PATH");
    if (!path_env) {
        return argv0;
    }
    const char* start = path_env;
    for (const char* p = path_env; ; ++p) {
        if (*p == ':' || *p == '\0') {
            std::string dir(start, p - start);
            std::string candidate = (dir.empty() ? "." : dir) + "/" + argv0;
            if (access(candidate.c_str(), X_OK) == 0) {
                return canonical_existing_path(candidate);
            }
            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }
    return argv0;
}

struct QCCBuildManifest {
    std::string kind;
    std::string output;
    std::string index_json;
    std::string depfile;
    std::string link_map;
    std::string aggregate_symbol;
    std::vector<std::string> inputs;
    std::vector<std::string> extra_inputs;
};

struct QCCFileFingerprint {
    bool exists = false;
    uint64_t size = 0;
    uint64_t hash = 0;
};

static QCCFileFingerprint qcc_file_fingerprint(const std::string& path) {
    QCCFileFingerprint rv;
    if (path.empty() || !is_file(path)) {
        return rv;
    }

    std::string contents;
    std::string error;
    if (!read_file_required(path, contents, error)) {
        return rv;
    }
    rv.exists = true;
    rv.size = contents.size();
    rv.hash = XXH64(contents.data(), contents.size(), 0);
    return rv;
}

static bool qcc_file_fingerprint_equal(const QCCFileFingerprint& a,
        const QCCFileFingerprint& b) {
    return a.exists == b.exists && a.size == b.size && a.hash == b.hash;
}

static std::string qcc_fingerprint_hash_string(const QCCFileFingerprint& fp) {
    return fp.exists ? "xxh64:" + hex64_string(fp.hash) : "";
}

static bool touch_qcc_file(const char* path, std::string& error) {
    if (!path || !*path) {
        return true;
    }

    FILE* f = fopen(path, "ab");
    if (!f) {
        error = "cannot open stamp '" + std::string(path) + "': " + strerror(errno);
        return false;
    }
    if (fclose(f) != 0) {
        error = "cannot close stamp '" + std::string(path) + "': " + strerror(errno);
        return false;
    }
    if (utime(path, nullptr) != 0) {
        error = "cannot touch stamp '" + std::string(path) + "': " + strerror(errno);
        return false;
    }
    return true;
}

static bool write_qcc_status_json_file(const QCCBuildManifest& manifest,
        bool built, bool skipped, bool output_changed,
        const QCCFileFingerprint& output_fp, std::string& error) {
    if (!write_status_json_path) {
        return true;
    }

    char* buf = nullptr;
    size_t size = 0;
    FILE* f = open_memstream(&buf, &size);
    if (!f) {
        error = "cannot allocate qcc status JSON buffer: " + std::string(strerror(errno));
        return false;
    }

    fputs("{\n", f);
    write_json_file_u64_field(f, "format", 1, 2);
    write_json_file_string_field(f, "tool", "qcc", 2);
    write_json_file_string_field(f, "kind", manifest.kind, 2);
    write_json_file_string_field(f, "output", manifest.output, 2);
    write_json_file_bool_field(f, "built", built, 2);
    write_json_file_bool_field(f, "skipped", skipped, 2);
    write_json_file_bool_field(f, "manifest_current", skipped, 2);
    write_json_file_bool_field(f, "output_changed", output_changed, 2);
    write_json_file_u64_field(f, "output_size", output_fp.size, 2);
    write_json_file_string_field(f, "output_hash", qcc_fingerprint_hash_string(output_fp), 2);
    fprintf(f, "  \"sidecars\": {\n");
    write_json_file_string_field(f, "status_json", write_status_json_path ? write_status_json_path : "", 4);
    write_json_file_string_field(f, "index_json", manifest.index_json, 4);
    write_json_file_string_field(f, "manifest", write_manifest_path ? write_manifest_path : "", 4);
    write_json_file_string_field(f, "depfile", manifest.depfile, 4);
    write_json_file_string_field(f, "link_map", manifest.link_map, 4);
    write_json_file_string_field(f, "success_stamp", success_stamp_path ? success_stamp_path : "", 4);
    write_json_file_string_field(f, "content_stamp", content_stamp_path ? content_stamp_path : "", 4, "");
    fputs("  }\n", f);
    fputs("}\n", f);

    bool ok = true;
    if (fclose(f) != 0) {
        error = "cannot close qcc status JSON buffer: " + std::string(strerror(errno));
        ok = false;
    }
    std::string content;
    if (ok) {
        content.assign(buf, size);
    }
    free(buf);
    return ok && write_generated_file_if_changed(write_status_json_path, content, error);
}

static bool build_qcc_manifest_content(const QCCBuildManifest& manifest,
        const char* argv0, std::string& content, std::string& error) {
    if (manifest.output.empty() || !is_file(manifest.output)) {
        return false;
    }

    char* buf = nullptr;
    size_t size = 0;
    FILE* f = open_memstream(&buf, &size);
    if (!f) {
        error = "cannot allocate qcc manifest buffer: " + std::string(strerror(errno));
        return false;
    }

    fputs("{\n", f);
    write_json_file_u64_field(f, "format", 1, 2);
    write_json_file_string_field(f, "tool", "qcc", 2);
    write_json_file_string_field(f, "manifest_version", "2026-06-14-qcc-build-v1", 2);
    write_json_file_string_field(f, "kind", manifest.kind, 2);
    std::string qcc_path = resolve_command_path(argv0);
    if (is_file(qcc_path)) {
        if (!write_json_file_record(f, "qcc", qcc_path, 2, error)) {
            fclose(f);
            free(buf);
            return false;
        }
    } else {
        write_json_file_string_field(f, "qcc", qcc_path, 2);
    }
    if (!write_json_file_record(f, "output", manifest.output, 2, error)) {
        fclose(f);
        free(buf);
        return false;
    }

    fprintf(f, "  \"sidecars\": {\n");
    write_json_file_string_field(f, "index_json", manifest.index_json, 4);
    write_json_file_string_field(f, "depfile", manifest.depfile, 4);
    write_json_file_string_field(f, "link_map", manifest.link_map, 4, "");
    fputs("  },\n", f);

    fprintf(f, "  \"options\": {\n");
    write_json_file_u64_field(f, "opt_level", opt_level, 4);
    write_json_file_string_field(f, "target_triple", target_triple ? target_triple : "", 4);
    write_json_file_bool_field(f, "include_source", include_source, 4);
    write_json_file_bool_field(f, "strip_debug_info", strip_debug_info, 4);
    write_json_file_bool_field(f, "warnings_are_errors", warnings_are_errors, 4);
    write_json_file_bool_field(f, "allow_unresolved_imports", allow_unresolved_qo_imports, 4);
    write_json_file_bool_field(f, "strict_call_relocations", strict_call_relocations, 4);
    write_json_file_bool_field(f, "script_aggregate_native_registers",
        script_aggregate_native_registers, 4);
    write_json_file_bool_field(f, "depfile_qo_input_content_stamps",
        depfile_qo_input_content_stamps, 4);
    write_json_file_string_field(f, "aggregate_symbol", manifest.aggregate_symbol, 4, "");
    fputs("  },\n", f);

    fprintf(f, "  \"environment\": {\n");
    const char* qore_include_dir = getenv("QORE_INCLUDE_DIR");
    const char* qore_module_dir = getenv("QORE_MODULE_DIR");
    const char* qore_big_fn_threshold = getenv("QORE_AOT_BIG_FN_THRESHOLD");
    const char* qore_metadata_compression = getenv("QORE_AOT_METADATA_COMPRESSION");
    write_json_file_string_field(f, "QORE_INCLUDE_DIR", qore_include_dir ? qore_include_dir : "", 4);
    write_json_file_string_field(f, "QORE_MODULE_DIR", qore_module_dir ? qore_module_dir : "", 4);
    write_json_file_string_field(f, "QORE_AOT_BIG_FN_THRESHOLD",
        qore_big_fn_threshold ? qore_big_fn_threshold : "", 4);
    write_json_file_string_field(f, "QORE_AOT_METADATA_COMPRESSION",
        qore_metadata_compression ? qore_metadata_compression : "", 4, "");
    fputs("  },\n", f);

    std::vector<std::string> all_inputs = manifest.inputs;
    for (const std::string& stub : stub_files) {
        all_inputs.push_back(stub);
    }
    all_inputs.insert(all_inputs.end(), manifest.extra_inputs.begin(), manifest.extra_inputs.end());
    all_inputs.insert(all_inputs.end(), manifest_inputs.begin(), manifest_inputs.end());
    if (!manifest.depfile.empty() && is_file(manifest.depfile)) {
        all_inputs.push_back(manifest.depfile);
        read_make_depfile_inputs(manifest.depfile, all_inputs);
    }
    if (!manifest.link_map.empty() && is_file(manifest.link_map)) {
        all_inputs.push_back(manifest.link_map);
    }
    if (!manifest.index_json.empty() && is_file(manifest.index_json)) {
        all_inputs.push_back(manifest.index_json);
    }
    if (!write_json_file_record_array(f, "inputs", all_inputs, 2, error)) {
        fclose(f);
        free(buf);
        return false;
    }

    fprintf(f, "  \"parse_defines\": [");
    for (size_t i = 0; i < parse_defines.size(); ++i) {
        if (i) {
            fputs(", ", f);
        }
        json_file_string(f, parse_defines[i]);
    }
    fputs("],\n", f);
    fprintf(f, "  \"parse_options\": [");
    for (size_t i = 0; i < parse_option_flags.size(); ++i) {
        if (i) {
            fputs(", ", f);
        }
        json_file_string(f, parse_option_flags[i]);
    }
    fputs("],\n", f);
    fprintf(f, "  \"load_modules\": [");
    for (size_t i = 0; i < load_modules.size(); ++i) {
        if (i) {
            fputs(", ", f);
        }
        json_file_string(f, load_modules[i]);
    }
    fputs("]\n", f);
    fputs("}\n", f);

    if (fclose(f) != 0) {
        error = "cannot close qcc manifest buffer: " + std::string(strerror(errno));
        free(buf);
        return false;
    }
    content.assign(buf, size);
    free(buf);
    return true;
}

static bool qcc_manifest_current(const QCCBuildManifest& manifest, const char* argv0) {
    if (!write_manifest_path || !is_file(write_manifest_path)) {
        return false;
    }
    std::string expected;
    std::string error;
    if (!build_qcc_manifest_content(manifest, argv0, expected, error)) {
        return false;
    }
    std::string old;
    return read_file(write_manifest_path, old) && old == expected;
}

static bool write_qcc_manifest_file(const QCCBuildManifest& manifest,
        const char* argv0, std::string& error) {
    if (!write_manifest_path) {
        return true;
    }
    std::string content;
    if (!build_qcc_manifest_content(manifest, argv0, content, error)) {
        if (error.empty()) {
            error = "cannot build qcc manifest for '" + manifest.output + "'";
        }
        return false;
    }
    return write_generated_file_if_changed(write_manifest_path, content, error);
}

static bool write_requested_sidecars(const QCCBuildManifest& manifest,
        const char* argv0, bool allow_empty_index, std::string& error) {
    if (write_index_json_path
            && !write_aot_index_json_file(write_index_json_path, manifest.output.c_str(),
                allow_empty_index, error)) {
        return false;
    }
    return write_qcc_manifest_file(manifest, argv0, error);
}

static bool finish_qcc_build(const QCCBuildManifest& manifest, const char* argv0,
        const QCCFileFingerprint& before, bool built, bool skipped,
        bool allow_empty_index, std::string& error) {
    if (built && !write_requested_sidecars(manifest, argv0, allow_empty_index, error)) {
        return false;
    }

    QCCFileFingerprint after = qcc_file_fingerprint(manifest.output);
    if (!after.exists) {
        error = "generated output missing after successful qcc command: '" + manifest.output + "'";
        return false;
    }

    bool output_changed = built && !qcc_file_fingerprint_equal(before, after);
    if (!write_qcc_status_json_file(manifest, built, skipped, output_changed, after, error)) {
        return false;
    }
    bool content_stamp_missing = content_stamp_path && !is_file(content_stamp_path);
    if (content_stamp_path && (output_changed || ((built || skipped) && content_stamp_missing))
            && !touch_qcc_file(content_stamp_path, error)) {
        return false;
    }
    if (success_stamp_path && !touch_qcc_file(success_stamp_path, error)) {
        return false;
    }
    return true;
}

static bool qcc_single_output_sidecars_requested() {
    return write_index_json_path || write_manifest_path || skip_if_manifest_current
        || write_status_json_path || success_stamp_path || content_stamp_path;
}

static void add_existing_qo_inputs_from_args(std::vector<std::string>& inputs,
        int begin, int argc, char** argv) {
    for (int i = begin; i < argc; ++i) {
        inputs.push_back(canonical_existing_path(argv[i]));
    }
}

static int dump_aot_info_for_file(const char* path) {
    printf("%s:\n", path);

    std::vector<AOTDumpMetadataBlob> blobs;
    std::set<std::string> seen;
    inspect_object_file(path, blobs, seen);

    std::string contents;
    if (!read_file(path, contents)) {
        return 1;
    }
    scan_aot_metadata_blobs(contents, blobs, seen);

    if (blobs.empty()) {
        printf("  AOT metadata: none found\n");
    } else {
        for (size_t i = 0; i < blobs.size(); ++i) {
            dump_aot_metadata_blob(blobs[i], i + 1);
        }
    }

    return 0;
}

static std::string get_default_output(const char* input_path, bool is_module,
        bool compile_only_mode = false) {
    std::string output = input_path;

    // Strip directory path for basename
    size_t slash = output.rfind('/');
    if (slash != std::string::npos) {
        output = output.substr(slash + 1);
    }

    // Strip extension
    size_t dot = output.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        output = output.substr(0, dot);
    }

    // -c emits a relocatable .qo regardless of source kind
    if (compile_only_mode) {
        output += ".qo";
    } else if (is_module) {
        output += ".qmod";
    }

    return output;
}

//! Create a private temporary directory for qcc-generated objects.
static bool make_qcc_temp_dir(std::string& temp_dir, std::string& error) {
    const char* base = std::getenv("TMPDIR");
    if (!base || !*base) {
        base = "/tmp";
    }
    std::string tmpl = join_path(base, "qcc-aot-XXXXXX");
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* path = mkdtemp(buf.data());
    if (!path) {
        error = std::string("cannot create temporary directory from '")
            + tmpl + "': " + strerror(errno);
        return false;
    }
    temp_dir = path;
    return true;
}

//! Remove regular temporary files generated under @p temp_dir.
static void cleanup_qcc_temp_dir(const std::string& temp_dir) {
    DIR* d = opendir(temp_dir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }
            std::string path = join_path(temp_dir, ent->d_name);
            unlink(path.c_str());
        }
        closedir(d);
    }
    rmdir(temp_dir.c_str());
}

//! Mirror QoreAOT.cpp's scriptBatchSourceId() for batch `.qo` output names.
static bool get_batch_object_path(const std::string& source_path,
        const std::string& output_dir, std::string& object_path,
        std::string& error) {
    char* resolved = realpath(source_path.c_str(), nullptr);
    if (!resolved) {
        error = "cannot resolve source file '" + source_path + "': "
            + strerror(errno);
        return false;
    }
    std::string canon = resolved;
    free(resolved);
    object_path = output_dir + "/" + sanitize_c_identifier(canon) + ".qo";
    return true;
}

//! Link script-context `.qo` fragments into an executable.
static int link_script_objects_to_executable(const std::string& output,
        const std::vector<std::string>& object_paths,
        const std::string& glue_path, const char* entry,
        const char* mode_label, int compiled_count = -1, int opt_level = -1,
        bool include_source = false) {
    if (object_paths.empty()) {
        fprintf(stderr, "error: %s link failed for output '%s': no .qo inputs\n",
            mode_label, output.c_str());
        return 1;
    }

    FILE* glue = fopen(glue_path.c_str(), "w");
    if (!glue) {
        fprintf(stderr,
            "error: %s link failed for output '%s': cannot write glue file '%s': %s\n",
            mode_label, output.c_str(), glue_path.c_str(), strerror(errno));
        return 1;
    }
    fprintf(glue,
        "// Auto-generated by qcc %s - do not edit.\n"
        "// C++ host for %zu .qo input%s.\n"
        "#include <qore/Qore.h>\n"
        "#include <qore/QoreAOT.h>\n"
        "#include <stdio.h>\n\n",
        QCC_VERSION, object_paths.size(),
        object_paths.size() == 1 ? "" : "s");

    std::vector<std::string> sans;
    sans.reserve(object_paths.size());
    fprintf(glue, "extern \"C\" {\n");
    for (const auto& op : object_paths) {
        std::string san = sanitize_c_identifier(basename_no_ext(op));
        fprintf(glue,
            "    void qore_%s_%s_script_register(QoreProgram*);\n",
            san.c_str(), san.c_str());
        sans.push_back(std::move(san));
    }
    fprintf(glue, "}\n\n");

    fprintf(glue,
        "int main(int /*argc*/, char** /*argv*/) {\n"
        "    qore_init(QL_GPL, \"UTF-8\", true);\n"
        "    QoreProgram* pgm = qore_create_program(\n"
        "        PO_NEW_STYLE | PO_STRICT_ARGS);\n"
        "    if (!pgm) {\n"
        "        fprintf(stderr, \"qore_create_program failed\\n\");\n"
        "        qore_cleanup();\n"
        "        return 1;\n"
        "    }\n"
        "    qore_aot_script_begin_batch(pgm);\n");
    for (const auto& san : sans) {
        fprintf(glue,
            "    qore_%s_%s_script_register(pgm);\n",
            san.c_str(), san.c_str());
    }
    fprintf(glue,
        "    int rc = qore_aot_script_end_batch(pgm);\n"
        "    if (rc != 0) {\n"
        "        fprintf(stderr, \"batch flush failed: %%s\\n\",\n"
        "            qore_last_error(pgm));\n"
        "        qore_destroy_program(pgm);\n"
        "        qore_cleanup();\n"
        "        return 1;\n"
        "    }\n"
        "    rc = qore_run_callable(pgm, \"%s\", NULL);\n"
        "    if (rc != 0) {\n"
        "        fprintf(stderr, \"entry %s returned %%d: %%s\\n\",\n"
        "            rc, qore_last_error(pgm));\n"
        "    }\n"
        "    qore_destroy_program(pgm);\n"
        "    qore_cleanup();\n"
        "    return rc;\n"
        "}\n",
        entry, entry);

    if (fclose(glue) != 0) {
        fprintf(stderr,
            "error: %s link failed for output '%s': cannot close glue file '%s': %s\n",
            mode_label, output.c_str(), glue_path.c_str(), strerror(errno));
        return 1;
    }

    if (verbose) {
        printf("qcc link: emitted glue main %s (%zu .qo inputs, entry fn: %s)\n",
            glue_path.c_str(), object_paths.size(), entry);
    }

    std::string cxx;
    if (const char* env_cxx = std::getenv("CXX")) {
        cxx = env_cxx;
    } else {
        cxx = "g++";
    }
    std::vector<std::string> include_dirs;
    std::vector<std::string> lib_dirs;
    collect_qcc_link_paths(include_dirs, lib_dirs);

    std::string cmd = cxx + " -std=c++17";
    for (const auto& dir : include_dirs) {
        cmd += " -I" + shell_quote(dir);
    }
    cmd += " " + shell_quote(glue_path);
    for (const auto& op : object_paths) {
        cmd += " " + shell_quote(op);
    }
    append_qcc_link_flags(cmd, lib_dirs);
    cmd += " -o ";
    cmd += shell_quote(output);
    if (verbose) {
        for (const auto& dir : include_dirs) {
            printf("qcc link: include dir: %s\n", dir.c_str());
        }
        for (const auto& dir : lib_dirs) {
            printf("qcc link: library dir: %s\n", dir.c_str());
        }
        printf("qcc link: %s\n", cmd.c_str());
    }
    int link_rc = std::system(cmd.c_str());
    if (link_rc != 0) {
        fprintf(stderr,
            "error: %s link failed for output '%s' from %zu .qo objects using glue '%s' (rc=%d): %s\n",
            mode_label, output.c_str(), object_paths.size(),
            glue_path.c_str(), link_rc, cmd.c_str());
        return 1;
    }
    int64_t output_size = file_size(output);
    std::string mode_suffix;
    if (opt_level >= 0) {
        mode_suffix = " (-O" + std::to_string(opt_level);
        if (include_source) {
            mode_suffix += ", include-source";
        }
        mode_suffix += ")";
    }
    if (compiled_count >= 0) {
        if (output_size >= 0) {
            printf("qcc: %s%s: %lld bytes for %d Qore code variants (%zu .qo inputs, entry: %s): %s\n",
                mode_label, mode_suffix.c_str(), (long long)output_size, compiled_count,
                object_paths.size(), entry, output.c_str());
        } else {
            printf("qcc: %s%s: %d Qore code variants (%zu .qo inputs, entry: %s): %s\n",
                mode_label, mode_suffix.c_str(), compiled_count, object_paths.size(), entry, output.c_str());
        }
    } else if (output_size >= 0) {
        printf("qcc: %s%s: %lld bytes from %zu .qo input%s (entry: %s): %s\n",
            mode_label, mode_suffix.c_str(), (long long)output_size, object_paths.size(),
            object_paths.size() == 1 ? "" : "s", entry, output.c_str());
    } else {
        printf("qcc: %s%s: %zu .qo input%s (entry: %s): %s\n",
            mode_label, mode_suffix.c_str(), object_paths.size(), object_paths.size() == 1 ? "" : "s",
            entry, output.c_str());
    }
    return 0;
}

int main(int argc, char** argv) {
    // Parse command-line options
    if (parse_options_cmdline(argc, argv) != 0) {
        return 1;
    }

    // Phase 1 compile-time opt: flags propagated to QoreAOT via env vars
    // (AOT layer already reads several QORE_AOT_* vars; this extends the set).
    if (strip_debug_info) {
        setenv("QORE_AOT_NO_DEBUG_INFO", "1", 1);
    }
    if (time_trace_path) {
        setenv("QORE_AOT_TIME_TRACE", time_trace_path, 1);
    }
    if (verbose) {
        setenv("QORE_AOT_VERBOSE", "1", 1);
    }
    if (warnings_are_errors) {
        setenv("QORE_AOT_WARNINGS_ARE_ERRORS", "1", 1);
    }
    if (metadata_compression) {
        setenv("QORE_AOT_METADATA_COMPRESSION", metadata_compression, 1);
    }
    // Propagate the CLI flag's value to QORE_AOT_BIG_FN_THRESHOLD so
    // the IR-to-LLVM lowerer picks it up.  Overwrite the env only when
    // the user explicitly passed --big-fn-threshold on the command line;
    // otherwise leave any pre-existing shell-env value alone.  This
    // keeps the CLI default (200) from silently clobbering
    // `QORE_AOT_BIG_FN_THRESHOLD=50 qcc ...`, while still letting
    // `qcc --big-fn-threshold=N` take effect in shells that have a
    // different value set.
    if (big_fn_threshold > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", big_fn_threshold);
        setenv("QORE_AOT_BIG_FN_THRESHOLD", buf,
               big_fn_threshold_cli_explicit ? 1 : 0);
    }
    // Propagate --jobs to QCC_JOBS so the backend-codegen path (emitObjectFile) picks it up.
    // An explicit flag overrides any pre-existing QCC_JOBS in the environment.
    if (aot_jobs > 0) {
        char jbuf[32];
        snprintf(jbuf, sizeof(jbuf), "%d", aot_jobs);
        setenv("QCC_JOBS", jbuf, 1);
    }

    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (show_version) {
        print_version();
        return 0;
    }

    if (show_targets) {
        QoreAOT::printSupportedTargets();
        return 0;
    }

    if (depfile_path && depfile_dir) {
        fprintf(stderr,
            "error: --depfile and --depfile-dir are mutually exclusive\n");
        return 1;
    }
    if (depfile_target_path && !depfile_path) {
        fprintf(stderr,
            "error: --depfile-target requires --depfile=FILE\n");
        return 1;
    }
    if (skip_if_manifest_current && !write_manifest_path) {
        fprintf(stderr,
            "error: --skip-if-manifest-current requires --write-manifest=FILE\n");
        return 1;
    }

    if (dump_index_json) {
        if (optind + 1 != argc) {
            fprintf(stderr, "error: --dump-index-json requires exactly one binary/object path\n");
            return 1;
        }
        qore_init(QL_GPL, "UTF-8", true);
        int rc = dump_aot_index_json_for_file(argv[optind]);
        qore_cleanup();
        return rc;
    }

    if (dump_symbols || dump_sections) {
        dump_info = true;
    }
    if (dump_info) {
        if (optind >= argc) {
            fprintf(stderr, "error: --dump-info requires at least one binary/object path\n");
            return 1;
        }
        qore_init(QL_GPL, "UTF-8", true);
        int rc = 0;
        for (int i = optind; i < argc; ++i) {
            if (dump_aot_info_for_file(argv[i])) {
                rc = 1;
            }
        }
        qore_cleanup();
        return rc;
    }

    if ((link_aggregate_symbol || qolink_map_path) && !link_qo) {
        fprintf(stderr,
            "error: --aggregate-symbol and --qolink-map are only valid with --link-qo\n");
        return 1;
    }
    if ((strict_call_relocations || allow_unresolved_qo_imports) && !link_qo) {
        fprintf(stderr,
            "error: --strict-call-relocations and --allow-unresolved-imports are only valid with --link-qo\n");
        return 1;
    }
    if (script_aggregate_native_registers && !script_aggregate_symbol) {
        fprintf(stderr,
            "error: --script-aggregate-native-registers is only valid with --script-aggregate\n");
        return 1;
    }

    if (link_qo) {
        if (compile_only || module_mode || archive_mode || from_objects || context_dir
                || batch_output_dir || script_aggregate_symbol) {
            fprintf(stderr,
                "error: --link-qo cannot be combined with compile/module/archive/source aggregate modes\n");
            return 1;
        }
        if (!output_path) {
            fprintf(stderr, "error: --link-qo requires -o/--output\n");
            return 1;
        }
        if (!link_aggregate_symbol || !*link_aggregate_symbol) {
            fprintf(stderr, "error: --link-qo requires --aggregate-symbol=SYM\n");
            return 1;
        }
        if (optind >= argc) {
            fprintf(stderr, "error: --link-qo requires at least one .qo input\n");
            return 1;
        }

        std::string map_path = qolink_map_path ? qolink_map_path
            : std::string(output_path) + ".qolink.json";
        QCCBuildManifest manifest;
        manifest.kind = "link-qo";
        manifest.output = output_path;
        manifest.index_json = write_index_json_path ? write_index_json_path : "";
        manifest.depfile = depfile_path ? depfile_path : "";
        manifest.link_map = map_path;
        manifest.aggregate_symbol = link_aggregate_symbol;
        add_existing_qo_inputs_from_args(manifest.inputs, optind, argc, argv);
        QCCFileFingerprint output_before = qcc_file_fingerprint(manifest.output);
        if (skip_if_manifest_current && qcc_manifest_current(manifest, argv[0])) {
            if (qcc_output_verbose()) {
                printf("qcc: manifest current, skipped qo link aggregate: %s\n", output_path);
            }
            std::string error;
            if (!finish_qcc_build(manifest, argv[0], output_before, false, true, true, error)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                return 1;
            }
            return 0;
        }

        std::vector<QOLinkInputInfo> inputs;
        inputs.reserve(argc - optind);
        qore_init(QL_GPL, "UTF-8", true);
        for (int i = optind; i < argc; ++i) {
            std::string error;
            if (!qo_link_check_cancel(static_cast<size_t>(i - optind),
                    "AOT qo-link input collection", error)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                qore_cleanup();
                return 1;
            }
            if (!has_qo_extension(argv[i])) {
                fprintf(stderr, "error: --link-qo input '%s' must end in .qo\n", argv[i]);
                qore_cleanup();
                return 1;
            }
            char* resolved = realpath(argv[i], nullptr);
            if (!resolved) {
                fprintf(stderr, "error: cannot resolve '%s': %s\n", argv[i], strerror(errno));
                qore_cleanup();
                return 1;
            }
            QOLinkInputInfo input;
            bool ok = collect_qo_link_input(resolved, input, error);
            free(resolved);
            if (!ok) {
                fprintf(stderr, "error: %s\n", error.c_str());
                qore_cleanup();
                return 1;
            }
            inputs.push_back(std::move(input));
        }

        QOLinkPlan plan;
        std::string error;
        if (!validate_qo_link_inputs(inputs, plan, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        if ((!allow_unresolved_qo_imports && !plan.unresolved_imports.empty())
                || !plan.ambiguous_imports.empty() || !plan.hash_mismatches.empty()) {
            print_qo_link_issues("unresolved import", plan.unresolved_imports);
            print_qo_link_issues("ambiguous import", plan.ambiguous_imports);
            print_qo_link_issues("hash mismatch", plan.hash_mismatches);
            qore_cleanup();
            return 1;
        }
        if (strict_call_relocations
                && (!plan.unresolved_call_relocations.empty()
                    || !plan.ambiguous_call_relocations.empty()
                    || !plan.call_relocation_hash_mismatches.empty())) {
            print_qo_link_call_relocation_issues("unresolved", plan.unresolved_call_relocations);
            print_qo_link_call_relocation_issues("ambiguous", plan.ambiguous_call_relocations);
            print_qo_link_call_relocation_issues("hash mismatch", plan.call_relocation_hash_mismatches);
            qore_cleanup();
            return 1;
        }

        std::vector<std::string> register_symbols;
        register_symbols.reserve(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (!qo_link_check_cancel(i, "AOT qo-link register-symbol collection", error)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                qore_cleanup();
                return 1;
            }
            register_symbols.push_back(inputs[i].register_symbol);
        }

        if (!QoreAOT::compileScriptRegisterAggregate(register_symbols, output_path,
                link_aggregate_symbol, error, opt_level, target_triple)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }

        if (!write_qo_link_map(map_path, output_path, link_aggregate_symbol,
                inputs, plan, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        std::vector<std::string> depfile_inputs = qcc_depfile_explicit_inputs(manifest.inputs);
        if (depfile_path && !write_depfile_list(depfile_path,
                qcc_depfile_target(output_path), depfile_inputs)) {
            qore_cleanup();
            return 1;
        }
        if (!finish_qcc_build(manifest, argv[0], output_before, true, false, true, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }

        if (qcc_output_verbose()) {
            printf("qcc: linked qo aggregate from %zu .qo input%s (-O%d): %s\n",
                inputs.size(), inputs.size() == 1 ? "" : "s", opt_level, output_path);
            printf("qcc: wrote qo link map: %s\n", map_path.c_str());
        }
        qore_cleanup();
        return 0;
    }

    // Phase C Item 2: link-mode — `qcc -o <binary> *.qo` emits a
    // synthesised C++ main (qore_init → create_program → begin_batch
    // → per-.qo qore_<sanfile>_<sanfile>_script_register calls →
    // end_batch → run entry fn → destroy_program → cleanup) and
    // invokes $CXX (fallback g++) to link the .qo set + -lqore into
    // a standalone executable.  Replaces the hand-written C++ main
    // pattern `examples/aot/qoa_link_test.cpp` documents.
    //
    // Entered when: `-o <file>` is set, all positional inputs end
    // in `.qo`, and no competing mode is active (-c, -m, -a, -F).
    if (output_path && !compile_only && !module_mode && !archive_mode
            && !from_objects && optind < argc
            && has_qo_extension(argv[optind])) {
        if (static_link) {
            fprintf(stderr,
                "error: .qo link mode does not support --static "
                "(output '%s'); link the generated host manually for static builds\n",
                output_path);
            return 1;
        }

        // All positional inputs must be .qo files.
        std::vector<std::string> object_paths;
        for (int i = optind; i < argc; ++i) {
            if (!has_qo_extension(argv[i])) {
                fprintf(stderr, "error: link-mode input '%s' must end "
                    "in .qo (mixing source files and .qo objects is not supported)\n", argv[i]);
                return 1;
            }
            char* resolved = realpath(argv[i], nullptr);
            if (resolved) {
                object_paths.emplace_back(resolved);
                free(resolved);
            } else {
                fprintf(stderr, "error: cannot resolve '%s': %s\n",
                    argv[i], strerror(errno));
                return 1;
            }
        }

        std::string glue_path = std::string(output_path) + ".main.cpp";
        return link_script_objects_to_executable(output_path, object_paths,
            glue_path, entry_fn, "link-mode");
    }

    // One-shot multi-source executable mode:
    //   qcc -o app src/lib.qr src/main.qr
    //
    // This is intentionally a thin composition of existing primitives:
    // batch-compile all sources into temporary `.qo` fragments with one
    // parse cycle, then link those fragments with the same link helper
    // used by explicit `.qo` link mode.
    if (!compile_only && !module_mode && !archive_mode && !from_objects
            && !context_dir && optind + 1 < argc
            && !has_qo_extension(argv[optind])) {
        if (!output_path) {
            fprintf(stderr,
                "error: multi-source executable mode requires -o/--output "
                "to name the output binary\n");
            return 1;
        }
        if (static_link) {
            fprintf(stderr,
                "error: multi-source executable mode does not support --static "
                "(output '%s'); link the generated host manually for static builds\n",
                output_path);
            return 1;
        }
        if (batch_output_dir) {
            fprintf(stderr,
                "error: --output-dir is only valid with -c/--compile-only; "
                "multi-source executable mode manages temporary objects internally\n");
            return 1;
        }
        if (!script_lib_dirs.empty()) {
            fprintf(stderr,
                "error: -L <dir> is per-file compile-only preload; "
                "multi-source executable mode parses all sources in one batch\n");
            return 1;
        }
        if (depfile_path) {
            fprintf(stderr,
                "error: --depfile is not supported in one-shot multi-source "
                "executable mode; use -c --output-dir for build-system integration\n");
            return 1;
        }
        if (depfile_dir) {
            fprintf(stderr,
                "error: --depfile-dir is not supported in one-shot multi-source "
                "executable mode; use -c --output-dir for build-system integration\n");
            return 1;
        }

        std::vector<std::string> target_files;
        target_files.reserve(argc - optind);
        for (int i = optind; i < argc; ++i) {
            if (has_qo_extension(argv[i])) {
                fprintf(stderr,
                    "error: multi-source executable mode does not allow mixed "
                    "source and .qo inputs; input '%s' is a .qo object\n",
                    argv[i]);
                return 1;
            }
            if (has_extension(argv[i], ".qm")) {
                fprintf(stderr,
                    "error: multi-source executable mode expects script/application "
                    "sources; input '%s' is a .qm module source\n", argv[i]);
                return 1;
            }
            if (has_extension(argv[i], ".qmod") || has_extension(argv[i], ".qoa")) {
                fprintf(stderr,
                    "error: multi-source executable mode expects source files; "
                    "input '%s' is a compiled artifact\n", argv[i]);
                return 1;
            }
            if (is_directory(argv[i])) {
                fprintf(stderr,
                    "error: multi-source executable mode does not accept directory "
                    "input '%s'; split modules require -m or --context workflows\n",
                    argv[i]);
                return 1;
            }
            target_files.emplace_back(argv[i]);
        }

        std::string temp_dir;
        std::string error;
        if (!make_qcc_temp_dir(temp_dir, error)) {
            fprintf(stderr,
                "error: multi-source executable setup failed for output '%s': %s\n",
                output_path, error.c_str());
            return 1;
        }

        qore_init(QL_GPL, "UTF-8", true);
        int compiled_count = 0;
        bool ok = QoreAOT::compileScriptFilesBatch(
            target_files, temp_dir, PO_DEFAULT, error,
            opt_level, target_triple, include_source,
            load_modules, stub_files, parse_defines,
            parse_option_flags, &compiled_count, false);
        qore_cleanup();
        if (!ok) {
            fprintf(stderr,
                "error: multi-source executable compile failed for output '%s' "
                "using temporary object directory '%s': %s\n",
                output_path, temp_dir.c_str(), error.c_str());
            if (save_temps) {
                fprintf(stderr, "qcc: temporary files saved in '%s'\n",
                    temp_dir.c_str());
            } else {
                cleanup_qcc_temp_dir(temp_dir);
            }
            return 1;
        }

        std::vector<std::string> object_paths;
        object_paths.reserve(target_files.size());
        for (const auto& source : target_files) {
            std::string object_path;
            if (!get_batch_object_path(source, temp_dir, object_path, error)) {
                fprintf(stderr,
                    "error: multi-source executable object collection failed "
                    "for output '%s' and source '%s': %s\n",
                    output_path, source.c_str(), error.c_str());
                if (save_temps) {
                    fprintf(stderr, "qcc: temporary files saved in '%s'\n",
                        temp_dir.c_str());
                } else {
                    cleanup_qcc_temp_dir(temp_dir);
                }
                return 1;
            }
            if (!is_file(object_path)) {
                fprintf(stderr,
                    "error: multi-source executable object collection failed "
                    "for output '%s': expected temporary object '%s' for "
                    "source '%s' was not generated\n",
                    output_path, object_path.c_str(), source.c_str());
                if (save_temps) {
                    fprintf(stderr, "qcc: temporary files saved in '%s'\n",
                        temp_dir.c_str());
                } else {
                    cleanup_qcc_temp_dir(temp_dir);
                }
                return 1;
            }
            object_paths.push_back(std::move(object_path));
        }

        std::string glue_path = join_path(temp_dir, "qcc-main.cpp");
        int rc = link_script_objects_to_executable(output_path, object_paths,
            glue_path, entry_fn, "multi-source executable", compiled_count,
            opt_level, include_source);
        if (save_temps) {
            fprintf(stderr, "qcc: temporary files saved in '%s'\n",
                temp_dir.c_str());
        } else {
            cleanup_qcc_temp_dir(temp_dir);
        }
        return rc;
    }

    // -a / --archive mode.  Same input shape as
    // --from-objects but produces a `.qoa` static archive instead of a
    // `.qmod`.  Must be checked before the --from-objects branch so
    // `-a -m` doesn't fall through to the .qmod aggregator.
    if (archive_mode) {
        if (from_objects) {
            fprintf(stderr,
                "error: -a/--archive and --from-objects are mutually exclusive\n");
            return 1;
        }
        if (!context_dir) {
            fprintf(stderr,
                "error: -a/--archive requires --context=DIR pointing at the "
                "split-module source directory\n");
            return 1;
        }
        if (!is_directory(context_dir)) {
            fprintf(stderr, "error: --context=%s is not a directory\n",
                context_dir);
            return 1;
        }
        if (optind >= argc) {
            fprintf(stderr,
                "error: -a/--archive requires at least one .qo input\n");
            return 1;
        }

        std::vector<std::string> object_paths;
        for (int i = optind; i < argc; ++i) {
            char* resolved = realpath(argv[i], nullptr);
            if (resolved) {
                object_paths.emplace_back(resolved);
                free(resolved);
            } else {
                object_paths.emplace_back(argv[i]);
            }
        }

        std::string output;
        if (output_path) {
            output = output_path;
        } else {
            std::string dir_str(context_dir);
            while (!dir_str.empty() && dir_str.back() == '/') {
                dir_str.pop_back();
            }
            size_t slash = dir_str.rfind('/');
            std::string basename = (slash != std::string::npos)
                ? dir_str.substr(slash + 1) : dir_str;
            output = basename + ".qoa";
        }

        qore_init(QL_GPL, "UTF-8", true);
        std::string error;
        bool ok = QoreAOT::archiveModuleFromObjects(
            context_dir, object_paths, output, PO_DEFAULT, error,
            opt_level, target_triple, include_source);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        if (qcc_output_verbose()) {
            printf("qcc: archived %zu .qo input%s into .qoa (-O%d%s): %s\n",
                object_paths.size(), object_paths.size() == 1 ? "" : "s",
                opt_level, source_mode_suffix(include_source), output.c_str());
        }
        qore_cleanup();
        return 0;
    }

    // --from-objects aggregator mode.  Takes a list of
    // per-file `.qo` inputs + --context=DIR (source dir) and produces a
    // `.qmod`.  All positional args are `.qo` files; validation happens
    // before we touch the standard single-file dispatch path.
    if (from_objects) {
        if (!module_mode) {
            fprintf(stderr,
                "error: --from-objects requires -m/--module\n");
            return 1;
        }
        if (!context_dir) {
            fprintf(stderr,
                "error: --from-objects requires --context=DIR pointing at "
                "the split-module source directory\n");
            return 1;
        }
        if (!is_directory(context_dir)) {
            fprintf(stderr, "error: --context=%s is not a directory\n",
                context_dir);
            return 1;
        }
        if (optind >= argc) {
            fprintf(stderr,
                "error: --from-objects requires at least one .qo input\n");
            return 1;
        }

        std::vector<std::string> object_paths;
        for (int i = optind; i < argc; ++i) {
            char* resolved = realpath(argv[i], nullptr);
            if (resolved) {
                object_paths.emplace_back(resolved);
                free(resolved);
            } else {
                object_paths.emplace_back(argv[i]);
            }
        }

        std::string output;
        if (output_path) {
            output = output_path;
        } else {
            // Derive from the context dir basename: "qlib/HttpServer/" -> "HttpServer.qmod"
            std::string dir_str(context_dir);
            while (!dir_str.empty() && dir_str.back() == '/') {
                dir_str.pop_back();
            }
            size_t slash = dir_str.rfind('/');
            std::string basename = (slash != std::string::npos)
                ? dir_str.substr(slash + 1) : dir_str;
            output = basename + ".qmod";
        }

        qore_init(QL_GPL, "UTF-8", true);
        std::string error;
        bool ok = QoreAOT::compileModuleFromObjects(
            context_dir, object_paths, output, PO_DEFAULT, error,
            opt_level, target_triple, include_source);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        if (qcc_output_verbose()) {
            printf("qcc: aggregated .qmod from %zu .qo input%s (-O%d%s): %s\n",
                object_paths.size(), object_paths.size() == 1 ? "" : "s",
                opt_level, source_mode_suffix(include_source), output.c_str());
        }
        // Aggregator mode re-parses every source in --context for
        // metadata extraction.  Per-.qo depfiles (emitted by the per-file
        // rule) cover the common case, but a new file added to the dir
        // without a matching `qcc -c` rule would be invisible without
        // this fallback.  Write empty-source + context depfile.
        if (depfile_path
                && !write_depfile(depfile_path, qcc_depfile_target(output),
                    std::string(), context_dir)) {
            qore_cleanup();
            return 1;
        }
        qore_cleanup();
        return 0;
    }

    if (script_aggregate_symbol) {
        if (!compile_only) {
            fprintf(stderr,
                "error: --script-aggregate requires -c/--compile-only\n");
            return 1;
        }
        if (module_mode || archive_mode || from_objects || context_dir) {
            fprintf(stderr,
                "error: --script-aggregate is only valid for script-context "
                "compile mode\n");
            return 1;
        }
        if (!output_path) {
            fprintf(stderr,
                "error: --script-aggregate requires -o/--output for the "
                "aggregate object\n");
            return 1;
        }
        if (batch_output_dir) {
            fprintf(stderr,
                "error: --script-aggregate writes a single output; use -o "
                "instead of --output-dir\n");
            return 1;
        }
        if (!script_lib_dirs.empty()) {
            fprintf(stderr,
                "error: -L <dir> is per-file-compile only; "
                "--script-aggregate parses all sources in one program\n");
            return 1;
        }
        if (optind >= argc) {
            fprintf(stderr,
                "error: --script-aggregate requires at least one source file\n");
            return 1;
        }

        std::vector<std::string> target_files;
        target_files.reserve(argc - optind);
        for (int i = optind; i < argc; ++i) {
            if (has_qo_extension(argv[i]) || has_extension(argv[i], ".qmod")
                    || has_extension(argv[i], ".qoa")) {
                fprintf(stderr,
                    "error: --script-aggregate input '%s' must be a source "
                    "file, not a compiled artifact\n", argv[i]);
                return 1;
            }
            if (has_extension(argv[i], ".qm")) {
                fprintf(stderr,
                    "error: --script-aggregate expects script/application "
                    "sources; input '%s' is a .qm module source\n", argv[i]);
                return 1;
            }
            if (is_directory(argv[i])) {
                fprintf(stderr,
                    "error: --script-aggregate does not accept directory "
                    "input '%s'\n", argv[i]);
                return 1;
            }
            target_files.emplace_back(argv[i]);
        }

        QCCBuildManifest manifest;
        manifest.kind = "script-aggregate";
        manifest.output = output_path;
        manifest.index_json = write_index_json_path ? write_index_json_path : "";
        manifest.depfile = depfile_path ? depfile_path : "";
        manifest.aggregate_symbol = script_aggregate_symbol;
        manifest.inputs = target_files;
        QCCFileFingerprint output_before = qcc_file_fingerprint(manifest.output);
        if (skip_if_manifest_current && qcc_manifest_current(manifest, argv[0])) {
            if (qcc_output_verbose()) {
                printf("qcc: manifest current, skipped script aggregate .qo: %s\n", output_path);
            }
            std::string error;
            if (!finish_qcc_build(manifest, argv[0], output_before, false, true, false, error)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                return 1;
            }
            return 0;
        }

        qore_init(QL_GPL, "UTF-8", true);
        std::string error;
        int compiled_count = 0;
        bool ok = QoreAOT::compileScriptAggregate(
            target_files, output_path, script_aggregate_symbol, PO_DEFAULT,
            error, opt_level, target_triple, include_source,
            load_modules, stub_files, parse_defines, parse_option_flags,
            &compiled_count, script_aggregate_native_registers);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        if (qcc_output_verbose()) {
            printf("qcc: compiled script aggregate .qo (-O%d, %d variants%s): %s\n",
                opt_level, compiled_count, source_mode_suffix(include_source),
                output_path);
        }
        if (depfile_path && !write_depfile_list(depfile_path,
                qcc_depfile_target(output_path), target_files)) {
            qore_cleanup();
            return 1;
        }
        if (!finish_qcc_build(manifest, argv[0], output_before, true, false, false, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        qore_cleanup();
        return 0;
    }

    // Get source file
    if (optind >= argc) {
        fprintf(stderr, "error: no source file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // Batch-compile mode.  When `-c` is used with
    // multiple positional source files (and no module-ish flag), parse
    // them into one QoreProgram (one parse cycle) and emit one `.qo`
    // per source into --output-dir.  Output names are derived from
    // canonical source paths to prevent same-basename sources in one
    // application from overwriting each other.  This avoids
    // compileScriptFile's O(N^2) sibling-preload cost when the build
    // system invokes qcc per-file.
    if (compile_only && !context_dir && (optind + 1 < argc)) {
        if (!batch_output_dir) {
            fprintf(stderr, "error: multiple source files require "
                "--output-dir=<dir> (batch-compile mode)\n");
            return 1;
        }
        if (output_path) {
            fprintf(stderr, "error: -o <file> is single-output; use "
                "--output-dir=<dir> with multiple sources\n");
            return 1;
        }
        if (!script_lib_dirs.empty()) {
            fprintf(stderr, "error: -L <dir> is per-file-compile only; "
                "not needed in batch mode (sources share one parse)\n");
            return 1;
        }
        if (depfile_path) {
            fprintf(stderr,
                "error: --depfile is single-output only; use --depfile-dir "
                "with batch -c --output-dir mode\n");
            return 1;
        }
        if (qcc_single_output_sidecars_requested()) {
            fprintf(stderr,
                "error: qcc build sidecars are single-output only; use one qcc "
                "invocation per output when qcc build sidecars or stamps are used\n");
            return 1;
        }

        std::vector<std::string> batch_sources;
        for (int i = optind; i < argc; ++i) {
            batch_sources.emplace_back(argv[i]);
        }
        if (verbose) {
            printf("Batch script-context .qo mode: %zu sources → %s\n",
                batch_sources.size(), batch_output_dir);
        }

        qore_init(QL_GPL, "UTF-8", true);
        std::string error;
        std::string depfile_dir_arg;
        const std::string* depfile_dir_ptr = nullptr;
        if (depfile_dir) {
            depfile_dir_arg = depfile_dir;
            depfile_dir_ptr = &depfile_dir_arg;
        }
        bool ok = QoreAOT::compileScriptFilesBatch(
            batch_sources, batch_output_dir, PO_DEFAULT, error,
            opt_level, target_triple, include_source,
            load_modules, stub_files, parse_defines,
            parse_option_flags, nullptr, true, depfile_dir_ptr);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        qore_cleanup();
        return 0;
    }

    const char* source_file = argv[optind];

    if (depfile_dir) {
        fprintf(stderr,
            "error: --depfile-dir is only valid with batch -c --output-dir "
            "mode and multiple source files\n");
        return 1;
    }

    // Resolve to absolute path so compiled executables can resolve relative %requires
    // paths even when run from a different working directory
    std::string abs_source_file;
    {
        char* resolved = realpath(source_file, nullptr);
        if (resolved) {
            abs_source_file = resolved;
            free(resolved);
            source_file = abs_source_file.c_str();
        }
    }

    // Check for multiple source files (not supported)
    if (optind + 1 < argc) {
        fprintf(stderr, "error: multiple source files not supported (got '%s' after '%s')\n",
            argv[optind + 1], source_file);
        return 1;
    }

    // Check if input is a directory (split module)
    bool is_split_module = is_directory(source_file);

    // Script-context mode.  When `-c` is used and
    // the input is neither a `.qm` nor a directory AND no
    // `--context=DIR` is supplied, compile as a plain script file.
    // Optional `-L <dir>` inputs preload sibling `.qo` decls so
    // cross-file refs resolve at parse time.
    bool script_mode = false;
    if (compile_only && !context_dir && !is_split_module) {
        size_t len = strlen(source_file);
        bool is_qm = (len > 3 && strcmp(source_file + len - 3, ".qm") == 0);
        if (!is_qm) {
            script_mode = true;
            if (verbose) {
                printf("Script-context .qo mode (target=%s, %zu -L paths)\n",
                    source_file, script_lib_dirs.size());
            }
        }
    }

    // --context=DIR opts the caller into per-file .qo
    // compilation. The input must be a single file in that directory
    // (either the module's `.qm` or one of its `.qc`/`.ql` components);
    // the directory is the parse context, not the input.
    bool per_file_mode = (context_dir != nullptr);
    if (per_file_mode) {
        if (!compile_only) {
            fprintf(stderr, "error: --context=DIR requires -c/--compile-only\n");
            return 1;
        }
        if (is_split_module) {
            fprintf(stderr, "error: --context=DIR is incompatible with a directory input; "
                    "pass a single .qm/.qc/.ql file from the directory instead\n");
            return 1;
        }
        if (!is_directory(context_dir)) {
            fprintf(stderr, "error: --context=%s is not a directory\n", context_dir);
            return 1;
        }
        module_mode = true;  // always module-shaped in per-file mode
        if (verbose) {
            printf("Per-file .qo mode: context=%s\n", context_dir);
        }
    }

    // Auto-detect module mode from extension or directory
    if (!module_mode) {
        if (is_split_module) {
            module_mode = true;
            if (verbose) {
                printf("Auto-detected split module mode from directory input\n");
            }
        } else {
            size_t len = strlen(source_file);
            if (len > 3 && strcmp(source_file + len - 3, ".qm") == 0) {
                module_mode = true;
                if (verbose) {
                    printf("Auto-detected module mode from .qm extension\n");
                }
            }
        }
    }

    // Warn about ignored options
    if (static_link && module_mode) {
        fprintf(stderr, "warning: --static is ignored when compiling modules\n");
        if (warnings_are_errors) {
            return 1;
        }
    }

    // -c (compile-only) applies to module inputs (.qm / split
    // dir / per-file fragment thereof) and also to
    // plain script files (any non-.qm source, including .qr/.q/.qc/.ql).
    // Only reject
    // the combination if we're in neither mode.
    if (compile_only && !module_mode && !script_mode) {
        fprintf(stderr, "error: -c/--compile-only requires either a module "
                "input (.qm / split-module dir / .qc fragment with --context), "
                "or a script-mode source file (.qr/.q/.qc/.ql or another non-.qm "
                "source with optional -L<dir> "
                "preload paths)\n");
        return 1;
    }

    // -L is meaningful only in script-context compile.
    if (!script_lib_dirs.empty() && !script_mode) {
        fprintf(stderr, "error: -L <dir> is only meaningful with script-mode "
                "compile (-c on a .qr/.q/.qc/.ql or another non-.qm source)\n");
        return 1;
    }

    // Reject `-o` mixed with `--output-dir` for
    // single-file compile (same rule as batch mode above).  They are
    // mutually exclusive output-naming strategies.
    if (output_path && batch_output_dir) {
        fprintf(stderr, "error: -o <file> and --output-dir=<dir> are "
                "mutually exclusive\n");
        return 1;
    }

    // Determine output path
    std::string output;
    if (output_path) {
        output = output_path;
    } else {
        if (is_split_module) {
            // For split modules, derive output from directory basename
            std::string dir_str(source_file);
            // Remove trailing slashes
            while (!dir_str.empty() && dir_str.back() == '/') {
                dir_str.pop_back();
            }
            // Extract basename
            size_t last_slash = dir_str.rfind('/');
            std::string basename = (last_slash != std::string::npos)
                ? dir_str.substr(last_slash + 1)
                : dir_str;
            output = basename + (compile_only ? ".qo" : ".qmod");
        } else if (per_file_mode) {
            // Per-file split-module compile (`--context=DIR`).
            // Preserve the source extension in the output name so primary
            // (`.qm`) and secondary (`.qc` / `.ql`) files that share a
            // basename don't collide — e.g. `DataProvider.qm → DataProvider.qm.qo`
            // and `DataProvider.qc → DataProvider.qc.qo`.  Mirrors the
            // symbol-mangling change in `compileSeparatedModuleFile`
            // that uses the full basename (extension included) for
            // `qore_<sanmod>_<sanfile>_*` symbols.
            std::string basename(source_file);
            size_t slash = basename.rfind('/');
            if (slash != std::string::npos) {
                basename = basename.substr(slash + 1);
            }
            output = basename + ".qo";
        } else {
            // Script-mode / single-file default: strip extension and
            // append `.qo` (e.g. `foo.qr → foo.qo`).  Script mode has no
            // primary/secondary split so there's no collision risk.
            output = get_default_output(source_file, module_mode, compile_only);
        }
        // Honor `--output-dir=DIR` for single-file
        // compile too (previously only applied to batch mode on ≥2
        // positional sources, making single-file builds silently write
        // `.qo`s to the current directory).  CMake build systems need
        // a consistent output location regardless of source count.
        if (batch_output_dir) {
            std::string dir_str(batch_output_dir);
            while (!dir_str.empty() && dir_str.back() == '/') {
                dir_str.pop_back();
            }
            output = dir_str + "/" + output;
        }
    }

    // For split modules, skip reading source file (compileSeparatedModule handles it)
    std::string source_text;
    if (!is_split_module) {
        if (!read_file(source_file, source_text)) {
            return 1;
        }
    }

    // Allow environment variable to override optimization level for debugging/testing
    const char* opt_override = getenv("QORE_AOT_OPT_LEVEL");
    if (opt_override) {
        opt_level = atoi(opt_override);
    }

    if (verbose) {
        if (is_split_module) {
            printf("Source: %s (split module directory)\n", source_file);
        } else {
            printf("Source: %s (%zu bytes)\n", source_file, source_text.size());
        }
        printf("Output: %s\n", output.c_str());
        printf("Mode: %s\n", is_split_module ? "split module" : (module_mode ? "module" : "executable"));
        printf("Optimization: -O%d\n", opt_level);
        if (static_link) {
            printf("Static linking: enabled\n");
        }
        if (target_triple) {
            printf("Target: %s\n", target_triple);
        }
    }

    QCCBuildManifest manifest;
    manifest.output = output;
    manifest.index_json = write_index_json_path ? write_index_json_path : "";
    manifest.depfile = depfile_path ? depfile_path : "";
    if (script_mode) {
        manifest.kind = "script-file";
        manifest.inputs.push_back(source_file);
        if (!manifest_skip_qo_library_inputs) {
            collect_qo_library_inputs(manifest.extra_inputs, script_lib_dirs, output);
        }
    } else if (per_file_mode) {
        manifest.kind = "module-file";
        manifest.inputs.push_back(source_file);
        if (context_dir) {
            manifest.extra_inputs.push_back(context_dir);
        }
    } else if (is_split_module) {
        manifest.kind = compile_only ? "split-module-qo" : "split-module";
        manifest.inputs.push_back(source_file);
    } else if (module_mode) {
        manifest.kind = compile_only ? "module-qo" : "module";
        manifest.inputs.push_back(source_file);
    } else {
        manifest.kind = "executable";
        manifest.inputs.push_back(source_file);
    }
    QCCFileFingerprint output_before = qcc_file_fingerprint(manifest.output);
    if (skip_if_manifest_current && qcc_manifest_current(manifest, argv[0])) {
        if (qcc_output_verbose()) {
            printf("qcc: manifest current, skipped %s: %s\n",
                manifest.kind.c_str(), output.c_str());
        }
        std::string error;
        if (!finish_qcc_build(manifest, argv[0], output_before, false, true, false, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
        return 0;
    }

    // Initialize Qore library
    qore_init(QL_GPL, "UTF-8", true);

    int rc = 0;
    std::string error;
    QoreParseOptions compile_po = PO_DEFAULT;
    if (!apply_parse_option_flags(compile_po, error)) {
        fprintf(stderr, "error: %s\n", error.c_str());
        qore_cleanup();
        return 1;
    }

    if (script_mode) {
        // Compile a single script-style source with
        // optional sibling-.qo decl preload.
        std::vector<std::string> parsed_files;
        QoreAOTSourceSymbolManifest source_symbols;
        const QoreAOTSourceSymbolManifest* source_symbol_arg = nullptr;
        if (source_symbol_manifest_path) {
            if (!read_source_symbol_manifest(source_symbol_manifest_path, source_symbols, error)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                qore_cleanup();
                return 1;
            }
            if (!source_symbols.empty()) {
                source_symbol_arg = &source_symbols;
            }
        }
        if (!QoreAOT::compileScriptFile(
                source_file,
                script_lib_dirs,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source,
                load_modules,
                stub_files,
                parse_defines,
                depfile_path ? &parsed_files : nullptr,
                source_symbol_arg)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            if (qcc_output_verbose()) {
                printf("qcc: compiled script-context .qo (-O%d, %zu -L path%s%s): %s\n",
                    opt_level, script_lib_dirs.size(),
                    script_lib_dirs.size() == 1 ? "" : "s",
                    source_mode_suffix(include_source), output.c_str());
            }
            // deps = the target source plus its `%include` closure, as
            // reported by compileScriptFile (the same set it used to filter
            // the `-L` preload).  This lets the build rebuild the `.qo` when
            // any `%include`d file changes — not just the target itself.
            if (depfile_path && !write_depfile_list(depfile_path,
                    qcc_depfile_target(output), parsed_files)) {
                rc = 1;
            }
        }
    } else if (per_file_mode) {
        // Compile a single file from a split module
        // directory.  The directory is the parse context; the file is the
        // sole source of emitted metadata and native functions.
        std::vector<std::string> dep_module_files;
        qore_aot_set_module_dep_sink(&dep_module_files);
        bool ok = QoreAOT::compileSeparatedModuleFile(
                context_dir,
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source);
        qore_aot_set_module_dep_sink(nullptr);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            if (qcc_output_verbose()) {
                printf("qcc: compiled per-file .qo (-O%d%s): %s\n", opt_level,
                    source_mode_suffix(include_source), output.c_str());
            }
            // deps = target source + every sibling .qm/.qc/.ql in --context=DIR
            // (matches compileSeparatedModuleFile's dir scan) + the .qmod files
            // of modules loaded for the %requires closure.
            if (depfile_path
                    && !write_depfile(depfile_path, qcc_depfile_target(output),
                                      source_file, context_dir,
                                      &dep_module_files)) {
                rc = 1;
            }
        }
    } else if (is_split_module) {
        // Compile split module directory
        std::vector<std::string> dep_module_files;
        qore_aot_set_module_dep_sink(&dep_module_files);
        bool ok = QoreAOT::compileSeparatedModule(
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source,
                compile_only);
        qore_aot_set_module_dep_sink(nullptr);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            if (qcc_output_verbose()) {
                printf("qcc: compiled split module (-O%d%s%s): %s\n", opt_level,
                    compile_only ? ", relocatable .qo" : "",
                    source_mode_suffix(include_source), output.c_str());
            }
            // source_file is the split-module directory itself;
            // pass it as `context` so every .qm/.qc/.ql inside counts as a dep.
            // Leave `source` empty so the target dir is not double-listed.
            // dep_module_files adds the .qmod files of the %requires closure.
            if (depfile_path
                    && !write_depfile(depfile_path, qcc_depfile_target(output),
                                      std::string(), source_file,
                                      &dep_module_files)) {
                rc = 1;
            }
        }
    } else if (module_mode) {
        // Compile single-file module
        std::vector<std::string> dep_module_files;
        qore_aot_set_module_dep_sink(&dep_module_files);
        bool ok = QoreAOT::compileModule(
                source_text.c_str(), (int)source_text.size(),
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source,
                compile_only);
        qore_aot_set_module_dep_sink(nullptr);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            if (qcc_output_verbose()) {
                printf("qcc: compiled module (-O%d%s%s): %s\n", opt_level,
                    compile_only ? ", relocatable .qo" : "",
                    source_mode_suffix(include_source), output.c_str());
            }
            // deps = the .qm file + the .qmod files of the %requires closure
            if (depfile_path && !write_depfile(depfile_path, qcc_depfile_target(output),
                                               source_file, nullptr,
                                               &dep_module_files)) {
                rc = 1;
            }
        }
    } else {
        // Create program and parse
        QoreProgram* qpgm = new QoreProgram(compile_po | QoreParseOptions(PO_NEW_STYLE | PO_STRICT_ARGS
            | PO_REQUIRE_TYPES));
        ExceptionSink xsink;
        ExceptionSink wsink;

        qpgm->parseFile(source_file, &xsink, &wsink, QP_WARN_DEFAULT);

        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 1;
        } else if (qccHandleWarnings(wsink)) {
            rc = 1;
        } else {
            // Compile to executable
            QoreParseOptions po = qpgm->getParseOptions() | compile_po;
            if (!QoreAOT::compile(
                    qpgm,
                    source_text.c_str(), (int)source_text.size(),
                    source_file,
                    output,
                    po,
                    error,
                    opt_level,
                    target_triple,
                    static_link,
                    include_source)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                rc = 1;
            } else {
                if (qcc_output_verbose()) {
                    printf("qcc: compiled executable (-O%d%s%s): %s\n", opt_level,
                        static_link ? ", static" : "",
                        source_mode_suffix(include_source), output.c_str());
                }
            }
        }

        qpgm->waitForTerminationAndDeref(&xsink);
    }

    if (!rc) {
        if (!finish_qcc_build(manifest, argv[0], output_before, true, false, false, error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        }
    }

    // Cleanup
    qore_cleanup();

    return rc;
}
