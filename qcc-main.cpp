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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
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
    {"from-objects",      no_argument,       nullptr, 'F'},
    {"archive",           no_argument,       nullptr, 'a'},
    {"verbose",           no_argument,       nullptr, 'v'},
    {"help",              no_argument,       nullptr, 'h'},
    {"version",           no_argument,       nullptr, 'V'},
    {nullptr,             0,                 nullptr, 0}
};

static int parse_options_cmdline(int argc, char** argv) {
    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:mcSt:TagvhV", long_options, nullptr)) != -1) {
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
                break;
            case 'C':
                context_dir = optarg;
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
    if (big_fn_threshold > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", big_fn_threshold);
        setenv("QORE_AOT_BIG_FN_THRESHOLD", buf, 1);
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
        qore_cleanup();
        return 0;
    }

    // Get source file
    if (optind >= argc) {
        fprintf(stderr, "error: no source file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
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

    // Phase 4: -c (compile-only) only makes sense when something module-shaped
    // is being compiled — a .qm, a split-module directory, or a per-file
    // fragment thereof. Reject script (executable) inputs early so users get
    // a clear error rather than a confusing failure deeper down.
    if (compile_only && !module_mode) {
        fprintf(stderr, "error: -c/--compile-only requires a module input "
                "(.qm file or split-module directory); see "
                "design/aot-phase4-qo-object-files.md\n");
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
        } else {
            // Per-file mode: default output is input-basename.qo
            // (same shape as normal single-file compile-only mode).
            output = get_default_output(source_file, module_mode, compile_only);
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

    if (per_file_mode) {
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
