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
#include "qore/intern/QoreAOTExprNodeRegistry.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"
#include "qore/intern/QoreIR.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <getopt.h>
#include <sys/stat.h>
#include <zlib.h>

#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/Error.h>

static const char* QCC_VERSION = "1.0";

//! Check if a path is a directory
static bool is_directory(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
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

// Check whether a path ends in `.qo`.  Link mode requires every
// positional input to match.
static bool has_qo_extension(const char* path) {
    size_t n = std::strlen(path);
    return n > 3 && std::strcmp(path + n - 3, ".qo") == 0;
}

// Write a Make-format dependency file at `path`.
// Target (LHS) is `output`; deps are `source` plus, when `context` is
// non-null, every `.qm`/`.qc`/`.ql` under it — the set the parser actually
// opens in `--context=DIR` mode per `compileSeparatedModuleFile`.
// Consumed by cmake's `add_custom_command(... DEPFILE ...)` so sibling-
// file edits in split-module dirs retrigger the affected per-file `.qo`.
// Only spaces and backslashes in paths are escaped — Make handles both.
static bool write_depfile(const char* path, const std::string& output,
                          const std::string& source, const char* context) {
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
        deps.push_back(canon(source));
    }

    if (context) {
        DIR* d = opendir(context);
        if (!d) {
            fprintf(stderr, "error: --depfile: cannot open context dir '%s': %s\n",
                context, strerror(errno));
            return false;
        }
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
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
            if (std::find(deps.begin(), deps.end(), full) == deps.end()) {
                deps.emplace_back(std::move(full));
            }
        }
        closedir(d);
        std::sort(deps.begin(), deps.end());
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

// Program options
static const char* output_path = nullptr;
static int opt_level = 3;
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
static bool verbose = false;
static bool show_help = false;
static bool show_version = false;
static bool dump_info = false;
static bool dump_symbols = false;
static bool dump_sections = false;

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
// Phase C Item 2: --entry=<fn> names the Qore function the emitted
// C++ main() dispatches to after registering every `.qo` input.
// Only meaningful in link mode (`qcc -o <binary> *.qo`).  Defaults
// to "main" — hosts that use a different convention override via
// `-e <fn>`.
static const char* entry_fn = "main";

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
           "                         incremental builds retrigger on sibling-file edits.\n");
    printf("      --from-objects     Aggregate mode: positional args are per-file .qo\n"
           "                         inputs; requires -m and --context=DIR; produces a\n"
           "                         standard .qmod by linking the .qo's + fresh glue\n");
    printf("  -a, --archive          Archive mode: positional args are per-file .qo inputs;\n"
           "                         requires --context=DIR; produces a .qoa static archive\n"
           "                         (ar rcs) exposing qore_qoa_register_all() for a C++ host\n");
    printf("  -S, --static           Link statically against libqore\n");
    printf("  -t, --target=TRIPLE    Target triple for cross-compilation\n");
    printf("      --show-targets     Show supported target architectures and quit\n");
    printf("      --include-source   Embed source text in AOT metadata\n");
    printf("      --strip-source     Strip source text (default)\n");
    printf("      --strip-debug-info Strip DWARF debug info (faster compile, no debugger)\n");
    printf("  -g                     Emit DWARF debug info (default)\n");
    printf("      --time-trace[=PATH]  Emit Chrome-format trace of opt+codegen passes\n");
    printf("                         (default PATH: qcc.trace.json; view at chrome://tracing)\n");
    printf("      --big-fn-threshold=N  Mark functions >= N IR blocks as OptimizeNone+NoInline\n");
    printf("                         (trades ~1-7%% runtime for up to 46x compile speedup;\n");
    printf("                         default: 200; 0 = off)\n");
    printf("  -e, --entry=FN         Qore function the emitted C++ main() calls after\n"
           "                         registering all `.qo` inputs (link mode only;\n"
           "                         default: main).  Only meaningful with\n"
           "                         `qcc -o <binary> *.qo`.\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -V, --version          Show version information\n");
    printf("      --dump-info        Inspect embedded Qore AOT metadata in object files,\n"
           "                         .qmod modules, .qo fragments, .qoa archives, or\n"
           "                         linked AOT executables without executing them\n");
    printf("      --dump-symbols     Include an nm-like symbol table view\n");
    printf("      --dump-sections    Include object and AOT section tables\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s script.qr                   # Compile to 'script' executable\n", prog);
    printf("  %s -o myapp script.qr          # Compile to 'myapp' executable\n", prog);
    printf("  %s -o myapp main.qo lib.qo     # Link-mode: .qo's -> 'myapp' binary\n", prog);
    printf("  %s -m MyModule.qm              # Compile single-file module to 'MyModule.qmod'\n", prog);
    printf("  %s -m qlib/DataProvider        # Compile split module directory\n", prog);
    printf("  %s -S -o myapp script.qr       # Static link (no libqore.so dependency)\n", prog);
    printf("\n");
    printf("Notes:\n");
    printf("  - Source files must use %%modern or have .qr extension\n");
    printf("  - Static linking requires libqore_static.a (build with -DBUILD_STATIC_LIBQORE=ON)\n");
    printf("  - Cross-compilation requires LLVM support for the target architecture\n");
}

