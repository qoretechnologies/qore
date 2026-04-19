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
#include "qore/intern/QoreAOT.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <string>
#include <vector>
#include <getopt.h>
#include <sys/stat.h>

static const char* QCC_VERSION = "1.0";

//! Check if a path is a directory
static bool is_directory(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// Phase 4 slice 10h: write a Make-format dependency file at `path`.
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
// Phase 4: compile-only mode — emit .qo (ELF relocatable) instead of
// .qmod. Can be linked into a C++ binary (qcc -a) or back into a
// .qmod (qcc -m --from-objects). Unimplemented beyond flag plumbing;
// see design/aot-phase4-qo-object-files.md.
static bool compile_only = false;
// Phase 4 slice 4: --context=DIR passes the owning module directory
// when compiling a single file (`.qm`, `.qc`, or `.ql`) from a split
// module so the parser has the full directory as context while the
// AOT writer emits only the target file's contributions.
static const char* context_dir = nullptr;
// Phase 4 slice 10i: --output-dir=<dir> for batch-mode script compile.
// When the user passes multiple positional sources in `-c` script
// mode, qcc parses them all into one QoreProgram (one parse cycle,
// no per-file sibling preload) and emits <basename>.qo into this dir.
static const char* batch_output_dir = nullptr;
// Phase 4 slice 10c: -L<dir> directories for sibling `.qo` preload
// when compiling a single-file script with `-c` + script context (no
// module wrapper).  Each directory is scanned for `*.qo`; their
// fragment metadata is preloaded into the compile program so the
// target source's cross-file references resolve at parse time.
// Semantically analogous to C's `-L<dir>` for linker library search.
static std::vector<std::string> script_lib_dirs;
// Phase 4 slice 11a: -l/--load=<mod> modules preloaded into the
// compile program before parsing.  Mirrors the runtime-load pattern
// used by Qorus binaries (e.g. qctl_main.cpp req_modules[]) and
// matches `qore -l <mod>` CLI convention.  Types from these modules
// become visible to the parser so sources that reference them
// without %requires directives compile cleanly.  Repeatable;
// loaded in declaration order via MM.parseLoadModule.
static std::vector<std::string> load_modules;
// Phase 4 slice 11b: --stub=<file> declarative-only sources parsed
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
// Phase 4 slice 11e: --define=NAME[=VALUE] preparser defines applied
// via `qpgm->parseDefine()` before any source is parsed.  Mirrors the
// runtime `qpgm->parseDefine(...)` calls emitted by Qorus main.cpp
// files (e.g. `qpgm->parseDefine("NO_ORACLE", true)` in
// exec/qdsp_main.cpp when oracle-datasource-pool is off) so
// AOT-compiled sources honor the same `%ifdef` / `%ifndef` surface
// the runtime sees.  Repeatable; applied in declaration order.
static std::vector<std::string> parse_defines;
// Phase 4 slice 11f: --parse-option=NAME OR's a `PO_*` flag into the
// compile program's parse options.  Mirrors the runtime
// `qpgm->parseSetParseOptions(QORUS_PARSE_OPTIONS)` pattern Qorus
// main.cpp files use to extend the initial `new QoreProgram(po)` set
// (e.g. PO_ALLOW_INJECTION so cross-module `Program::loadApplyTo*`
// calls type-check at parse time).  Without this, methods whose
// `[dom=...]` mask names PO_ALLOW_* report "parse options do not
// allow access" during AOT compile.  Repeatable.
static std::vector<std::string> parse_option_flags;
// Phase 4 slice 6: --from-objects signals aggregator mode — the
// positional inputs are per-file `.qo` objects (produced by
// `qcc -c --context=DIR <file>`) that get linked together plus a
// freshly-computed metadata glue into the output `.qmod`.  See
// design/aot-phase4-qo-object-files.md.
static bool from_objects = false;
// Phase 4 slice 7: -a / --archive mode.  Combined with --context=DIR
// + positional .qo inputs, produces a `.qoa` static archive (ar rcs)
// exposing `qore_qoa_register_all(QoreProgram*)`.  Target use case:
// static linkage into a C++ host (e.g. qorus-core).
static bool archive_mode = false;
static bool include_source = false;
static bool verbose = false;
static bool show_help = false;
static bool show_version = false;
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
// Phase 4 slice 10h: --depfile=FILE emits a Make-format dependency
// file after a successful compile, listing every source file the
// target output (re)builds against.  cmake wires it via
// add_custom_command(... DEPFILE ${out}.d) so a touch on any sibling
// `.qc` in a split-dir module's --context=DIR retriggers the
// affected `.qo`.  GCC's `-MMD -MF <path>` pair maps onto this
// single long option.
static const char* depfile_path = nullptr;

static void print_usage(const char* prog) {
    printf("Qore Code Compiler (qcc) v%s\n", QCC_VERSION);
    printf("Compiles Qore scripts and modules to native executables\n\n");
    printf("Usage: %s [options] <source-file>\n\n", prog);
    printf("Options:\n");
    printf("  -o, --output=FILE      Output file path (default: input name without extension)\n");
    printf("  -O, --opt-level=N      Optimization level 0-3 (default: 3)\n");
    printf("  -m, --module           Compile as module (.qm -> .qmod)\n");
    printf("  -c, --compile-only     Compile to .qo relocatable object (Phase 4, WIP)\n");
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
    printf("      --include-source   Include source text for runtime fallback\n");
    printf("      --strip-source     Strip source text (default)\n");
    printf("      --strip-debug-info Strip DWARF debug info (faster compile, no debugger)\n");
    printf("  -g                     Emit DWARF debug info (default)\n");
    printf("      --time-trace[=PATH]  Emit Chrome-format trace of opt+codegen passes\n");
    printf("                         (default PATH: qcc.trace.json; view at chrome://tracing)\n");
    printf("      --big-fn-threshold=N  Mark functions >= N IR blocks as OptimizeNone+NoInline\n");
    printf("                         (trades ~1-7%% runtime for up to 46x compile speedup;\n");
    printf("                         default: 200; 0 = off)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -V, --version          Show version information\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s script.q                    # Compile to 'script' executable\n", prog);
    printf("  %s -o myapp script.q           # Compile to 'myapp' executable\n", prog);
    printf("  %s -m MyModule.qm              # Compile single-file module to 'MyModule.qmod'\n", prog);
    printf("  %s -m qlib/DataProvider        # Compile split module directory\n", prog);
    printf("  %s -S -o myapp script.q        # Static link (no libqore.so dependency)\n", prog);
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
    {"from-objects",      no_argument,       nullptr, 'F'},
    {"archive",           no_argument,       nullptr, 'a'},
    {"verbose",           no_argument,       nullptr, 'v'},
    {"help",              no_argument,       nullptr, 'h'},
    {"version",           no_argument,       nullptr, 'V'},
    {nullptr,             0,                 nullptr, 0}
};

static int parse_options_cmdline(int argc, char** argv) {
    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:mcSt:TL:l:agvhV", long_options, nullptr)) != -1) {
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
            case 'F':
                from_objects = true;
                break;
            case 'a':
                archive_mode = true;
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

    content.resize(fsize);
    size_t nread = fread(&content[0], 1, fsize, f);
    fclose(f);
    content.resize(nread);

    return true;
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

    // Phase 4: -c emits a relocatable .qo regardless of source kind
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

    // Phase 4 slice 7: -a / --archive mode.  Same input shape as
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

    // Phase 4 slice 6: --from-objects aggregator mode.  Takes a list of
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
        // slice 10h: aggregator re-parses every source in --context for
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

    // Phase 4 slice 10i: batch-compile mode.  When `-c` is used with
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

    // Phase 4 slice 10c: script-context mode.  When `-c` is used and
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

    // Phase 4 slice 4: --context=DIR opts the caller into per-file .qo
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

    // Phase 4: -c (compile-only) applies to module inputs (.qm / split
    // dir / per-file fragment thereof) and, as of slice 10c, also to
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

    // Phase 4 slice 10j: reject `-o` mixed with `--output-dir` for
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
            // Per-file split-module compile (slice 4, `--context=DIR`).
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
        // Phase 4 slice 10j: honor `--output-dir=DIR` for single-file
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

    if (script_mode) {
        // Phase 4 slice 10c: compile a single script-style source with
        // optional sibling-.qo decl preload.
        if (!QoreAOT::compileScriptFile(
                source_file,
                script_lib_dirs,
                output,
                PO_DEFAULT,
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
            // slice 10h: deps = target source only (script mode has no
            // --context dir; -L preload is a linker-style decl path,
            // not a parser-opened source set).  Not yet wired in cmake.
            if (depfile_path && !write_depfile(depfile_path, output, source_file, nullptr)) {
                rc = 1;
            }
        }
    } else if (per_file_mode) {
        // Phase 4 slice 4: compile a single file from a split module
        // directory.  The directory is the parse context; the file is the
        // sole source of emitted metadata and native functions.
        if (!QoreAOT::compileSeparatedModuleFile(
                context_dir,
                source_file,
                output,
                PO_DEFAULT,
                error,
                opt_level,
                target_triple,
                include_source)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled per-file .qo (O%d%s)\n", output.c_str(), opt_level,
                include_source ? "" : ", source-stripped");
            // slice 10h: deps = target source + every sibling .qm/.qc/.ql
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
                PO_DEFAULT,
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
            // slice 10h: source_file is the split-module directory itself;
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
                PO_DEFAULT,
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
            // slice 10h: deps = just the .qm file
            if (depfile_path && !write_depfile(depfile_path, output, source_file, nullptr)) {
                rc = 1;
            }
        }
    } else {
        // Create program and parse
        QoreProgram* qpgm = new QoreProgram(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
        ExceptionSink xsink;

        qpgm->parseFile(source_file, &xsink);

        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 1;
        } else {
            // Compile to executable
            QoreParseOptions po = qpgm->getParseOptions();
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
