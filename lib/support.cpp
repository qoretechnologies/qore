/*
    support.cpp

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

#include "qore/intern/qore_program_private.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <string>
#include <vector>

extern bool threads_initialized;

#define QORE_SERIALIZE_DEBUGGING_OUTPUT 1

#ifdef QORE_SERIALIZE_DEBUGGING_OUTPUT
static QoreThreadLock debug_output_lock;
#endif

#define QORE_QUICK_TIMESTAMP_LOG

int printe(const char* fmt, ...) {
    va_list args;
    QoreString buf;

    while (true) {
        va_start(args, fmt);
        int rc = buf.vsprintf(fmt, args);
        va_end(args);
        if (!rc)
        break;
    }

    fputs(buf.c_str(), stderr);
    fflush(stderr);
    return 0;
}

#ifdef QORE_QUICK_TIMESTAMP_LOG
static void get_timestamp(int &secs, int &us) {
    static int64 startSecs;
    static bool initFlag = false;
    int64 secs64 = q_epoch_us(us);
    if (initFlag) {
        secs = secs64-startSecs;
    } else {
        secs = 0;
        startSecs = secs64;
        initFlag = true;
    }
}
#else
static void get_timestamp(QoreString &str) {
    if (!(threads_initialized && is_valid_qore_thread()))
        return;

    int us;
    int64 secs = q_epoch_us(us);
    DateTime now;
    now.setDate(currentTZ(), secs, us);
    now.format(str, "YYYY-MM-DD HH:mm:SS.xx");
}
#endif

int print_debug(int level, const char *fmt, ...) {
    if (level > debug) {
        return 0;
    }

    va_list args;
    QoreString buf;

    while (true) {
        va_start(args, fmt);
        int rc = buf.vsprintf(fmt, args);
        va_end(args);
        if (!rc)
        break;
    }

    int tid = (threads_initialized && is_valid_qore_thread()) ? q_gettid() : -1;
#ifdef QORE_QUICK_TIMESTAMP_LOG
    int secs, us;
    get_timestamp(secs, us);
#ifdef QORE_SERIALIZE_DEBUGGING_OUTPUT
    AutoLocker al(debug_output_lock);
#endif
    fprintf(stderr, "%d.%d: TID %d: %s", secs, us, tid, buf.c_str());
#else
    QoreString ts;
    get_timestamp(ts);

#ifdef QORE_SERIALIZE_DEBUGGING_OUTPUT
    AutoLocker al(debug_output_lock);
#endif
    fprintf(stderr, "%s: TID %d: %s", ts.c_str(), tid, buf.c_str());
#endif
    fflush(stderr);
    return 0;
}

void trace_function(int code, const char* funcname) {
    if (!qore_trace) {
        return;
    }

#ifdef QORE_QUICK_TIMESTAMP_LOG
    int secs, us;
    get_timestamp(secs, us);
    if (code == TRACE_IN) {
#if defined(QORE_MANAGE_STACK) && defined(QORE_CHECKPOINT_STACK)
        checkpoint_stack_pos(funcname);
#endif
        printe("%d.%d: TID %d: %s entered\n", secs, us,
            threads_initialized && is_valid_qore_thread() ? q_gettid() : 0, funcname);
    } else {
        printe("%d.%d: TID %d: %s exited\n", secs, us,
            threads_initialized && is_valid_qore_thread() ? q_gettid() : 0, funcname);
    }
#else
    QoreString ts;
    get_timestamp(ts);
    if (code == TRACE_IN) {
        printe("%s: TID %d: %s entered\n", ts.c_str(),
            threads_initialized && is_valid_qore_thread() ? q_gettid() : 0, funcname);
    } else {
        printe("%s: TID %d: %s exited\n", ts.c_str(),
            threads_initialized && is_valid_qore_thread() ? q_gettid() : 0, funcname);
    }
#endif
}

char* remove_trailing_newlines(char *str) {
    int i = strlen(str);
    while (i && (str[i - 1] == '\n')) {
        str[--i] = '\0';
    }
    return str;
}

char* remove_trailing_blanks(char *str) {
    int i = strlen(str);
    while (i && (str[--i] == ' ')) {
        str[i] = '\0';
    }
    return str;
}

void parse_error(const QoreProgramLocation& loc, const char *fmt, ...) {
    printd(5, "parse_error(\"%s\", ...) called\n", fmt);

    QoreStringNode *desc = new QoreStringNode;
    while (true) {
        va_list args;
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc)
        break;
    }
    qore_program_private::makeParseException(getProgram(), loc, desc);
}

void parseException(const QoreProgramLocation& loc, const char *err, QoreStringNode *desc) {
    printd(5, "parseException(%s, %s) called\n", err, desc->c_str());
    qore_program_private::makeParseException(getProgram(), loc, err, desc);
}

void parseException(const QoreProgramLocation& loc, const char *err, const char *fmt, ...) {
    QoreStringNode *desc = new QoreStringNode;
    while (true) {
        va_list args;
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }
    parseException(loc, err, desc);
}

int q_edit_distance(const char* a, const char* b, int max) {
    assert(a);
    assert(b);
    const size_t la = strlen(a);
    const size_t lb = strlen(b);

    // handle trivial cases
    if (!la) {
        return (int)lb;
    }
    if (!lb) {
        return (int)la;
    }

    // quick lower bound: the distance is at least the difference in lengths
    if (max >= 0 && (size_t)std::abs((long)la - (long)lb) > (size_t)max) {
        return max + 1;
    }

    // Optimal String Alignment: three rolling rows (prev-prev, prev, current)
    std::vector<int> prev2(lb + 1, 0);
    std::vector<int> prev(lb + 1, 0);
    std::vector<int> curr(lb + 1, 0);

    for (size_t j = 0; j <= lb; ++j) {
        prev[j] = (int)j;
    }

    for (size_t i = 1; i <= la; ++i) {
        curr[0] = (int)i;
        int row_min = curr[0];
        for (size_t j = 1; j <= lb; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int v = prev[j] + 1;            // deletion
            const int ins = curr[j - 1] + 1;    // insertion
            if (ins < v) {
                v = ins;
            }
            const int sub = prev[j - 1] + cost; // substitution
            if (sub < v) {
                v = sub;
            }
            // adjacent transposition
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                const int trans = prev2[j - 2] + 1;
                if (trans < v) {
                    v = trans;
                }
            }
            curr[j] = v;
            if (v < row_min) {
                row_min = v;
            }
        }
        // early exit: every subsequent row value can only grow from this row's minimum
        if (max >= 0 && row_min > max) {
            return max + 1;
        }
        prev2.swap(prev);
        prev.swap(curr);
    }

    return prev[lb];
}

QoreSuggestionList::QoreSuggestionList(const char* targ) : target(targ ? targ : "") {
    // scale the acceptable edit distance with the target length: short names tolerate fewer typos
    const size_t len = target.size();
    threshold = (int)(len / 3);
    if (threshold < 1) {
        threshold = 1;
    } else if (threshold > 2) {
        threshold = 2;
    }
}

void QoreSuggestionList::add(const char* candidate) {
    if (!candidate || !*candidate || target.empty()) {
        return;
    }
    // an exact match cannot be a suggestion (the name failed to resolve)
    if (target == candidate) {
        return;
    }
    // a pure capitalization difference is the strongest possible hint
    if (!strcasecmp(target.c_str(), candidate)) {
        matches.push_back(std::make_pair(-1, std::string(candidate)));
        return;
    }
    const int d = q_edit_distance(target.c_str(), candidate, threshold);
    if (d <= threshold) {
        matches.push_back(std::make_pair(d, std::string(candidate)));
    }
}

std::vector<std::pair<int, std::string>> QoreSuggestionList::rank() const {
    std::vector<std::pair<int, std::string>> sorted(matches);
    // closest first; stable so equal-distance names keep insertion order
    std::stable_sort(sorted.begin(), sorted.end(),
        [](const std::pair<int, std::string>& l, const std::pair<int, std::string>& r) {
            return l.first < r.first;
        });
    // de-duplicate by name (the same name can be reached from multiple scopes) and cap the result
    std::vector<std::pair<int, std::string>> result;
    for (auto& i : sorted) {
        bool dup = false;
        for (auto& r : result) {
            if (r.second == i.second) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            result.push_back(i);
            if (result.size() >= 3) {
                break;
            }
        }
    }
    return result;
}

bool QoreSuggestionList::empty() const {
    return matches.empty();
}

std::vector<std::string> QoreSuggestionList::getSuggestions() const {
    std::vector<std::string> rv;
    for (auto& i : rank()) {
        rv.push_back(i.second);
    }
    return rv;
}

std::string QoreSuggestionList::getHint() const {
    std::vector<std::pair<int, std::string>> r = rank();
    if (r.empty()) {
        return std::string();
    }

    std::string hint("did you mean ");
    for (size_t i = 0; i < r.size(); ++i) {
        if (i) {
            hint += (i == r.size() - 1) ? " or " : ", ";
        }
        hint += "'";
        hint += r[i].second;
        hint += "'";
    }
    hint += "?";
    // if the single closest match differs only in capitalization, call that out
    if (r.size() == 1 && r[0].first == -1) {
        hint += " (check capitalization)";
    }
    return hint;
}

// returns 1 for success
static int try_include_dir(QoreString& dir, const char* file) {
    //printd(5, "try_include_dir(dir='%s', file='%s')\n", dir.c_str(), file);

    // make fully-justified path
#ifdef _Q_WINDOWS
    if (dir.strlen() && dir.c_str()[dir.strlen() - 1] != '\\' && dir.c_str()[dir.strlen() - 1] != '/')
#else
    if (dir.strlen() && dir.c_str()[dir.strlen() - 1] != QORE_DIR_SEP)
#endif
        dir.concat(QORE_DIR_SEP);
    dir.concat(file);
    struct stat sb;
    //printd(5, "try_include_dir() trying \"%s\"\n", dir.c_str());
    return !stat(dir.c_str(), &sb);
}

// FIXME: this could be a lot more efficient
int qore_find_file_in_path(QoreString& str, const char *file, const char *path) {
    // if path is empty, return null
    if (!path || !path[0])
        return -1;

    // duplicate string for invasive searches
    QoreString plist(path);
    char *idir = (char *)plist.c_str();
    //printd(5, "findFileInEnvPath() %s=%s\n", varname, idir);

    // try each directory
    while (char *p = strchr(idir, ':')) {
        if (p != idir) {
#ifdef _Q_WINDOWS
        // do not assume ':' separates paths on windows if it's the second character in a path
        if (p == idir + 1) {
            p = strchr(p + 1, ':');
            if (!p)
            break;
        }
#endif
        *p = '\0';
        str = idir;
        if (try_include_dir(str, file))
            return 0;
        }
        idir = p + 1;
    }

    // try last directory
    if (idir[0]) {
        str = idir;
        if (try_include_dir(str, file))
        return 0;
    }

    return -1;
}


// FIXME: this could be a lot more efficient
QoreString *findFileInPath(const char *file, const char *path) {
    TempString str;
    return qore_find_file_in_path(**str, file, path) ? 0 : str.release();
}

// FIXME: this could be a lot more efficient
QoreString *findFileInEnvPath(const char *file, const char *varname) {
    //printd(5, "findFileInEnvPath(file=%s var=%s)\n", file, varname);

    // if the file is an absolute path, then return it
    if (q_absolute_path(file))
        return new QoreString(file);

    // get path from environment
    QoreString str;
    if (SysEnv.get(varname, str))
        return 0;

    return findFileInPath(file, str.c_str());
}