static void print_version() {
    printf("qcc (Qore Code Compiler) v%s\n", QCC_VERSION);
    printf("Using Qore library v%s\n", qore_version_string);
    printf("Built with LLVM for JIT/AOT compilation\n");
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
    {"from-objects",      no_argument,       nullptr, 'F'},
    {"archive",           no_argument,       nullptr, 'a'},
    {"entry",             required_argument, nullptr, 'e'},
    {"verbose",           no_argument,       nullptr, 'v'},
    {"help",              no_argument,       nullptr, 'h'},
    {"version",           no_argument,       nullptr, 'V'},
    {nullptr,             0,                 nullptr, 0}
};

static int parse_options_cmdline(int argc, char** argv) {
    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:mcSt:TL:l:age:vhV", long_options, nullptr)) != -1) {
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
            case 'F':
                from_objects = true;
                break;
            case 'a':
                archive_mode = true;
                break;
            case 'e':
                entry_fn = optarg;
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
    if (compression == 0) {
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

    if (compression == 1) {
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
            return dump_skip_string_ref(reader, p, end)
                && dump_skip_string_ref(reader, p, end)
                && dump_read_u8(p, end, nargs)
                && skip_n_args(nargs);
        }

        case AOTExprKind::NEW_OBJECT:
        case AOTExprKind::SCOPED_NEW_OBJECT:
            if (slot_form) {
                return dump_skip_string_ref(reader, p, end)
                    && dump_skip_string_ref(reader, p, end);
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
        case AOTExprKind::NULL_COAL:
        case AOTExprKind::VALUE_COAL:
        case AOTExprKind::FOLDL:
        case AOTExprKind::FOLDR:
        case AOTExprKind::MAP:
        case AOTExprKind::SELECT:
        case AOTExprKind::LOG_AND:
        case AOTExprKind::LOG_OR:
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
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::QUESTION:
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
            std::string value_error;
            QoreValue v = reader.readValue(p, end, value_error);
            v.discard(nullptr);
            return value_error.empty();
        }

        case AOTExprKind::HASHDECL_NEW:
        case AOTExprKind::COMPLEX_HASH_NEW:
        case AOTExprKind::COMPLEX_LIST_NEW: {
            uint8_t nargs = 0;
            return dump_skip_string_ref(reader, p, end)
                && dump_read_u8(p, end, nargs)
                && skip_n_args(nargs);
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
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx)
                && dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::PARSE_REF:
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_PARSE_REF_TYPE) != 0
                    && !dump_skip_string_ref(reader, p, end)) {
                return false;
            }
            return dump_skip_inline_expr(reader, p, end, summary, func_name, child_ctx);

        case AOTExprKind::CAST_HASHDECL: {
            if (!dump_skip_string_ref(reader, p, end) || !dump_skip_bytes(p, end, 1)) {
                return false;
            }
            if (!slot_form) {
                uint8_t has_inner = 0;
                if (!dump_read_u8(p, end, has_inner)) {
                    return false;
                }
                if (has_inner && !dump_skip_inline_expr(reader, p, end, summary, func_name,
                        child_ctx)) {
                    return false;
                }
            }
            return true;
        }

        case AOTExprKind::CAST_COMPLEX_HASH:
        case AOTExprKind::CAST_COMPLEX_LIST:
        case AOTExprKind::CAST_CLASS:
        case AOTExprKind::CAST_ENUM:
            return dump_skip_string_ref(reader, p, end) && dump_skip_bytes(p, end, 1);

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
                if (has_default) {
                    std::string value_error;
                    QoreValue dv = reader.readValue(p, end, value_error);
                    dv.discard(nullptr);
                    if (!value_error.empty()) {
                        return false;
                    }
                }
            }
            if (!dump_skip_bytes(p, end, 1)) {
                return false;
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

        bool malformed = false;
        for (uint16_t i = 0; i < num_locals && !malformed; ++i) {
            malformed = !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_bytes(p, entry_end, 3);
        }
        for (uint16_t i = 0; i < num_globals && !malformed; ++i) {
            malformed = !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_string_ref(reader, p, entry_end)
                || !dump_skip_bytes(p, entry_end, 1);
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

static bool dump_skip_len_string_ref(const QoreAOTBinaryReader& reader,
        const uint8_t*& p, const uint8_t* end) {
    uint32_t len = 0;
    return dump_read_u32(p, end, len) && dump_skip_string_ref(reader, p, end);
}

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

        case QoreAOTValueTag::VT_LIST: {
            uint32_t count = 0;
            return dump_read_u32(p, end, count)
                && dump_skip_serialized_value_args(reader, p, end, count, summary, error);
        }

        case QoreAOTValueTag::VT_HASH: {
            uint32_t count = 0;
            if (!dump_read_u32(p, end, count)) {
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
            return dump_skip_bytes(p, end, 1)
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
    printf("    size: %zu bytes%s\n", blob.bytes.size(), hdr.compression ? " compressed" : "");
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

    if (dump_symbols || dump_sections) {
        dump_info = true;
    }
    if (dump_info) {
        if (optind >= argc) {
            fprintf(stderr, "error: --dump-info requires at least one binary/object path\n");
            return 1;
        }
        int rc = 0;
        for (int i = optind; i < argc; ++i) {
            if (dump_aot_info_for_file(argv[i])) {
                rc = 1;
            }
        }
        return rc;
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
        // All positional inputs must be .qo files.
        std::vector<std::string> object_paths;
        for (int i = optind; i < argc; ++i) {
            if (!has_qo_extension(argv[i])) {
                fprintf(stderr, "error: link-mode input '%s' must end "
                    "in .qo (mix of .q and .qo not supported)\n", argv[i]);
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

        // Emit the glue main.cpp next to the final binary so a
        // developer can inspect it with --verbose or rebuild by hand.
        std::string glue_path = std::string(output_path) + ".main.cpp";
        FILE* glue = fopen(glue_path.c_str(), "w");
        if (!glue) {
            fprintf(stderr, "error: cannot write '%s': %s\n",
                glue_path.c_str(), strerror(errno));
            return 1;
        }
        fprintf(glue,
            "// Auto-generated by qcc %s — do not edit.\n"
            "// Link-mode C++ host for %zu .qo input%s (Phase C Item 2).\n"
            "#include <qore/Qore.h>\n"
            "#include <qore/QoreAOT.h>\n"
            "#include <stdio.h>\n\n",
            QCC_VERSION, object_paths.size(),
            object_paths.size() == 1 ? "" : "s");

        // Collect sanitized basenames for the extern decl + call
        // pattern `qore_<san>_<san>_script_register` that `qcc -c`
        // emits per .qo.
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
            entry_fn, entry_fn);
        fclose(glue);

        if (verbose) {
            printf("qcc link: emitted glue main %s (%zu .qo inputs, "
                "entry fn: %s)\n", glue_path.c_str(),
                object_paths.size(), entry_fn);
        }

        // Invoke the C++ compiler to link the glue + .qo set +
        // -lqore into the final binary.  Prefer $CXX, fall back to
        // g++.  No shell interpolation — build the argv and call
        // execvp via system()'s shell (quoting preserved by the
        // \"...\"wrapping).
        std::string cxx;
        if (const char* env_cxx = std::getenv("CXX")) {
            cxx = env_cxx;
        } else {
            cxx = "g++";
        }
        std::string cmd = cxx + " -std=c++17";
        cmd += " \"" + glue_path + "\"";
        for (const auto& op : object_paths) {
            cmd += " \"" + op + "\"";
        }
        cmd += " -lqore -o \"";
        cmd += output_path;
        cmd += "\"";
        if (verbose) {
            printf("qcc link: %s\n", cmd.c_str());
        }
        int link_rc = std::system(cmd.c_str());
        if (link_rc != 0) {
            fprintf(stderr, "error: link step failed (rc=%d): %s\n",
                link_rc, cmd.c_str());
            return 1;
        }
        printf("%s: link-mode binary (%zu .qo inputs, entry: %s)\n",
            output_path, object_paths.size(), entry_fn);
        return 0;
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
        printf("%s: archived %zu .qo input%s into .qoa (O%d%s)\n",
            output.c_str(), object_paths.size(),
            object_paths.size() == 1 ? "" : "s", opt_level,
            include_source ? "" : ", source-stripped");
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
        printf("%s: aggregated .qmod from %zu .qo input%s (O%d%s)\n",
            output.c_str(), object_paths.size(),
            object_paths.size() == 1 ? "" : "s", opt_level,
            include_source ? "" : ", source-stripped");
        // Aggregator mode re-parses every source in --context for
        // metadata extraction.  Per-.qo depfiles (emitted by the per-file
        // rule) cover the common case, but a new file added to the dir
        // without a matching `qcc -c` rule would be invisible without
        // this fallback.  Write empty-source + context depfile.
        if (depfile_path
                && !write_depfile(depfile_path, output, std::string(), context_dir)) {
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
    // per source into --output-dir.  This avoids compileScriptFile's
    // O(N^2) sibling-preload cost when the build system invokes qcc
    // per-file.
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
        bool ok = QoreAOT::compileScriptFilesBatch(
            batch_sources, batch_output_dir, PO_DEFAULT, error,
            opt_level, target_triple, include_source,
            load_modules, stub_files, parse_defines,
            parse_option_flags);
        if (!ok) {
            fprintf(stderr, "error: %s\n", error.c_str());
            qore_cleanup();
            return 1;
        }
        qore_cleanup();
        return 0;
    }

    const char* source_file = argv[optind];

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
    }

    // -c (compile-only) applies to module inputs (.qm / split
    // dir / per-file fragment thereof) and also to
    // plain script files (arbitrary .q/.qc/.ql sources).  Only reject
    // the combination if we're in neither mode.
    if (compile_only && !module_mode && !script_mode) {
        fprintf(stderr, "error: -c/--compile-only requires either a module "
                "input (.qm / split-module dir / .qc fragment with --context), "
                "or a script-mode source file (.q/.qc/.ql with optional -L<dir> "
                "preload paths)\n");
        return 1;
    }

    // -L is meaningful only in script-context compile.
    if (!script_lib_dirs.empty() && !script_mode) {
        fprintf(stderr, "error: -L <dir> is only meaningful with script-mode "
                "compile (-c on a .q/.qc/.ql source)\n");
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
            // append `.qo` (e.g. `foo.qc → foo.qo`).  Script mode has no
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
        printf("Optimization: O%d\n", opt_level);
        if (static_link) {
            printf("Static linking: enabled\n");
        }
        if (target_triple) {
            printf("Target: %s\n", target_triple);
        }
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
                stub_files)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled script-context .qo (O%d, %zu -L path%s%s)\n",
                output.c_str(), opt_level, script_lib_dirs.size(),
                script_lib_dirs.size() == 1 ? "" : "s",
                include_source ? "" : ", source-stripped");
            // deps = target source only (script mode has no
            // --context dir; -L preload is a linker-style decl path,
            // not a parser-opened source set).  Not yet wired in cmake.
            if (depfile_path && !write_depfile(depfile_path, output, source_file, nullptr)) {
                rc = 1;
            }
        }
    } else if (per_file_mode) {
        // Compile a single file from a split module
        // directory.  The directory is the parse context; the file is the
        // sole source of emitted metadata and native functions.
        if (!QoreAOT::compileSeparatedModuleFile(
                context_dir,
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled per-file .qo (O%d%s)\n", output.c_str(), opt_level,
                include_source ? "" : ", source-stripped");
            // deps = target source + every sibling .qm/.qc/.ql
            // in --context=DIR (matches compileSeparatedModuleFile's dir scan).
            if (depfile_path
                    && !write_depfile(depfile_path, output, source_file, context_dir)) {
                rc = 1;
            }
        }
    } else if (is_split_module) {
        // Compile split module directory
        if (!QoreAOT::compileSeparatedModule(
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source,
                compile_only)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled split module (O%d%s%s)\n", output.c_str(), opt_level,
                compile_only ? ", relocatable .qo" : "",
                include_source ? "" : ", source-stripped");
            // source_file is the split-module directory itself;
            // pass it as `context` so every .qm/.qc/.ql inside counts as a dep.
            // Leave `source` empty so the target dir is not double-listed.
            if (depfile_path
                    && !write_depfile(depfile_path, output, std::string(), source_file)) {
                rc = 1;
            }
        }
    } else if (module_mode) {
        // Compile single-file module
        if (!QoreAOT::compileModule(
                source_text.c_str(), (int)source_text.size(),
                source_file,
                output,
                compile_po,
                error,
                opt_level,
                target_triple,
                include_source,
                compile_only)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled module (O%d%s%s)\n", output.c_str(), opt_level,
                compile_only ? ", relocatable .qo" : "",
                include_source ? "" : ", source-stripped");
            // deps = just the .qm file
            if (depfile_path && !write_depfile(depfile_path, output, source_file, nullptr)) {
                rc = 1;
            }
        }
    } else {
        // Create program and parse
        QoreProgram* qpgm = new QoreProgram(compile_po | QoreParseOptions(PO_NEW_STYLE | PO_STRICT_ARGS
            | PO_REQUIRE_TYPES));
        ExceptionSink xsink;

        qpgm->parseFile(source_file, &xsink);

        if (xsink.isException()) {
            xsink.handleExceptions();
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
                printf("%s: compiled (O%d%s%s)\n", output.c_str(), opt_level,
                    static_link ? ", static" : "",
                    include_source ? "" : ", source-stripped");
            }
        }

        qpgm->waitForTerminationAndDeref(&xsink);
    }

    // Cleanup
    qore_cleanup();

    return rc;
}
