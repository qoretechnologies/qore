# Foreign threads at the end of the process

A foreign thread is a thread of another runtime that registered with `q_register_foreign_thread()` (a daemon thread of
the Python module, for example). The library does not control it: it can be blocked in Qore code (`Queue::get()`)
for as long as the process lives, and the other runtime can end the process without waking it.

## The rule

`qore_cleanup()` first waits for the threads of the library (`thread_counter`) and then sets `qore_shutdown`. From then
on nothing waits for a foreign thread, and nothing destroys data that it uses:

- A `Program` tracks the TIDs of the foreign threads that run in it (`qore_program_private::foreign_tids`, changed with
  `tidmap` under `plock`). `waitForAllThreadsToTerminateIntern()` does not wait for them once `qore_shutdown` is set.
- `waitForTerminationAndClear()` decides, before it destroys anything, whether a foreign thread runs in the Program.
  If one does, the Program is left as it is: its data is not cleared and the Program is kept allocated
  (`leaveAllocated()`: a dependency reference, and a list that lives as long as the process, so the Program stays
  reachable). Otherwise it sets `foreign_teardown`, and `canEnterIntern()` refuses foreign threads from then on (a
  `PROGRAM-ERROR` in the thread), so the decision is final and no foreign thread can enter a Program whose data is being
  destroyed. The decision is the same for every way a Program is deleted: `clearRemainingPrograms()`, the owner's last
  dereference, the module manager and the AOT module cleanup.
- A thread that runs module code is counted by the thread, not by the Program of the module
  (`qore_module_uncounted_entry()`), so the Program cannot tell. `qore_stop_module_uncounted_entries()` does not wait for
  a foreign thread, and records that one runs module code (`qore_foreign_threads_in_module_code`); every Program of a
  module is then left, `QoreModuleManager::delUser()` leaves the modules as they are, and `qore_cleanup()` returns
  before it deletes the modules.
- When a Program was left, `qore_cleanup()` returns before it frees the library's own data (the types, the system
  namespace, the thread data): the foreign thread can use them.

## What is not cleaned up

At the end of a process with such a thread, the destructors of the objects in the Programs that were left do not run,
and their memory is not freed. A leak checker (valgrind) can report the values that the left Programs hold as lost
once the library is not torn down, for a Program of a module.

## Limits

- A foreign thread that is blocked in Qore code outside any Program (a module API called directly) is not tracked: the
  library is torn down under it as before.
- The flag `qore_foreign_threads_in_module_code` is never reset; one foreign thread in module code leaves every module
  Program, also if it is not blocked.
- Objects of left Programs are not destroyed: a buffered stream that flushes only in its destructor does not flush.
- A leak checker reports the values that the left Programs hold as lost (the library is not torn down then).

## Tests

`program-object-outlives-cleanup.qtest` (#6088); `program-cleanup-at-exit.qtest` (the test module `staticpgmcode`: a foreign thread blocked in its Program, in the Program
of a user module, and a foreign thread that tries to enter a Program during its teardown), `AOTModuleForeignThread.qtest`
(a compiled module).
