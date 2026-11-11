/*
    stale-artifact-host.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    A host of the AOT C API: an executable or module compiled before issue #5682 can store parse options with
    %modern and a compatibility option that %modern does not allow; creating its Program is refused with an error,
    instead of creating a Program with both in effect (see modern-option-consistency.qtest)
*/

#include <qore/Qore.h>

#include <cstdint>
#include <cstdio>

extern "C" QoreProgram* qore_create_program_ext(int64_t parse_options_lo, int64_t parse_options_hi);
extern "C" const char* qore_last_error(QoreProgram* pgm);

int main(int /*argc*/, char** /*argv*/) {
    qore_init(QL_GPL, "UTF-8", true);
    int rc = 0;

    // the options of a stale artifact: %modern and %broken-range
    QoreParseOptions stale = QoreParseOptions(PO_MODERN) | QoreParseOptions(PO_BROKEN_RANGE);
    QoreProgram* pgm = qore_create_program_ext(static_cast<int64_t>(stale.getLo()),
        static_cast<int64_t>(stale.getHi()));
    if (pgm) {
        printf("STALE-CREATED\n");
        pgm->waitForTerminationAndDeref(nullptr);
        rc = 1;
    } else {
        const char* err = qore_last_error(nullptr);
        printf("STALE-REFUSED: %s\n", err ? err : "(no error)");
    }

    // consistent options are accepted
    QoreParseOptions modern(PO_MODERN);
    pgm = qore_create_program_ext(static_cast<int64_t>(modern.getLo()), static_cast<int64_t>(modern.getHi()));
    if (pgm) {
        printf("MODERN-CREATED\n");
        pgm->waitForTerminationAndDeref(nullptr);
    } else {
        const char* err = qore_last_error(nullptr);
        printf("MODERN-REFUSED: %s\n", err ? err : "(no error)");
        rc = 1;
    }

    qore_cleanup();
    return rc;
}
