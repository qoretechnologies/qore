# The cost of creating and destroying objects

Creating an object and destroying it runs the general call path for each constructor and destructor in its class
hierarchy (`CodeEvaluationHelper`: the variant search, the argument checks, the Program context, the call frame), as
for any call.  For classes that need little of it, the shortcuts below do only what the general path does for them
(qore#7336).  Each shortcut is taken only when everything it leaves out cannot apply; anything else takes the general
path.

The environment variable `QORE_DISABLE_DIRECT_OBJECT_CALLS` (read once; see `qore_direct_object_calls_disabled()`)
turns all of them off, so that the tests can compare what they do with what the general path does
(`examples/test/qore/classes/object-construction-shortcuts/`: every script runs in every execution mode, on the IR
tier and compiled with qcc, with and without the shortcuts, and must give the output of the general path).

## Members without initializers

`qore_class_private::initMembers()` sets the object context (a `CodeContextHelperBase` and `self`) and initializes
each member of the member initialization list with `initMember()`, which evaluates the member's initializer, if any.
When no member of the list has an initializer, no code runs and no value is assigned: the members are created without
a value in their storage (the object's hash, or the `private:internal` hash of their class), as `initMember()` creates
a member without an initializer, without the object context.  Whether the list has an initializer is cached with the
code level of its initializers (`getMemberInitState()`, `memberInitIsPlain()`), computed when the first object of the
class is created and computed again when the list changes.
