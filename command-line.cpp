/*
    command-line.cpp

    Qore Programming Language

    this whole file comes out of a very old getopt-stype implementation

    it should offer POSIX style command-line handling on any platform...

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
#include <qore/safe_dslist>

#include "command-line.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <cmath>
#include <string>
#include <vector>
#include <map>

#define is_assign_char(a) ((((a) == '=') || ((a) == ':')))

// license type for initializing the library
static qore_license_t license = QL_GPL;

// list of modules to load after library initialization
typedef safe_dslist<std::string> cl_mod_list_t;
static cl_mod_list_t cl_mod_list;

// global parse_option
static int64 parse_options = PO_DEFAULT;
static int warnings = QP_WARN_DEFAULT;
static int qore_lib_options = QLO_NONE;

// lock options
static bool lock_options = false;

// command-line specified default character set
static const char* def_charset = 0;

// classname to instantiate as program
static const char* exec_class_name = 0;

// time zone to set after initialization
const char* cmd_zone = 0;

// show module errors
static bool show_mod_errs = false;

// execute class
static bool exec_class = false;

// treat warnings as errors
static bool warnings_are_errors = false;

// stop writing parse exceptions after 1st one
static bool only_first_except = false;

// force interactive REPL mode
static bool interactive_mode = false;

// disable automatic REPL mode when stdin is a tty
static bool no_repl = false;

// program text given on the command-line
static const char* cl_pgm = 0;

// REPL program that is executed when entering interactive mode
static const char repl_pgm[] =
   "%new-style\n"
   "%require-types\n"
   "%strict-args\n"
   "%requires QoreRepl\n"
   "\n"
   "QoreRepl::QoreRepl repl();\n"
   "repl.run();\n";

// argument to evaluate given on the command-line
static const char* eval_arg = 0;

// program name
static char* pn;

// define map type
typedef std::map<std::string, std::string> defmap_t;

// parse define map
static defmap_t defmap;

static int opt_errors = 0;

static const char usage[] = "usage: %s [option(s)]... [program file]\n";
static const char suggest[] = "try '%s -h' for more information.\n";

static const char helpstr[] =
   "  -a, --show-aliases           displays the list of character sets aliases\n"
   "  -b, --disable-signals        disables signal handling\n"
   "  -B, --show-build-options     show Qore build options and quit\n"
   "  -c, --charset=arg            sets default character set encoding\n"
   "  -D, --define=arg             sets the value of a parse define\n"
   "  -e, --exec=arg               execute program given on command-line\n"
   "  -g, --disable-gc             disable the garbage collector\n"
   "  -h, --help                   shows this help text and exit\n"
   "  -i, --list-warnings          list all warnings and quit\n"
   "  -l, --load=arg               load module 'arg' immediately\n"
   "      --lgpl,--mit             sets the library's license flag to LGPL or\n"
   "                               MIT; GPL modules cannot be loaded\n"
   "  -m, --show-module-errors     shows error messages related to loading and\n"
   "                               initializing qore modules\n"
   "      --module-dir             show qore binary module directory and exit\n"
   "      --user-module-dir        show qore user module directory and exit\n"
   "      --module-path            show qore module search path and exit\n"
   "      --module-api             show compatible qore module API version and\n"
   "                               exit\n"
   "      --module-apis            show all qore module API versions\n"
   "      --latest-module-api      show most recent module API version and exit\n"
   "  -o, --list-parse-options     list all parse options\n"
   "  -p, --set-parse-option=arg   set parse option (ex: -penable-debug)\n"
   "  -r, --warnings-are-errors    treat warnings as errors\n"
   "      --only-first-exception   don't write all parsing exceptions\n"
   "                               stop after 1st one\n"
   "  -s, --show-charsets          displays known character encodings\n"
   "  -V, --version                show program version information and quit\n"
   "      --version-animation      show animated version display and quit\n"
   "      --short-version          show short version information and quit\n"
   "  -W, --enable-all-warnings    turn on all non-strict warnings (recommended)\n"
   "      --strict-warnings        turn on strict warnings (ambiguous overloads/calls)\n"
   "  -w, --enable-warning=arg     turn on warning given by argument\n"
   "  -x, --exec-class[=arg]       instantiate class with same name as file name\n"
   "                               (override with arg, also sets --no-top-level)\n"
   "  -X, --eval=arg               evaluates argument and displays result\n"
   "  -z, --time-zone=arg          sets the time zone from the argument; can be\n"
   "                               either a region name (ex: 'Europe/Prague') or a\n"
   "                               UTC offset with format S[DD[:DD[:DD]]], S=+ or -\n"
   "\n"
   " REPL OPTIONS:\n"
   "      --interactive            force interactive REPL mode\n"
   "      --no-repl                disable automatic REPL when stdin is a terminal\n"
   "\n"
   " PARSE OPTIONS:\n"
   "  -G, --enable-debug           enable @debug and @assert statements\n"
   "  -M, --modern                 turn on recommended parse options\n"
   "      --no-global-vars         make global variable definitions illegal\n"
   "  -H, --parse-option-help      display options controlling parse options";

static const char parseopts[] =    "qore options controlling parse options:\n"
   "  -o, --list-parse-options     list all parse options\n"
   "  -p, --set-parse-option=arg   set parse option (ex: -pno-database)\n"
   "\n PARSE OPTIONS:\n"
   "  -A, --lock-warnings          do not allow changes in warning levels\n"
   "  -G, --enable-debug           enable @debug and @assert statements\n"
   "      --lockdown               only allow single-threaded code execution with\n"
   "                               no external access or terminal or GUI I/O\n"
   "      --allow-bare-refs        allow refs to vars without '$' and refs to\n"
   "                               class members without '$.'\n"
   "      --allow-reparse          allow multiple parse cycles (for REPL)\n"
   "      --assume-local           assume local scope for variables declared\n"
   "                               without 'my' or 'our'\n"
   "      --no-database            disallow access to database functionality\n"
   "      --no-external-access     disallow all external access (filesystem,\n"
   "                               network, external processes, etc)\n"
   "      --no-external-info       disallow access to external info\n"
   "  -E, --no-external-process    make access to external processes illegal\n"
   "  -F, --no-filesystem          disallow access to the local filesystem\n"
   "      --no-global-vars         make global variable definitions illegal\n"
   "      --no-gui                 do not allow access to GUI functionality\n"
   "      --no-io                  do not allow any I/O of any sort,\n"
   "  -I, --no-child-restrictions  do not restrict subprograms' parse options\n"
   "  -K, --lock-options           disable changes to parse options in program\n"
   "  -L, --no-top-level           make top-level statements illegal\n"
   "  -M, --modern                 turns on 'new-style', 'require-types', and\n"
   "                               'strict-args' (recommended)\n"
   "  -n, --new-style              turns on 'allow-bare-refs' and 'assume-local'\n"
   "                               for programming style more similar to C++/Java\n"
   "  -O, --require-our            require 'our' with global vars (recommended)\n"
   "      --require-types          require type declarations\n"
   "      --require-prototypes     require type declarations in method and function\n"
   "                               signatures\n"
   "      --no-locale-control      make locale control illegal (time zone, etc)\n"
   "  -P, --no-process-control     make process control illegal (fork(), exit(),\n"
   "                               etc)\n"
   "      --strict-args            do not ignore type errors or excess args in\n"
   "                               function and method calls\n"
   "      --no-terminal-io         do not allow access to the text terminal\n"
   "  -R, --no-thread-control      make thread control operations illegal\n"
   "      --no-thread-info         disallow access to thread info\n"
   "  -S, --no-subroutine-defs     make subroutine definitions illegal\n"
   "  -T, --no-threads             disallow thread access and control\n"
   "      --no-thread-classes      disallow access to thread classes\n"
   "  -Y, --no-network             disallow access to the network";
////12345678901234567890123456789012345678901234567890123456789012345678901234567890
static const char debugstr[] = "\n DEBUGGING OPTIONS:\n"
   "  -d, --debug=arg              sets debugging level (higher number = more output)\n"
   "  -t, --trace                  turns on function tracing"
   ;

static inline void show_usage() {
   printf(usage, pn);
}

static void show_parse_option_help(const char* arg) {
   printf("%s\n", parseopts);
   exit(0);
}

static void do_debug(const char* arg) {
   debug = atoi(arg);
}

static void set_eval_arg(const char* arg) {
   eval_arg = arg;
}

static void do_trace(const char* arg) {
   qore_trace = 1;
}

static void show_module_path(const char* arg) {
   printf("%s:%s:%s:%s\n", qore_user_module_ver_dir, qore_user_module_dir, qore_module_ver_dir, qore_module_dir);
   exit(0);
}

static void show_binary_module_dir(const char* arg) {
   printf("%s\n", qore_module_dir);
   exit(0);
}

static void show_user_module_dir(const char* arg) {
   printf("%s\n", qore_user_module_dir);
   exit(0);
}

static void show_module_api(const char* arg) {
   printf("%d.%d\n", qore_min_mod_api_major, qore_min_mod_api_minor);
   exit(0);
}

static void show_module_apis(const char* arg) {
   // show all module apis
   printf("%d.%d", qore_mod_api_list[0].major, qore_mod_api_list[0].minor);
   for (unsigned i = 1; i < qore_mod_api_list_len; ++i)
      printf(", %d.%d", qore_mod_api_list[i].major, qore_mod_api_list[i].minor);
   printf("\n");

   exit(0);
}

static void show_latest_module_api(const char* arg) {
   printf("%d.%d\n", qore_mod_api_list[0].major, qore_mod_api_list[0].minor);
   exit(0);
}

static void set_parse_option(const char* arg) {
   int64 code = ParseOptionMap::find_code64(arg);
   if (code == -1) {
      fprintf(stderr, "unknown parse option '%s', use -o or --list-parse-options\n", arg);
      exit(1);
   }
   parse_options |= code;
}

static void only_first_exception(const char* arg) {
   only_first_except = true;
}

static void do_interactive(const char* arg) {
   interactive_mode = true;
}

static void do_no_repl(const char* arg) {
   no_repl = true;
}

static void list_parse_options(const char* arg) {
   ParseOptionMap::list_options();
   exit(0);
}

static void do_help(const char* arg) {
   show_usage();
   puts(helpstr);
   if (qore_has_debug())
      puts(debugstr);
   exit(0);
}

static void disable_signals(const char* arg) {
   qore_lib_options |= QLO_DISABLE_SIGNAL_HANDLING;
}

static void load_module(const char* arg) {
   cl_mod_list.push_back(arg);
}

static void warn_to_err(const char* arg) {
   warnings_are_errors = true;
}

static void enable_warnings(const char* arg) {
   warnings = QP_WARN_ALL;
}

static void enable_warning(const char* arg) {
   int code = get_warning_code(arg);
   if (!code) {
      printf("cannot enable unknown warning '%s'\n", arg);
      exit(1);
   }
   warnings |= code;
}

static void list_warnings(const char* arg) {
   for (unsigned i = 0; i < qore_num_warnings; i++)
      printf("%s\n", qore_warnings[i]);
   exit(0);
}

static void do_no_terminal_io(const char* arg) {
   parse_options |= PO_NO_TERMINAL_IO;
}

static void do_no_io(const char* arg) {
   parse_options |= PO_NO_IO;
}

static void do_no_gui(const char* arg) {
   parse_options |= PO_NO_GUI;
}

static void do_no_class_defs(const char* arg) {
   fprintf(stderr, "WARNING: --no-class-defs is deprecated and has no effect (PO_NO_CLASS_DEFS was never enforced)\n");
   parse_options |= PO_NO_CLASS_DEFS;
}

static void do_no_database(const char* arg) {
   parse_options |= PO_NO_DATABASE;
}

static void do_lockdown(const char* arg) {
   parse_options |= PO_LOCKDOWN;
}

static void do_lock_warnings(const char* arg) {
   parse_options |= PO_LOCK_WARNINGS;
}

static void do_no_global_vars(const char* arg) {
   parse_options |= PO_NO_GLOBAL_VARS;
}

static void do_no_subroutine_defs(const char* arg) {
   parse_options |= PO_NO_SUBROUTINE_DEFS;
}

static void do_no_network(const char* arg) {
   parse_options |= PO_NO_NETWORK;
}

static void do_no_threads(const char* arg) {
   parse_options |= PO_NO_THREADS;
}

static void do_no_thread_control(const char* arg) {
   parse_options |= PO_NO_THREAD_CONTROL;
}

static void do_no_thread_classes(const char* arg) {
   parse_options |= PO_NO_THREAD_CLASSES;
}

static void do_no_top_level(const char* arg) {
   parse_options |= PO_NO_TOP_LEVEL_STATEMENTS;
}

static void do_allow_reparse(const char* arg) {
   parse_options |= PO_ALLOW_REPARSE;
}

static void do_no_filesystem(const char* arg) {
   parse_options |= PO_NO_FILESYSTEM;
}

static void do_no_child_po_restrictions(const char* arg) {
   parse_options |= PO_NO_CHILD_PO_RESTRICTIONS;
}

static void do_no_external_access(const char* arg) {
   parse_options |= PO_NO_EXTERNAL_ACCESS;
}

static void do_no_external_info(const char* arg) {
   parse_options |= PO_NO_EXTERNAL_INFO;
}

static void do_no_external_process(const char* arg) {
   parse_options |= PO_NO_EXTERNAL_PROCESS;
}

static void do_no_locale_control(const char* arg) {
   parse_options |= PO_NO_LOCALE_CONTROL;
}

static void do_no_process_control(const char* arg) {
   parse_options |= PO_NO_PROCESS_CONTROL;
}

static void do_no_thread_info(const char* arg) {
   parse_options |= PO_NO_THREAD_INFO;
}

static void do_require_our(const char* arg) {
   parse_options |= PO_REQUIRE_OUR;
}

static void do_require_types(const char* arg) {
   parse_options |= PO_REQUIRE_TYPES;
}

static void do_require_prototypes(const char* arg) {
   parse_options |= PO_REQUIRE_PROTOTYPES;
}

static void do_strict_args(const char* arg) {
   parse_options |= PO_STRICT_ARGS;
}

static void do_strict_warnings(const char* arg) {
   warnings |= QP_WARN_STRICT;
}

static void do_lock_options(const char* arg) {
   lock_options = true;
}

static void allow_bare_refs(const char* arg) {
   parse_options |= PO_ALLOW_BARE_REFS;
}

static void assume_local(const char* arg) {
   parse_options |= PO_ASSUME_LOCAL;
}

static void new_style(const char* arg) {
   parse_options |= PO_NEW_STYLE;
}

static void do_modern(const char* arg) {
   parse_options |= PO_MODERN;
}

static void do_enable_debug(const char* arg) {
   parse_options |= PO_ENABLE_DEBUG;
}

static void short_version(const char* arg) {
   printf("%s\n", qore_version_string);
   exit(0);
}

static void set_time_zone(const char* arg) {
   cmd_zone = arg;
}

static void set_define(const char* carg) {
   QoreString str(carg);
   // trim trailing and leading whitespace
   str.trim();
   // find assignment character, if any
   const char* p = strchr(str.getBuffer(), '=');
   QoreString arg;
   if (p) {
      // copy arg to arg string
      arg.set(p + 1);
      // trim leading whitespace, if any
      arg.trim_leading();
      // remove argument from define string
      str.terminate(p - str.getBuffer());
   }
   // check define string
   if (str.empty()) {
      fprintf(stderr, "missing argument for --define\n");
      exit(1);
   }

   if (defmap.find(str.getBuffer()) != defmap.end()) {
      fprintf(stderr, "more than one definition for parse define '%s' given\n", str.getBuffer());
      exit(1);
   }

   defmap[str.getBuffer()] = arg.getBuffer();
}

static const char* tlist[] = { "OPTION", "ALGORITHM", "FUNCTION", "UNKNOWN" };

static void show_build_options(const char* arg) {
   printf("this build has options:\n");
   // find longest option name
   int len = 0;
   for (size_t j = 0; j < qore_option_list_size; ++j) {
      int ilen = strlen(qore_option_list[j].option);
      if (ilen > len)
         len = ilen;
   }
   // create format string
   QoreString fmt(" %9s %-");
   fmt.sprintf("%d", len + 1);
   fmt.concat("s = %s\n");

   for (unsigned j = 0; j < qore_option_list_size; ++j) {
      int type = qore_option_list[j].type;
      if (type > QO_FUNCTION)
         type = QO_FUNCTION + 1;
      printf(fmt.getBuffer(), tlist[type], qore_option_list[j].option,
             qore_option_list[j].value ? "true" : "false");
   }
   exit(0);
}

// word-wrap a string to max_width, returning wrapped lines
// indent is prepended to continuation lines
static std::vector<std::string> wrap_line(const std::string& line, int max_width,
        const std::string& indent) {
    std::vector<std::string> result;
    if (max_width <= 0 || (int)line.size() <= max_width) {
        result.push_back(line);
        return result;
    }

    std::string remaining = line;
    bool first = true;
    while (!remaining.empty()) {
        int width = first ? max_width : max_width - (int)indent.size();
        if (width <= 0) {
            width = 1;
        }
        if ((int)remaining.size() <= width) {
            result.push_back(first ? remaining : indent + remaining);
            break;
        }
        // find last space within width
        int split = width;
        while (split > 0 && remaining[split] != ' ') {
            --split;
        }
        if (split == 0) {
            // no space found; hard break
            split = width;
        }
        result.push_back((first ? "" : indent) + remaining.substr(0, split));
        // skip the space at the split point
        remaining = remaining.substr(remaining[split] == ' ' ? split + 1 : split);
        first = false;
    }
    return result;
}

static const char* version_art[] = {
    "                  .,l~-?-~l,.                 ",
    "             ',l~-]]]]]]]]]-<l,.             ",
    "         ',!~?]]]]?????????]]]]?~!,'         ",
    "     ':!+?]]]]???????]]]???????]]]]?~!,'     ",
    "    !?]]]]??????]]]]]_~-]]]]???????]]]]?!    ",
    "   ']]????]]]]]]]_>;^  .^I>_]]]]]]]????]]'   ",
    "   '???]]]]]]?>;`           ^;<??]]]]]?\?\?'   ",
    "   '?]?]]]]?]?.               .?]?]]]]?]?'   ",
    "   '?]?]]]]?]?`               `?]?]]]]?]?'   ",
    "   '?]?]]]]?]?'               '?]?]]]]?]?'   ",
    "   '?]?]]]]?]?^               ^?]?]]]]?]?'   ",
    "   '???]]]]]]]-~!,'      .`,l~-?]]]]]]?\?\?'   ",
    "   .?]]????]]]]]]]?~!,'  ;~]]]]]??????]]?.   ",
    "    \">_]]]]]??????]]]]?+i:^`,l~?]]]]]]_>^    ",
    "      .^;>_]]]]]??????]]]]?+!:^`\"I<>I^.      ",
    "           `;i_?]]]]??????]]]]]_i:`          ",
    "               `:i+?]]]]???????]]]]?~:       ",
    "                   ',!~-]]]]]]]]?+i;^.       ",
    "                       ',!+-?_>;`            ",
};
static const int ART_LINES = 19;
static const int ART_WIDTH = 45;
static const int ART_GAP = 2;

// Build version info lines, word-wrapped to fit the terminal
static void build_version_info(std::vector<std::string>& info, int max_info) {
    std::vector<std::string> raw_info;

    QoreString buf;
    buf.sprintf("QORE for %s %s (%d-bit build), Copyright (C) 2003 - 2026 David Nichols",
        qore_target_os, qore_target_arch, qore_target_bits);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("version %s", qore_version_string);
    FeatureList::iterator i = qoreFeatureList.begin();
    if (i != qoreFeatureList.end()) {
        buf.concat(" (builtin features: ");
        while (i != qoreFeatureList.end()) {
            buf.concat((*i).c_str());
            i++;
            if (i != qoreFeatureList.end()) {
                buf.concat(", ");
            }
        }
        buf.concat(')');
    }
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("git hash: %s", qore_git_hash);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("module API: %d.%d", qore_mod_api_list[0].major, qore_mod_api_list[0].minor);
    if (qore_mod_api_list_len > 1) {
        buf.concat(" (");
        for (unsigned j = 1; j < qore_mod_api_list_len; ++j) {
            buf.sprintf("%d.%d", qore_mod_api_list[j].major, qore_mod_api_list[j].minor);
            if (j != (qore_mod_api_list_len - 1)) {
                buf.concat(", ");
            }
        }
        buf.concat(")");
    }
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("build host: %s", qore_build_host);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("C++ compiler: %s", qore_cplusplus_compiler);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("CFLAGS: %s", qore_cflags);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("LDFLAGS: %s", qore_ldflags);
    raw_info.push_back(buf.c_str());

    buf.clear();
    buf.sprintf("MPFR: %s", mpfrInfo.getBuffer());
    raw_info.push_back(buf.c_str());

    raw_info.push_back("use -B to show build options");

    for (const auto& line : raw_info) {
        std::string indent;
        size_t pos = line.find_first_not_of(' ');
        if (pos != std::string::npos && pos > 0) {
            indent = std::string(pos + 2, ' ');
        } else {
            indent = "    ";
        }
        auto wrapped = wrap_line(line, max_info, indent);
        for (const auto& wl : wrapped) {
            info.push_back(wl);
        }
    }
}

// Render static colored ASCII art with info text on the right
static void render_static_version(const char* qc, const char* reset, bool use_color,
                                  const std::vector<std::string>& info, int info_start,
                                  int total_lines) {
    for (int line = 0; line < total_lines; ++line) {
        if (line < ART_LINES) {
            const char* a = version_art[line];
            int len = (int)strlen(a);
            bool in_color = false;
            for (int c = 0; c < ART_WIDTH; ++c) {
                char ch = c < len ? a[c] : ' ';
                if (ch != ' ') {
                    if (!in_color && use_color) {
                        fputs(qc, stdout);
                        in_color = true;
                    }
                    putchar(ch);
                } else {
                    if (in_color) {
                        fputs(reset, stdout);
                        in_color = false;
                    }
                    putchar(' ');
                }
            }
            if (in_color) {
                fputs(reset, stdout);
            }
        } else {
            printf("%-*s", ART_WIDTH, "");
        }

        int info_idx = line - info_start;
        if (info_idx >= 0 && info_idx < (int)info.size()) {
            printf("  %s", info[info_idx].c_str());
        }

        putchar('\n');
    }
}

static void do_version(const char* arg) {
    bool is_tty = isatty(STDOUT_FILENO);

    // Non-TTY: plain text output for machine parseability
    if (!is_tty) {
        printf("QORE for %s %s (%d-bit build), Copyright (C) 2003 - 2026 David Nichols\n",
            qore_target_os, qore_target_arch, qore_target_bits);

        printf("version %s", qore_version_string);
        FeatureList::iterator i = qoreFeatureList.begin();
        if (i != qoreFeatureList.end()) {
            printf(" (builtin features: ");
            while (i != qoreFeatureList.end()) {
                fputs((*i).c_str(), stdout);
                i++;
                if (i != qoreFeatureList.end()) {
                    printf(", ");
                }
            }
            putchar(')');
        }

        printf("\n  git hash: %s", qore_git_hash);

        printf("\n  module API: %d.%d", qore_mod_api_list[0].major, qore_mod_api_list[0].minor);
        if (qore_mod_api_list_len > 1) {
            printf(" (");
            for (unsigned j = 1; j < qore_mod_api_list_len; ++j) {
                printf("%d.%d", qore_mod_api_list[j].major, qore_mod_api_list[j].minor);
                if (j != (qore_mod_api_list_len - 1)) {
                    printf(", ");
                }
            }
            printf(")");
        }

        printf("\n  build host: %s\n  C++ compiler: %s\n  CFLAGS: %s\n  LDFLAGS: %s\n  MPFR: %s\n",
            qore_build_host, qore_cplusplus_compiler, qore_cflags, qore_ldflags, mpfrInfo.getBuffer());

        printf("use -B to show build options\n");

        exit(0);
    }

    // TTY: colored ASCII art with version info on the right
    bool use_color = !getenv("NO_COLOR");
    const char* qc = use_color ? "\033[1;38;2;255;50;140m" : "";
    const char* reset = use_color ? "\033[0m" : "";

    // Determine terminal width
    int term_width = 0;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        term_width = ws.ws_col;
    }
    int max_info = term_width > ART_WIDTH + ART_GAP ? term_width - ART_WIDTH - ART_GAP : 0;

    std::vector<std::string> info;
    build_version_info(info, max_info);

    int info_start = (ART_LINES - (int)info.size()) / 2;
    if (info_start < 0) {
        info_start = 0;
    }

    int total_lines = ART_LINES;
    if (info_start + (int)info.size() > total_lines) {
        total_lines = info_start + (int)info.size();
    }

    render_static_version(qc, reset, use_color, info, info_start, total_lines);

    exit(0);
}

static void do_version_animation(const char* arg) {
    bool is_tty = isatty(STDOUT_FILENO);

    // Non-TTY: fall back to static version
    if (!is_tty) {
        do_version(arg);
        return;
    }

    // TTY: spinning coin animation followed by static art
    bool use_color = !getenv("NO_COLOR");
    const char* qc = use_color ? "\033[1;38;2;255;50;140m" : "";
    const char* reset = use_color ? "\033[0m" : "";

    // Determine terminal width
    int term_width = 0;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        term_width = ws.ws_col;
    }
    int max_info = term_width > ART_WIDTH + ART_GAP ? term_width - ART_WIDTH - ART_GAP : 0;

    std::vector<std::string> info;
    build_version_info(info, max_info);

    int info_start = (ART_LINES - (int)info.size()) / 2;
    if (info_start < 0) {
        info_start = 0;
    }

    int total_lines = ART_LINES;
    if (info_start + (int)info.size() > total_lines) {
        total_lines = info_start + (int)info.size();
    }

    // Character density for dimming art characters during rotation.
    // Returns 0 (invisible) to 10 (densest).
    auto char_density = [](char c) -> int {
        switch (c) {
            case '.': case ',': case '\'': case '`': case '^': return 1;
            case ':': case ';': return 2;
            case '-': case '~': case '!': return 3;
            case '=': case '+': return 4;
            case '*': case 'i': case 'l': return 5;
            case '<': case '>': case 'I': return 6;
            case '?': case '"': return 7;
            case ']': case '[': case '_': return 8;
            case '#': return 9;
            case '%': case '@': return 10;
            default: return 4;
        }
    };

    // Reverse mapping: density index -> representative character
    static const char density_char[] = " .:-=*<?]#@";
    static const int MAX_DENSITY = 10;

    double center_x = ART_WIDTH / 2.0;

    // Animation: one full 360-degree rotation over 3 seconds
    static const int ANIM_FRAMES = 180;
    static const long ANIM_DURATION_NS = 3000000000L; // 3 seconds in nanoseconds

    // Pre-compute art string lengths
    int art_len[ART_LINES];
    for (int y = 0; y < ART_LINES; ++y) {
        art_len[y] = (int)strlen(version_art[y]);
    }

    // Hide cursor
    fputs("\033[?25l", stdout);

    // Set stdin to raw non-blocking mode so we can detect keypress
    struct termios orig_termios, raw_termios;
    bool raw_mode = (tcgetattr(STDIN_FILENO, &orig_termios) == 0);
    if (raw_mode) {
        raw_termios = orig_termios;
        raw_termios.c_lflag &= ~(ICANON | ECHO);
        raw_termios.c_cc[VMIN] = 0;
        raw_termios.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
    }

    struct timespec anim_start;
    clock_gettime(CLOCK_MONOTONIC, &anim_start);

    for (int frame = 0; frame < ANIM_FRAMES; ++frame) {
        // Time-warped cosine: theta advances faster at face-on (less dwell)
        // and slower at edge-on. Perfectly smooth — no kinks or bouncing.
        double t = (double)frame / ANIM_FRAMES;
        double theta = 2.0 * M_PI * t + 0.35 * sin(4.0 * M_PI * t);
        double cos_theta = cos(theta);
        double abs_cos = fabs(cos_theta);

        // Move cursor up to top of art area (except for first frame)
        if (frame > 0) {
            printf("\033[%dA", total_lines);
        }

        for (int line = 0; line < total_lines; ++line) {
            // Render art area (exactly ART_WIDTH characters)
            if (line < ART_LINES) {
                bool in_color = false;
                for (int x = 0; x < ART_WIDTH; ++x) {
                    // Inverse-project screen x to object x through Y-axis rotation
                    // Forward: screen_x = center + (obj_x - center) * cos(theta)
                    // Inverse: obj_x = center + (screen_x - center) / cos(theta)
                    char render_char = ' ';
                    bool filled = false;

                    if (abs_cos > 0.05) {
                        // Negative cos_theta (back face) naturally mirrors via division
                        int obj_x = (int)(center_x + (x - center_x) / cos_theta + 0.5);

                        if (obj_x >= 0 && obj_x < art_len[line]) {
                            char art_char = version_art[line][obj_x];
                            if (art_char != ' ') {
                                // Dim the character based on viewing angle
                                if (abs_cos > 0.95) {
                                    // Near face-on: use original art character
                                    render_char = art_char;
                                } else {
                                    int density = char_density(art_char);
                                    int dimmed = (int)(density * abs_cos + 0.5);
                                    if (dimmed > MAX_DENSITY) {
                                        dimmed = MAX_DENSITY;
                                    }
                                    render_char = density_char[dimmed];
                                }
                                filled = (render_char != ' ');
                            }
                        }
                    }

                    if (filled) {
                        if (!in_color && use_color) {
                            fputs(qc, stdout);
                            in_color = true;
                        }
                        putchar(render_char);
                    } else {
                        if (in_color) {
                            fputs(reset, stdout);
                            in_color = false;
                        }
                        putchar(' ');
                    }
                }
                if (in_color) {
                    fputs(reset, stdout);
                }
            } else {
                printf("%-*s", ART_WIDTH, "");
            }

            // Print info text only on first frame (it stays in place)
            if (frame == 0) {
                int info_idx = line - info_start;
                if (info_idx >= 0 && info_idx < (int)info.size()) {
                    printf("  %s", info[info_idx].c_str());
                }
            }

            putchar('\n');
        }

        fflush(stdout);

        // Check for keypress to skip animation
        if (raw_mode) {
            struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
            if (poll(&pfd, 1, 0) > 0) {
                // Consume the key and break to final frame
                char discard;
                while (read(STDIN_FILENO, &discard, 1) > 0) {
                }
                break;
            }
        }

        // Sleep until the wall-clock time for the next frame
        long next_frame_ns = ANIM_DURATION_NS * (frame + 1) / ANIM_FRAMES;
        struct timespec target;
        target.tv_sec = anim_start.tv_sec + next_frame_ns / 1000000000L;
        target.tv_nsec = anim_start.tv_nsec + next_frame_ns % 1000000000L;
        if (target.tv_nsec >= 1000000000L) {
            target.tv_sec++;
            target.tv_nsec -= 1000000000L;
        }
        // Sleep until the target time (portable: works on macOS and Linux)
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long sleep_ns = (target.tv_sec - now.tv_sec) * 1000000000L
            + (target.tv_nsec - now.tv_nsec);
        if (sleep_ns > 0) {
            struct timespec rem, req;
            req.tv_sec = sleep_ns / 1000000000L;
            req.tv_nsec = sleep_ns % 1000000000L;
            while (nanosleep(&req, &rem) != 0) {
                req = rem;
            }
        }
    }

    // Restore terminal mode
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }

    // Final frame: render the static colored ASCII art
    printf("\033[%dA", total_lines);
    render_static_version(qc, reset, use_color, info, info_start, total_lines);

    // Show cursor
    fputs("\033[?25h", stdout);
    fflush(stdout);

    exit(0);
}

static void set_charset(const char* arg) {
    def_charset = arg;
}

static void show_charsets(const char* arg) {
    QEM.showEncodings();
    exit(0);
}

static void show_charset_aliases(const char* arg) {
    QEM.showAliases();
    exit(0);
}

static void set_exec(const char* arg) {
    cl_pgm = arg;
}

static void disable_gc(const char* arg) {
    qore_lib_options |= QLO_DISABLE_GARBAGE_COLLECTION;
}

static void show_module_errors(const char* arg) {
    show_mod_errs = true;
}

static void do_exec_class(const char* arg) {
    //printf("do_exec_class(%s)\n", arg);
    exec_class = true;
    exec_class_name = arg;
    parse_options |= PO_NO_TOP_LEVEL_STATEMENTS;
}

static void set_lgpl(const char* arg) {
    license = QL_LGPL;
}

static void set_mit(const char* arg) {
    license = QL_MIT;
}

#define ARG_NONE 0
#define ARG_MAND 1
#define ARG_OPT  2

static struct opt_struct_s {
      char short_opt;
      const char* long_opt;
      int arg;
      void (*opt_func)(const char* arg);
} options[] = {
   { 'a', "show-aliases",          ARG_NONE, show_charset_aliases },
   { 'B', "show-built-options",    ARG_NONE, show_build_options },
   { 'c', "charset",               ARG_MAND, set_charset },
   { 'e', "exec",                  ARG_MAND, set_exec },
   { 'g', "disable-gc",            ARG_NONE, disable_gc },
   { 'h', "help",                  ARG_NONE, do_help },
   { 'i', "list-warnings",         ARG_NONE, list_warnings },
   { 'l', "load",                  ARG_MAND, load_module },
   { 'm', "show-module-errors",    ARG_NONE, show_module_errors },
   { 'o', "list-parse-options",    ARG_NONE, list_parse_options },
   { 'p', "set-parse-option",      ARG_MAND, set_parse_option },
   { '\0', "only-first-exception", ARG_NONE, only_first_exception },
   { '\0', "interactive",          ARG_NONE, do_interactive },
   { '\0', "no-repl",              ARG_NONE, do_no_repl },
   { 'r', "warnings-are-errors",   ARG_NONE, warn_to_err },
   { 's', "show-charsets",         ARG_NONE, show_charsets },
   { '\0', "strict-warnings",      ARG_NONE, do_strict_warnings },
   { 'w', "enable-warning",        ARG_MAND, enable_warning },
   { 'x', "exec-class",            ARG_OPT,  do_exec_class },
   { '\0', "lockdown",             ARG_NONE, do_lockdown },
   { 'A', "lock-warnings",         ARG_NONE, do_lock_warnings },
   { '\0', "allow-bare-refs",      ARG_NONE, allow_bare_refs },
   { '\0', "allow-reparse",        ARG_NONE, do_allow_reparse },
   { '\0', "assume-local",         ARG_NONE, assume_local },
   { 'M', "modern",                ARG_NONE, do_modern },
   { 'n', "new-style",             ARG_NONE, new_style },
   { '\0', "no-class-defs",        ARG_NONE, do_no_class_defs },
   { 'G', "enable-debug",          ARG_NONE, do_enable_debug },
   { '\0', "no-database",          ARG_NONE, do_no_database },
   { 'D', "define",                ARG_MAND, set_define },
   { 'E', "no-external-process",   ARG_NONE, do_no_external_process },
   { '\0', "no-external-access",   ARG_NONE, do_no_external_access },
   { '\0', "no-external-info",     ARG_NONE, do_no_external_info },
   { 'F', "no-filesystem",         ARG_NONE, do_no_filesystem },
   { '\0', "no-global-vars",       ARG_NONE, do_no_global_vars },
   { 'H', "parse-option-help",     ARG_NONE, show_parse_option_help },
   { 'I', "no-child-restrictions", ARG_NONE, do_no_child_po_restrictions },
   { 'K', "lock-options",          ARG_NONE, do_lock_options },
   { 'L', "no-top-level",          ARG_NONE, do_no_top_level },
   { 'O', "require-our",           ARG_NONE, do_require_our },
   { '\0', "require-types",        ARG_NONE, do_require_types },
   { '\0', "no-locale-controle",   ARG_NONE, do_no_locale_control },
   { '\0', "require-prototypes",   ARG_NONE, do_require_prototypes },
   { '\0', "strict-args",          ARG_NONE, do_strict_args },
   { 'P', "no-process-control",    ARG_NONE, do_no_process_control },
   { 'R', "no-thread-control",     ARG_NONE, do_no_thread_control },
   { 'S', "no-subroutine-defs",    ARG_NONE, do_no_subroutine_defs },
   { 'T', "no-threads",            ARG_NONE, do_no_threads },
   { 'V', "version",               ARG_NONE, do_version },
   { '\0', "version-animation",    ARG_NONE, do_version_animation },
   { 'W', "enable-all-warnings",   ARG_NONE, enable_warnings },
   { '\0', "no-thread-classes",    ARG_NONE, do_no_thread_classes },
   { '\0', "no-thread-info",       ARG_NONE, do_no_thread_info },
   { 'X', "eval",                  ARG_MAND, set_eval_arg },
   { 'Y', "no-network",            ARG_NONE, do_no_network },
   { 'z', "time-zone",             ARG_MAND, set_time_zone },
   { '\0', "no-terminal-io",       ARG_NONE, do_no_terminal_io },
   { '\0', "no-gui",               ARG_NONE, do_no_gui },
   { '\0', "no-io",                ARG_NONE, do_no_io },
   { '\0', "lgpl",                 ARG_NONE, set_lgpl },
   { '\0', "mit",                  ARG_NONE, set_mit },
   { '\0', "module-dir",           ARG_NONE, show_binary_module_dir },
   { '\0', "user-module-dir",      ARG_NONE, show_user_module_dir },
   { '\0', "module-path",          ARG_NONE, show_module_path },
   { '\0', "short-version",        ARG_NONE, short_version },
   { '\0', "module-api",           ARG_NONE, show_module_api },
   { '\0', "module-apis",          ARG_NONE, show_module_apis },
   { '\0', "latest-module-api",    ARG_NONE, show_latest_module_api },
   // debugging options
   { 'b', "disable-signals",       ARG_NONE, disable_signals },
   { 'd', "debug",                 ARG_MAND, do_debug },
   { 't', "trace",                 ARG_NONE, do_trace },
};

#define NUM_OPTS (sizeof(options) / sizeof(struct opt_struct_s))

static inline void missing_char_option(int i) {
   printe("option '%c' requires an argument.\n", options[i].short_opt);
   opt_errors++;
}

static inline void missing_str_option(int i) {
   printe("option '%s' requires an argument.\n", options[i].long_opt);
   opt_errors++;
}

static inline void excess_option(int i) {
   printe("option '%s' does not take an argument.\n", options[i].long_opt);
   opt_errors++;
}

static inline void invalid_option(char* opt) {
   printe("error: '--%s' is not a valid long option.\n", opt);
   opt_errors++;
}

static inline void invalid_option(char opt) {
   printe("error: '-%c' is not a valid short option.\n", opt);
   opt_errors++;
}

// *i is the argument position, *j is the index in the string
static const char* get_arg(char* argv[], unsigned *i, unsigned *j, unsigned argc) {
   if (*i >= argc)
      return 0;
   // if next character is an assignment character, then advance character pointer
   if (is_assign_char(argv[*i][*j]))
      (*j)++;
   // if at end of argument string, try next one
   if (!argv[*i][*j])
   {
      // increment argument pointer
      (*i)++;
      // set character pointer to first character
      (*j) = 0;
      // if there are no more strings then return 0!
      if ((*i) == argc)
         return 0;
   }
   return &argv[*i][*j];
}

static void process_str_opt(char* argv[], unsigned *i, unsigned j, unsigned argc);

// *i is the argument position, *j is the index in the string
static int process_char_opt(char* argv[], unsigned *i, unsigned *j, unsigned argc) {
   unsigned x;

   if (isblank(argv[*i][*j])) {
      do
         (*j)++;
      while (isblank(argv[*i][*j]));
      if (argv[*i][*j] == '-') {
         if (argv[*i][*j] == '-') {
            process_str_opt(argv, i, *j + 1, argc);
            return 1;
         }
         else
            (*j)++;
      }
   }

   char c = argv[*i][*j];

   for (x = 0; x < NUM_OPTS; x++)
      if (options[x].short_opt == c) {
         //printf("found '%c' %s (%d)\n", c, options[x].long_opt, options[x].arg);
         if (options[x].arg == ARG_MAND ||
             (options[x].arg == ARG_OPT && (argv[*i][(*j) + 1] == '='))) {
            const char* arg;

            // increment string index
            (*j)++;
            if (!(arg = get_arg(argv, i, j, argc)))
               missing_char_option(x);
            else
               options[x].opt_func(arg);
            /* as the argument pointer always advances to the next argument
             * due to the get_arg() function, return 1 to break out of the
             * character loop for the current arg */
            return 1;
         }
         options[x].opt_func(0);
         return 0;
      }
   // if the option is not present, then raise an error
   invalid_option(c);
   return 0;
}

