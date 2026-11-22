%modern
# foreign-call-returns.q: a thread of another runtime is blocked in a call of a closure of the script when the script
# ends, and an exit hook releases it: the end of the process waits for the call to return, as before (qore#6904)
Queue q();
Counter ready(1);
staticpgm_foreign_call(sub () { ready.dec(); q.get(); printf("foreign call returned\n"); });
ready.waitForZero();
register_exit_hook(sub () { printf("exit hook releases the call\n"); q.push(1); });
printf("script ends\n");
