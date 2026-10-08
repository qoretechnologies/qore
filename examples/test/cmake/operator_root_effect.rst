Operator root-effect regression
===============================

Copyright 2026 Qore Technologies, s.r.o.

The four expression operator templates distinguish a root-level lvalue effect
from effects in child expressions. A public, unambiguous lvalue base makes the
root result known at compile time. Other template bases retain the runtime
cast, including a most-derived object with a separate lvalue subobject.

The native fixture includes the actual internal operator classes. Its concrete
test subclasses supply the unrelated abstract interfaces; parsing and background
copying are explicitly unsupported by the fixture. The runner extracts the
unchanged hidden default ``AbstractQoreNode::parseInit`` implementation because
``SimpleQoreNode`` requires it when linking an internal-header consumer.
The fixture never calls it. No operator implementation is copied or replaced.

Coverage includes all four templates with plain, direct lvalue, indirect lvalue,
and multiple-inheritance cross-cast layouts. Each runs with a nothing value,
negative integer, UTF-8 string, and effectful child node. Independent RTTI is the
root-effect reference. Repeated queries, ignored return values, child effects,
reference ownership, and complete destruction are checked. The indirect lvalue
base deliberately overrides its root predicate to false, proving the template
continues to classify its own inheritance rather than delegating to that override.

Build Qore using the existing installation prefix, then run::

    QORE_TEST_BUILD_DIR=$PWD/build \
      python3 -B -W error examples/test/cmake/test_operator_root_effect.py -v
    QORE_TEST_BUILD_DIR=$PWD/build-debug QORE_TEST_CXXFLAGS="-Og -g -DDEBUG" \
      QORE_TEST_VALGRIND=1 \
      python3 -B -W error examples/test/cmake/test_operator_root_effect.py -v

``QORE_TEST_INCLUDE_DIR`` selects matching runtime headers for a separately
built runtime. The four changed template bodies always come from this checkout.
``QORE_TEST_OUTPUT_DIR`` retains compilation commands and raw outputs.

The targeted compile uses ``-Werror=address`` and restores the former bodies in
an isolated header overlay as a negative control. The old source must fail with
exactly eight address diagnostics; the corrected source must compile silently
and pass 449 native checks. This compile probe does not replace full RPM compiler
flags or native package qualification. It changes neither the shipped compiler
policy nor runtime behavior. Valgrind is opt-in locally; qualification runs it
with signals disabled through ``QLO_DISABLE_SIGNAL_HANDLING``.