// *i is the argument position
static void process_str_opt(char* argv[], unsigned *i, unsigned j, unsigned argc)
{
   unsigned x, option_present = 0;
   char* opt = &argv[*i][j];

   // find option string (left side of string if there is an assignment char)
   for (x = 2; x < strlen(argv[*i]); x++) {
      if (is_assign_char(argv[*i][x])) {
         option_present = x + 1;
         opt = (char* )malloc(sizeof(char) * (x - 1));
         strncpy(opt, &argv[*i][2], x - 2);
         opt[x - 2] = '\0';
         break;
      }
   }
   // if the option is not in the same argument, then increment the argument pointer
   if (!option_present)
      ++(*i);

   for (x = 0; x < NUM_OPTS; x++) {
      if (!strcmp(options[x].long_opt, opt)) {
         if (!options[x].arg) {
            if (option_present)
               excess_option(x);
            else
               --(*i);
            options[x].opt_func(0);
         }
         else if (options[x].arg == ARG_OPT) {
            const char* arg = 0;

            if (option_present)
               arg = get_arg(argv, i, &option_present, argc);
            else
               --(*i);
            options[x].opt_func(arg);
         }
         else {
            const char* arg;

            if (!(arg = get_arg(argv, i, &option_present, argc)))
               missing_str_option(x);
            else
               options[x].opt_func(arg);
         }
         break;
      }
   }

   // if the option is not present, then raise an error
   if (x == NUM_OPTS)
      invalid_option(opt);

   if (option_present)
      free(opt);
}

