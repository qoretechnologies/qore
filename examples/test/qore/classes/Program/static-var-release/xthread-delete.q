# -*- mode: qore; indent-tabs-mode: nil -*-
# xthread-delete.q - script of static-var-release.qtest: a Program created by a destructor during the release of
# static variables is deleted in another thread while the release is in progress; another one is kept and deleted
# later in another thread after its creator is gone

# Copyright 2026 Qore Technologies, s.r.o.

%modern

our list<Program> kept = ();
our Queue work();

sub worker() {
    while (True) {
        hash<auto> h = work.get();
        if (!h.p) {
            break;
        }
        delete h.p;
        h.done.dec();
    }
}

sub del_elsewhere(Program p) {
    Counter done(1);
    work.push({"p": p, "done": done});
    remove p;
    done.waitForZero();
}

background worker();
{
    # the worker stops in any case, so that an exception does not keep the script from exiting
    on_exit work.push({});
    {
        Program child(PO_NEW_STYLE);
        child.importGlobalVariable("kept");
        child.importFunction("del_elsewhere");
        child.parse(ReadOnlyFile::readTextFile(get_script_dir() + "xthread-child.qinc"), "child");
        delete child;
    }
    printf("after delete: kept %d\n", kept.size());
    foreach Program p in (kept) {
        del_elsewhere(p);
    }
    kept = ();
}
printf("main done\n");
