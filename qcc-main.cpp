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

static const char* QCC_VERSION = "1.0";

// Program options
static const char* output_path = nullptr;
static int opt_level = 2;
static const char* target_triple = nullptr;
static bool static_link = false;
static bool strip_source = false;
static bool module_mode = false;
static bool verbose = false;
static bool show_help = false;
static bool show_version = false;

static void print_usage(const char* prog) {
    printf("Qore Code Compiler (qcc) v%s\n", QCC_VERSION);
    printf("Compiles Qore scripts and modules to native executables\n\n");
    printf("Usage: %s [options] <source-file>\n\n", prog);
    printf("Options:\n");
    printf("  -o, --output=FILE      Output file path (default: input name without extension)\n");
    printf("  -O, --opt-level=N      Optimization level 0-3 (default: 2)\n");
    printf("  -m, --module           Compile as module (.qm -> .qmod)\n");
    printf("  -s, --strip-source     Strip source code from binary (IP protection)\n");
    printf("  -S, --static           Link statically against libqore\n");
    printf("  -t, --target=TRIPLE    Target triple for cross-compilation\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -V, --version          Show version information\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s script.q                    # Compile to 'script' executable\n", prog);
    printf("  %s -o myapp script.q           # Compile to 'myapp' executable\n", prog);
    printf("  %s -m MyModule.qm              # Compile to 'MyModule.qmod'\n", prog);
    printf("  %s -O3 -s script.q             # Max optimization, strip source\n", prog);
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
    {"output",       required_argument, nullptr, 'o'},
    {"opt-level",    required_argument, nullptr, 'O'},
    {"module",       no_argument,       nullptr, 'm'},
    {"strip-source", no_argument,       nullptr, 's'},
    {"static",       no_argument,       nullptr, 'S'},
    {"target",       required_argument, nullptr, 't'},
    {"verbose",      no_argument,       nullptr, 'v'},
    {"help",         no_argument,       nullptr, 'h'},
    {"version",      no_argument,       nullptr, 'V'},
    {nullptr,        0,                 nullptr, 0}
};

static int parse_options_cmdline(int argc, char** argv) {
    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:msSt:vhV", long_options, nullptr)) != -1) {
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
            case 's':
                strip_source = true;
                break;
            case 'S':
                static_link = true;
                break;
            case 't':
                target_triple = optarg;
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

static std::string get_default_output(const char* input_path, bool is_module) {
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

    // Add .qmod extension for modules
    if (is_module) {
        output += ".qmod";
    }

    return output;
}

int main(int argc, char** argv) {
    // Parse command-line options
    if (parse_options_cmdline(argc, argv) != 0) {
        return 1;
    }

    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (show_version) {
        print_version();
        return 0;
    }

    // Get source file
    if (optind >= argc) {
        fprintf(stderr, "error: no source file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    const char* source_file = argv[optind];

    // Check for multiple source files (not supported)
    if (optind + 1 < argc) {
        fprintf(stderr, "error: multiple source files not supported (got '%s' after '%s')\n",
            argv[optind + 1], source_file);
        return 1;
    }

    // Auto-detect module mode from extension
    if (!module_mode) {
        size_t len = strlen(source_file);
        if (len > 3 && strcmp(source_file + len - 3, ".qm") == 0) {
            module_mode = true;
            if (verbose) {
                printf("Auto-detected module mode from .qm extension\n");
            }
        }
    }

    // Warn about ignored options
    if (static_link && module_mode) {
        fprintf(stderr, "warning: --static is ignored when compiling modules\n");
    }

    // Determine output path
    std::string output;
    if (output_path) {
        output = output_path;
    } else {
        output = get_default_output(source_file, module_mode);
    }

    // Read source file
    std::string source_text;
    if (!read_file(source_file, source_text)) {
        return 1;
    }

    if (verbose) {
        printf("Source: %s (%zu bytes)\n", source_file, source_text.size());
        printf("Output: %s\n", output.c_str());
        printf("Mode: %s\n", module_mode ? "module" : "executable");
        printf("Optimization: O%d\n", opt_level);
        if (strip_source) {
            printf("Source stripping: enabled\n");
        }
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

    if (module_mode) {
        // Compile module
        if (!QoreAOT::compileModule(
                source_text.c_str(), (int)source_text.size(),
                source_file,
                output,
                PO_DEFAULT,
                error,
                opt_level,
                target_triple,
                strip_source)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            rc = 1;
        } else {
            printf("%s: compiled module (O%d%s)\n", output.c_str(), opt_level,
                strip_source ? ", source-stripped" : "");
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
            if (!QoreAOT::compile(
                    qpgm,
                    source_text.c_str(), (int)source_text.size(),
                    source_file,
                    output,
                    qpgm->getParseOptions64(),
                    error,
                    opt_level,
                    target_triple,
                    static_link,
                    strip_source)) {
                fprintf(stderr, "error: %s\n", error.c_str());
                rc = 1;
            } else {
                printf("%s: compiled (O%d%s%s)\n", output.c_str(), opt_level,
                    strip_source ? ", source-stripped" : "",
                    static_link ? ", static" : "");
            }
        }

        qpgm->waitForTerminationAndDeref(&xsink);
    }

    // Cleanup
    qore_cleanup();

    return rc;
}