// returns either 0 or a string that must be freed with free()
// also sets up the global ARGV argument list
static char* parse_command_line(unsigned argc, char* argv[]) {
   pn = basename(argv[0]);

   // file name to return, if any
   char* fn = 0;

   unsigned i = 1;

   // check all arguments
   for (; i < argc; i++) {
      printd(5, "parse_command_line() %d/%d=%s\n", i, argc, argv[i]);
      if (argv[i][0] == '-') {
         if (!argv[i][1]) {
            i++;
            break;
         }
         else {
            if (argv[i][1] == '-') {
               if (!argv[i][2]) {
                  i++;
                  break;
               }
               process_str_opt(argv, &i, 2, argc);
            }
            else {
               unsigned j;

               for (j = 1; j < strlen(argv[i]); j++)
                  if (process_char_opt(argv, &i, &j, argc))
                     break;
            }
         }
      }
      else {
         // only set the file name if the --exec option has not been set
         if (!cl_pgm)
            fn = strdup(argv[i++]);
         break;
      }
   }

// pvanek - argc check removed, just because the i value is checked
// directly in qore_setup_argv(). ARGV and mainly QORE_ARGV should
// be set in any case.
   qore_setup_argv(i, argc, argv);

   if (opt_errors) {
      printe(suggest, pn);
      exit(1);
   }
   return fn;
}

