%modern
# foreign-call-blocked.q: a thread of another runtime (staticpgm_foreign_call() of the test module "staticpgmcode")
# is blocked forever in a call of a closure of the script when the script ends (qore#6904); the end of the process
# waits for it at most the bound that the first argument gives (in ms, default: the default bound), then ends
*string bound = ARGV[0];
if (bound) {
    # called by name, so that a library without the function runs the script with the default bound
    call_function("dbg_set_process_end_foreign_wait", bound.toInt());
}
Queue q();
Counter ready(1);
staticpgm_foreign_call(sub () { ready.dec(); q.get(); printf("foreign call returned\n"); });
ready.waitForZero();
# a background thread of the library is still waited for: it is released by the exit hook
Queue bq();
background sub () { bq.get(); printf("background thread ended\n"); }();
register_exit_hook(sub () { bq.push(1); });
if (ARGV[1] == "exit") {
    printf("script exits\n");
    exit(5);
}
printf("script ends\n");
