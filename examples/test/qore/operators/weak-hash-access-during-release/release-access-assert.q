%modern
# the program of weak-hash-access-during-release.qtest that accesses the members of a hash during its release with
# dbg_hash_access_during_release() (qore#7569): a debug build must end the process with an assertion; the
# arguments are the access ("lookup", "lvalue" or "iterate") and "deep" or "shallow"
call_function("dbg_hash_access_during_release", ARGV[0], ARGV[1] == "deep");
printf("no assertion\n");
