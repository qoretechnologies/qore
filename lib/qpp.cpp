/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qpp.cpp

    Qore Pre-Processor

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

// cannot include Qore.h with the Oracle Sun Studio compiler
// when not linking with libqore as link errors result
//#include <qore/intern/config.h>
#include "../include/qore/common.h"
#include "../include/qore/intern/config.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libgen.h>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#include <vector>
#include <unistd.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "qore/intern/getopt_long.h"
#endif

const char usage_str[] = "usage: %s [options] <input file(s)...>\n"
    " -d, --dox-output=arg   doxygen output file name\n"
    " -D, --define=arg       define a preprocessor macro (can be used multiple times)\n"
    " -h, --help             this help text\n"
    " -j, --javadoc=arg      javadoc output directory name\n"
    " -o, --output=arg       cpp output file name\n"
    " -t, --table=arg        process the given file for doxygen tables (|!...)\n"
    " -u, --unit=arg         qtest (QUnit) output file name\n"
    " -v, --verbose          increases verbosity level\n";

static const option pgm_opts[] = {
    {"define", required_argument, nullptr, 'D'},
    {"dox-output", required_argument, nullptr, 'd'},
    {"help", no_argument, nullptr, 'h'},
    {"javadoc", required_argument, nullptr, 'j'},
    {"output", required_argument, nullptr, 'o'},
    {"table", required_argument, nullptr, 't'},
    {"unit", required_argument, nullptr, 'u'},
    {"verbose", optional_argument, nullptr, 'v'},
    {nullptr, 0, nullptr, 0}
};

enum LogLevel {
    LL_CRITICAL = 0,
    LL_IMPORTANT = 1,
    LL_INFO = 2,
    LL_DETAIL = 3,
    LL_DEBUG = 4
};

static struct qpp_opts {
    std::string output_fn;
    std::string dox_fn;
    std::string javadoc_fn;
    std::string unit_test_fn;
    std::string table_fn;
    int verbose;

    qpp_opts() : verbose(LL_INFO) {
    }
} opts;

// program name
std::string pn;

// extra c++ initialization string
std::string initcode;

// code attribute type
typedef unsigned int attr_t;

typedef std::vector<std::string> strlist_t;
typedef std::map<std::string, std::string> strmap_t;
typedef std::map<std::string, attr_t> amap_t;
typedef std::set<std::string> strset_t;

// for qppval values
strmap_t qppval;

// code attribute map
static amap_t amap;

// qore type to qore c++ typeinfo map
static strmap_t tmap;

// qore type to mangled qore type code
static strmap_t mtmap;

// qore value to c++ value map
static strmap_t valmap;
// qore value to c++ QoreSinpleValue map
static strmap_t simple_valmap;

// set of types indicating objects
static strset_t oset;

// map of functional domains to parse restriction codes
static strmap_t dmap;

// map of negative functional domains to parse restriction codes
static strmap_t dnmap;

// enum name to base type map
static strmap_t enum_base_type_map;

// set of possible code flags
static strset_t fset;

// predefined preprocessor macros from command line (-D)
static strset_t cmdline_defines;

// parameter structure
struct Param {
    std::string type,           // param type
        name,                   // param name
        val,                    // param default value
        qore;                   // param Qore class name (for objects)

    bool is_static;

    Param(const std::string& t, const std::string& n, const std::string& v, const std::string& q, bool is_static = false)
            : type(t), name(n), val(v), qore(q), is_static(is_static) {
    }
};

// parameter list
typedef std::vector<Param> paramlist_t;

// code attribute bitfield defines
#define QCA_NONE             0
#define QCA_PUBLIC           (1 << 0)
#define QCA_PRIVATE          (1 << 1)
#define QCA_PRIVATE_INTERNAL (1 << 2)
#define QCA_STATIC           (1 << 3)
#define QCA_SYNCHRONIZED     (1 << 4)
#define QCA_USES_EXTRA_ARGS  (1 << 5)
#define QCA_ABSTRACT         (1 << 6)

#define BUFSIZE 1024

static const char* get_access(int mods) {
    if (mods & QCA_PRIVATE) return "Private";
    if (mods & QCA_PRIVATE_INTERNAL) return "Internal";
    return "Public";
}

/*
static char* strchrs(const char* str, const char* chars) {
    while (*str) {
        if (strchr(chars, *str))
            return (char* )str;
        str++;
    }
    return nullptr;
}
*/

static void replace(std::string& str, const char* orig, const char* newstr) {
    size_t index = 0;
    int len = -1;
    int newlen = -1;
    while (true) {
        // Locate the substring to replace
        index = str.find(orig, index);
        if (index == std::string::npos) {
            break;
        }

        // Make the replacement
        if (len == -1) {
            len = strlen(orig);
            newlen = strlen(newstr);
        }
        str.replace(index, len, newstr);

        // Advance index forward so the next iteration doesn't pick it up as well
        index += newlen;
    }
}

static bool idchar(const char c) {
    return isalnum(c) || c == '_';
}

static bool idnschar(const char c) {
    return isalnum(c) || c == '_' || c == ':';
}

static int my_vsprintf(std::string& buf, const char* fmt, va_list args) {
    if (buf.size() < BUFSIZE)
        buf.resize(BUFSIZE);

    // copy formatted string to priv->buffer
    int i =::vsnprintf((char*) buf.c_str(), buf.size(), fmt, args);

#ifdef HPUX
    // vsnprintf failed but didn't tell us how big the priv->buffer should be
    if (i < 0) {
        buf.resize(buf.size() << 1);
        return -1;
    }
#endif
    bool err = i >= (int)buf.size();
    buf.resize(i + 1);
    return err ? -1 : 0;
}

static void error(const char* fmt, ...) {
    std::string buf;
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = my_vsprintf(buf, fmt, args);
        va_end(args);
        if (!rc)
            break;
    }

    fprintf(stderr, "%s: ERROR: ", pn.c_str());
    fputs(buf.c_str(), stderr);
    //fflush(stderr);
}

static void warning(const char* fmt, ...) {
    std::string buf;
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = my_vsprintf(buf, fmt, args);
        va_end(args);
        if (!rc)
            break;
    }

    fprintf(stderr, "%s: WARNING: ", pn.c_str());
    fputs(buf.c_str(), stderr);
    //fflush(stderr);
}

static void log(LogLevel ll, const char* fmt, ...) {
    if (ll > opts.verbose)
        return;

    std::string buf;
    va_list args;

    while (true) {
        va_start(args, fmt);
        int rc = my_vsprintf(buf, fmt, args);
        va_end(args);
        if (!rc)
            break;
    }

    fprintf(stderr, "%s: ", pn.c_str());
    if (ll == LL_DEBUG)
        fputs("DEBUG: ", stdout);
    fputs(buf.c_str(), stdout);
    fflush(stdout);
}

void toupper(std::string& str) {
    for (unsigned i = 0; i < str.size(); ++i)
        str[i] = toupper(str[i]);
}

static bool whitespace(int c) {
    return c == ' ' || c == '\n' || c == '\t';
}

static void trim_end(std::string& str) {
    while (true) {
        size_t len = str.size();
        if (!len || !whitespace(str[len - 1]))
            break;
        str.erase(len - 1);
    }
}

static void trim(std::string& str) {
    while (whitespace(str[0]))
        str.erase(0, 1);
    trim_end(str);
}

// Preprocessor conditional state for #ifdef/#ifndef/#if/#elif/#else/#endif support
struct PreprocessorCondition {
    std::string directive;      // "ifdef", "ifndef", "if", "elif", "else"
    std::string expression;     // The condition expression
    bool was_true;              // Was any branch in this group true?
    bool currently_active;      // Is this branch currently active?
    bool pass_through;          // Unknown macro - pass through to C++ without skipping
    unsigned line;              // Line number for error reporting

    PreprocessorCondition(const std::string& dir, const std::string& expr, bool active, bool passthru, unsigned ln)
        : directive(dir), expression(expr), was_true(active), currently_active(active),
          pass_through(passthru), line(ln) {
    }
};

class PreprocessorState {
    std::set<std::string> defines;              // Currently defined macros
    std::vector<PreprocessorCondition> stack;   // Condition nesting stack
    const char* fileName;

    // Parse "defined(MACRO)" or "defined MACRO" and return the macro name
    static bool parseDefinedExpr(const std::string& expr, size_t& pos, std::string& macro) {
        size_t start = pos;
        // Skip whitespace
        while (pos < expr.size() && whitespace(expr[pos])) ++pos;

        // Check for "defined"
        if (expr.compare(pos, 7, "defined") != 0) {
            pos = start;
            return false;
        }
        pos += 7;

        // Skip whitespace
        while (pos < expr.size() && whitespace(expr[pos])) ++pos;

        bool has_paren = false;
        if (pos < expr.size() && expr[pos] == '(') {
            has_paren = true;
            ++pos;
            while (pos < expr.size() && whitespace(expr[pos])) ++pos;
        }

        // Get macro name
        size_t name_start = pos;
        while (pos < expr.size() && idchar(expr[pos])) ++pos;
        if (pos == name_start) {
            pos = start;
            return false;
        }
        macro = expr.substr(name_start, pos - name_start);

        if (has_paren) {
            while (pos < expr.size() && whitespace(expr[pos])) ++pos;
            if (pos >= expr.size() || expr[pos] != ')') {
                pos = start;
                return false;
            }
            ++pos;
        }
        return true;
    }

    // Evaluate a primary expression (defined(), macro name, integer, or parenthesized expr)
    bool evalPrimary(const std::string& expr, size_t& pos) const {
        // Skip whitespace
        while (pos < expr.size() && whitespace(expr[pos])) ++pos;

        if (pos >= expr.size()) return false;

        // Handle negation
        if (expr[pos] == '!') {
            ++pos;
            return !evalPrimary(expr, pos);
        }

        // Handle parentheses
        if (expr[pos] == '(') {
            ++pos;
            bool result = evalOr(expr, pos);
            while (pos < expr.size() && whitespace(expr[pos])) ++pos;
            if (pos < expr.size() && expr[pos] == ')') ++pos;
            return result;
        }

        // Handle "defined(MACRO)" or "defined MACRO"
        std::string macro;
        if (parseDefinedExpr(expr, pos, macro)) {
            return isDefined(macro);
        }

        // Handle integer literal
        if (isdigit(expr[pos])) {
            size_t num_start = pos;
            while (pos < expr.size() && isdigit(expr[pos])) ++pos;
            std::string num_str = expr.substr(num_start, pos - num_start);
            return std::stoi(num_str) != 0;
        }

        // Handle bare macro name (treated as defined check)
        if (idchar(expr[pos])) {
            size_t name_start = pos;
            while (pos < expr.size() && idchar(expr[pos])) ++pos;
            macro = expr.substr(name_start, pos - name_start);
            return isDefined(macro);
        }

        return false;
    }

    // Evaluate AND expressions (left-to-right with short-circuit)
    bool evalAnd(const std::string& expr, size_t& pos) const {
        bool result = evalPrimary(expr, pos);
        while (true) {
            while (pos < expr.size() && whitespace(expr[pos])) ++pos;
            if (pos + 1 < expr.size() && expr[pos] == '&' && expr[pos + 1] == '&') {
                pos += 2;
                result = result && evalPrimary(expr, pos);
            } else {
                break;
            }
        }
        return result;
    }

    // Evaluate OR expressions (left-to-right with short-circuit)
    bool evalOr(const std::string& expr, size_t& pos) const {
        bool result = evalAnd(expr, pos);
        while (true) {
            while (pos < expr.size() && whitespace(expr[pos])) ++pos;
            if (pos + 1 < expr.size() && expr[pos] == '|' && expr[pos + 1] == '|') {
                pos += 2;
                result = result || evalAnd(expr, pos);
            } else {
                break;
            }
        }
        return result;
    }

public:
    PreprocessorState() : fileName(nullptr) {
    }

    void setFileName(const char* fn) {
        fileName = fn;
    }

    void initFromCommandLine() {
        defines = cmdline_defines;
    }

    // Check if we should skip current content
    // Returns false for pass-through blocks (unknown macros)
    bool isSkipping() const {
        for (const auto& cond : stack) {
            if (cond.pass_through) continue;  // Unknown macro - don't skip, process normally
            if (!cond.currently_active) return true;
        }
        return false;
    }

    // Define a macro
    void define(const std::string& name) {
        defines.insert(name);
    }

    // Undefine a macro
    void undef(const std::string& name) {
        defines.erase(name);
    }

    // Check if a macro is defined
    bool isDefined(const std::string& name) const {
        return defines.find(name) != defines.end();
    }

    // Evaluate a condition expression
    bool evaluateCondition(const std::string& expr) const {
        size_t pos = 0;
        return evalOr(expr, pos);
    }

    // Handle #ifdef
    // If the macro is unknown (not passed via -D), push a pass_through entry so #endif matching works
    // Returns true if we should track this condition, false if unknown (pass-through)
    bool pushIfdef(const std::string& name, unsigned line) {
        bool known = isDefined(name);  // Only consider macros from -D as "known"
        if (!known) {
            // Unknown macro - push pass_through entry so #endif can match, but don't skip content
            stack.emplace_back("ifdef", name, true, true, line);
            return false;
        }
        bool active = !isSkipping() && isDefined(name);
        stack.emplace_back("ifdef", name, active, false, line);
        return true;
    }

    // Handle #ifndef
    // If the macro is unknown (not passed via -D), push a pass_through entry so #endif matching works
    // Returns true if we should track this condition, false if unknown (pass-through)
    bool pushIfndef(const std::string& name, unsigned line) {
        bool known = isDefined(name);  // Only consider macros from -D as "known"
        if (!known) {
            // Unknown macro - push pass_through entry so #endif can match, but don't skip content
            stack.emplace_back("ifndef", name, true, true, line);
            return false;
        }
        bool active = !isSkipping() && !isDefined(name);
        stack.emplace_back("ifndef", name, active, false, line);
        return true;
    }

    // Handle #if
    // For complex expressions, we can only evaluate if all macros are known
    // Push a pass_through entry so #endif matching works
    // Returns true if we should track this condition, false if unknown (pass-through)
    bool pushIf(const std::string& expr, unsigned line) {
        // For #if, push pass_through entry - let C++ compiler handle the condition
        stack.emplace_back("if", expr, true, true, line);
        return false;
    }

    // Handle #elif - returns true if tracked, false if pass-through (unknown macro context)
    bool handleElif(const std::string& expr, unsigned line) {
        if (stack.empty()) {
            error("%s:%d: #elif without #if\n", fileName, line);
            return false;
        }
        auto& top = stack.back();
        if (top.pass_through) {
            // Pass-through context - don't track
            return false;
        }
        if (top.directive == "else") {
            error("%s:%d: #elif after #else\n", fileName, line);
            return false;
        }
        // Only evaluate if no previous branch was true and parent is active
        bool parent_active = true;
        for (size_t i = 0; i + 1 < stack.size(); ++i) {
            if (!stack[i].currently_active && !stack[i].pass_through) {
                parent_active = false;
                break;
            }
        }
        if (!top.was_true && parent_active && evaluateCondition(expr)) {
            top.currently_active = true;
            top.was_true = true;
        } else {
            top.currently_active = false;
        }
        top.directive = "elif";
        top.expression = expr;
        return true;
    }

    // Handle #else - returns true if tracked, false if pass-through (unknown macro context)
    bool handleElse(unsigned line) {
        if (stack.empty()) {
            error("%s:%d: #else without #if\n", fileName, line);
            return false;
        }
        auto& top = stack.back();
        if (top.pass_through) {
            // Pass-through context - don't track
            return false;
        }
        if (top.directive == "else") {
            error("%s:%d: duplicate #else\n", fileName, line);
            return false;
        }
        // Only active if no previous branch was true and parent is active
        bool parent_active = true;
        for (size_t i = 0; i + 1 < stack.size(); ++i) {
            if (!stack[i].currently_active && !stack[i].pass_through) {
                parent_active = false;
                break;
            }
        }
        top.currently_active = !top.was_true && parent_active;
        top.directive = "else";
        return true;
    }

    // Handle #endif - returns true if tracked, false if pass-through (unknown macro context)
    bool handleEndif(unsigned line) {
        if (stack.empty()) {
            error("%s:%d: #endif without #if\n", fileName, line);
            return false;
        }
        bool was_pass_through = stack.back().pass_through;
        stack.pop_back();
        return !was_pass_through;
    }

    // Handle #define
    void handleDefine(const std::string& name) {
        if (!isSkipping()) {
            define(name);
        }
    }

    // Handle #undef
    void handleUndef(const std::string& name) {
        if (!isSkipping()) {
            undef(name);
        }
    }

    // Check for unclosed conditionals (only tracked ones, not pass-through)
    bool hasUnclosedConditions() const {
        for (const auto& cond : stack) {
            if (!cond.pass_through) return true;
        }
        return false;
    }

    // Get info about unclosed conditionals for error reporting (only tracked ones)
    std::string getUnclosedConditionInfo() const {
        std::string result;
        for (const auto& cond : stack) {
            if (cond.pass_through) continue;
            if (!result.empty()) result += ", ";
            result += "#" + cond.directive + " at line " + std::to_string(cond.line);
        }
        return result;
    }

    // Handle a preprocessor directive line - returns true if it was a directive we handle
    // For unknown macros (not defined via -D), returns false so the line passes through normally
    // Only tracks and handles directives for known macros
    bool handleDirective(const std::string& line, unsigned lineNumber, std::string& output) {
        if (line.empty() || line[0] != '#') return false;

        std::string trimmed = line.substr(1);
        // Skip whitespace after #
        size_t pos = 0;
        while (pos < trimmed.size() && whitespace(trimmed[pos])) ++pos;
        trimmed = trimmed.substr(pos);

        std::string directive;
        pos = 0;
        while (pos < trimmed.size() && isalpha(trimmed[pos])) {
            directive += trimmed[pos++];
        }

        // Skip whitespace after directive
        while (pos < trimmed.size() && whitespace(trimmed[pos])) ++pos;

        // Get the rest as the argument
        std::string arg = trimmed.substr(pos);
        trim_end(arg);

        if (directive == "ifdef") {
            if (!pushIfdef(arg, lineNumber)) {
                // Unknown macro - don't track, let it pass through as regular text
                return false;
            }
            output += line;  // Pass through to C++ output
            return true;
        } else if (directive == "ifndef") {
            if (!pushIfndef(arg, lineNumber)) {
                // Unknown macro - don't track, let it pass through as regular text
                return false;
            }
            output += line;
            return true;
        } else if (directive == "if") {
            if (!pushIf(arg, lineNumber)) {
                // Complex expression - don't track, let it pass through
                return false;
            }
            output += line;
            return true;
        } else if (directive == "elif") {
            if (!handleElif(arg, lineNumber)) {
                // Not tracking (pass-through context) - let it pass through
                return false;
            }
            output += line;
            return true;
        } else if (directive == "else") {
            if (!handleElse(lineNumber)) {
                // Not tracking (pass-through context) - let it pass through
                return false;
            }
            output += line;
            return true;
        } else if (directive == "endif") {
            if (!handleEndif(lineNumber)) {
                // Not tracking (pass-through context) - let it pass through
                return false;
            }
            output += line;
            return true;
        } else if (directive == "define") {
            // Only handle simple defines without values
            std::string name;
            size_t i = 0;
            while (i < arg.size() && idchar(arg[i])) {
                name += arg[i++];
            }
            if (!name.empty()) {
                handleDefine(name);
            }
            output += line;  // Pass through
            return true;
        } else if (directive == "undef") {
            std::string name;
            size_t i = 0;
            while (i < arg.size() && idchar(arg[i])) {
                name += arg[i++];
            }
            if (!name.empty()) {
                handleUndef(name);
            }
            output += line;
            return true;
        }

        // Not a directive we handle - let it pass through unchanged
        return false;
    }
};

static int read_line(unsigned &lineNumber, std::string& str, FILE* fp) {
    while (true) {
        int c = fgetc(fp);
        if (c == EOF)
            return -1;

        str += c;
        if (c == '\n') {
            ++lineNumber;
            break;
        }
    }
    return 0;
}

static int get_dox_comment(const char* fileName, unsigned &lineNumber, std::string& str, FILE* fp,
        bool already_started = false) {
    if (!already_started) {
        std::string buf;
        if (read_line(lineNumber, buf, fp)) {
            error("%s:%d: premature EOF reading doxygen comment\n", fileName, lineNumber);
            return -1;
        }

        if (buf.compare(0, 3, "/**")) {
            error
                ("%s:%d: missing block comment marker '/**' in line following //! comment (str='%s' buf='%s')\n",
                 fileName, lineNumber, str.c_str(), buf.c_str());
            return -1;
        }

        str += buf;
    }

    int lc = 0;

    while (true) {
        int c = fgetc(fp);
        if (c == EOF) {
            error("%s:%d: premature EOF reading doxygen comment\n", fileName, lineNumber);
            return -1;
        }

        str += c;

        if (lc && c == '/')
            break;

        lc = (c == '*');

        if (c == '\n')
            ++lineNumber;
    }

    // now read to EOL
    read_line(lineNumber, str, fp);

    return 0;
}

static void strip_leading_spaces(std::string& str) {
    size_t i = str.find_first_not_of(' ');
    if (i > 0) {
        str.erase(0, i);
    }
}

static int get_string_list(strlist_t& l, const std::string& str, char separator = ',',
        bool respect_angle_brackets = false) {
    size_t start = 0;
    while (true) {
        size_t sep = str.find(separator, start);
        if (sep == std::string::npos) {
            break;
        }
        std::string element(str, start, sep - start);
        // remove leading whitespace from strings
        strip_leading_spaces(element);

        if (respect_angle_brackets) {
            // angle bracket count
            unsigned ac = 0;

            std::string::difference_type open = std::count(element.begin(), element.end(), '<');
            std::string::difference_type close = std::count(element.begin(), element.end(), '>');

            if (close > open) {
                error("unbalanced angle brackets in '%s'\n", element.c_str());
                return -1;
            }
            ac += open - close;
            if (ac) {
                element += ',';
                while (true) {
                    if (++sep == str.size()) {
                        // Reached end of string - check if brackets are balanced
                        if (ac) {
                            error("unbalanced angle brackets in '%s'\n", element.c_str());
                            return -1;
                        }
                        // Brackets are balanced, this is the last element
                        // The element includes the comma and everything after it as part of the type
                        l.push_back(element);
                        return 0;
                    }
                    char c = str[sep];
                    element += c;
                    if (c == '<') {
                        ++ac;
                    } else if (c == '>') {
                        if (!ac) {
                            error("unbalanced angle brackets in '%s'\n", element.c_str());
                            return -1;
                        }
                        --ac;
                    } else if (c == ',' && ac == 0) {
                        element.pop_back();
                        break;
                    }
                }
            }
        }

        l.push_back(element);
        start = sep + 1;
    }

    std::string element(str, start);
    // remove leading whitespace from strings
    strip_leading_spaces(element);
    l.push_back(element);
    //for (unsigned i = 0; i < l.size(); ++i)
    //   printf("DBG: list %u/%lu: %s\n", i, l.size(), l[i].c_str());
    return 0;
}