int qore_main_intern(int argc, char* argv[], int other_po) {
   int rc = 0;

   if (other_po)
      parse_options |= other_po;

   // parse the command line
   char* program_file_name = parse_command_line(argc, argv);
   ON_BLOCK_EXIT(free, program_file_name);

   // initialize Qore subsystem
   qore_init(license, def_charset, show_mod_errs, qore_lib_options);

   ExceptionSink wsink, xsink;
   {
      QoreProgramHelper qpgm(parse_options, xsink);
      bool mod_errs = false;

      // set parse defines
      qpgm->parseCmdLineDefines(xsink, wsink, warnings, defmap);

      if (xsink.isException()) {
         rc = 2;
         xsink.handleExceptions();
         goto exit;
      }

      // load any modules requested on the command-line
      for (cl_mod_list_t::iterator i = cl_mod_list.begin(), e = cl_mod_list.end(); i != e; ++i) {
         // display any error messages
         SimpleRefHolder<QoreStringNode> err(MM.parseLoadModule((*i).c_str(), *qpgm));
         if (err) {
            printf("cannot load '%s': %s\n", (*i).c_str(), err->getBuffer());
            mod_errs = true;
         }
      }

      cl_mod_list.clear();
      if (mod_errs) {
         printf("please fix the errors listed above and try again.\n");
         rc = 2;
         goto exit;
      }

      // set time zone if requested
      if (cmd_zone)
         qpgm->parseSetTimeZone(cmd_zone);

      // lock the parse options if necessary
      if (lock_options)
         qpgm->lockOptions();

      // parse immediate argument if any
      if (eval_arg) {
         QoreString str("printf(\"%N\\n\", (");
         str.concat(eval_arg);
         str.concat("));");
         qpgm->parse(str.getBuffer(), "<command-line>", &xsink, &wsink, warnings);
      }
      else  {
         // set for program class execution if "exec_class" is set
         if (exec_class) {
            if (exec_class_name)
               qpgm->setExecClass(exec_class_name);
            else if (program_file_name) {
               char* cn = make_class_name(program_file_name);
               qpgm->setExecClass(cn);
               free(cn);
            }
            else {
               fprintf(stderr, "error, missing class name to instantiate as application\n");
               rc = 1;
               goto exit;
            }
         }

         // parse the program
         if (cl_pgm)
            qpgm->parse(cl_pgm, "<command-line>", &xsink, &wsink, warnings);
         else if (program_file_name)
            qpgm->parseFile(program_file_name, &xsink, &wsink, warnings, only_first_except);
         else if (interactive_mode || (!no_repl && isatty(STDIN_FILENO))) {
            // enter REPL mode
            qpgm->parse(repl_pgm, "<repl>", &xsink, &wsink, warnings);
         }
         else
            qpgm->parse(stdin, "<stdin>", &xsink, &wsink, warnings);
      }

      // display any warnings now
      if (wsink.isException()) {
         wsink.handleWarnings();
         if (warnings_are_errors && !xsink.isException()) {
            printf("exiting due to the above warnings...\n");
            rc = 2; // set return code to 2 if there were parse warnings to be treated as errors
            goto exit;
         }
      }

      // if there were no parse exceptions, execute the program
      if (!xsink.isException()) {
         {
            // execute the program and get the return value
            QoreValue rv = qpgm->run(&xsink);
            // set the return code for this program from the core returned by the Qore program
            rc = rv.getAsBigInt();
            rv.discard(&xsink);
         }

         // if there is any unhandled exception, set the return code to 3
         if (xsink.isException())
            rc = 3;
      }
      else // set return code to 2 if there were parse errors
         rc = 2;

      // run the default exception handler on any unhandled exceptions in the primary thread or during parsing
      xsink.handleExceptions();

exit:
      ;
   }
   // run the default exception handler on any unhandled exceptions if necessary (again)
   // -- exceptions could have been thrown in the QoreProgram object's destructor
   xsink.handleExceptions();

   // cleanup Qore subsystem (deallocate memory, etc)
   qore_cleanup();

   return rc;
}
