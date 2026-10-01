/* Copyright (C) 2026 Qore Technologies, s.r.o.  SPDX-License-Identifier: MIT */
/*  A custom host that registers a script object whose functions use a top-level local variable.

    Script objects never run their sources' top-level statements, so the variable is never instantiated: a write to
    it must raise an exception rather than being silently lost.  Each argument names a function to call; the host
    prints each call's result code and error.
*/
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <cstdio>

extern "C" void qore_toplevel_local_toplevel_local_script_register(QoreProgram*);

int main(int argc, char** argv) {
    qore_init(QL_GPL, "UTF-8", true);
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE | PO_STRICT_ARGS);
    if (!pgm) {
        qore_cleanup();
        return 1;
    }
    qore_aot_script_begin_batch(pgm);
    qore_toplevel_local_toplevel_local_script_register(pgm);
    int rc = qore_aot_script_end_batch(pgm);
    if (rc) {
        printf("registration failed: %s\n", qore_last_error(pgm));
    }
    for (int i = 1; !rc && i < argc; ++i) {
        int call_rc = qore_run_callable(pgm, argv[i], nullptr);
        printf("%s: rc=%d err=%s\n", argv[i], call_rc, call_rc ? qore_last_error(pgm) : "");
    }
    qore_destroy_program(pgm);
    qore_cleanup();
    return rc;
}