static void get_string_list(strlist_t& l, const std::string& str, const char* separator) {
    size_t start = 0;
    size_t len = strlen(separator);
    while (true) {
        size_t sep = str.find(separator, start);
        if (sep == std::string::npos) {
            break;
        }
        std::string element(str, start, sep - start);
        // remove leading whitespace from strings
        strip_leading_spaces(element);
        l.push_back(element);
        start = sep + len;
    }

    std::string element(str, start);
    // remove leading whitespace from strings
    strip_leading_spaces(element);
    l.push_back(element);
    //for (unsigned i = 0; i < l.size(); ++i)
    //   printf("DBG: list %u/%lu: %s\n", i, l.size(), l[i].c_str());
}

std::string get_java_doc(const std::string& docs, int offset = 0) {
    //printf("BEFORE: '%s'\n", docs.c_str());
    std::string rv = docs;
    if (rv[0] == '/' && rv[1] == '/' && rv[2] == '!') {
        size_t t = rv.find("/**", 3);
        if (t == std::string::npos) {
            rv += "*/";
        } else {
            rv.erase(t, 3);
            if (!offset) {
                rv.insert(t, "   ");
            }
        }
        rv[1] = '*';
        rv[2] = '*';
        rv.insert(3, " @brief");
        if (offset) {
            rv.insert(0, "    ");
            replace(rv, "\n", "\n    ");
        }
    }
    //printf("AFTER: '%s'\n", rv.c_str());
    trim_end(rv);
    rv += "\n";
    return rv;
}

static void output_file(FILE* fp, const std::string& text) {
    unsigned e = text.size();
    while (e && whitespace(text[e - 1]))
        --e;

    unsigned i = 0;
    while (i < e && text[i] == '\n')
        ++i;

    unsigned nc = 0;
    for (; i != e; ++i) {
        char c = text[i];
        if (c == '\n') {
            ++nc;
            if (nc > 2)
                continue;
        } else
            nc = 0;
        fputc(c, fp);
    }

    if (e && e != text.size())
        fputc('\n', fp);
}

int parse_attributes(const char* fileName, unsigned &lineNumber, attr_t& attr, std::string& return_type, std::string& sc, size_t i) {
    strlist_t pre;
    // get return type and flags
    std::string pstr(sc, 0, i - 1);
    //log(LL_DEBUG, "PSTR='%s'\n", pstr.c_str());
    get_string_list(pre, pstr, ' ');

    for (unsigned xi = 0; xi < pre.size(); ++xi) {
        //printf("DBG: METHOD list %u/%lu: %s\n", xi, pre.size(), pre[xi].c_str());
        amap_t::iterator ai = amap.find(pre[xi]);
        if (ai == amap.end()) {
            if (!return_type.empty()) {
                size_t nopen = std::count(return_type.begin(), return_type.end(), '<');
                size_t nclose = std::count(return_type.begin(), return_type.end(), '>');
                log(LL_DEBUG, "'%s' ('%s') nopen: %d nclose: %d\n", return_type.c_str(), pre[xi].c_str(), (int)nopen, (int)nclose);
                if (nopen > nclose) {
                    return_type += pre[xi];
                    continue;
                }

                error("%s:%d: multiple return types or unknown code attribute '%s' given\n", fileName, lineNumber, pre[xi].c_str());
                return -1;
            }
            return_type = pre[xi];
        }
        else
            attr |= ai->second;
    }

    int pcnt = 0;
    if (attr & QCA_PUBLIC) ++pcnt;
    if (attr & QCA_PRIVATE) ++pcnt;
    if (attr & QCA_PRIVATE_INTERNAL) ++pcnt;

    if (pcnt > 1) {
        error("%s:%d: declared with contradictory access specifiers\n", fileName, lineNumber);
        return -1;
    }

    return 0;
}

static int missing_value_error(const char* fileName, unsigned lineNumber, const std::string& propstr) {
    error("%s:%d: missing '=' or value in property string '%s'\n", fileName, lineNumber, propstr.c_str());
    return -1;
}

static int add_element(const char* fileName, unsigned lineNumber, const std::string& propstr, strmap_t& props,
        size_t start, size_t end) {
    size_t eq = propstr.find('=', start);
    if (eq == std::string::npos || eq >= end) {
        return 0;
    }
    while (start < eq && (propstr[start] == ' ' || propstr[start] == '\n')) {
        if (propstr[start] == '\n') {
            ++lineNumber;
        }
        ++start;
    }
    if (start == eq) {
        error("%s:%d: missing property name in property string '%s'\n", fileName, lineNumber, propstr.c_str());
        return -1;
    }
    size_t tend = end;
    while (tend > eq && (propstr[tend] == ' ' || propstr[tend] == '\n')) {
        if (propstr[tend] == '\n') {
            ++lineNumber;
        }
        --tend;
    }

    if (tend == eq)
        return missing_value_error(fileName, lineNumber, propstr);

    std::string key(propstr, start, eq - start);
    if (props.find(key) != props.end()) {
        error("%s:%d property '%s' set twice in property string '%s'\n",
              fileName, lineNumber, key.c_str(), propstr.c_str());
        return -1;
    }
    std::string value(propstr, eq + 1, end - eq - 1);

    //value.erase(std::remove(value.begin(), value.end(), '\n'), value.cend());
    std::string::size_type i = 0;
    while (i < value.length()) {
        i = value.find("  ", i);
        if (i == std::string::npos) {
            break;
        }
        value.erase(i, 1);
    }
    i = 0;
    while (i < value.length()) {
        i = value.find('\n', i);
        if (i == std::string::npos) {
            break;
        }
        ++lineNumber;
        value.erase(i, 1);
    }

    props[key] = value;
    return 0;
}

int parse_properties(const char* fileName, unsigned lineNumber, std::string& propstr, strmap_t& props) {
    size_t start = 0;
    while (true) {
        size_t end = propstr.find(';', start);
        if (end == std::string::npos)
            break;
        if (add_element(fileName, lineNumber, propstr, props, start, end))
            return -1;
        start = end + 1;
    }

    return add_element(fileName, lineNumber, propstr, props, start, propstr.size());
}

int parse_params_and_flags(const char* fileName, unsigned &lineNumber, strmap_t& flags, paramlist_t& params,
        attr_t& attr, std::string& sc, size_t p, const std::string& dn, bool abstract = false) {
    // Find the closing ')' that matches the opening '(' at position p-1
    // Must handle nested parens and angle brackets in complex types like code<int(string, int)>
    size_t i = std::string::npos;
    int paren_depth = 1;  // We're already inside the opening paren
    int angle_depth = 0;
    for (size_t j = p; j < sc.size(); ++j) {
        char c = sc[j];
        if (c == '<') {
            ++angle_depth;
        } else if (c == '>') {
            --angle_depth;
            if (angle_depth < 0) {
                error("%s:%d: unmatched '>' in parameter list for %s()\n", fileName, lineNumber, dn.c_str());
                return -1;
            }
        } else if (c == '(' && angle_depth == 0) {
            ++paren_depth;
        } else if (c == ')' && angle_depth == 0) {
            --paren_depth;
            if (paren_depth == 0) {
                i = j;
                break;
            }
        }
    }
    if (i == std::string::npos) {
        error("%s:%d: premature EOL reading parameters for %s()\n", fileName, lineNumber, dn.c_str());
        return -1;
    }

    size_t cs;
    if (abstract)
        cs = sc.size() - 1;
    else {
        cs = sc.find('{', i + 1);
        if (cs == std::string::npos) {
            error("%s:%d: premature EOL looking for open brace for %s()\n", fileName, lineNumber, dn.c_str());
            return -1;
        }
    }

    // get flags if any
    if (cs - i > 2) {
        std::string fstr(sc, i + 1, cs - i - 2);
        size_t f = fstr.find('[');
        if (f != std::string::npos) {
            fstr.erase(0, f + 1);
            size_t fe = fstr.find(']');
            if (fe == std::string::npos) {
                error("%s:%d: premature EOL looking for ']' in flags for %s()\n", fileName, lineNumber, dn.c_str());
                return -1;
            }
            fstr.erase(fe);

            log(LL_DEBUG, "fstr='%s'\n", fstr.c_str());

            // parse properties
            if (parse_properties(fileName, lineNumber, fstr, flags))
                return -1;
        }
    }
    // get params
    if (i != p + 1) {
        std::string pstr(sc, p, i - p);
        trim(pstr);
        if (!pstr.empty()) {
            strlist_t pl;
            if (get_string_list(pl, pstr, ',', true)) {
                error("%s:%d: %s(): error parsing parameter list '%s'\n", fileName, lineNumber, dn.c_str(),
                    pstr.c_str());
                return -1;
            }

            for (unsigned xi = 0; xi < pl.size(); ++xi) {
                trim(pl[xi]);

                // handle special case arg "..." - which is just for documentation
                if (pl[xi] == "...") {
                    params.push_back(Param(pl[xi], pl[xi], "", ""));
                    attr |= QCA_USES_EXTRA_ARGS;
                    continue;
                }

                bool is_static = false;
                if (pl[xi].rfind("static ", 0) == 0) {
                    is_static = true;
                    pl[xi].erase(0, 7);
                }

                // Find space between type and parameter name, respecting nested brackets/parens
                // Types like code<int(hash<string, int>)> have spaces inside them
                i = std::string::npos;
                int angle_depth = 0;
                int paren_depth = 0;
                for (size_t j = 0; j < pl[xi].size(); ++j) {
                    char c = pl[xi][j];
                    if (c == '<') {
                        ++angle_depth;
                    } else if (c == '>') {
                        --angle_depth;
                        if (angle_depth < 0) {
                            angle_depth = 0;
                        }
                    } else if (c == '(') {
                        ++paren_depth;
                    } else if (c == ')') {
                        --paren_depth;
                        if (paren_depth < 0) {
                            paren_depth = 0;
                        }
                    } else if (c == ' ' && angle_depth == 0 && paren_depth == 0) {
                        i = j;
                        break;
                    }
                }
                if (i == std::string::npos) {
                    error("%s:%d: %s(): cannot find type for parameter '%s'\n", fileName, lineNumber, dn.c_str(),
                        pl[xi].c_str());
                    return -1;
                }
                std::string type(pl[xi], 0, i);
                std::string param(pl[xi], i + 1);
                trim(type);
                trim(param);

                std::string val;
                // see if there is a default value
                i = param.find('=');
                if (i != std::string::npos) {
                    val.assign(param, i + 1, std::string::npos);
                    param.erase(i);
                    trim(val);
                    trim(param);
                }

                std::string qore;

                // see if there's a Qore class name
                if (type[type.size() - 1] == ']') {
                    i = type.find('[');
                    if (i == std::string::npos) {
                        error("%s:%d: %s(): cannot find open square bracket '[' in type name '%s' in parameter "
                            "'%s'\n", fileName, lineNumber, dn.c_str(), type.c_str(), param.c_str());
                        return -1;
                    }
                    type.erase(type.size() - 1);
                    qore = type.c_str() + i + 1;
                    type.erase(i);
                }

                log(LL_DEBUG, "+ %s() param %d type '%s' name '%s' default value '%s'\n", dn.c_str(), xi,
                    type.c_str(), param.c_str(), val.c_str());
                params.push_back(Param(type, param, val, qore, is_static));
            }
        }
    }
    // erase function/method header
    sc.erase(0, cs + 1);
    bool eol = false;
    while (sc[0] == ' ' || sc[0] == '\n') {
        if (!eol && sc[0] == '\n')
            eol = true;
        else if (eol && sc[0] == ' ')
            break;
        sc.erase(0, 1);
    }

    if (!abstract) {
        // remove trailing '}' and newlines
        size_t len = sc.size();
        assert(sc[len - 1] == '}');
        do {
            sc.erase(--len);
        } while (len && (sc[len - 1] == ' ' || sc[len - 1] == '\n'));
    }

    return 0;
}

static void get_type_name(std::string& t, const std::string& type) {
    size_t cp = type.rfind("::");
    if (cp == std::string::npos)
        t = type;
    else
        t.assign(type, cp + 2, -1);
    if (t[0] == '*' || t[0] == '!')
        t.erase(0, 1);
}

static std::string get_java_type_name(const std::string& type, const std::string& ns) {
    std::string t = type;
    if (t[0] == '*' || t[0] == '!') {
        t.erase(0, 1);
    }
    if (t.find("::") == std::string::npos) {
        t.insert(0, "::");
        t.insert(0, ns);
    }
    replace(t, "::", ".");
    t.insert(0, "qore.");
    return t;
}

static std::string get_java_type_name(const std::string& type) {
    return get_java_type_name(type, std::string("Qore"));
}

// takes into account quotes and @code @endcode
static void get_string_list2(strlist_t& l, const std::string& str, char separator = ',') {
    size_t start = 0, p = 0;
    int quote = 0;
    bool code = false;

    while (p < str.size()) {
        int c = str[p++];

        if (c == '@') {
            if (code) {
                if (!str.compare(p, 7, "endcode")) {
                    code = false;
                    p += 7;
                }
                continue;
            }
            if (!str.compare(p, 4, "code")) {
                code = true;
                p += 4;
            }
            continue;
        }
        if (code)
            continue;

        if (c == '\\') {
            ++p;
            continue;
        }

        if (c == '\'' || c == '\"') {
            if (quote == c)
                quote = 0;
            else if (!quote)
                quote = c;
            continue;
        }
        if (quote)
            continue;

        if (c == separator) {
            l.push_back(std::string(str, start, p - start - 1));
            start = p;
        }
    }
    l.push_back(std::string(str, start));

    //for (unsigned i = 0; i < l.size(); ++i)
    //   printf("DBG: list %u/%lu: %s\n", i, l.size(), l[i].c_str());
}

// Parse comma-separated subtypes while respecting nested angle brackets and parentheses
static void parseSubtypes(const std::string& str, std::vector<std::string>& subtypes) {
    int bracket_depth = 0;
    int paren_depth = 0;
    std::string current;

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == '<') {
            ++bracket_depth;
            current += c;
        } else if (c == '>') {
            --bracket_depth;
            current += c;
        } else if (c == '(') {
            ++paren_depth;
            current += c;
        } else if (c == ')') {
            --paren_depth;
            current += c;
        } else if (c == ',' && bracket_depth == 0 && paren_depth == 0) {
            trim(current);
            if (!current.empty()) {
                subtypes.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    trim(current);
    if (!current.empty()) {
        subtypes.push_back(current);
    }
}

static int get_qore_type(const std::string& qt, std::string& cppt) {
    if (qt.empty()) {
        cppt = "nothingTypeInfo";
        return 0;
    }

    strmap_t::iterator i = tmap.find(qt);
    if (i == tmap.end()) {
        std::string qc;
        bool on = qt[0] == '*';

        // see if it's a complex type
        if (!qt.compare(on ? 1 : 0, 5, "hash<")) {
            // extract subtype name
            std::string subtype = qt.substr((on ? 6 : 5), qt.size() - (on ? 7 : 6));

            // cannot support complex hash types other than hashdecls yet
            if (subtype.find(',') != std::string::npos) {
                if (!subtype.compare(0, 7, "string,")) {
                    subtype.erase(0, 7);
                    trim(subtype);
                    std::string subtype_qt;
                    get_qore_type(subtype, subtype_qt);

                    // generate type name
                    if (on)
                        qc = "qore_get_complex_hash_or_nothing_type(" + subtype_qt + ")";
                    else
                        qc = "qore_get_complex_hash_type(" + subtype_qt + ")";
                    log(LL_DEBUG, "registering complex hash return type '%s': '%s'\n", qt.c_str(), qc.c_str());
                } else {
                    log(LL_CRITICAL, "unsupported complex hash type found: '%s'\n", qt.c_str());
                    assert(false);
                }
            } else {
                if (subtype == "auto")
                    qc = on ? "autoHashOrNothingTypeInfo" : "autoHashTypeInfo";
                else {
                    size_t i = subtype.rfind(':');
                    if (i != std::string::npos)
                        subtype.erase(0, i + 1);
                    // generate type name
                    qc = "hashdecl" + subtype + "->getTypeInfo(" + (on ? "true" : "false") + ")";
                    log(LL_DEBUG, "registering hashdecl return type '%s': '%s'\n", qt.c_str(), qc.c_str());
                }
            }
        } else if (!qt.compare(on ? 1 : 0, 5, "list<")) {
            // extract subtype name - supports nested complex types like list<hash<string, int>>
            std::string subtype = qt.substr((on ? 6 : 5), qt.size() - (on ? 7 : 6));

            std::string subtype_qt;
            get_qore_type(subtype, subtype_qt);

            // generate type name
            if (subtype == "auto")
                qc = on ? "autoListOrNothingTypeInfo" : "autoListTypeInfo";
            else {
                if (on)
                    qc = "qore_get_complex_list_or_nothing_type(" + subtype_qt + ")";
                else
                    qc = "qore_get_complex_list_type(" + subtype_qt + ")";
            }
            log(LL_DEBUG, "registering complex list return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 9, "softlist<")) {
            // extract subtype name - supports nested complex types like softlist<hash<string, int>>
            std::string subtype = qt.substr((on ? 10 : 9), qt.size() - (on ? 11 : 10));

            std::string subtype_qt;
            get_qore_type(subtype, subtype_qt);

            // generate type name
            // generate type name
            if (subtype == "auto")
                qc = on ? "softAutoListOrNothingTypeInfo" : "softAutoListTypeInfo";
            else {
                if (on)
                    qc = "qore_get_complex_softlist_or_nothing_type(" + subtype_qt + ")";
                else
                    qc = "qore_get_complex_softlist_type(" + subtype_qt + ")";
            }
            log(LL_DEBUG, "registering complex softlist return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 10, "reference<")) {
            // extract subtype name - supports nested complex types like reference<hash<string, int>>
            std::string subtype = qt.substr((on ? 11 : 10), qt.size() - (on ? 12 : 11));

            std::string subtype_qt;
            get_qore_type(subtype, subtype_qt);

            // generate type name
            if (on)
                qc = "qore_get_complex_reference_or_nothing_type(" + subtype_qt + ")";
            else
                qc = "qore_get_complex_reference_type(" + subtype_qt + ")";
            log(LL_DEBUG, "registering complex reference return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 5, "date<")) {
            // extract subtype name
            std::string subtype = qt.substr((on ? 6 : 5), qt.size() - (on ? 7 : 6));
            if (subtype == "absolute") {
                qc = on ? "dateAbsoluteOrNothingTypeInfo" : "dateAbsoluteTypeInfo";
            } else if (subtype == "relative") {
                qc = on ? "dateRelativeOrNothingTypeInfo" : "dateRelativeTypeInfo";
            } else {
                log(LL_CRITICAL, "unsupported date subtype '%s' in '%s'\n", subtype.c_str(), qt.c_str());
                assert(false);
            }
            log(LL_DEBUG, "registering date subtype return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 6, "union<")) {
            // extract subtypes: union<type1, type2, ...>
            std::string subtypes_str = qt.substr((on ? 7 : 6), qt.size() - (on ? 8 : 7));

            std::vector<std::string> subtypes;
            parseSubtypes(subtypes_str, subtypes);

            if (subtypes.size() < 2) {
                log(LL_CRITICAL, "union type requires at least 2 member types: '%s'\n", qt.c_str());
                assert(false);
            }

            // Generate type_vec_t initialization - build element by element for GCC 15
            std::string type_vec = "[&]() { type_vec_t v; ";
            for (size_t j = 0; j < subtypes.size(); ++j) {
                std::string subtype_qt;
                get_qore_type(subtypes[j], subtype_qt);
                type_vec += "v.push_back(" + subtype_qt + "); ";
            }
            type_vec += "return v; }()";

            if (on) {
                qc = "qore_get_union_or_nothing_type(" + type_vec + ")";
            } else {
                qc = "qore_get_union_type(" + type_vec + ")";
            }
            log(LL_DEBUG, "registering union return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 5, "code<")) {
            // extract signature: code<ReturnType(ParamTypes...)>
            std::string sig = qt.substr((on ? 6 : 5), qt.size() - (on ? 7 : 6));

            // Find opening paren for params (handle nested types in return type)
            int bracket_depth = 0;
            size_t paren_pos = std::string::npos;
            for (size_t j = 0; j < sig.size(); ++j) {
                if (sig[j] == '<') {
                    ++bracket_depth;
                } else if (sig[j] == '>') {
                    --bracket_depth;
                } else if (sig[j] == '(' && bracket_depth == 0) {
                    paren_pos = j;
                    break;
                }
            }

            if (paren_pos == std::string::npos) {
                log(LL_CRITICAL, "invalid code type '%s': missing '('\n", qt.c_str());
                assert(false);
            }

            // Extract return type and param types
            std::string ret_type = sig.substr(0, paren_pos);
            trim(ret_type);
            std::string params_str = sig.substr(paren_pos + 1, sig.size() - paren_pos - 2);
            trim(params_str);

            // Check for varargs
            bool has_varargs = false;
            if (params_str.size() >= 3 && params_str.substr(params_str.size() - 3) == "...") {
                has_varargs = true;
                params_str = params_str.substr(0, params_str.size() - 3);
                trim(params_str);
                if (!params_str.empty() && params_str.back() == ',') {
                    params_str.pop_back();
                    trim(params_str);
                }
            }

            // Get return type
            std::string ret_type_qt;
            if (ret_type.empty() || ret_type == "nothing") {
                ret_type_qt = "nothingTypeInfo";
            } else {
                get_qore_type(ret_type, ret_type_qt);
            }

            // Parse param types - build element by element for GCC 15
            std::string params_vec = "[&]() { type_vec_t v; ";
            if (!params_str.empty()) {
                std::vector<std::string> param_types;
                parseSubtypes(params_str, param_types);
                for (size_t j = 0; j < param_types.size(); ++j) {
                    std::string param_qt;
                    get_qore_type(param_types[j], param_qt);
                    params_vec += "v.push_back(" + param_qt + "); ";
                }
            }
            params_vec += "return v; }()";

            if (on) {
                qc = "qore_get_complex_code_or_nothing_type(" + ret_type_qt + ", " + params_vec +
                     ", " + (has_varargs ? "true" : "false") + ")";
            } else {
                qc = "qore_get_complex_code_type(" + ret_type_qt + ", " + params_vec +
                     ", " + (has_varargs ? "true" : "false") + ")";
            }
            log(LL_DEBUG, "registering code return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else if (!qt.compare(on ? 1 : 0, 5, "enum<")) {
            // extract enum name: enum<EnumName> or enum<Ns::EnumName>
            std::string enum_name = qt.substr((on ? 6 : 5), qt.size() - (on ? 7 : 6));

            // Handle namespaced enums (Ns::EnumName -> EnumName for variable name)
            std::string var_name = enum_name;
            size_t j = var_name.rfind(':');
            if (j != std::string::npos) {
                var_name.erase(0, j + 1);
            }

            qc = "enum" + var_name + "->getTypeInfo(" + (on ? "true" : "false") + ")";
            log(LL_DEBUG, "registering enum return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        } else {
            // assume a Qore object of the given class
            get_type_name(qc, qt);
            toupper(qc);
            qc = "QC_" + qc;
            qc += on ? "->getOrNothingTypeInfo()" : "->getTypeInfo()";
        }
        log(LL_DEBUG, "registering reference return type '%s': '%s'\n", qt.c_str(), qc.c_str());
        oset.insert(qt);
        tmap[qt] = qc;
        cppt = tmap[qt];
    } else
        cppt = i->second;
    return 0;
}

static bool is_object(const std::string& qt) {
    return !qt.empty() && (oset.find(qt) != oset.end()
                           || tmap.find(qt) == tmap.end());
}

// reads until one of the given characters is encountered and returns the character read, returns -1 for EOF
int read_until_chars(const char* fileName, unsigned& lineNumber, std::string& str, FILE* fp, const char* chrs) {
    int c;
    while (true) {
        c = fgetc(fp);
        if (c == EOF) {
            error("%s: premature EOF on line %d\n", fileName, lineNumber);
            return -1;
        }
        if (c == '\n')
            ++lineNumber;
        if (strchr(chrs, c))
            break;
    }
    return c;
}

int read_until_close(const char* fileName, unsigned& lineNumber, std::string& str, FILE* fp, int cchar = '}') {
    int quote = 0;

    // curly bracket count
    unsigned bc = 1;

    bool backquote = false;
    bool line_comment = false;
    // current character
    int c = 0;
    // last character read
    int last;

    while (true) {
        last = c;
        c = fgetc(fp);
        if (c == EOF) {
            error("%s: premature EOF on line %d\n", fileName, lineNumber);
            return -1;
        }

        str += c;

        if (backquote == true) {
            backquote = false;
            continue;
        }

        if (c == '\n') {
            ++lineNumber;
            line_comment = false;
            continue;
        }

        if (line_comment)
            continue;

        if (c == '"' || c == '\'') {
            log(LL_DEBUG, "found %c on line %d (quote: %c %d)\n", c, lineNumber, quote ? quote : 'x', quote);
            if (quote == c)
                quote = 0;
            else if (!quote)
                quote = c;
            continue;
        }

        if (quote) {
            if (c == '\\')
                backquote = true;
            continue;
        }

        if (c == '/' && last == '/') {
            line_comment = true;
            continue;
        }

        if (cchar == '}' && c == '{') {
            log(LL_DEBUG, "found '{' on line %d (bc: %d)\n", lineNumber, bc);
            ++bc;
            continue;
        }

        if (c == cchar) {
            log(LL_DEBUG, "found '%c' on line %d (bc: %d)\n", cchar, lineNumber, bc);
            if (!--bc)
                break;
        }
    }

    return 0;
}

static void add_init_code(FILE* fp) {
    if (initcode.empty())
        return;

    fprintf(fp, "\n    // initialization code added by qppinit\n    %s", initcode.c_str());
    initcode.clear();
}

static int mkdir_chdir(const char* path) {
    // check if target path exists
    struct stat sb;
    if (stat(path, &sb)) {
        //printf("stat('%s'): %s\n", path, strerror(errno));
#if defined _MSC_VER || ((defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__)
        if (mkdir(path)) {
#else
        if (mkdir(path, 0755)) {
#endif
            // ignore EEXIST: with a parallel build this can happen normally
            if (errno != EEXIST) {
                fprintf(stderr, "mkdir '%s': %s\n", path, strerror(errno));
                return -1;
            }
        }
    }

    if (chdir(path)) {
        fprintf(stderr, "chdir '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

static FILE* get_java_file(const std::string& ns, const std::string& name, std::string& pkg) {
    // open class file
    strlist_t dirvec = { "qore" };
    // convert namespace to package
    pkg = ns;
    if (ns.empty()) {
        pkg = "Qore";
        dirvec.push_back("Qore");
    } else {
        get_string_list(dirvec, pkg, "::");
        replace(pkg, "::", ".");
    }

    // start in root java output dir
    if (mkdir_chdir(opts.javadoc_fn.c_str())) {
        exit(1);
    }

    //printf("dir: '%s' pkg: '%s'\n", opts.javadoc_fn.c_str(), pkg.c_str());
    for (auto& i : dirvec) {
        if (mkdir_chdir(i.c_str())) {
            exit(1);
        }
    }

    std::string fn = name;
    fn += ".java";
    FILE* fp = fopen(fn.c_str(), "w");
    if (!fp) {
        char buf[PATH_MAX + 1];
        fprintf(stderr, "cannot create java output file '%s/%s': %s\n", getcwd(buf, PATH_MAX), fn.c_str(),
            strerror(errno));
        exit(1);
    }
    return fp;
}

#define T_INT        0
#define T_FLOAT      1
#define T_STRING     2
#define T_BOOL       3
#define T_HASH       4
#define T_BIN        5
#define T_FUNC       6
#define T_RELDATE    7
#define T_QORE       8
#define T_OTHER      9
#define T_CSTRING   10

static int get_val_type(const std::string& str) {
    if (!str.empty()) {
        size_t lc = str.size() - 1;
        if ((str[0] == '"' && str[lc] == '"')
            || (str[0] == '\'' && str[lc] == '\''))
            return T_STRING;
        if (str[0] == '(' && str[lc] == ')')
            return T_HASH;
        if (str[0] == '<' && str[lc] == '>')
            return T_BIN;
        if (str[0] == '{') {
            size_t i = str.find('}');
            if (i == std::string::npos)
                return T_OTHER;
            std::string ns(str, i + 1);
            return get_val_type(ns);
        }
        if (!str.compare(0, 4, "str(") && str[lc] == ')')
            return T_CSTRING;
        if (!str.compare(0, 5, "bool(") && str[lc] == ')')
            return T_BOOL;
        if (!str.compare(0, 5, "qore(") && str[lc] == ')')
            return T_QORE;
        if (!str.compare(0, 2, "0x"))
            return T_INT;
    }

    bool pint = false,          // has integers
        pucus = false,          // has uppercase and/or underscore chars
        pdec = false,           // has a decimal point
        poth = false,           // has other chars
        ptrrd = false;          // is an integer with trailing relative date chars

    size_t start = 0;
    if (str[0] == '-') {
        start = 1;
        pint = true;
    }
    for (size_t i = start, e = str.size(); i != e; ++i) {
        char c = str[i];
        if (!pucus && pint && !pdec && !poth && (c == 'm' || c == 's'))
            ptrrd = true;
        else {
            if (ptrrd)
                ptrrd = false;

            if (isdigit(c))
                pint = true;
            else if (isupper(c) || c == '_' || (i != start && islower(c)))
                pucus = true;
            else if (c == '.')
                pdec = true;
            else
                poth = true;
        }
    }

    if (ptrrd)
        return T_RELDATE;
    if (poth)
        return T_OTHER;
    if (pdec)
        return pint && !pucus ? T_FLOAT : T_OTHER;
    if (pucus || pint)
        return T_INT;
    return T_OTHER;
}

static int get_dox_value(const std::string& qv, std::string& v) {
    if (qv[0] == '{') {
        size_t i = qv.find('}', 1);
        if (i == std::string::npos) {
            error("cannot find end of doc text\n");
            return -1;
        }

        v.assign(qv, 1, i - 1);
        return 0;
    }

    v = qv;
    return 0;
}

static int get_qore_value(const std::string& qv, std::string& v, const char* cname = nullptr, const char** prefix = nullptr, bool force_node = false) {
    if (qv[0] == '{') {
        size_t i = qv.find('}', 1);
        if (i == std::string::npos) {
            error("cannot find end of doc text\n");
            return -1;
        }

        std::string tmp(qv, i + 1);
        return get_qore_value(tmp, v, cname, prefix, force_node);
    }

    {
        strmap_t& vmap = force_node ? simple_valmap : valmap;
        strmap_t::iterator i = vmap.find(qv);
        if (i != vmap.end()) {
            v = i->second;
            return 0;
        }
    }

    int t = get_val_type(qv);
    switch (t) {
        case T_RELDATE: {
            int val = atoi(qv.c_str());
            size_t len = qv.size();
            char lc = qv[len - 1];
            if (force_node) {
                v = "QoreSimpleValue(";
            }
            v += "DateTimeNode::makeRelative(0, 0, 0, 0, ";
            if (qv[len - 2] == 'm' && lc == 's') {
                v += "0, 0, ";
                char buf[20];
                sprintf(buf, "%d", val * 1000);
                v += buf;
            } else {
                char buf[20];
                sprintf(buf, "%d", val);
                if (lc == 's') {
                    v += "0, ";
                    v += buf;
                    v += ", 0";
                }
                else if (lc == 'm') {
                    v += buf;
                    v += ", 0, 0";
                }
                else
                    error("cannot interpret relative date/time value '%s'\n", qv.c_str());
            }
            v += ")";
            if (force_node) {
                v += ")";
            }

            return 0;
        }

        case T_INT: {
            if (force_node) {
                v = "QoreSimpleValue";
            }
            v += "((int64)";
            v += qv;
            v += ")";
            return 0;
        }

        case T_FLOAT: {
            if (force_node) {
                v = "QoreSimpleValue";
            }
            v += "((double)";
            v += qv;
            v += ")";
            return 0;
        }

        case T_STRING: {
            if (force_node) {
                v = "QoreSimpleValue(";
            }
            v += "new QoreStringNode(";
            v += qv;
            v += ")";
            if (force_node) {
                v += ")";
            }
            return 0;
        }

        case T_CSTRING: {
            if (force_node) {
                v = "QoreSimpleValue(";
            }
            v += "new QoreStringNode(";
            v.append(qv, 4, qv.size() - 5);
            v += ")";
            if (force_node) {
                v += ")";
            }
            return 0;
        }

        case T_BOOL: {
            if (force_node) {
                v = "QoreSimpleValue";
            }
            v += "((bool)";
            v.append(qv, 5, qv.size() - 6);
            v += ")";
            return 0;
        }

        case T_QORE: {
            if (force_node) {
                v = "QoreSimpleValue(";
                v.append(qv, 4, qv.size() - 4);
                v += ")";
            }
            else {
                v.assign(qv, 4, qv.size() - 4);
            }
            return 0;
        }

        case T_BIN: {
            if (!cname) {
                error("cannot create a binary object for a non-constant value\n");
                return -1;
            }
            assert(!force_node);
            assert(prefix);
            (*prefix) = "bin";

            v = "BinaryNode* bin_";
            v += cname;
            v += " = new BinaryNode;\n    bin_";
            v += cname;
            v += "->append(\"";

            for (unsigned i = 1; i < qv.size() - 1; ++i) {
                v += "\\x";
                v += qv[i];
                v += qv[++i];
            }
            v += "\", ";
            char buf[20];
            sprintf(buf, "%ld", (long)(qv.size() / 2 - 1));
            v += buf;
            v += ");\n";
            return 0;
        }

        case T_HASH: {
            if (!cname) {
                error("cannot create a hash for a non-constant value\n");
                return -1;
            }
            assert(!force_node);
            assert(prefix);
            (*prefix) = "hash";

            v = "QoreHashNode* hash_";
            v += cname;
            v += " = new QoreHashNode(autoTypeInfo);\n";

            std::string hv(qv, 1, qv.size() - 2);

            strlist_t l;
            get_string_list2(l, hv, ',');

            for (unsigned i = 0; i < l.size(); ++i) {
                std::string& str = l[i];
                trim(str);
                if (str.empty())
                    continue;
                size_t sep = str.find(':');
                if (sep == std::string::npos) {
                    error("cannot find ':' in hash line: '%s' (line %d)\n", str.c_str(), i);
                    return -1;
                }
                std::string key(str, 0, sep);
                trim(key);
                if (key.empty()) {
                    error("hash key is empty\n");
                    return -1;
                }

                if (key[0] == '"') {
                    if (key[key.size() - 1] != '"') {
                        error("missing end quote in hash key '%s'\n", key.c_str());
                        return -1;
                    }
                } else {
                    key.insert(0, "MAKE_STRING_FROM_SYMBOL(");
                    key += ")";
                }

                std::string value(str, sep + 1);
                trim(value);
                std::string qoreval;
                if (get_qore_value(value, qoreval))
                    return -1;

                v += "    hash_";
                v += cname;
                v += "->setKeyValue(" + key + ", " + qoreval + ", 0);\n";
            }
            return 0;
        }
    }

    {
        strmap_t::iterator i = qppval.find(qv);
        if (i != qppval.end()) {
            if (force_node) {
                v = "QoreSimpleValue(";
            }
            v += i->second;
            if (force_node) {
                v += ")";
            }
            return 0;
        }
    }

    error("could not match qore value '%s' to a c++ value\n", qv.c_str());
    return -1;
}

int dom_get(strlist_t& dom, const std::string& str) {
    get_string_list(dom, str);
    for (strlist_t::iterator di = dom.begin(), e = dom.end(); di != e; ++di) {
        toupper(*di);
        if (dmap.find(*di) == dmap.end() && dnmap.find(*di) == dnmap.end()) {
            error("unknown domain code: '%s'; domains must be listed separated by commas and without the QDOM_ prefix\n", (*di).c_str());
            return -1;
        }
    }
    return 0;
}

static void dom_output_cpp(FILE* fp, const strlist_t& dom) {
    if (dom.empty()) {
        fputs("QDOM_DEFAULT", fp);
        return;
    }

    for (strlist_t::const_iterator i = dom.begin(), e = dom.end(); i != e; ++i) {
        if (i != dom.begin())
            fputc('|', fp);
        fprintf(fp, "QDOM_%s", (*i).c_str());
    }
}

int flags_get(strlist_t& flags, const std::string& str) {
    get_string_list(flags, str);
    for (strlist_t::iterator fi = flags.begin(), e = flags.end(); fi != e; ++fi) {
        toupper(*fi);
        if (fset.find(*fi) == fset.end()) {
            error("unknown code flag: '%s'; code flags must be listed separated by commas and without the QCF_ prefix\n", (*fi).c_str());
            return -1;
        }
    }
    return 0;
}

static void flags_output_cpp(FILE* fp, const strlist_t& flags, bool uses_extra_args) {
    if (flags.empty()) {
        fputs(uses_extra_args ? "QCF_USES_EXTRA_ARGS" : "QCF_NO_FLAGS", fp);
        return;
    }

    for (strlist_t::const_iterator i = flags.begin(), e = flags.end(); i != e; ++i) {
        if (i != flags.begin())
            fputc('|', fp);
        fprintf(fp, "QCF_%s", (*i).c_str());
    }
    if (uses_extra_args)
        fputs("|QCF_USES_EXTRA_ARGS", fp);
}

static void doRow(strlist_t& sl, std::string& tstr) {
    tstr += "    <tr>\n";
    for (unsigned k = 0; k < sl.size(); ++k) {
        tstr += "        ";
        if (sl[k][0] == '!') {
            sl[k].erase(0, 1);
            tstr += "<td class=\"qore\"><b>" + sl[k] + "</b></td>\n";
        }
        else
            tstr += "<td>" + sl[k] + "</td>\n";
    }
    tstr += "    </tr>\n";
}

static size_t justify_start(std::string& str) {
    // find start of long comment
    size_t start = str.find("/**");
    if (start == std::string::npos)
        return start;

    start += 3;

    // erase all whitespace between the open comment marker and the content of the comment
    if (str[start] == ' ')
        ++start;
    size_t be = start;
    while (true) {
        char c = str[be];
        if (c == ' ' || c == '\n' || c == '\t') {
            ++be;
            continue;
        }
        break;
    }
    if (be != start)
        str.erase(start, be - start);

    return start;
}

static size_t find_start(std::string& str) {
    // find start of long comment and justify text in comment
    size_t start = justify_start(str);
    if (start == std::string::npos)
        return start;

    size_t lc_start = start;

    size_t i = str.find("@par Platform Availability", start);
    if (i != std::string::npos) {
        start = str.find('\n', i);
        if (start == std::string::npos) {
            error("Error: cannot find end of Platform Availability data\n");
            return lc_start;
        }
        start = str.find('\n', start + 1);
        if (start == std::string::npos) {
            error("Error: cannot find end of Platform Availability data\n");
            return lc_start;
        }
        str.insert(start + 1, "\n    ");
        return start + 5;
    }
    i = str.find("@par", start);
    if (i == std::string::npos)
        return start;
    // find the beginning of the line if possible
    i = str.find_last_of('\n', i);
    if (i == std::string::npos || i < lc_start) {
        str.insert(lc_start, "    ");
        return lc_start;
    }
    str.insert(i + 1, "\n    ");
    return i + 5;
}

static void process_comment(std::string& buf) {
    size_t start = 0;

    // edit references to pseudo-methods
    while (true) {
        size_t i = buf.find(">::", start);
        if (i == std::string::npos || !i)
            break;

        start = i + 1;
        size_t j = buf.find_last_of('<', i - 1);
        if (j == std::string::npos)
            continue;

        size_t l = (i - j);
        if (l > 20 || l < 4)
            continue;

        buf.replace(i, 1, "zzz9");
        buf.replace(j, 1, "Qore::zzz8");
    }

    start = 0;
    while (true) {
        size_t i = buf.find("|!", start);
        log(LL_DEBUG, "serialize_dox_comment() looking for |! i: %lu\n", i);
        if (i == std::string::npos)
            break;

        size_t start = i;

        size_t end = 0;

        i += 1;
        std::string str;
        // closure to get one line, allowing for line continuations with '\' in the last position
        auto get_line = [&]() {
            str.clear();
            // find end of line
            size_t j = buf.find('\n', i);
            if (j == std::string::npos) {
                return false;
            }
            while (true) {
                end = j;
                if (buf[j - 1] != '\\') {
                    str.append(buf, i, j - i);
                    i = j + 1;
                    break;
                }
                str.append(buf, i, j - i - 1);
                i = j + 1;
                end = j;
                j = buf.find('\n', i);
                if (j == std::string::npos) {
                    break;
                }
            }
            return true;
        };

        if (!get_line()) {
            break;
        }

        std::string tstr = "@htmlonly <style><!-- td.qore { background-color: #5b9409; color: white; } --></style> @endhtmlonly\n    <table>\n";

        while (true) {
            strlist_t sl;
            get_string_list2(sl, str, '|');

            doRow(sl, tstr);
            if (!get_line()) {
                break;
            }

            // find start of next row, if any
            size_t k = str.find('|');
            if (k == std::string::npos) {
                break;
            }

            str.erase(0, k + 1);
        }

        tstr += "    </table>\n";

        if (!str.empty()) {
            tstr.append(str);
        }
        buf.replace(start, end - start, tstr);
    }
}

static int serialize_dox_comment(FILE* fp, std::string& buf, const strlist_t& dom = strlist_t(),
        const strlist_t& flags = strlist_t()) {
    process_comment(buf);

    size_t start = 0;
    if (!flags.empty()) {
        start = find_start(buf);
        if (start == std::string::npos) {
            error("cannot insert code flags: missing long comment\n");
        } else {
            std::string fbuf = "@par Code Flags:\n    ";
            for (strlist_t::const_iterator i = flags.begin(), e = flags.end(); i != e; ++i) {
                if (i != flags.begin())
                    fbuf += ", ";
                fbuf += "@ref ";
                fbuf += *i;
            }
            fbuf += "\n\n";
            buf.insert(start, fbuf);
        }
    } else if (dom.empty())
        justify_start(buf);

    if (!dom.empty()) {
        if (!start)
            start = find_start(buf);
        if (start == std::string::npos) {
            error("cannot insert domain flags: missing long comment\n");
        } else {
            std::string fbuf = "@par Restrictions:\n    ";
            for (strlist_t::const_iterator i = dom.begin(), e = dom.end(); i != e; ++i) {
                if (i != dom.begin())
                    fbuf += ", ";
                strmap_t::const_iterator di = dmap.find(*i);
                if (di != dmap.end()) {
                    fbuf += "@ref Qore::";
                    fbuf += di->second;
                }
                else {
                    di = dnmap.find(*i);
                    assert(di != dnmap.end());
                    fbuf += "MUST HAVE: @ref Qore::";
                    fbuf += di->second;
                }
            }
            fbuf += "\n\n";
            buf.insert(start, fbuf);
        }
    }

    output_file(fp, buf);
    return 0;
}

class ConstantGroupElement {
protected:
    std::string name, doc, value;

public:
    ConstantGroupElement(const std::string& n, const std::string& d, const std::string& v) : name(n), doc(d), value(v) {
    }

    int serializeCpp(FILE* fp) {
        // should never be called
        assert(false);
        return 0;
    }

    int serializeCppBinding(FILE* fp) const {
        std::string qv;
        const char* prefix = 0;
        if (get_qore_value(value, qv, name.c_str(), &prefix))
            return -1;
        if (prefix)
            fprintf(fp, "    %s\n    ns.addConstant(\"%s\", %s_%s);\n", qv.c_str(), name.c_str(), prefix, name.c_str());
        else
            fprintf(fp, "    ns.addConstant(\"%s\", %s);\n", name.c_str(), qv.c_str());
        return 0;
    }

    int serializeDox(FILE* fp) {
        process_comment(doc);
        output_file(fp, doc);

        std::string qv;
        if (get_dox_value(value, qv))
            return -1;
        fprintf(fp, "    const %s = %s;\n", name.c_str(), qv.c_str());
        return 0;
    }
};

class TextGroupElement {
protected:
    std::string text;
    unsigned startLine;
    std::string fileName;

public:
    TextGroupElement(const std::string& t, unsigned sl, const std::string& fn):text(t), startLine(sl), fileName(fn) {
    }

    int serializeUnitTest(FILE* fp) const {
        //fprintf(fp, "# TextGroupElement::serializeUnitTest\n");
        //fprintf(fp, "/*\n%s\n*/\n", text.c_str());
        return 0;
    }

    int serializeCpp(FILE* fp) const {
        fprintf(fp, "# %d \"%s\"\n", startLine, fileName.c_str());
        output_file(fp, text);
        return 0;
    }

    int serializeCppBinding(FILE* fp) const {
        output_file(fp, text);
        return 0;
    }

    int serializeDox(FILE* fp) const {
        output_file(fp, text);
        return 0;
    }
};

class CodeBase {
protected:
    enum ReturnType {
        RT_ANY = 0, RT_INT = 1, RT_BOOL = 2, RT_OBJ = 3, RT_FLOAT = 4
    };

    std::string fileName,       // source file name
        name,                   // name
        vname,                  // variant name
        docs,                   // docs
        return_type,            // return type
        code;                   // c++ code

    strlist_t flags,            // flags
        dom;                    // functional domains

    attr_t attr;
    paramlist_t params;
    ReturnType rt;
    unsigned line;
    bool has_return, doconly,   // only for documentation
        valid;

    void serializeArgs(FILE* fp, const char* cname = 0, bool rv = true) const {
        for (unsigned i = 0; i < params.size(); ++i) {
            const Param& p = params[i];

            if (!p.qore.empty()) {
                // skip args that are only there for documentation (otherwise we get unused variable warnings)
                if (p.qore == "doc")
                    continue;

                //log(LL_DEBUG, "found object class '%s' param name '%s'\n", p.type.c_str(), p.name.c_str());
                //HARD_QORE_OBJ_DATA(cert, QoreSSLCertificate, args, 0, CID_SSLCERTIFICATE, "Socket::setCertificate()", "SSLCertificate", xsink);
                std::string cid;
                get_type_name(cid, p.type);
                toupper(cid);

                if (p.type[0] == '!') {
                    fprintf(fp,
                            "    TAKE_HARD_QORE_VALUE_OBJ_DATA(%s, %s, args, %d, CID_%s, \"%s%s%s()\", \"%s\", xsink);\n    if (*xsink)\n        return%s;\n",
                            p.name.c_str(), p.qore.c_str(), i,
                            cid.c_str(), cname ? cname : "",
                            cname ? "::" : "", name.c_str(), p.type.c_str() + 1, rv ? " 0" : "");
                } else if (p.type[0] != '*') {
                    fprintf(fp,
                            "    HARD_QORE_VALUE_OBJ_DATA(%s, %s, args, %d, CID_%s, \"%s%s%s()\", \"%s\", xsink);\n    if (*xsink)\n        return%s;\n",
                            p.name.c_str(), p.qore.c_str(), i,
                            cid.c_str(), cname ? cname : "",
                            cname ? "::" : "", name.c_str(), p.type.c_str(), rv ? " 0" : "");
                } else {
                    fprintf(fp,
                            "    HARD_QORE_VALUE_OBJ_OR_NOTHING_DATA(%s, %s, args, %d, CID_%s, xsink);\n    if (*xsink)\n        return%s;\n",
                            p.name.c_str(), p.qore.c_str(), i, cid.c_str(), rv ? " 0" : "");
                }

                continue;
            }

            // do not process complex types in arg handling
            std::string ptype;
            {
                size_t i = p.type.find('<');
                if (i == std::string::npos)
                    ptype = p.type;
                else
                    ptype = p.type.substr(0, i);
            }

            if (ptype == "int" || ptype == "softint" || ptype == "timeout") {
                fprintf(fp, "    int64 %s = HARD_QORE_VALUE_INT(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*int" || ptype == "*softint" || ptype == "*timeout") {
                fprintf(fp, "    int64 %s = HARD_QORE_VALUE_INT(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "float" || ptype == "softfloat") {
                fprintf(fp, "    double %s = HARD_QORE_VALUE_FLOAT(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*float" || ptype == "*softfloat") {
                fprintf(fp, "    double %s = HARD_QORE_VALUE_FLOAT(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "number" || ptype == "softnumber") {
                fprintf(fp, "    const QoreNumberNode* %s = HARD_QORE_VALUE_NUMBER(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "bool" || ptype == "softbool") {
                fprintf(fp, "    bool %s = HARD_QORE_VALUE_BOOL(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*bool" || ptype == "*softbool") {
                fprintf(fp, "    bool %s = HARD_QORE_VALUE_BOOL(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "string" || ptype == "softstring") {
                fprintf(fp, "    const QoreStringNode* %s = HARD_QORE_VALUE_STRING(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*string" || ptype == "*softstring") {
                fprintf(fp,
                        "    const QoreStringNode* %s = get_param_value(args, %d).get<const QoreStringNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "date" || ptype == "softdate") {
                fprintf(fp, "    const DateTimeNode* %s = HARD_QORE_VALUE_DATE(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*date" || ptype == "*softdate") {
                fprintf(fp,
                        "    const DateTimeNode* %s = get_param_value(args, %d).get<const DateTimeNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "binary" || ptype == "softbinary") {
                fprintf(fp, "    const BinaryNode* %s = HARD_QORE_VALUE_BINARY(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*binary" || ptype == "*softbinary") {
                fprintf(fp,
                        "    const BinaryNode* %s = get_param_value(args, %d).get<const BinaryNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "list" || ptype == "softlist") {
                fprintf(fp, "    const QoreListNode* %s = HARD_QORE_VALUE_LIST(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*list" || ptype == "*softlist") {
                fprintf(fp,
                        "    const QoreListNode* %s = get_param_value(args, %d).get<const QoreListNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "hash") {
                fprintf(fp, "    const QoreHashNode* %s = HARD_QORE_VALUE_HASH(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*hash") {
                fprintf(fp,
                        "    const QoreHashNode* %s = get_param_value(args, %d).get<const QoreHashNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "reference") {
                fprintf(fp, "    const ReferenceNode* %s = HARD_QORE_VALUE_REF(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*reference") {
                fprintf(fp,
                        "    const ReferenceNode* %s = get_param_value(args, %d).get<const ReferenceNode>();\n",
                        p.name.c_str(), i);
                continue;
            }
            if (ptype == "object") {
                fprintf(fp, "    QoreObject* %s = HARD_QORE_VALUE_OBJECT(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*object") {
                fprintf(fp,
                        "    QoreObject* %s = get_param_value(args, %d).get<QoreObject>();\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "code" || ptype == "*code") {
                fprintf(fp,
                        "    const ResolvedCallReferenceNode* %s = get_param_value(args, %d).get<const ResolvedCallReferenceNode>();\n",
                        p.name.c_str(), i);
                continue;
            }

            if (ptype == "enum" || ptype == "*enum") {
                size_t lt = p.type.find('<');
                size_t gt = p.type.rfind('>');
                std::string ename;
                if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                    ename = p.type.substr(lt + 1, gt - lt - 1);
                    trim(ename);
                    if (!ename.compare(0, 2, "::")) {
                        ename.erase(0, 2);
                    }
                }
                strmap_t::iterator ei = enum_base_type_map.find(ename);
                if (ei == enum_base_type_map.end()) {
                    size_t nspos = ename.rfind(':');
                    if (nspos != std::string::npos) {
                        std::string short_name = ename.substr(nspos + 1);
                        ei = enum_base_type_map.find(short_name);
                    }
                }

                if (ei != enum_base_type_map.end()) {
                    const std::string& base = ei->second;
                    if (base == "int") {
                        fprintf(fp, "    int64 %s = HARD_QORE_VALUE_INT(args, %d);\n", p.name.c_str(), i);
                        continue;
                    }
                    if (base == "string") {
                        fprintf(fp, "    const QoreStringNode* %s = HARD_QORE_VALUE_STRING(args, %d);\n",
                            p.name.c_str(), i);
                        continue;
                    }
                    if (base == "float") {
                        fprintf(fp, "    double %s = HARD_QORE_VALUE_FLOAT(args, %d);\n", p.name.c_str(), i);
                        continue;
                    }
                    if (base == "number") {
                        fprintf(fp, "    const QoreNumberNode* %s = HARD_QORE_VALUE_NUMBER(args, %d);\n",
                            p.name.c_str(), i);
                        continue;
                    }
                }

                fprintf(fp, "    QoreValue %s = get_param_value(args, %d);\n", p.name.c_str(), i);
                continue;
            }

            if (ptype == "any" || ptype == "data" || ptype == "auto") {
                fprintf(fp, "    QoreValue %s = get_param_value(args, %d);\n", p.name.c_str(), i);
                continue;
            }
            if (ptype == "*data") {
                fprintf(fp, "    const AbstractQoreNode* %s = "
                    "get_param_value(args, %d).get<const AbstractQoreNode>();\n", p.name.c_str(), i);
                continue;
            }
            // skip "..." arg which is just for documentation
            if (ptype == "...") {
                continue;
            }
            // assume class name with no private data
            fprintf(fp, "    const QoreObject* %s = get_param_value(args, %d).get<const QoreObject>();\n",
                p.name.c_str(), i);
        }
    }

    int serializeQoreParams(FILE* fp) const {
        for (unsigned i = 0; i < params.size(); ++i) {
            if (i)
                fputs(", ", fp);
            const Param& p = params[i];
            if (p.type == "...")
                fputs("...", fp);
            else {
                if (p.type[0] == '!')
                    fputs(p.type.c_str() + 1, fp);
                else
                    fputs(p.type.c_str(), fp);

                fprintf(fp, " %s", p.name.c_str());
                if (!p.val.empty()) {
                    std::string qv;
                    if (get_dox_value(p.val, qv))
                        return -1;

                    fprintf(fp, " = %s", qv.c_str());
                }
            }
        }
        return 0;
    }

    int serializeBindingArgs(FILE* fp) const {
        size_t size = params.size();
        if (size && params[size - 1].type == "...")
            --size;

        if (size) {
            fprintf(fp, ", %lu", (unsigned long)size);
            for (unsigned i = 0; i < size; ++i) {
                std::string str;
                if (get_qore_type(params[i].type, str))
                    return -1;
                fprintf(fp, ", %s, ", str.c_str());
                if (params[i].val.empty())
                    fputs("QORE_PARAM_NO_ARG", fp);
                else {
                    std::string vs;
                    if (get_qore_value(params[i].val, vs, nullptr, nullptr, true))
                        return -1;
                    fputs(vs.c_str(), fp);
                }
                fprintf(fp, ", \"%s\"", params[i].name.c_str());
            }
        }
        return 0;
    }

    // Generate vector-based arguments for abstract methods to avoid varargs ABI issues
    // Build vectors element by element to avoid GCC 15 initializer_list issues
    int serializeBindingArgsVectors(FILE* fp) const {
        size_t size = params.size();
        if (size && params[size - 1].type == "...")
            --size;

        // Generate type_vec_t - build element by element
        fputs(", [&]() { type_vec_t v; ", fp);
        for (unsigned i = 0; i < size; ++i) {
            std::string str;
            if (get_qore_type(params[i].type, str))
                return -1;
            fprintf(fp, "v.push_back(%s); ", str.c_str());
        }
        fputs("return v; }()", fp);

        // Generate arg_vec_t - build element by element
        fputs(", [&]() { arg_vec_t v; ", fp);
        for (unsigned i = 0; i < size; ++i) {
            if (params[i].val.empty())
                fputs("v.push_back(QoreValue()); ", fp);
            else {
                std::string vs;
                if (get_qore_value(params[i].val, vs, nullptr, nullptr, true))
                    return -1;
                fprintf(fp, "v.push_back(%s); ", vs.c_str());
            }
        }
        fputs("return v; }()", fp);

        // Generate name_vec_t - build element by element
        fputs(", [&]() { name_vec_t v; ", fp);
        for (unsigned i = 0; i < size; ++i) {
            fprintf(fp, "v.push_back(\"%s\"); ", params[i].name.c_str());
        }
        fputs("return v; }()", fp);

        return 0;
    }

    void serializeQoreAttrComment(FILE* fp, unsigned indent = 0, const char* comment_str = "//") const {
        while (indent--)
            fputc(' ', fp);
        fputs(comment_str, fp);
        fputs(" ", fp);
        if (attr & QCA_SYNCHRONIZED)
            fputs("synchronized ", fp);
        if (attr & QCA_STATIC)
            fputs("static ", fp);
        if (attr & QCA_PRIVATE)
            fputs("private ", fp);
        if (attr & QCA_PRIVATE_INTERNAL)
            fputs("private:internal ", fp);
    }

    const char* getReturnType() const {
        return "QoreValue";
    }

    std::string getJavaReturnType() const {
        return getJavaType(return_type);
    }

    static void serializeQoreCppType(FILE* fp, const std::string& tstr) {
        if (tstr == "...")
            return;

        if (tstr[0] == '!') {
            fputs(tstr.c_str() + 1, fp);
            return;
        }

        size_t i = tstr.find('*');
        if (i == std::string::npos) {
            fputs(tstr.c_str(), fp);
            return;
        }

        std::string t = tstr;
        t.replace(i, 1, "__7_ ");
        fputs(t.c_str(), fp);
    }

    const char* getFunctionType() const {
        return "q_func_t";
    }

    int appendMangledParamCodes() {
        for (paramlist_t::const_iterator i = params.begin(), e = params.end(); i != e; ++i) {
            if (i == params.begin())
                vname += '_';

            strmap_t::iterator ti = mtmap.find((*i).type);
            if (ti == mtmap.end()) {
                size_t p = (*i).type.rfind("::");
                std::string cn;
                if (p != std::string::npos)
                    cn.append((*i).type, p + 2, -1);
                else {
                    size_t j = (*i).type.find_first_of("<>*(),: ");
                    if (j != std::string::npos) {
                        std::string ptype = (*i).type;
                        while (j != std::string::npos) {
                            ptype.replace(j, 1, "_");
                            j = ptype.find_first_of("<>*(),: ");
                        }
                        cn = ptype;
                    }
                    else
                        cn = (*i).type;
                }

                char buf[20];
                sprintf(buf, "%ld", (long)cn.size());
                vname += "C";
                vname += buf;
                vname += cn;
            }
            else
                vname += ti->second;
        }
        return 0;
    }

public:
    CodeBase(const std::string& fn, const std::string& n_name, attr_t n_attr, const paramlist_t& n_params,
            const std::string& n_docs, const std::string& n_return_type, const strlist_t& n_flags,
            const strlist_t& n_dom, const std::string& n_code, unsigned n_line, bool n_doconly)
            : fileName(fn), name(n_name), vname(name), docs(n_docs),
        return_type(n_return_type), code(n_code), flags(n_flags),
        dom(n_dom), attr(n_attr), params(n_params), rt(RT_ANY),
        line(n_line), has_return(false), doconly(n_doconly), valid(true) {
        if (return_type == "int" || return_type == "softint")
            rt = RT_INT;
        else if (return_type == "bool" || return_type == "softbool")
            rt = RT_BOOL;
        else if (return_type == "float" || return_type == "softfloat")
            rt = RT_FLOAT;
        else if (is_object(return_type))
            rt = RT_OBJ;

        // check if there is a return statement on the last line
        size_t i = code.rfind('\n');
        if (i == std::string::npos)
            i = 0;

        has_return = code.find("return", i) != std::string::npos;

        // assign variant name based on args
        appendMangledParamCodes();

        log(LL_DEBUG, "CodeBase::CodeBase() name %s\n", name.c_str());
    }

    const char* getName() const {
        return name.c_str();
    }

    operator bool() const {
        return valid;
    }

    static std::string getJavaType(const std::string& qore_type, bool obj = false) {
        if (qore_type == "nothing" || qore_type == "null") {
            return "void";
        }
        if (qore_type == "int" || qore_type == "timeout") {
            return obj ? "Long" : "long";
        }
        if (qore_type == "bool") {
            return obj ? "Boolean" : "boolean";
        }
        if (qore_type == "float") {
            return obj ? "Double" : "double";
        }
        if (qore_type == "string") {
            return "String";
        }
        if (qore_type.rfind("hash", 0) == 0) {
            return "org.qore.jni.Hash";
        }
        if (qore_type.rfind("list", 0) == 0) {
            return "Object[]";
        }
        if (qore_type == "number") {
            return "java.math.BigDecimal";
        }
        if (qore_type == "binary") {
            return "byte[]";
        }
        if (qore_type == "code" || qore_type == "closure" || qore_type == "callref") {
            return "org.qore.jni.QoreClosure";
        }
        if (qore_type.rfind("reference", 0) == 0) {
            return "Object";
        }
        if (qore_type == "object" || qore_type == "any" || qore_type == "auto") {
            return "Object";
        }
        if (qore_type.rfind("soft", 0) == 0) {
            std::string tmp = qore_type.substr(4);
            return getJavaType(tmp);
        }
        if (qore_type[0] == '*') {
            std::string tmp = qore_type.substr(1);
            return getJavaType(tmp, true);
        }
        return get_java_type_name(qore_type);
    }
};

class FunctionGroupElement : public CodeBase {
protected:
    void serializeQorePrototypeComment(FILE* fp, unsigned indent = 0, const char* comment_str = "//") const {
        serializeQoreAttrComment(fp, indent, comment_str);
        fprintf(fp, "%s %s(", return_type.empty()? "nothing" : return_type.c_str(), name.c_str());
        serializeQoreParams(fp);
        fputs(") {}\n", fp);
    }

public:
    FunctionGroupElement(const char* fn, const std::string& n_name, attr_t n_attr, const paramlist_t& n_params,
            const std::string& n_docs, const std::string& n_return_type, const strlist_t& n_flags,
            const strlist_t& n_dom, const std::string& n_code, unsigned n_line, bool n_doconly)
            : CodeBase(fn, n_name, n_attr, n_params, n_docs, n_return_type, n_flags, n_dom, n_code, n_line,
                n_doconly) {
    }

    int serializeUnitTest(FILE* fp) const {
        //fprintf(fp, "# FunctionGroupElement::serializeUnitTest\n");

        if (doconly)
            return 0;

        serializeQorePrototypeComment(fp, 8, "#");

        return 0;
    }

    int serializeCpp(FILE* fp) const {
        if (doconly)
            return 0;

        serializeQorePrototypeComment(fp);

        fprintf(fp, "static %s f_%s(const QoreListNode* args, RuntimeConfig& runtime_cfg, ExceptionSink* xsink) {\n",
            getReturnType(), vname.c_str());
        serializeArgs(fp);
        fprintf(fp, "# %d \"%s\"\n", line, fileName.c_str());
        output_file(fp, code);

        if (!has_return)
            fprintf(fp, "\n    return QoreValue();");

        fputs("\n}\n\n", fp);
        return 0;
    }

    int serializeCppBinding(FILE* fp) const {
        if (doconly)
            return 0;

        serializeQorePrototypeComment(fp, 4);

        // get return type
        std::string cppt;
        if (get_qore_type(return_type, cppt))
            return -1;

        fprintf(fp, "    ns.addBuiltinVariant(\"%s\", (%s)f_%s, ", name.c_str(), getFunctionType(), vname.c_str());

        flags_output_cpp(fp, flags, attr & QCA_USES_EXTRA_ARGS);
        fputs(", ", fp);
        dom_output_cpp(fp, dom);
        fprintf(fp, ", %s", cppt.c_str());

        if (serializeBindingArgs(fp))
            return -1;

        fputs(");\n", fp);

        return 0;
    }

    int serializeDox(FILE* fp) {
        serialize_dox_comment(fp, docs, dom, flags);

        serializeQoreCppType(fp, return_type);
        fprintf(fp, " %s(", name.c_str());

        for (unsigned i = 0; i < params.size(); ++i) {
            if (i)
                fputs(", ", fp);
            const Param& p = params[i];
            serializeQoreCppType(fp, p.type);
            fputc(' ', fp);
            fputs(p.name.c_str(), fp);
            if (!p.val.empty()) {
                fputs(" = ", fp);
                std::string qv;
                if (get_dox_value(p.val, qv))
                    return -1;
                fputs(qv.c_str(), fp);
            }
        }
        fputs(");\n\n", fp);

        return 0;
    }
};

typedef std::vector<TextGroupElement*> tlist_raw_t;
class tlist_t : public tlist_raw_t {
public:
    ~tlist_t() {
        clear();
    }

    void clear() {
        for (unsigned i = 0; i < size(); ++i)
            delete at(i);
        tlist_raw_t::clear();
    }
};

typedef std::map<std::string, ConstantGroupElement*> cmap_raw_t;
typedef std::multimap<std::string, FunctionGroupElement*> fmap_raw_t;

template<class T> class MyMap : public T {
public:
    ~MyMap() {
        clear();
    }

    void clear() {
        for (typename T::iterator i = T::begin(), e = T::end(); i != e; ++i)
            delete i->second;
        T::clear();
    }
};

typedef MyMap<cmap_raw_t> cmap_t;
typedef MyMap<fmap_raw_t> fmap_t;

class NamespaceElement {
public:
    void outputNamespaceStart(FILE* fp) {
        ns_size = 0;
        std::vector<std::string> ns_list = getNamespacePath(ns);
        for (auto& i : ns_list) {
            fprintf(fp, "//! %s namespace\nnamespace %s {\n", i.c_str(), i.c_str());
            ++ns_size;
        }
    }

    void outputNamespaceEnd(FILE* fp) {
        if (!ns_size) {
            ns_size = 1;
        }
        for (unsigned i = 0; i < ns_size; ++i) {
            fprintf(fp, "}\n");
        }
    }

    static std::vector<std::string> getNamespacePath(const std::string str) {
        std::vector<std::string> rv;
        size_t start = 0;
        while (true) {
            size_t pos = str.find("::", start);
            rv.push_back(str.substr(start, pos - start));
            if (pos == std::string::npos) {
                break;
            }
            start = pos + 2;
        }
        return rv;
    }

    std::string ns;
    unsigned ns_size = 0;
};

class Group : protected NamespaceElement {
protected:
    cmap_t cmap;                // constant map
    fmap_t fmap;                // function multimap
    tlist_t tlist;              // list of text elements

    std::string doc;            // doc header for group

    bool valid;
    const char* fileName;
    unsigned startLineNumber;
    unsigned& lineNumber;
    PreprocessorState& ppState; // preprocessor state for #ifdef handling

    void checkBuf(std::string& buf) {
        if (!buf.empty()) {
            tlist.push_back(new TextGroupElement(buf, startLineNumber, fileName));
            startLineNumber = lineNumber;
            buf.clear();
        }
    }

    int readUntilOpenGroup(const char* fileName, unsigned &lineNumber, std::string& str, FILE* fp) {
        bool open_group = false;
        while (true) {
            std::string line;
            if (read_line(lineNumber, line, fp)) {
                error("%s:%d: premature EOF reading group description\n", fileName, lineNumber);
                valid = false;
                return -1;
            }

            str += line;

            if (!line.compare(0, 5, "///@{")) {
                break;
            } else if (line.find("@{") != std::string::npos) {
                open_group = true;
            } else if (open_group && !line.compare(0, 2, "*/")) {
                break;
            }
        }

        return 0;
    }

    int parseFunction(std::string& code, FILE* fp, std::string& doc) {
        unsigned line = lineNumber;
        if (read_until_close(fileName, lineNumber, code, fp)) {
            error("%s:%d: premature EOF reading method definition starting on line %d\n", fileName, lineNumber, line);
            return -1;
        }
        // find start of parameter list
        size_t i = code.find('(');
        if (i == std::string::npos) {
            error("%s:%d: expecting function definition\n", fileName, lineNumber);
            return -1;
        }
        // save the position of the '('
        size_t p = i;

        // find beginning of function name
        while (i && code[i - 1] != ' ')
            --i;

        // get function name
        std::string fn(code, i, p - i);

        // function attributes
        attr_t attr = QCA_NONE;
        // return type
        std::string return_type;

        if (i && parse_attributes(fileName, lineNumber, attr, return_type, code, i))
            return -1;

        if (attr & QCA_ABSTRACT) {
            error("invalid 'abstract' defining function %s()\n", fn.c_str());
            return -1;
        }

        strmap_t flags;
        paramlist_t params;

        if (parse_params_and_flags(fileName, lineNumber, flags, params, attr, code, p + 1, fn))
            return -1;

        strlist_t cf;
        strlist_t dom;
        bool doconly = false;
        // parse flags
        for (strmap_t::const_iterator i = flags.begin(), e = flags.end(); i != e; ++i) {
            //log(LL_DEBUG, "+ method %s::%s() flag '%s': '%s'\n", name.c_str(), mname.c_str(), i->first.c_str(), i->second.c_str());
            if (i->first == "dom") {
                if (dom_get(dom, i->second))
                    return -1;
            } else if (i->first == "flags") {
                if (flags_get(cf, i->second))
                    return -1;
            } else if (i->first == "doconly")
                doconly = true;
            else {
                error("unknown flag '%s' = '%s' defining function %s()\n", i->first.c_str(), i->second.c_str(), fn.c_str());
                return -1;
            }
        }

        //log(LL_INFO, "+ method %s::%s() attr: 0x%x (static: %d)\n", name.c_str(), mname.c_str(), attr, attr & QCA_STATIC);

        FunctionGroupElement* fge = new FunctionGroupElement(fileName, fn, attr, params, doc, return_type, cf, dom, code, line, doconly);
        fmap.insert(fmap_t::value_type(fn, fge));

        return !*fge;
    }

public:
    Group(std::string& buf, FILE* fp, const char* fn, unsigned& ln, PreprocessorState& pps)
            : valid(true), fileName(fn), startLineNumber(ln), lineNumber(ln), ppState(pps) {
        doc = buf;
        if (readUntilOpenGroup(fileName, lineNumber, doc, fp)) {
            error("%s:%d: could not find start of group\n", fileName, lineNumber);
            valid = false;
            return;
        }
        buf.clear();

        while (true) {
            std::string line;
            if (read_line(lineNumber, line, fp)) {
                error("%s:%d: a premature EOF reading group\n", fileName, lineNumber);
                valid = false;
                return;
            }

            // Handle preprocessor directives
            if (!line.empty() && line[0] == '#') {
                if (ppState.handleDirective(line, lineNumber, buf)) {
                    continue;
                }
                // Not a recognized/tracked directive - add to buf and continue
                // This handles unknown #ifdef/#endif macros as pass-through to C++
                buf += line;
                continue;
            }

            // Skip content in inactive conditional blocks
            if (ppState.isSkipping()) {
                buf += line;  // Keep the raw text for pass-through to C++
                continue;
            }

            if (!line.compare(0, 10, "namespace ")) {
                size_t p = line.find(';');
                if (p == std::string::npos) {
                    error("%s:%d: cannot find ';' in namespace declaration\n", fileName, lineNumber);
                    valid = false;
                    return;
                }

                if (!ns.empty()) {
                    error("%s:%d: duplicate namespace declaration\n", fileName, lineNumber);
                    valid = false;
                    return;
                }

                ns.assign(line, 10, p - 10);
                trim(ns);
                continue;
            }

            if (!line.compare(0, 3, "//!")) {
                checkBuf(buf);

                // check for block comment
                std::string cdoc = line;
                line.clear();
                if (read_line(lineNumber, line, fp)) {
                    error("%s:%d: premature EOF reading group\n", fileName, lineNumber);
                    valid = false;
                    return;
                }
                if (!line.compare(0, 3, "/**")) {
                    // Check if the block comment is complete on this line
                    // Start search at position 3 (after /**) for efficiency
                    if (line.find("*/", 3) == std::string::npos) {
                        // Multi-line block comment - need to read more
                        if (get_dox_comment(fileName, lineNumber, line, fp, true)) {
                            valid = false;
                            return;
                        }
                    }
                    cdoc += line;
                    line.clear();

                    if (read_line(lineNumber, line, fp)) {
                        error("%s:%d: premature EOF reading group\n", fileName, lineNumber);
                        valid = false;
                        return;
                    }
                }

                if (!line.compare(0, 6, "const ")) {
                    checkBuf(buf);

                    // trim off trailing newline and spaces
                    line.erase(line.size() - 1);
                    trim(line);
                    if (line[line.size() - 1] != ';') {
                        // re-add newline
                        line += '\n';
                        if (read_until_close(fileName, lineNumber, line, fp, ';')) {
                            error("%s:%d: expecting constant definition; got: %s (%c)\n", fileName, lineNumber, line.c_str(), line[line.size() - 1]);
                            valid = false;
                        }
                    }

                    line.erase(0, 6);
                    // erase trailing ';'
                    line.erase(line.size() - 1);
                    trim(line);

                    size_t i = line.find('=');
                    if (i == std::string::npos) {
                        error("%s:%d: invalid constant definition: const %s\n", fileName, lineNumber, line.c_str());
                        valid = false;
                        return;
                    }

                    std::string name, val;
                    name.assign(line, 0, i);
                    val.assign(line, i + 1, -1);
                    trim(name);
                    trim(val);
                    if (cmap.find(name) != cmap.end()) {
                        error("%s:%d: constant '%s' defined twice\n", fileName, lineNumber, name.c_str());
                        valid = false;
                        return;
                    }

                    cmap[name] = new ConstantGroupElement(name, cdoc, val);
                    continue;
                }

                if (parseFunction(line, fp, cdoc)) {
                    valid = false;
                    return;
                }

                continue;
            }
            //log(LL_INFO, "Group line: %s", line.c_str());
            if (line.find("@}") != std::string::npos) {
                break;
            }

            buf += line;
        }
        checkBuf(buf);

        if (!fmap.empty() && !cmap.empty()) {
            error("group has both constants and functions defined in it; this is invalid\n");
            valid = false;
        }
    }

    operator bool() const {
        return valid;
    }

    strlist_t precalculateUnitTest() const {
        strlist_t ret;

        std::string mock_filename = fileName;
        std::replace(mock_filename.begin(), mock_filename.end(), '.', '_');

        std::string v = "test_" + mock_filename + "_" + std::to_string(startLineNumber);
        ret.push_back(v);

        return ret;
    }

    int serializeUnitTest(FILE* fp) const {
        //fprintf(fp, "# Group::serializeUnitTest\n");
        fprintf(fp, "    # %s: %d\n", fileName, startLineNumber);

        if (fmap.empty())
            return 0;

        strlist_t names = precalculateUnitTest();
        fprintf(fp, "    private %s() {\n", names[0].c_str());

        // first serialize text
        for (unsigned i = 0; i < tlist.size(); ++i) {
            //fprintf(fp, "        # text\n");
            tlist[i]->serializeUnitTest(fp);
        }
        // now serialize function group members//fputc('\n', fp);
            for (fmap_t::const_iterator i = fmap.begin(), e = fmap.end(); i != e; ++i) {
            //fprintf(fp, "        # func group: %s\n", i->first.c_str());
            if (i->second->serializeUnitTest(fp))
                return -1;
        }

        fprintf(fp, "        assertEq(True, False, \"assertion placeholder; the real test needs to be implemented\");\n");
        fprintf(fp, "    }");

        return 0;
    }

    int serializeFunctionCpp(FILE* fp) const {
        if (fmap.empty())
            return 0;

        // first serialize text
        for (unsigned i = 0; i < tlist.size(); ++i)
            tlist[i]->serializeCpp(fp);

        // now serialize function group members
        //fputc('\n', fp);
        for (fmap_t::const_iterator i = fmap.begin(), e = fmap.end(); i != e; ++i)
            if (i->second->serializeCpp(fp))
                return -1;

        return 0;
    }
    int serializeCppConstantBindings(FILE* fp) const {
        // first serialize text
        for (unsigned i = 0; i < tlist.size(); ++i)
            tlist[i]->serializeCpp(fp);

        if (!cmap.empty()) {
            //fputc('\n', fp);
            for (cmap_t::const_iterator i = cmap.begin(), e = cmap.end(); i != e; ++i)
                if (i->second->serializeCppBinding(fp))
                    return -1;
        }
        return 0;
    }

    int serializeCppFunctionBindings(FILE* fp) const {
        if (!fmap.empty()) {
            //fputc('\n', fp);
            for (fmap_t::const_iterator i = fmap.begin(), e = fmap.end(); i != e; ++i)
                if (i->second->serializeCppBinding(fp))
                    return -1;
        }
        return 0;
    }

    int serializeFunctionDox(FILE* fp) {
        if (fmap.empty())
            return 0;

        if (ns.empty())
            fputs("\n//! main Qore-language namespace\nnamespace Qore {\n", fp);
        else {
            outputNamespaceStart(fp);
        }

        // serialize group header doc
        serialize_dox_comment(fp, doc);

        fputc('\n', fp);
        for (fmap_t::const_iterator i = fmap.begin(), e = fmap.end(); i != e; ++i)
            if (i->second->serializeDox(fp))
                return -1;

        // serialize group trailer
        fputs("///@}\n", fp);

        outputNamespaceEnd(fp);

        return 0;
    }

    int serializeConstantDox(FILE* fp, bool needs_prefix) {
        if (cmap.empty())
            return 0;

        if (needs_prefix) {
            if (ns.empty())
                fputs("\n//! main Qore-language namespace\nnamespace Qore {\n", fp);
            else {
                outputNamespaceStart(fp);
            }
        }
        // serialize group header doc
        serialize_dox_comment(fp, doc);

        fputc('\n', fp);
        for (cmap_t::const_iterator i = cmap.begin(), e = cmap.end(); i != e; ++i)
            if (i->second->serializeDox(fp))
                return -1;

        // serialize group trailer
        fputs("///@}\n", fp);

        if (needs_prefix) {
            outputNamespaceEnd(fp);
        }

        return 0;
    }

    bool hasFunctions() const {
        return !fmap.empty();
    }

    bool hasConstants() const {
        return !cmap.empty();
    }
};

typedef std::vector<Group*> grouplist_t;

class Groups {
protected:
    grouplist_t grouplist;
    bool has_funcs, has_constants, cccp_done, cdox_done;

public:
    Groups() : has_funcs(false), has_constants(false), cccp_done(false), cdox_done(false) {
    }

    ~Groups() {
        clear();
    }

    int add(Group*  g) {
        grouplist.push_back(g);
        if (!has_funcs && g->hasFunctions())
            has_funcs = true;
        else if (!has_constants && g->hasConstants())
            has_constants = true;
        return !(*g);
    }

    bool empty() const {
        return grouplist.empty();
    }

    strlist_t precalculateUnitTest() const {
        strlist_t ret;

        if (!has_funcs)
            return ret;

        for (unsigned i = 0; i < grouplist.size(); ++i) {
            strlist_t v = grouplist[i]->precalculateUnitTest();
            ret.insert(ret.end(), v.begin(), v.end());
        } return ret;
    }

    int serializeUnitTest(FILE* fp) const {
        //fprintf(fp, "# Groups::serializeUnitTest\n");

        if (!has_funcs)
            return 0;

        for (unsigned i = 0; i < grouplist.size(); ++i) {
            if (grouplist[i]->serializeUnitTest(fp)) {
                return -1;
            }
        }
        return 0;
    }

    int serializeFunctionCpp(FILE* fp, const char* rootName) {
        if (!has_funcs)
            return 0;

        for (unsigned i = 0; i < grouplist.size(); ++i)
            if (grouplist[i]->serializeFunctionCpp(fp))
                return -1;

        // now serialize function bindings
        fprintf(fp, "\nDLLLOCAL void init_%s_functions(QoreNamespace& ns) {\n", rootName);

        add_init_code(fp);

        for (unsigned i = 0; i < grouplist.size(); ++i)
            if (grouplist[i]->serializeCppFunctionBindings(fp))
                return -1;

        // serialize any constants
        //if (serializeCppConstantBindings(fp))
        //   return -1;

        fputs("\n}\n", fp);

        return 0;
    }

    int serializeConstantCpp(FILE* fp, const char* rootName) {
        log(LL_DEBUG, "Groups::serializeConstantCpp() has_constants=%d\n", has_constants);
        if (!has_constants || cccp_done || !has_constants)
            return 0;

        // now serialize constant bindings
        fprintf(fp, "\nDLLLOCAL void init_%s_constants(QoreNamespace& ns) {\n", rootName);

        add_init_code(fp);

        // serialize constants
        if (serializeCppConstantBindings(fp))
            return -1;

        fputs("\n}\n", fp);

        return 0;
    }

    int serializeCppConstantBindings(FILE* fp) {
        if (cccp_done || !has_constants)
            return 0;
        cccp_done = true;

        for (unsigned i = 0; i < grouplist.size(); ++i)
            if (grouplist[i]->serializeCppConstantBindings(fp))
                return -1;
        return 0;
    }

    int serializeFunctionDox(FILE* fp) {
        if (!has_funcs)
            return 0;

        for (unsigned i = 0; i < grouplist.size(); ++i)
            if (grouplist[i]->serializeFunctionDox(fp))
                return -1;

        return 0;
    }

    int serializeConstantDox(FILE* fp, bool needs_prefix = false) {
        if (cdox_done || !has_constants)
            return 0;
        cdox_done = true;

        for (unsigned i = 0; i < grouplist.size(); ++i)
            if (grouplist[i]->serializeConstantDox(fp, needs_prefix))
                return -1;
        return 0;
    }

    void clear() {
        for (unsigned i = 0; i < grouplist.size(); ++i)
            delete grouplist[i];
        grouplist.clear();
    }
};

static Groups groups;

class AbstractElement {
public:
    virtual~ AbstractElement() {
    }

    virtual int serializeCpp(FILE* fp) = 0;
    virtual int serializeDox(FILE* fp) = 0;
    virtual int serializeJavadoc() = 0;
    virtual int serializeUnitTest(FILE* fp) = 0;
    virtual strlist_t precalculateUnitTest() = 0;
};

struct HashDeclInfo {
    std::string comment;
    std::string type;
    std::string value;

    HashDeclInfo(std::string&& comment, std::string&& type, std::string&& value) : comment(comment), type(type), value(value) {
    }

    int serializeCpp(FILE* fp, const char* name, const std::string& hdname) {
        std::string qtype;
        if (get_qore_type(type, qtype))
            return -1;

        size_t i = qtype.find(hdname);
        if (i != std::string::npos)
            qtype.replace(i, hdname.size(), "hd");

        std::string valexp;
        if (!value.empty()) {
            if (get_qore_value(value, valexp))
                return -1;
        }
        fprintf(fp, "    hd->addMember(\"%s\", %s, QoreValue(%s));\n", name, qtype.c_str(),
            valexp.empty() ? "" : valexp.c_str());
        return 0;
    }

    int serializeDox(FILE* fp, const char* name) {
        process_comment(comment);

        fprintf(fp, "    %s\n", comment.c_str());
        fprintf(fp, "    %s %s", type.c_str(), name);
        if (!value.empty()) {
            fprintf(fp, " = %s", value.c_str());
        }
        fprintf(fp, ";\n");
        return 0;
    }
};

class HashDecl : public AbstractElement, protected NamespaceElement {
public:
    HashDecl(std::string&& comment, std::string& str, FILE* fp, const char* fileName, unsigned& lineNumber) : comment(comment) {
        const char* p = str.c_str();
        while (*p && *p == ' ')
            ++p;

        if (!*p) {
            error("%s:%d: premature EOF reading hashdecl\n", fileName, lineNumber);
            valid = false;
            return;
        }

        const char* p1 = p;

        while (*p1 && idnschar(*p1))
            ++p1;

        if (!*p1) {
            error("%s:%d: premature EOF reading hashdecl\n", fileName, lineNumber);
            valid = false;
            return;
        }

        name.assign(p, p1 - p);

        {
            size_t i = name.rfind(':');
            if (i != std::string::npos) {
                ns = name.substr(0, i - 1);
                name.erase(0, i + 1);
            }
        }

        //printf("hashdecl '%s'\n", name.c_str());

        // skip whitespace
        while (whitespace(*p1))
            ++p1;

        // Check for inheritance: hashdecl Child inherits Parent { ... }
        if (!strncmp(p1, "inherits", 8) && (whitespace(p1[8]) || !p1[8])) {
            p1 += 8;
            while (whitespace(*p1))
                ++p1;

            const char* parent_start = p1;
            while (*p1 && (idnschar(*p1) || *p1 == ':'))
                ++p1;

            parent_name.assign(parent_start, p1 - parent_start);

            while (whitespace(*p1))
                ++p1;
        }

        if (*p1 != '{') {
            error("%s:%d: missing open bracket '{' after the hashdecl name\n", fileName, lineNumber);
            valid = false;
            return;
        }

        ++p1;
        // skip whitespace
        while (whitespace(*p1))
            ++p1;

        if (*p1) {
            error("%s:%d: invalid character '%c' in hashdecl declaration; only whitespace is acceptable after the '{' "
                "char\n", fileName, lineNumber, *p1);
            valid = false;
            return;
        }

        while (true) {
            std::string line;

            if (read_line(lineNumber, line, fp)) {
                error("%s:%d: premature EOF reading hashdecl declaration\n", fileName, lineNumber);
                valid = false;
                return;
            }

            trim(line);

            // got a declaration
            if (!line.compare(0, 3, "//!")) {
                // check for block comment
                std::string cdoc = line;
                line.clear();
                if (read_line(lineNumber, line, fp)) {
                    error("%s:%d: premature EOF reading hashdecl declaration\n", fileName, lineNumber);
                    valid = false;
                    return;
                }
                size_t t = line.find("/**", 3);
                if (t != std::string::npos) {
                    // Check if the block comment is complete on this line
                    // Search for */ starting after the /** (position t + 3)
                    if (line.find("*/", t + 3) == std::string::npos) {
                        // Multi-line block comment - need to read more
                        if (get_dox_comment(fileName, lineNumber, line, fp, true)) {
                            valid = false;
                            return;
                        }
                    }
                    trim_end(line);
                    cdoc += "\n";
                    cdoc += line;
                    line.clear();

                    if (read_line(lineNumber, line, fp)) {
                        error("%s:%d: premature EOF reading hashdecl declaration\n", fileName, lineNumber);
                        valid = false;
                        return;
                    }
                }

                // parse declaration: type name [= exp];
                std::string type;
                if (getType(line, type, fileName, lineNumber))
                    return;
                std::string name;
                if (getName(line, name, fileName, lineNumber))
                    return;

                if (hdmap.find(name) != hdmap.end()) {
                    error("%s:%d: hashdecl key '%s' defined twice\n", fileName, lineNumber, name.c_str());
                    valid = false;
                    return;
                }

                //log(LL_DEBUG, "B line: '%s'\n", line.c_str());

                std::string exp;
                if (getKeyExp(line, exp, fileName, lineNumber, fp))
                    return;

                //log(LL_DEBUG, "A line: '%s', exp: '%s'\n", line.c_str(), exp.c_str());

                // save hashdecl
                hdmap.insert(hdmap_t::value_type(name, HashDeclInfo(std::move(cdoc), std::move(type), std::move(exp))));
                //log(LL_DEBUG, "hashdecl '%s' %s = '%s'; %s\n", this->name.c_str(), name.c_str(), exp.c_str(), cdoc.c_str());
                continue;
            }

            if (line == "}") {
                break;
            }
        }
    }

    virtual int serializeCpp(FILE* fp) {
        std::string ns_path;
        if (ns.empty()) {
            ns_path = "::Qore::";
            ns_path += name;
        } else {
            if (ns[0] == ':') {
                ns_path = ns;
            } else {
                ns_path = "::";
                ns_path += ns;
            }
            ns_path += "::";
            ns_path += name;
        }

        fprintf(fp, "TypedHashDecl* init_hashdecl_%s(QoreNamespace& ns) {\n", name.c_str());
        fprintf(fp, "    TypedHashDecl* hd = new TypedHashDecl(\"%s\", \"%s\");\n", name.c_str(), ns_path.c_str());

        // Set parent hashdecl if there is inheritance
        if (!parent_name.empty()) {
            std::string parent_var = parent_name;
            size_t i = parent_var.rfind(':');
            if (i != std::string::npos) {
                parent_var.erase(0, i + 1);
            }
            fprintf(fp, "    hd->setParent(hashdecl%s);\n", parent_var.c_str());
        }

        // get type name to substitute references to self if necessary
        std::string tname = "hashdecl" + name;
        for (auto& i : hdmap) {
            if (i.second.serializeCpp(fp, i.first.c_str(), tname))
                return -1;
        }

        fprintf(fp, "    ns.addSystemHashDecl(hd);\n    return hd;\n}\n\n");
        return 0;
    }

    virtual int serializeDox(FILE* fp) {
        if (ns.empty())
            fputs("\n//! main Qore-language namespace\nnamespace Qore {\n", fp);
        else {
            outputNamespaceStart(fp);
        }

        // serialize hashdecl doc
        process_comment(comment);
        fprintf(fp, "%s", comment.c_str());
        if (!parent_name.empty()) {
            fprintf(fp, "struct %s : %s {\n", name.c_str(), parent_name.c_str());
        } else {
            fprintf(fp, "struct %s {\n", name.c_str());
        }
        for (auto& i : hdmap)
            if (i.second.serializeDox(fp, i.first.c_str()))
                return -1;
        fputs("};\n", fp);
        outputNamespaceEnd(fp);
        return 0;
    }

    virtual int serializeUnitTest(FILE* fp) {
        return 0;
    }

    virtual int serializeJavadoc() {
        return 0;
    }

    virtual strlist_t precalculateUnitTest() {
        return strlist_t();
    }

    operator bool() const {
        return valid;
    }

protected:
    std::string comment;
    std::string name;
    std::string parent_name;  // parent hashdecl name for inheritance
    typedef std::map<std::string, HashDeclInfo> hdmap_t;
    hdmap_t hdmap;
    bool valid = true;

    int getType(std::string& line, std::string& type, const char* fileName, unsigned& lineNumber) {
        int bc = 0;
        unsigned i = 0;

        while (i < line.size() && whitespace(line[i]))
            ++i;

        while (i < line.size()) {
            switch (line[i]) {
                case '<':
                    ++bc;
                    break;
                case '>':
                    if (!bc) {
                        error("%s:%d: error reading key type in hashdecl declaration (%s)\n", fileName, lineNumber, line.c_str());
                        valid = false;
                        return -1;
                    }
                    --bc;
            }
            if (!bc && whitespace(line[i]))
                break;
            type += line[i];

            ++i;
        }
        line.erase(0, i);
        //log(LL_DEBUG, "hashdecl key type '%s'\n", type.c_str());
        return 0;
    }

    int getName(std::string& line, std::string& name, const char* fileName, unsigned& lineNumber) {
        unsigned i = 0;
        while (i < line.size() && whitespace(line[i]))
            ++i;
        while (i < line.size()) {
            if (!idchar(line[i]))
                break;
            name += line[i];
            ++i;
        }
        if (name.empty()) {
            valid = false;
            error("%s:%d: error reading key name in hashdecl declaration ('%s')\n", fileName, lineNumber, line.c_str());
            return -1;
        }
        line.erase(0, i);
        //log(LL_DEBUG, "hashdecl key name '%s'\n", name.c_str());
        return 0;
    }

    int getKeyExp(std::string& line, std::string& exp, const char* fileName, unsigned& lineNumber, FILE* fp) {
        //log(LL_DEBUG, "line: '%s'\n", line.c_str());
        // trim off trailing newline and spaces
        line.erase(line.size() - 1);
        trim(line);
        // issue #2838 line could be empty by this point
        if (line.empty() || line[line.size() - 1] != ';') {
            // re-add newline
            line += '\n';
            if (read_until_close(fileName, lineNumber, line, fp, ';')) {
                error("%s:%d: expecting hashdecl key definition; got: %s (%c)\n", fileName, lineNumber, line.c_str(),
                    line[line.size() - 1]);
                valid = false;
                return -1;
            }
        }

        size_t i = line.find('=');
        if (i != std::string::npos) {
            line.erase(line.size() - 1);
            line.erase(0, i + 1);
            trim(line);

            exp = line;
        }
        return 0;
    }
};

struct EnumMemberInfo {
    std::string comment;
    std::string value;           // explicit value (may be empty for auto-increment)
    std::string computed_value;  // computed value (set during parsing for auto-increment)

    EnumMemberInfo(std::string&& c, std::string&& v, std::string&& cv)
        : comment(std::move(c)), value(std::move(v)), computed_value(std::move(cv)) {}

    int serializeCpp(FILE* fp, const char* n, const std::string& baseType) const {
        // Use computed_value which handles auto-increment
        // For string enums, wrap the value in QoreStringNode to avoid implicit bool conversion
        if (baseType == "string") {
            fprintf(fp, "    ed->addMember(\"%s\", new QoreStringNode(%s));\n", n, computed_value.c_str());
        } else {
            fprintf(fp, "    ed->addMember(\"%s\", %s);\n", n, computed_value.c_str());
        }
        return 0;
    }

    int serializeDox(FILE* fp, const char* n) const {
        if (!comment.empty()) {
            std::string c = comment;
            process_comment(c);
            fprintf(fp, "%s", c.c_str());
        }
        fprintf(fp, "    %s", n);
        // Show explicit value in docs if provided
        if (!value.empty()) {
            fprintf(fp, " = %s", value.c_str());
        }
        fprintf(fp, ",\n");
        return 0;
    }
};

class Enum : public AbstractElement, protected NamespaceElement {
public:
    Enum(std::string&& c, std::string& str, FILE* fp, const char* fileName, unsigned& lineNumber)
        : comment(std::move(c)), baseType("int") {
        const char* p = str.c_str();
        while (*p && *p == ' ') {
            ++p;
        }

        // Extract enum name
        const char* p1 = p;
        while (*p1 && idnschar(*p1)) {
            ++p1;
        }

        if (p1 == p) {
            error("%s:%d: missing enum name\n", fileName, lineNumber);
            valid = false;
            return;
        }

        name.assign(p, p1 - p);

        // Handle namespace prefix
        {
            size_t i = name.rfind(':');
            if (i != std::string::npos) {
                ns = name.substr(0, i - 1);
                name.erase(0, i + 1);
            }
        }

        while (whitespace(*p1)) {
            ++p1;
        }

        // Check for optional base type
        if (*p1 == ':') {
            ++p1;
            while (whitespace(*p1)) {
                ++p1;
            }

            const char* type_start = p1;
            while (*p1 && idchar(*p1)) {
                ++p1;
            }

            baseType.assign(type_start, p1 - type_start);

            // Validate base type
            if (baseType != "int" && baseType != "string" && baseType != "float" && baseType != "number") {
                error("%s:%d: invalid enum base type '%s'; must be int, string, float, or number\n",
                      fileName, lineNumber, baseType.c_str());
                valid = false;
                return;
            }

            while (whitespace(*p1)) {
                ++p1;
            }
        }

        if (*p1 != '{') {
            error("%s:%d: missing '{' after enum declaration\n", fileName, lineNumber);
            valid = false;
            return;
        }

        {
            std::string key = name;
            enum_base_type_map[key] = baseType;
            if (!ns.empty()) {
                enum_base_type_map[ns + "::" + name] = baseType;
            }
        }

        if (parseMembers(fp, fileName, lineNumber)) {
            valid = false;
        }
    }

    virtual int serializeCpp(FILE* fp) override {
        std::string ns_path;
        if (ns.empty()) {
            ns_path = "::Qore::" + name;
        } else {
            ns_path = (ns[0] == ':') ? ns : ("::" + ns);
            ns_path += "::" + name;
        }

        // Map base type to C++ type info
        std::string base_type_info;
        if (baseType == "int") {
            base_type_info = "bigIntTypeInfo";
        } else if (baseType == "string") {
            base_type_info = "stringTypeInfo";
        } else if (baseType == "float") {
            base_type_info = "floatTypeInfo";
        } else if (baseType == "number") {
            base_type_info = "numberTypeInfo";
        }

        fprintf(fp, "QoreEnumDecl* init_enum_%s(QoreNamespace& ns) {\n", name.c_str());
        fprintf(fp, "    QoreEnumDecl* ed = new QoreEnumDecl(\"%s\", \"%s\", %s);\n",
                name.c_str(), ns_path.c_str(), base_type_info.c_str());

        for (auto& m : members) {
            m.second.serializeCpp(fp, m.first.c_str(), baseType);
        }

        fprintf(fp, "    ns.addSystemEnum(ed);\n    return ed;\n}\n\n");
        return 0;
    }

    virtual int serializeDox(FILE* fp) override {
        if (ns.empty()) {
            fputs("\n//! main Qore-language namespace\nnamespace Qore {\n", fp);
        } else {
            outputNamespaceStart(fp);
        }

        // serialize enum doc
        process_comment(comment);
        fprintf(fp, "%s", comment.c_str());

        if (baseType == "int") {
            fprintf(fp, "enum %s {\n", name.c_str());
        } else {
            fprintf(fp, "enum %s : %s {\n", name.c_str(), baseType.c_str());
        }

        for (auto& m : members) {
            m.second.serializeDox(fp, m.first.c_str());
        }

        fputs("};\n", fp);
        outputNamespaceEnd(fp);
        return 0;
    }

    virtual int serializeUnitTest(FILE* fp) override {
        return 0;
    }

    virtual int serializeJavadoc() override {
        return 0;
    }

    virtual strlist_t precalculateUnitTest() override {
        return strlist_t();
    }

    operator bool() const {
        return valid;
    }

protected:
    std::string comment;
    std::string name;
    std::string baseType;  // "int" (default), "string", "float", "number"
    std::vector<std::pair<std::string, EnumMemberInfo>> members;
    bool valid = true;

    int parseMembers(FILE* fp, const char* fileName, unsigned& lineNumber) {
        int64_t next_int_value = 0;  // for auto-increment

        while (true) {
            std::string line;
            if (read_line(lineNumber, line, fp)) {
                error("%s:%d: premature EOF reading enum\n", fileName, lineNumber);
                return -1;
            }

            trim(line);

            // Skip empty lines
            if (line.empty()) {
                continue;
            }

            // End of enum
            if (line == "}" || line == "};") {
                break;
            }

            // Handle documentation comment
            std::string cdoc;
            if (!line.compare(0, 3, "//!")) {
                cdoc = line;
                line.clear();
                if (read_line(lineNumber, line, fp)) {
                    error("%s:%d: premature EOF reading enum\n", fileName, lineNumber);
                    return -1;
                }
                trim(line);
            }

            // Parse: IDENTIFIER [= expression] [,]
            const char* p = line.c_str();

            // Get member name
            const char* p1 = p;
            while (*p1 && idchar(*p1)) {
                ++p1;
            }

            if (p1 == p) {
                error("%s:%d: expected enum member name\n", fileName, lineNumber);
                return -1;
            }

            std::string member_name(p, p1 - p);
            p = p1;

            while (whitespace(*p)) {
                ++p;
            }

            // Check for explicit value
            std::string explicit_value;
            std::string computed_value;

            if (*p == '=') {
                ++p;
                while (whitespace(*p)) {
                    ++p;
                }

                // Read value until comma or end
                const char* val_start = p;
                while (*p && *p != ',' && *p != '\n') {
                    ++p;
                }
                explicit_value.assign(val_start, p);
                trim(explicit_value);

                // For int base type, try to parse for next auto-increment
                if (baseType == "int") {
                    try {
                        next_int_value = std::stoll(explicit_value) + 1;
                    } catch (...) {
                        // Non-integer expression, just use as-is
                    }
                }
                computed_value = explicit_value;
            } else {
                // Auto-increment (only for numeric types)
                if (baseType == "string") {
                    error("%s:%d: string enum members require explicit values\n", fileName, lineNumber);
                    return -1;
                }
                computed_value = std::to_string(next_int_value);
                next_int_value++;
            }

            members.push_back({member_name, EnumMemberInfo(std::move(cdoc),
                              std::move(explicit_value), std::move(computed_value))});

            // Skip trailing comma
            while (*p == ',' || whitespace(*p)) {
                ++p;
            }
        }

        return 0;
    }
};

class TextElement : public AbstractElement {
protected:
    std::string buf;

public:
    TextElement(const std::string& n_buf):buf(n_buf) {
        //log(LL_DEBUG, "TextElement::TextElement() str=%s", n_buf.c_str());
    }

    virtual int serializeJavadoc() {
        return 0;
    }

    virtual int serializeCpp(FILE* fp) {
        output_file(fp, buf);
        return 0;
    }

    virtual int serializeDox(FILE* fp) {
        //return serialize_dox_comment(fp, buf);
        return 0;
    }

    virtual strlist_t precalculateUnitTest() {
        strlist_t ret;
        return ret;
    }

    virtual int serializeUnitTest(FILE* fp) {
        return 0;
    }
};

class Method : public CodeBase {
protected:
    std::string pseudo_arg;
    std::string pseudo_var;

    void serializeCppConstructor(FILE* fp, const char* cname) const {
        serializeQoreConstructorPrototypeComment(fp, cname);
        fprintf(fp,
            "static void %s_%s(QoreObject* self, const QoreListNode* args, RuntimeConfig& runtime_cfg, ExceptionSink* xsink) {\n",
            cname, vname.c_str());
        serializeArgs(fp, cname, false);
        fprintf(fp, "# %d \"%s\"\n", line, fileName.c_str());
        output_file(fp, code);
        fputs("\n}\n\n", fp);
    }

    void serializeCppDestructor(FILE* fp, const char* cname, const char* arg) const {
        serializeQoreDestructorCopyPrototypeComment(fp, cname);
        fprintf(fp,
            "static void %s_destructor(QoreObject* self, %s, RuntimeConfig& runtime_cfg, ExceptionSink* xsink) {\n",
            cname, arg);
        fprintf(fp, "# %d \"%s\"\n", line, fileName.c_str());
        output_file(fp, code);
        fputs("\n}\n\n", fp);
    }

    void serializeCppCopy(FILE* fp, const char* cname, const char* arg) const {
        serializeQoreDestructorCopyPrototypeComment(fp, cname);
        fprintf(fp,
            "static void %s_copy(QoreObject* self, QoreObject* old, %s, RuntimeConfig& runtime_cfg, ExceptionSink* xsink) {\n",
            cname, arg);
        fprintf(fp, "# %d \"%s\"\n", line, fileName.c_str());
        output_file(fp, code);
        fputs("\n}\n\n", fp);
    }

    int serializeCppConstructorBinding(FILE* fp, const char* cname, const char* UC) const {
        fputc('\n', fp);
        serializeQoreConstructorPrototypeComment(fp, cname, 4);

        fprintf(fp, "    QC_%s->%s(%s_%s, %s, ", UC, "addConstructor", cname, vname.c_str(), get_access(attr));
        flags_output_cpp(fp, flags, attr & QCA_USES_EXTRA_ARGS);
        fputs(", ", fp);
        dom_output_cpp(fp, dom);

        if (serializeBindingArgs(fp))
            return -1;

        fputs(");\n", fp);

        return 0;
    }

    int serializeCppDestructorBinding(FILE* fp, const char* cname, const char* UC) const {
        fputc('\n', fp);
        serializeQoreDestructorCopyPrototypeComment(fp, cname, 4);
        fprintf(fp, "    QC_%s->setDestructor((q_destructor_t)%s_destructor);\n", UC, cname);
        return 0;
    }

    int serializeCppCopyBinding(FILE* fp, const char* cname, const char* UC) const {
        fputc('\n', fp);
        serializeQoreDestructorCopyPrototypeComment(fp, cname, 4);
        fprintf(fp, "    QC_%s->setCopy((q_copy_t)%s_copy);\n", UC, cname);
        return 0;
    }

    void serializeQoreDestructorCopyPrototypeComment(FILE* fp, const char* cname, unsigned indent = 0, const char* comment_str = "//") const {
        serializeQoreAttrComment(fp, indent, comment_str);
        fprintf(fp, "%s::%s() {}\n", cname, name.c_str());
    }

    void serializeQoreConstructorPrototypeComment(FILE* fp, const char* cname, unsigned indent = 0, const char* comment_str = "//") const {
        serializeQoreAttrComment(fp, indent, comment_str);
        fprintf(fp, "%s::constructor(", cname);
        serializeQoreParams(fp);
        fputs(") {}\n", fp);
    }

    void serializeQorePrototypeComment(FILE* fp, const char* cname, unsigned indent = 0, const char* comment_str = "//") const {
        serializeQoreAttrComment(fp, indent, comment_str);
        fprintf(fp, "%s %s::%s(", return_type.empty()? "nothing" : return_type.c_str(), cname, name.c_str());
        serializeQoreParams(fp);
        fputs(")", fp);
        if (attr & QCA_ABSTRACT)
            fputc(';', fp);
        else
            fputs("{}", fp);
        fputc('\n', fp);
    }

    const char* getMethodType() const {
        return "q_method_t";
    }

    std::string getJavaAttrs() const {
        std::string attrs;
        if (!(attr & (QCA_PRIVATE | QCA_PRIVATE_INTERNAL))) {
            attrs += "public ";
        }
        if (attr & QCA_SYNCHRONIZED) {
            attrs += "synchronized ";
        }
        if (attr & QCA_ABSTRACT) {
            attrs += "abstract ";
        }
        return attrs;
    }

    std::string getJavaDoc() const {
        return get_java_doc(docs, 4);
    }

    std::string getJavaArgs() const {
        std::string rv;
        size_t size = params.size();
        if (size && params[size - 1].type == "...") {
            --size;
        }

        if (size) {
            for (unsigned i = 0; i < size; ++i) {
                std::string javatype = CodeBase::getJavaType(params[i].type);
                // FIXME: process default args and add variants
                rv += javatype.c_str();
                rv += " ";
                rv += params[i].name;
                if (i != (size - 1)) {
                    rv += ", ";
                }
            }
        }
        return rv;
    }

    void serializeJavadocConstructor(FILE* fp, const char* cname) const {
        serializeQoreConstructorPrototypeComment(fp, cname, 4);

        std::string doc = getJavaDoc();
        fprintf(fp, "%s", doc.c_str());
        std::string attrs = getJavaAttrs();
        std::string args = getJavaArgs();
        fprintf(fp, "    %s%s(%s) throws Throwable {\n", attrs.c_str(), cname, args.c_str());
        fputs("    }\n\n", fp);
    }

public:
    Method(const std::string& fn, const std::string& n_name, attr_t n_attr, const paramlist_t& n_params, const std::string& n_docs, const std::string& n_return_type, const strlist_t& n_flags, const strlist_t& n_dom, const std::string& n_code, unsigned n_line, bool n_doconly, const char* n_pseudo_arg) :
               CodeBase(fn, n_name, n_attr, n_params, n_docs, n_return_type, n_flags, n_dom, n_code, n_line, n_doconly) {
        if (n_pseudo_arg && n_pseudo_arg[0]) {
            pseudo_arg = n_pseudo_arg;
            std::string::size_type i = pseudo_arg.find('=');
            if (i != std::string::npos) {
                bool valid = true;
                --i;
                bool found = false;
                size_t end = 0;
                while (true) {
                    if (!i) {
                        valid = false;
                        break;
                    }
                    if (found) {
                        if (pseudo_arg[i] == ' ' || pseudo_arg[i] == '*') {
                            ++i;
                            break;
                        }
                    } else if (pseudo_arg[i] != ' ') {
                        end = i + 1;
                        found = true;
                    }
                    --i;
                }

                if (!valid) {
                    error("cannot find pseudo var name in '%s'\n", pseudo_arg.c_str());
                    exit(1);
                }

                pseudo_var = pseudo_arg.substr(i, end - i);
                //log(LL_DEBUG, "Method::Method() %s:%d '%s' pseudo_arg: '%s' pseudo_var: '%s'\n", fn.c_str(), line, name.c_str(), pseudo_arg.c_str(), pseudo_var.c_str());
            }
        }
        //log(LL_DEBUG, "Method::Method() %s:%d '%s'\n", fn.c_str(), line, name.c_str());
    }

    virtual ~Method() {
    }

    void serializeUnitTestMethod(FILE* fp, const char* cname) const {
        serializeQorePrototypeComment(fp, cname, 8, "#");
    }

    void serializeUnitTestStatic(FILE* fp, const char* cname) const {
        serializeQorePrototypeComment(fp, cname, 8, "#");
    }

    void serializeNormalJavadocMethod(FILE* fp, const char* cname) const {
        if (name == "constructor") {
            serializeJavadocConstructor(fp, cname);
            return;
        }
        // ignore destructor and copy methods in Java
        if (name == "destructor" || name == "copy") {
            return;
        }

        serializeQorePrototypeComment(fp, cname, 4);

        std::string doc = getJavaDoc();
        fprintf(fp, "%s", doc.c_str());
        std::string attrs = getJavaAttrs();
        std::string args = getJavaArgs();
        std::string rt = getJavaReturnType();
        fprintf(fp, "    %s%s %s(%s) throws Throwable {\n", attrs.c_str(), rt.c_str(), name.c_str(), args.c_str());
        fputs("    }\n\n", fp);
    }

    void serializeStaticJavadocMethod(FILE* fp, const char* cname) const {
        serializeQorePrototypeComment(fp, cname, 4);

        std::string doc = getJavaDoc();
        fprintf(fp, "%s", doc.c_str());
        std::string attrs = getJavaAttrs();
        std::string args = getJavaArgs();
        std::string rt = getJavaReturnType();
        fprintf(fp, "    static %s%s %s(%s) throws Throwable {\n", attrs.c_str(), rt.c_str(), name.c_str(),
            args.c_str());
        fputs("    }\n\n", fp);
    }

    void serializeNormalCppMethod(FILE* fp, const char* cname, const char* arg) const {
        assert(!(attr & QCA_STATIC));

        if (doconly)
            return;

        if (name == "constructor") {
            serializeCppConstructor(fp, cname);
            return;
        }
        if (name == "destructor") {
            serializeCppDestructor(fp, cname, arg);
            return;
        }
        if (name == "copy") {
            serializeCppCopy(fp, cname, arg);
            return;
        }

        if (attr & QCA_ABSTRACT)
            return;

        serializeQorePrototypeComment(fp, cname);

        fprintf(fp, "static %s %s_%s(QoreObject* self, %s, const QoreListNode* args, RuntimeConfig& runtime_cfg, " \
            "ExceptionSink* xsink) {\n", getReturnType(), cname, vname.c_str(), arg);

        serializeArgs(fp, cname);
        if (!pseudo_arg.empty()) {
            bool found = false;
            // see if the pseudo arg is referenced
            size_t i = 0;
            //log(LL_DEBUG, "code: %s var: %s\n", code.c_str(), pseudo_var.c_str());
            while (true) {
                i = code.find(pseudo_var, i);
                if (i == std::string::npos)
                    break;
                //log(LL_DEBUG, "testing %c and %c (%s)\n", code[i - 1], code[i + pseudo_var.size()], pseudo_var.c_str());
                if ((!i || !idchar(code[i - 1])) && ((i + pseudo_var.size() == code.size())
                                                     || !idchar(code[i + pseudo_var.size()]))) {
                    found = true;
                    break;
                }
                ++i;
                if (i == code.size())
                    break;
            }
            if (found)
                fprintf(fp, "    %s;\n", pseudo_arg.c_str());
        }
        fprintf(fp, "# %d \"%s\"\n    ", line, fileName.c_str());
        output_file(fp, code);

        if (!has_return)
            fprintf(fp, "\n    return QoreValue();");

        fputs("\n}\n\n", fp);
    }

    int serializeNormalCppBinding(FILE* fp, const char* cname, const char* UC) const {
        if (doconly)
            return 0;

        if (name == "constructor")
            return serializeCppConstructorBinding(fp, cname, UC);

        if (name == "destructor")
            return serializeCppDestructorBinding(fp, cname, UC);

        if (name == "copy")
            return serializeCppCopyBinding(fp, cname, UC);

        fputc('\n', fp);
        serializeQorePrototypeComment(fp, cname, 4);

        // get return type
        std::string cppt;
        if (get_qore_type(return_type, cppt))
            return -1;

        fprintf(fp, "    QC_%s->", UC);
        if (attr & QCA_ABSTRACT)
            fprintf(fp, "addAbstractMethod(\"%s\", %s, ", name.c_str(), get_access(attr));
        else
            fprintf(fp, "%s(\"%s\", (%s)%s_%s, %s, ", "addMethod",
                    name.c_str(), getMethodType(), cname, vname.c_str(), get_access(attr));

        flags_output_cpp(fp, flags, attr & QCA_USES_EXTRA_ARGS);
        fputs(", ", fp);
        if (!(attr & QCA_ABSTRACT)) {
            dom_output_cpp(fp, dom);
            fprintf(fp, ", ");
        }
        fprintf(fp, "%s", cppt.c_str());

        // Use vector-based arguments for abstract methods to avoid varargs ABI issues
        // with NaN-boxed QoreValue across shared library boundaries
        if (attr & QCA_ABSTRACT) {
            if (serializeBindingArgsVectors(fp))
                return -1;
        } else {
            if (serializeBindingArgs(fp))
                return -1;
        }

        fputs(");\n", fp);

        return 0;
    }

    void serializeStaticCppMethod(FILE* fp, const char* cname, const char* arg) const {
        assert(attr & QCA_STATIC);

        if (doconly)
            return;

        serializeQorePrototypeComment(fp, cname);

        fprintf(fp, "static %s static_%s_%s(const QoreListNode* args, RuntimeConfig& runtime_cfg, ExceptionSink* xsink) " \
            "{\n", getReturnType(), cname, vname.c_str());
        serializeArgs(fp, cname);
        fprintf(fp, "# %d \"%s\"\n", line, fileName.c_str());
        output_file(fp, code);

        if (!has_return)
            fprintf(fp, "\n    return QoreValue();");

        fputs("\n}\n\n", fp);
    }

    int serializeStaticCppBinding(FILE* fp, const char* cname, const char* UC) const {
        assert(attr & QCA_STATIC);

        if (doconly)
            return 0;

        fputc('\n', fp);
        serializeQorePrototypeComment(fp, cname, 4);

        // get return type
        std::string cppt;
        if (get_qore_type(return_type, cppt))
            return -1;

        fprintf(fp, "    QC_%s->addStaticMethod(\"%s\", (%s)static_%s_%s, %s, ", UC, name.c_str(), getFunctionType(), cname, vname.c_str(), get_access(attr));
        flags_output_cpp(fp, flags, attr & QCA_USES_EXTRA_ARGS);
        fputs(", ", fp);
        dom_output_cpp(fp, dom);
        fprintf(fp, ", %s", cppt.c_str());

        if (serializeBindingArgs(fp))
            return -1;

        fputs(");\n", fp);

        return 0;
    }

    int serializeDox(FILE* fp) {
        if (attr & QCA_PRIVATE)
            fputs("\nprotected:\n", fp);
        else if (attr & QCA_PRIVATE_INTERNAL)
            fputs("\nprivate:\n", fp);
        else
            fputs("\npublic:\n", fp);

        serialize_dox_comment(fp, docs, dom, flags);

        //fputs("    ", fp);
        if (attr & QCA_STATIC)
            fputs("static ", fp);
        else if (attr & QCA_ABSTRACT)
            fputs("abstract ", fp);

        serializeQoreCppType(fp, return_type);
        fprintf(fp, " %s(", name.c_str());

        for (unsigned i = 0; i < params.size(); ++i) {
            if (i)
                fputs(", ", fp);
            const Param& p = params[i];
            serializeQoreCppType(fp, p.type);
            fputc(' ', fp);
            fputs(p.name.c_str(), fp);
            if (!p.val.empty()) {
                fputs(" = ", fp);
                std::string qv;
                if (get_dox_value(p.val, qv))
                    return -1;
                fputs(qv.c_str(), fp);
            }
        }
        fputs(");\n", fp);
        return 0;
    }
};

class ClassElement : public AbstractElement, protected NamespaceElement {
protected:
    typedef std::multimap<std::string, Method*> mmap_t;

    std::string fileName,          // file name
        name,                      // class name
        doc,                       // doc string
        arg,                       // argument for non-static methods
        defbase,                   // default builtin base class
        scons,                     // system constructor
        serializer,                // class serializer function
        deserializer;              // class deserializer function

    strlist_t vparents;            // builtin virtual base/parent classes

    paramlist_t public_members;    // public members
    paramlist_t private_members;   // private members
    paramlist_t internal_members;  // internal members

    strlist_t dom;                 // functional domains

    mmap_t normal_mmap,            // normal method map
        static_mmap;               // static method map

    bool valid = true,
        upm = false,               // unset public member flag
        is_pseudo = false,
        is_final = false,
        module_public = true;

    void addElement(strlist_t& l, const std::string& str, size_t start, size_t end = std::string::npos) {
        std::string se(str, start, end);
        trim(se);
        l.push_back(se);
    }

    void parseMembers(ClassAccess access, const std::string& x) {
        strlist_t pml;
        if (get_string_list(pml, x, ',', true)) {
            valid = false;
            return;
        }

        for (unsigned i = 0; i < pml.size(); ++i) {
            bool is_static = false;
            if (pml[i].rfind("static ", 0) == 0) {
                is_static = true;
                pml[i].erase(0, 7);
            }

            size_t ab = pml[i].rfind('>');
            if (ab == std::string::npos) {
                ab = 0;
            } else {
                ++ab;
            }

            size_t p = pml[i].find(' ', ab);
            if (p == std::string::npos) {
                error("class '%s' has member without type: '%s'\n", name.c_str(), pml[i].c_str());
                valid = false;
                continue;
            }
            std::string type(pml[i], 0, p);
            std::string name(pml[i], p + 1);

            if (access == Public) {
                public_members.push_back(Param(type, name, "", "", is_static));
            } else if (access == Private) {
                private_members.push_back(Param(type, name, "", "", is_static));
            } else {
                internal_members.push_back(Param(type, name, "", "", is_static));
            }
        }
    }

public:
    ClassElement(const char* fn, const std::string& n_name, const strmap_t& props, const std::string& n_doc) :
        fileName(fn), name(n_name), doc(n_doc) {
        log(LL_DETAIL, "parsing Qore class '%s'\n", name.c_str());

        if (name.size() && name[0] == '<' && name[name.size() - 1] == '>')
            is_pseudo = true;

        // process properties
        for (strmap_t::const_iterator i = props.begin(), e = props.end(); i != e; ++i) {
            log(LL_DEBUG, "+ prop: '%s': '%s'\n", i->first.c_str(), i->second.c_str());

            // parse domain
            if (i->first == "dom") {
                if (dom_get(dom, i->second))
                    valid = false;
                continue;
            }

            if (i->first == "arg") {
                arg = i->second;
                log(LL_DEBUG, "+ arg: %s\n", arg.c_str());
                continue;
            }

            if (i->first == "flags") {
                strlist_t l;
                get_string_list(l, i->second);

                for (unsigned i = 0; i < l.size(); ++i) {
                    if (l[i] == "unsetPublicMemberFlag") {
                        upm = true;
                        continue;
                    }
                    if (l[i] == "final") {
                        is_final = true;
                        continue;
                    }
                    error("class '%s' has unknown flag: '%s'\n", name.c_str(), l[i].c_str());
                    valid = false;
                }
                continue;
            }

            if (i->first == "public_members") {
                parseMembers(Public, i->second);
                continue;
            }

            if (i->first == "private_members") {
                parseMembers(Private, i->second);
                continue;
            }

            if (i->first == "internal_members") {
                parseMembers(Internal, i->second);
                continue;
            }

            if (i->first == "system_constructor") {
                scons = i->second;
                log(LL_DEBUG, "+ system_constructor: %s\n", scons.c_str());
                continue;
            }

            if (i->first == "ns") {
                ns = i->second;
                log(LL_DEBUG, "+ namespace: %s\n", ns.c_str());
                continue;
            }

            if (i->first == "vparent") {
                get_string_list(vparents, i->second);
                for (unsigned i = 0; i < vparents.size(); ++i) {
                    log(LL_DEBUG, "+ virtual base class: %s\n", vparents[i].c_str());
                    if (is_pseudo) {
                        size_t ps = vparents[i].size();
                        if (!ps || vparents[i][0] != '<' || vparents[i][ps - 1] != '>') {
                            error("virtual parent '%s' is invalid for a pseudo-class (must be '<name>')\n", vparents[i].c_str());
                            valid = false;
                        }
                        else if (vparents.size() > 1) {
                            error("pseudo-classes can only have 1 parent class\n");
                            valid = false;
                        }
                    }
                }
                continue;
            }

            if (i->first == "default_base") {
                defbase = i->second;
                log(LL_DEBUG, "+ default builtin base class: %s\n", defbase.c_str());
                continue;
            }

            if (i->first == "serializer") {
                serializer = i->second;
                log(LL_DEBUG, "+ serializer function: %s\n", serializer.c_str());
                continue;
            }

            if (i->first == "deserializer") {
                deserializer = i->second;
                log(LL_DEBUG, "+ deserializer function: %s\n", deserializer.c_str());
                continue;
            }

            if (i->first == "module_public") {
                if (i->second == "false") {
                    module_public = false;
                } else if (i->second != "true") {
                    error("prop: '%s': '%s' - must be \"true\" or \"false\"\n", i->first.c_str(), i->second.c_str());
                }
                continue;
            }

            error("prop: '%s': '%s' - unknown property '%s'\n", i->first.c_str(), i->second.c_str(), i->first.c_str());
            valid = false;
        }

        if (arg.empty() && !is_pseudo) {
            valid = false;
            error("class '%s' has no 'arg' property\n", name.c_str());
        }
    }

    virtual ~ClassElement() {
        for (mmap_t::iterator i = normal_mmap.begin(), e = normal_mmap.end(); i != e; ++i) {
            delete i->second;
        }

        for (mmap_t::iterator i = static_mmap.begin(), e = static_mmap.end(); i != e; ++i) {
            delete i->second;
        }
    }

    operator bool() const {
        return valid;
    }

    const char* getName() const {
        return name.c_str();
    }

    int addMethod(const std::string& mname, attr_t attr, const std::string& return_type, const paramlist_t& params,
            const strmap_t& flags, const std::string& code, const std::string& doc, unsigned line) {
        log(LL_DETAIL, "adding method %s%s'%s'::'%s'()\n", return_type.c_str(), return_type.empty() ? "" : " ",
            name.c_str(), mname.c_str());

        strlist_t cf;
        strlist_t dom;

        bool doconly = false;
        // parse flags
        for (strmap_t::const_iterator i = flags.begin(), e = flags.end(); i != e; ++i) {
            //log(LL_DEBUG, "+ method %s::%s() flag '%s': '%s'\n", name.c_str(), mname.c_str(), i->first.c_str(), i->second.c_str());
            if (i->first == "dom") {
                if (dom_get(dom, i->second))
                    return -1;
            } else if (i->first == "flags") {
                if (flags_get(cf, i->second))
                    return -1;
            } else if (i->first == "doconly")
                doconly = true;
            else {
                error("unknown flag '%s' = '%s' defining method %s::%s()\n", i->first.c_str(), i->second.c_str(), name.c_str(), mname.c_str());
                return -1;
            }
        }

        //log(LL_INFO, "+ method %s::%s() attr: 0x%x (static: %d)\n", name.c_str(), mname.c_str(), attr, attr & QCA_STATIC);

        if ((attr & QCA_STATIC) && is_pseudo) {
            error("pseudo class '%s' cannot define static method '%s'\n", name.c_str(), mname.c_str());
            return -1;
        }

        mmap_t& mmap = (attr & QCA_STATIC) ? static_mmap : normal_mmap;

        Method* m = new Method(fileName, mname, attr, params, doc, return_type, cf, dom, code, line, doconly, is_pseudo ? arg.c_str() : 0);
        mmap.insert(mmap_t::value_type(mname, m));
        return !*m;
    }

    virtual strlist_t precalculateUnitTest() {
        strlist_t ret;

        std::string v = "test_" + (ns.empty()? "Qore" : ns) + "_" + name;
        ret.push_back(v);

        return ret;
    }

    virtual int serializeUnitTest(FILE* fp) {
        //fprintf(fp, "# ClassElement::serializeUnitTest\n");
        fprintf(fp, "    # Qore class %s::%s\n", ns.empty()? "Qore" : ns.c_str(), name.c_str());
        strlist_t names = precalculateUnitTest();
        fprintf(fp, "    private %s() {\n", names[0].c_str());

        for (mmap_t::const_iterator i = normal_mmap.begin(), e = normal_mmap.end(); i != e; ++i) {
            i->second->serializeUnitTestMethod(fp, name.c_str());
        }

        for (mmap_t::const_iterator i = static_mmap.begin(), e = static_mmap.end(); i != e; ++i) {
            i->second->serializeUnitTestStatic(fp, name.c_str());
        }

        fprintf(fp, "        assertEq(True, False, \"assertion placeholder; the real test needs to be implemented\");\n");
        fprintf(fp, "    }\n\n");
        return 0;
    }

    virtual int serializeJavadoc() {
        // do not serialize pseudo classes
        if (is_pseudo || opts.javadoc_fn.empty()) {
            return 0;
        }

        std::string pkg;
        FILE* fp = get_java_file(ns, name, pkg);

        fprintf(fp, "// Java created from Qore class %s::%s\n", ns.empty() ? "Qore" : ns.c_str(), name.c_str());

        fprintf(fp, "package qore.%s;\n\n", pkg.c_str());

        // output class docs
        std::string javadoc = get_java_doc(doc);
        fprintf(fp, "%s", javadoc.c_str());

        // get parent class, if any
        std::string parent;
        if (!defbase.empty()) {
            parent = get_java_type_name(defbase, ns);
        } else if (!vparents.empty()) {
            parent = get_java_type_name(vparents[0], ns);
        }

        if (!parent.empty()) {
            parent = " extends " + parent;
        }

        fprintf(fp, "public class %s%s {\n", name.c_str(), parent.c_str());

        for (auto& i : normal_mmap) {
            i.second->serializeNormalJavadocMethod(fp, name.c_str());
        }

        for (auto& i : static_mmap) {
            i.second->serializeStaticJavadocMethod(fp, name.c_str());
        }

        fprintf(fp, "}\n");
        return 0;
    }

    virtual int serializeCpp(FILE* fp) {
        fprintf(fp, "/* Qore class %s::%s */\n\n", ns.empty()? "Qore" : ns.c_str(), name.c_str());

        std::string ns_path;
        if (ns.empty()) {
            ns_path = "::Qore::";
            ns_path += name;
        } else {
            if (ns[0] == ':') {
                ns_path = ns;
            } else {
                ns_path = "::";
                ns_path += ns;
            }
            ns_path += "::";
            ns_path += name;
        }

        std::string UC;
        for (unsigned i = 0; i < name.size(); ++i)
            UC += toupper(name[i]);

        std::string lname = name;
        if (is_pseudo) {
            size_t nl = name.size();
            UC.erase(nl - 1);
            UC.erase(0, 1);
            UC.insert(0, "PSEUDO");

            lname.erase(nl - 1);
            char c = lname[1];
            lname.erase(0, 2);
            lname.insert((unsigned)0, (unsigned)1, (char)toupper(c));
            lname.insert(0, "Pseudo");
        }

        if (upm) {
            fprintf(fp, "#include \"qore/intern/QoreClassIntern.h\"\n");
        }

        fprintf(fp, "qore_classid_t CID_%s;\nQoreClass* QC_%s;\n\n", UC.c_str(), UC.c_str());

        for (mmap_t::const_iterator i = normal_mmap.begin(), e = normal_mmap.end(); i != e; ++i) {
            i->second->serializeNormalCppMethod(fp, lname.c_str(), is_pseudo ? "const QoreValue& v" : arg.c_str());
        }

        for (mmap_t::const_iterator i = static_mmap.begin(), e = static_mmap.end(); i != e; ++i) {
            i->second->serializeStaticCppMethod(fp, lname.c_str(), arg.c_str());
        }

        fprintf(fp, "DLLLOCAL void preinit%sClass() {\n    QC_%s = new QoreBuiltinClass(\"%s\", \"%s\", ", lname.c_str(),
            UC.c_str(), name.c_str(), ns_path.c_str());
        dom_output_cpp(fp, dom);
        fprintf(fp, ");\n    CID_%s = QC_%s->getID();\n", UC.c_str(), UC.c_str());
        fprintf(fp, "    QC_%s->setSystem();\n", UC.c_str());
        if (!module_public) {
            fprintf(fp, "    reinterpret_cast<QoreBuiltinClass*>(QC_%s)->setModulePublic(false);\n", UC.c_str());
        }
        fprintf(fp, "}\n\n");

        if (is_pseudo)
            fprintf(fp, "DLLLOCAL QoreClass* init%sClass() {\n", lname.c_str());
        else
            fprintf(fp, "DLLLOCAL QoreClass* init%sClass(QoreNamespace& ns) {\n", lname.c_str());
        fprintf(fp, "    if (!QC_%s)\n        preinit%sClass();\n", UC.c_str(), lname.c_str());

        if (!defbase.empty()) {
            fprintf(fp, "\n    // set default builtin base class\n    assert(%s);\n    "
                "QC_%s->addDefaultBuiltinBaseClass(%s);\n", defbase.c_str(), UC.c_str(), defbase.c_str());
        }

        if (!serializer.empty()) {
            fprintf(fp, "\n    // set serializer function\n    QC_%s->setSerializer(%s);\n", UC.c_str(), serializer.c_str());
        }

        if (!deserializer.empty()) {
            fprintf(fp, "\n    // set deserializer function\n    QC_%s->setDeserializer(%s);\n", UC.c_str(), deserializer.c_str());
        }

        for (unsigned i = 0; i < vparents.size(); ++i) {
            std::string vp;
            if (is_pseudo)
                vp.assign(vparents[i], 1, vparents[i].size() - 2);
            else
                get_type_name(vp, vparents[i]);
            toupper(vp);
            if (is_pseudo)
                vp.insert(0, "PSEUDO");
            fprintf(fp, "\n    // set parent class\n    assert(QC_%s);\n    QC_%s->addBuiltinVirtualBaseClass(QC_%s);\n", vp.c_str(), UC.c_str(), vp.c_str());
        }

        add_init_code(fp);

        // set system constructor if any
        if (!scons.empty())
            fprintf(fp, "\n    // set system constructor\n    QC_%s->setSystemConstructor(%s);\n", UC.c_str(), scons.c_str());

        // output public members if any
        if (!public_members.empty()) {
            fputs("\n    // public members\n", fp);
            for (paramlist_t::iterator i = public_members.begin(), e = public_members.end(); i != e; ++i) {
                std::string mt;
                get_qore_type((*i).type, mt);
                if ((*i).is_static) {
                    fprintf(fp, "    QC_%s->addBuiltinStaticVar(\"%s\", QoreValue(), Public, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                } else {
                    fprintf(fp, "    QC_%s->addMember(\"%s\", Public, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                }
            }
        }
        // output private members if any
        if (!private_members.empty()) {
            fputs("\n    // private members\n", fp);
            for (paramlist_t::iterator i = private_members.begin(), e = private_members.end(); i != e; ++i) {
                std::string mt;
                get_qore_type((*i).type, mt);
                if ((*i).is_static) {
                    fprintf(fp, "    QC_%s->addBuiltinStaticVar(\"%s\", QoreValue(), Private, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                } else {
                    fprintf(fp, "    QC_%s->addMember(\"%s\", Private, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                }
            }
        }
        // output internal members if any
        if (!internal_members.empty()) {
            fputs("\n    // private:internal members\n", fp);
            for (paramlist_t::iterator i = internal_members.begin(), e = internal_members.end(); i != e; ++i) {
                std::string mt;
                get_qore_type((*i).type, mt);
                if ((*i).is_static) {
                    fprintf(fp, "    QC_%s->addBuiltinStaticVar(\"%s\", QoreValue(), Internal, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                } else {
                    fprintf(fp, "    QC_%s->addMember(\"%s\", Internal, %s);\n", UC.c_str(), (*i).name.c_str(), mt.c_str());
                }
            }
        }

        if (upm)
            fprintf(fp, "\n    qore_class_private::get(*QC_%s)->unsetPublicMemberFlag();\n", UC.c_str());

        if (is_final)
            fprintf(fp, "\n    QC_%s->setFinal();\n", UC.c_str());

        for (mmap_t::const_iterator i = normal_mmap.begin(), e = normal_mmap.end(); i != e; ++i) {
            if (i->second->serializeNormalCppBinding(fp, lname.c_str(), UC.c_str())) {
                error("error processing bindings for %s::%s()\n", name.c_str(), i->second->getName());
                valid = false;
                return -1;
            }
        }

        for (mmap_t::const_iterator i = static_mmap.begin(), e = static_mmap.end(); i != e; ++i) {
            if (i->second->serializeStaticCppBinding(fp, lname.c_str(), UC.c_str())) {
                error("error processing bindings for static method %s::%s()\n", name.c_str(), i->second->getName());
                valid = false;
                return -1;
            }
        }

        // only serialize constants if we are not emitting a pseudo class
        if (!is_pseudo && groups.serializeCppConstantBindings(fp))
            return -1;

        fprintf(fp, "\n    return QC_%s;\n}\n", UC.c_str());
        return 0;
    }

    virtual int serializeDox(FILE* fp) {
        if (ns.empty())
            fputs("\n//! main Qore-language namespace\nnamespace Qore {\n", fp);
        else {
            outputNamespaceStart(fp);
        }

        serialize_dox_comment(fp, doc, dom);

        size_t nl = name.size();
        if (name[0] == '<' && name[nl - 1] == '>') {
            std::string nn = "zzz8";
            nn.append(name, 1, nl - 2);
            nn += "zzz9";
            fprintf(fp, "class %s", nn.c_str());
        }
        else
            fprintf(fp, "class %s", name.c_str());

        if (!vparents.empty()) {
            fprintf(fp, " : ");
            for (unsigned i = 0; i < vparents.size(); ++i) {
                if (!is_pseudo) {
                    fputs("public ", fp);
                    fputs(vparents[i].c_str(), fp);
                    if (i != (vparents.size() - 1))
                        fputs(", ", fp);
                }
                else {
                    std::string vp;
                    vp.assign(vparents[i], 1, vparents[i].size() - 2);
                    fprintf(fp, "public zzz8%szzz9", vp.c_str());
                }
            }
        }

        fputs(" {\n", fp);

        for (mmap_t::const_iterator i = normal_mmap.begin(), e = normal_mmap.end(); i != e; ++i)
            i->second->serializeDox(fp);

        for (mmap_t::const_iterator i = static_mmap.begin(), e = static_mmap.end(); i != e; ++i)
            i->second->serializeDox(fp);

        fputs("};\n", fp);

        groups.serializeConstantDox(fp);

        outputNamespaceEnd(fp);
        return 0;
    }
};

typedef std::map<std::string, ClassElement*> cemap_t;

typedef std::vector<AbstractElement*> source_t;

class Code {
protected:
    const char* fileName;
    std::string cppFileName, doxFileName, rootName, unitTestFileName;
    // argument to fopen()
    const char* cpp_open_flag,* dox_open_flag;
    unsigned lineNumber;
    source_t source;
    // class element map
    cemap_t cemap;
    bool valid, has_class;      // has at least 1 class element
    // preprocessor state for #ifdef/#ifndef/#if/#elif/#else/#endif
    PreprocessorState ppState;

    void checkBuf(std::string& buf) {
        if (!buf.empty()) {
            bool ws = true;
            for (unsigned i = 0, e = buf.size(); i < e; ++i) {
                if (buf[i] != '\n') {
                    ws = false;
                    break;
                }
            }
            if (ws) {
                buf.clear();
                return;
            }

            source.push_back(new TextElement(buf));
            buf.clear();
        }
    }

    int parse() {
        FILE* fp = fopen(fileName, "r");
        if (!fp) {
            error("%s: %s\n", fileName, strerror(errno));
            return -1;
        }

        lineNumber = 1;

        // Initialize preprocessor state
        ppState.setFileName(fileName);
        ppState.initFromCommandLine();

        int rc = 0;

        std::string str;
        std::string buf;
        while (true) {
            str.clear();
            read_line(lineNumber, str, fp);

            log(LL_DEBUG, "%d: %s", lineNumber, str.c_str());

            if (str.empty())
                break;

            // Handle preprocessor directives
            if (!str.empty() && str[0] == '#') {
                if (ppState.handleDirective(str, lineNumber, buf)) {
                    continue;
                }
                // Not a recognized/tracked directive - add to buf and continue
                // This handles unknown #ifdef/#endif macros as pass-through to C++
                buf += str;
                continue;
            }

            // Skip content in inactive conditional blocks
            if (ppState.isSkipping()) {
                buf += str;  // Keep the raw text for pass-through to C++
                continue;
            }

            if (!str.compare(0, 13, "/** @defgroup")) {
                checkBuf(buf);

                if (groups.add(new Group(str, fp, fileName, lineNumber, ppState))) {
                    rc = -1;
                    break;
                }
                continue;
            }

            if (!str.compare(0, 3, "//!")) {
                if (!str.compare(0, 14, "//! @qppinit: ")) {
                    str.erase(0, 14);
                    log(LL_DEBUG, "got qppinit: %s", str.c_str());

                    initcode += str;
                    continue;
                }

                if (!str.compare(0, 13, "//! @qppval: ")) {
                    str.erase(0, 13);

                    size_t i = str.find('=');
                    if (i == std::string::npos) {
                        error("%s:%d: qppval has no '=' sign for key assignment: %s", fileName, lineNumber, str.c_str());
                        rc = -1;
                        break;
                    }
                    std::string key(str, 0, i);
                    trim(key);
                    std::string val(str, i + 1);
                    trim(val);

                    log(LL_DEBUG, "got qppval: '%s' = '%s'\n", key.c_str(), val.c_str());

                    if (qppval.find(key) != qppval.end()) {
                        error("%s:%d: qppval '%s' given twice\n", fileName, lineNumber, key.c_str());
                        rc = -1;
                        break;
                    }

                    qppval[key] = val;
                    continue;
                }

                if (get_dox_comment(fileName, lineNumber, str, fp)) {
                    rc = -1;
                    break;
                }

                std::string sc;
                if (read_line(lineNumber, sc, fp)) {
                    error("%s:%d: premature EOF reading code signature line\n", fileName, lineNumber);
                    rc = -1;
                    break;
                }
                //log(LL_DEBUG, "SC: %s", sc.c_str());

                if (!sc.compare(0, 9, "hashdecl ")) {
                    sc.erase(0, 9);
                    checkBuf(buf);
                    source.push_back(new HashDecl(std::move(str), sc, fp, fileName, lineNumber));
                    continue;
                }

                if (!sc.compare(0, 5, "enum ")) {
                    sc.erase(0, 5);
                    checkBuf(buf);
                    source.push_back(new Enum(std::move(str), sc, fp, fileName, lineNumber));
                    continue;
                }

                if (!strncmp(sc.c_str(), "qclass ", 7)) {
                    const char* p = sc.c_str() + 7;
                    while (*p && *p == ' ')
                        ++p;
                    if (!*p) {
                        error("%s:%d: premature EOF reading class header line\n", fileName, lineNumber);
                        rc = -1;
                        break;
                    }
                    const char* p1 = p;
                    while (*p1 && *p1 != ' ')
                        ++p1;

                    std::string cn(p, p1 - p);

                    // get class properties
                    p = strchr(sc.c_str(), '[');
                    if (p) {
                        size_t start = p - sc.c_str();
                        const char* p0 = p;
                        p1 = strchr(p0 + 1, ']');
                        while (true) {
                            if (p1) {
                                break;
                            }
                            size_t offset = sc.size();
                            if (read_line(lineNumber, sc, fp)) {
                                error("%s:%d: premature EOF reading class header line\n", fileName, lineNumber);
                                rc = -1;
                                break;
                            }
                            // reset '[' position in potentially reallocated buffer
                            p = sc.c_str() + start;
                            p1 = strchr(sc.c_str() + offset, ']');
                        }
                        if (rc == -1) {
                            break;
                        }
                    } else {
                        p1 = nullptr;
                    }
                    //p1 = p ? strchr(p + 1, ']') : 0;
                    if (!p1) {
                        error("%s:%d: missing class properties ('[...]') reading class header line\n", fileName, lineNumber);
                        rc = -1;
                        break;
                    }

                    std::string propstr(p + 1, p1 - p - 1);

                    // parse properties
                    strmap_t props;
                    if (parse_properties(fileName, lineNumber, propstr, props)) {
                        rc = -1;
                        break;
                    }

                    ClassElement* ce = new ClassElement(fileName, cn, props, str);
                    cemap[cn] = ce;
                    checkBuf(buf);
                    source.push_back(ce);
                    // mark code as invalid if class element is invalid
                    if (!*ce)
                        valid = false;
                    has_class = true;
                    continue;
                }

                const char* p0;
                if ((p0 = strstr(sc.c_str(), "::"))) {
                    trim_end(sc);
                    if (sc[sc.size() - 1] == ';') {
                        if (parseMethod(sc, fp, str, true)) {
                            valid = false;
                            break;
                        }
                        continue;
                    }
                    // issue #4366: read in multi-line signature
                    const char* p1;
                    while (true) {
                        // if we have no args or we have a block start, then stop reading
                        if (!(p1 = strchr(p0 + 1, '(')) || strchr(p1 + 1, '{')) {
                            break;
                        }

                        // read in a new line and add it to the signature
                        std::string sc1;
                        if (read_line(lineNumber, sc1, fp)) {
                            error("%s:%d: premature EOF reading code signature lines\n", fileName, lineNumber);
                            rc = -1;
                            break;
                        }
                        sc += sc1;
                        p0 = strstr(sc.c_str(), "::");
                    }
                    if (rc == -1) {
                        break;
                    }
                    if (strchr(sc.c_str(), '{')) {
                        if (parseMethod(sc, fp, str)) {
                            valid = false;
                            break;
                        }
                        continue;
                    }
                }

                continue;
            }

            buf += str;
        }

        checkBuf(buf);

        // Check for unclosed preprocessor conditionals
        if (ppState.hasUnclosedConditions()) {
            error("%s: unclosed preprocessor conditionals: %s\n", fileName, ppState.getUnclosedConditionInfo().c_str());
            rc = -1;
        }

        fclose(fp);

        return rc;
    }

    int parseMethod(std::string& sc, FILE* fp, std::string& doc, bool abstract = false) {
        unsigned sl = lineNumber;
        if (!abstract) {
            if (read_until_close(fileName, lineNumber, sc, fp)) {
                error("%s:%d: premature EOF reading method definition starting on line %d\n", fileName, lineNumber, sl);
                return -1;
            }
        }

        size_t i = sc.find("::");
        assert(i != std::string::npos);

        // save position of the '::'
        size_t p = i;

        if (!i) {
            error("%s:%d: missing class name in method definition", fileName, lineNumber);
            return -1;
        }
        // find beginning of class name
        while (i && sc[i - 1] != ' ')
            --i;

        // get class name
        std::string cn(sc, i, p - i);

        cemap_t::iterator ci = cemap.find(cn);
        if (ci == cemap.end()) {
            error("%s:%d: reference to undefined class '%s'\n", fileName, lineNumber, cn.c_str());
            return -1;
        }
        // method attributes
        attr_t attr = QCA_NONE;
        // return type
        std::string return_type;

        if (i && parse_attributes(fileName, lineNumber, attr, return_type, sc, i))
            return -1;

        if (abstract && !(attr & QCA_ABSTRACT)) {
            error("%s:%d: non-abstract method with no implementation for class '%s'\n", fileName, lineNumber, cn.c_str());
            return -1;
        }
        // get method name
        p += 2;
        i = p;
        while (i < sc.size() && sc[i] != ' ' && sc[i] != '(')
            ++i;

        if (i == sc.size()) {
            error("%s:%d: premature EOL reading method definition in class %s\n", fileName, lineNumber, cn.c_str());
            return -1;
        }

        std::string mn(sc, p, i - p);

        p = sc.find('(', i);
        if (p == std::string::npos) {
            error("%s:%d: premature EOL reading parameters for %s::%s()\n", fileName, lineNumber, cn.c_str(), mn.c_str());
            return -1;
        }
        ++p;

        strmap_t flags;
        paramlist_t params;

        std::string dn(cn);
        dn += "::";
        dn += mn;

        if (parse_params_and_flags(fileName, lineNumber, flags, params, attr, sc, p, dn, abstract))
            return -1;

        return ci->second->addMethod(mn, attr, return_type, params, flags, sc, doc, sl);
    }

public:
    Code(const char* fn, bool cpp_append = false, bool dox_append = false) : fileName(fn),
        cpp_open_flag(cpp_append ? "a" : "w"),
        dox_open_flag(dox_append ? "a" : "w"), lineNumber(0), valid(true), has_class(false) {
        std::string base;
        std::string dir;

        std::string bnc(fn);
        base = basename((char*)bnc.c_str());
        bnc = fn;
        dir = dirname((char*)bnc.c_str());

        if (base.size() > 4 && !strcmp(base.c_str() + base.size() - 4, ".qpp")) {
            base.erase(base.size() - 4);
        } else {
            warning("'%s' does not end in extension '.qpp'\n", fn);
            size_t d = base.rfind('.');
            if (d != std::string::npos)
                base.erase(d);
        }

        cppFileName = !opts.output_fn.empty() ? opts.output_fn : dir + "/" + base + ".cpp";
        doxFileName = !opts.dox_fn.empty() ? opts.dox_fn : dir + "/" + base + ".dox.h";
        if (!opts.unit_test_fn.empty()) {
            unitTestFileName = opts.unit_test_fn;
        }

        if (parse())
            valid = false;

        size_t i = base.find('_');
        if ((i == std::string::npos) || (i == (base.size() - 1)))
            rootName = base;
        else
            rootName.assign(base, i + 1, -1);
    }

    ~Code() {
        for (source_t::iterator i = source.begin(), e = source.end(); i != e; ++i)
            delete *i;
    }

    int serializeUnitTest() {
        if (unitTestFileName.empty()) {
            return 0;
        }
        FILE* fp = fopen(unitTestFileName.c_str(), cpp_open_flag);
        if (!fp) {
            error("%s: %s\n", unitTestFileName.c_str(), strerror(errno));
            return -1;
        }
        log(LL_INFO, "creating unit test file %s -> %s\n", fileName, unitTestFileName.c_str());

        fprintf(fp, "#!/usr/bin/env qore\n");
        fprintf(fp, "# -*- mode: qore; indent-tabs-mode: nil -*-\n");
        fprintf(fp, "\n");
        fprintf(fp, "%%new-style\n");
        fprintf(fp, "%%enable-all-warnings\n");
        fprintf(fp, "%%require-types\n");
        fprintf(fp, "%%strict-args\n");
        fprintf(fp, "\n");
        fprintf(fp, "%%requires ../../../qlib/QUnit.qm\n");
        fprintf(fp, "\n");
        fprintf(fp, "%%exec-class Main\n");
        fprintf(fp, "\n");
        fprintf(fp, "class Main inherits QUnit::Test {\n");
        fprintf(fp, "\n");
        fprintf(fp, "    constructor() : Test(\"Main\", \"1.0\", \\ARGV) {\n");
        fprintf(fp, "\n");

        for (source_t::const_iterator i = source.begin(), e = source.end(); i != e; ++i) {
            strlist_t names = (*i)->precalculateUnitTest();
            for (unsigned i = 0; i < names.size(); i++) {
                fprintf(fp, "        addTestCase(\"%s\", \\%s());\n", names[i].c_str(), names[i].c_str());
            }
        }

        strlist_t gnames = groups.precalculateUnitTest();
        for (unsigned i = 0; i < gnames.size(); i++) {
            fprintf(fp, "        addTestCase(\"%s\", \\%s());\n", gnames[i].c_str(), gnames[i].c_str());
        }

        fprintf(fp, "\n");
        fprintf(fp, "        # Return for compatibility with test harness that checks return value.\n");
        fprintf(fp, "        set_return_value(main());\n");
        fprintf(fp, "    }\n");
        fprintf(fp, "\n");

        for (source_t::const_iterator i = source.begin(), e = source.end(); i != e; ++i) {
            if ((*i)->serializeUnitTest(fp)) {
                fclose(fp);
                return -1;
            }
        }

        if (groups.serializeUnitTest(fp)) {
            fclose(fp);
            return -1;
        }

        fprintf(fp, "\n\n} # class Main \n");

        fclose(fp);

        chmod(unitTestFileName.c_str(), 0744);
        return 0;
    }

    int serializeJavadoc() {
        for (auto& i : source) {
            if (i->serializeJavadoc()) {
                valid = false;
                return -1;
            }
        }

        return 0;
    }

    int serializeCpp() {
        FILE* fp = fopen(cppFileName.c_str(), cpp_open_flag);
        if (!fp) {
            error("%s: %s\n", cppFileName.c_str(), strerror(errno));
            return -1;
        }
        log(LL_INFO, "creating cpp file %s -> %s\n", fileName, cppFileName.c_str());

        for (source_t::const_iterator i = source.begin(), e = source.end(); i != e; ++i) {
            if ((*i)->serializeCpp(fp)) {
                valid = false;
                fclose(fp);
                return -1;
            }
        }

        if (groups.serializeFunctionCpp(fp, rootName.c_str())) {
            fclose(fp);
            return -1;
        }
        if (groups.serializeConstantCpp(fp, rootName.c_str())) {
            fclose(fp);
            return -1;
        }

        fclose(fp);
        return 0;
    }

    int serializeDox() {
        FILE* fp = fopen(doxFileName.c_str(), dox_open_flag);
        if (!fp) {
            error("%s: %s\n", doxFileName.c_str(), strerror(errno));
            return -1;
        }
        log(LL_INFO, "creating dox file %s -> %s\n", fileName, doxFileName.c_str());

        for (source_t::const_iterator i = source.begin(), e = source.end(); i != e; ++i) {
            if ((*i)->serializeDox(fp)) {
                valid = false;
                fclose(fp);
                return -1;
            }
        }

        groups.serializeFunctionDox(fp);
        groups.serializeConstantDox(fp, true);
        fclose(fp);

        return 0;
    }

    operator bool() const {
        return valid;
    }
};

int do_table_file() {
    FILE* ifp = fopen(opts.table_fn.c_str(), "r");
    if (!ifp) {
        error("%s: %s\n", opts.table_fn.c_str(), strerror(errno));
        return -1;
    }

    FILE* ofp = fopen(opts.output_fn.c_str(), "w");
    if (!ofp) {
        error("%s: %s\n", opts.output_fn.c_str(), strerror(errno));
        fclose(ifp);
        return -1;
    }

    std::string buf;
    while (true) {
        int c = fgetc(ifp);
        if (c == EOF)
            break;

        buf += c;
    }
    fclose(ifp);

    serialize_dox_comment(ofp, buf);
    fclose(ofp);

    return 0;
}

void init() {
    // initialize attribute map
    amap["public"] = QCA_PUBLIC;
    amap["private"] = QCA_PRIVATE;
    amap["private:internal"] = QCA_PRIVATE_INTERNAL;
    amap["static"] = QCA_STATIC;
    amap["synchronized"] = QCA_SYNCHRONIZED;
    amap["abstract"] = QCA_ABSTRACT;

    // initialize qore type to c++ typeinfo map
    tmap["int"] = "bigIntTypeInfo";
    mtmap["int"] = "Vi";

    tmap["softint"] = "softBigIntTypeInfo";
    mtmap["softint"] = "vi";

    tmap["*int"] = "bigIntOrNothingTypeInfo";
    mtmap["*int"] = "Ni";

    tmap["*softint"] = "softBigIntOrNothingTypeInfo";
    mtmap["*softint"] = "ni";

    tmap["float"] = "floatTypeInfo";
    mtmap["float"] = "Vf";

    tmap["softfloat"] = "softFloatTypeInfo";
    mtmap["softfloat"] = "vf";

    tmap["*float"] = "floatOrNothingTypeInfo";
    mtmap["*float"] = "Nf";

    tmap["*softfloat"] = "softFloatOrNothingTypeInfo";
    mtmap["*softfloat"] = "nf";

    tmap["number"] = "numberTypeInfo";
    mtmap["number"] = "Vn";

    tmap["softnumber"] = "softNumberTypeInfo";
    mtmap["softnumber"] = "vn";

    tmap["*number"] = "numberOrNothingTypeInfo";
    mtmap["*number"] = "Nn";

    tmap["*softnumber"] = "softNumberOrNothingTypeInfo";
    mtmap["*softnumber"] = "nn";

    tmap["bool"] = "boolTypeInfo";
    mtmap["bool"] = "Vb";

    tmap["softbool"] = "softBoolTypeInfo";
    mtmap["softbool"] = "vb";

    tmap["*bool"] = "boolOrNothingTypeInfo";
    mtmap["*bool"] = "Nb";

    tmap["*softbool"] = "softBoolOrNothingTypeInfo";
    mtmap["*softbool"] = "nb";

    tmap["string"] = "stringTypeInfo";
    mtmap["string"] = "Vs";

    tmap["softstring"] = "softStringTypeInfo";
    mtmap["softstring"] = "vs";

    tmap["*string"] = "stringOrNothingTypeInfo";
    mtmap["*string"] = "Ns";

    tmap["*softstring"] = "softStringOrNothingTypeInfo";
    mtmap["*softstring"] = "ns";

    tmap["list"] = "listTypeInfo";
    mtmap["list"] = "Vl";

    tmap["softlist"] = "softListTypeInfo";
    mtmap["softlist"] = "vl";

    tmap["*list"] = "listOrNothingTypeInfo";
    mtmap["*list"] = "Nl";

    tmap["*softlist"] = "softListOrNothingTypeInfo";
    mtmap["*softlist"] = "nl";

    tmap["date"] = "dateTypeInfo";
    mtmap["date"] = "Vd";

    tmap["softdate"] = "softDateTypeInfo";
    mtmap["softdate"] = "vd";

    tmap["*date"] = "dateOrNothingTypeInfo";
    mtmap["*date"] = "Nd";

    tmap["*softdate"] = "softDateOrNothingTypeInfo";
    mtmap["*softdate"] = "nd";

    tmap["binary"] = "binaryTypeInfo";
    mtmap["binary"] = "Vy";

    tmap["softbinary"] = "softBinaryTypeInfo";
    mtmap["softbinary"] = "vy";

    tmap["*binary"] = "binaryOrNothingTypeInfo";
    mtmap["*binary"] = "Ny";

    tmap["*softbinary"] = "softBinaryOrNothingTypeInfo";
    mtmap["*softbinary"] = "ny";

    tmap["hash"] = "hashTypeInfo";
    mtmap["hash"] = "Vh";

    tmap["*hash"] = "hashOrNothingTypeInfo";
    mtmap["*hash"] = "Nh";

    tmap["null"] = "nullTypeInfo";
    mtmap["null"] = "Vu";

    tmap["*null"] = "nullOrNothingTypeInfo";
    mtmap["*null"] = "Nu";

    tmap["timeout"] = "timeoutTypeInfo";
    mtmap["timeout"] = "Vt";

    tmap["*timeout"] = "timeoutOrNothingTypeInfo";
    mtmap["*timeout"] = "Nt";

    tmap["object"] = "objectTypeInfo";
    mtmap["object"] = "Vo";

    tmap["*object"] = "objectOrNothingTypeInfo";
    mtmap["*object"] = "No";

    tmap["code"] = "codeTypeInfo";
    mtmap["code"] = "Vc";

    tmap["*code"] = "codeOrNothingTypeInfo";
    mtmap["*code"] = "Nc";

    tmap["any"] = "anyTypeInfo";
    mtmap["any"] = "Va";

    tmap["auto"] = "autoTypeInfo";
    mtmap["auto"] = "VA";

    tmap["nothing"] = "nothingTypeInfo";
    mtmap["nothing"] = "Vn";

    tmap["reference"] = "referenceTypeInfo";
    mtmap["reference"] = "Vr";

    tmap["*reference"] = "referenceOrNothingTypeInfo";
    mtmap["*reference"] = "Nr";

    tmap["data"] = "dataTypeInfo";
    mtmap["data"] = "VD";

    tmap["*data"] = "dataOrNothingTypeInfo";
    mtmap["*data"] = "ND";

    mtmap["..."] = "VV";

    // initialize qore value to c++ value map
    valmap["0"] = "0";
    valmap["0.0"] = "(double)0.0";
    valmap["binary()"] = "new BinaryNode";
    valmap["QCS_DEFAULT->getCode()"] = "QCS_DEFAULT->getCode()";
    valmap["True"] = "true";
    valmap["False"] = "false";

    // initialize qore value to QoreSimpleValue map
    // Use constructors instead of .assign() because QoreValue::assign() returns AbstractQoreNode*
    // not a reference, so the expression value would be the wrong type for va_arg
    simple_valmap["0"] = "QoreSimpleValue((int64)0)";
    simple_valmap["0.0"] = "QoreSimpleValue((double)0.0)";
    simple_valmap["binary()"] = "QoreSimpleValue(new BinaryNode)";
    simple_valmap["True"] = "QoreSimpleValue(true)";
    simple_valmap["False"] = "QoreSimpleValue(false)";

    // initialize domain maps
    dmap["DEFAULT"] = "PO_DEFAULT";
    dmap["PROCESS"] = "PO_NO_PROCESS_CONTROL";
    dmap["NETWORK"] = "PO_NO_NETWORK";
    dmap["EXTERNAL_PROCESS"] = "PO_NO_EXTERNAL_PROCESS";
    dmap["FILESYSTEM"] = "PO_NO_FILESYSTEM";
    dmap["THREAD_CLASS"] = "PO_NO_THREAD_CLASSES";
    dmap["THREAD_CONTROL"] = "PO_NO_THREAD_CONTROL";
    dmap["DATABASE"] = "PO_NO_DATABASE";
    dmap["GUI"] = "PO_NO_GUI";
    dmap["TERMINAL_IO"] = "PO_NO_TERMINAL_IO";
    dmap["EXTERNAL_INFO"] = "PO_NO_EXTERNAL_INFO";
    dmap["THREAD_INFO"] = "PO_NO_THREAD_INFO";
    dmap["LOCALE_CONTROL"] = "PO_NO_LOCALE_CONTROL";
    dmap["MODULES"] = "PO_NO_MODULES";
    dmap["IN_MODULE"] = "PO_IN_MODULE";
    dmap["EMBEDDED_LOGIC"] = "PO_NO_EMBEDDED_LOGIC";
    dmap["REFLECTION"] = "PO_NO_REFLECTION";

    dnmap["INJECTION"] = "PO_ALLOW_INJECTION";
    dnmap["DEBUGGER"] = "PO_ALLOW_DEBUGGER";

    // initialize code flag set
    fset.insert("NO_FLAGS");
    fset.insert("NOOP");
    fset.insert("USES_EXTRA_ARGS");
    fset.insert("CONSTANT_INTERN");
    fset.insert("DEPRECATED");
    fset.insert("RET_VALUE_ONLY");
    fset.insert("RUNTIME_NOOP");
    fset.insert("CONSTANT");
}

void usage() {
    printf(usage_str, pn.c_str());
    exit(1);
}

void process_command_line(int& argc, char**& argv) {
    pn = basename(argv[0]);

    int ch;
    while ((ch = getopt_long(argc, argv, "d:D:hj:o:t:u:v:V", pgm_opts, nullptr)) != -1) {
        //log(LL_INFO, "ch=%c optarg=%p (%s)\n", ch, optarg, optarg ? optarg : "(null)");

        switch (ch) {
            case 'd':
                opts.dox_fn = optarg;
                break;

            case 'D':
                cmdline_defines.insert(optarg);
                log(LL_INFO, "defined preprocessor macro: %s\n", optarg);
                break;

            case 'h':
                usage();
                break;

            case 'j':
                opts.javadoc_fn = optarg;
                break;

            case 'o':
                opts.output_fn = optarg;
                break;

            case 't':
                opts.table_fn = optarg;
                break;

            case 'u':
                opts.unit_test_fn = optarg;
                break;

            case 'v':
                if (optarg)
                    opts.verbose = strtoll(optarg, nullptr, 10);
                else
                    ++opts.verbose;
                break;
        }
    }

    argc -= optind;
    argv += optind;

    if (!argc) {
        if (opts.table_fn.empty()) {
            error("at least one input file must be given\n");
            usage();
        }
        else if (opts.output_fn.empty()) {
            error("output file name must be given with -t\n");
            usage();
        }
    } else if (!opts.table_fn.empty()) {
        error("table file name must not be given along with an input file list\n");
        usage();
    }
}

int main(int argc, char* argv[]) {
    process_command_line(argc, argv);

    // initialize static reference data
    init();

    for (int i = 0; i < argc; ++i) {
        Code code(argv[i], i && !opts.output_fn.empty(), i && !opts.dox_fn.empty());
        if (!code) {
            error("please correct the errors above and try again\n");
            exit(1);
        }
        // create cpp output file
        if (code.serializeCpp())
            return -1;

        // create unit test output file
        if (code.serializeUnitTest())
            return -1;

        // create dox output file
        code.serializeDox();

        // create javadoc output file
        code.serializeJavadoc();
    }

    if (!opts.table_fn.empty())
        do_table_file();

    return 0;
}
