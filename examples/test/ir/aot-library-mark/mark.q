%modern

# LibA's initialization loads LibD by name from the module path and PathC by its path from outside it
# (LIBMARK_C_PATH): LibD is library code, PathC is not, whether the modules are loaded from source or compiled by qcc
# (qore#7361)

%requires LibA
%requires LibD

# PathC, which LibA's initialization loaded, is applied to this Program too
load_module(ENV.LIBMARK_C_PATH);

printf("D runs its init closure: %s\n", LibD::runOwn());
printf("C init closure qualifies: %y\n", call_function("PathC::ownQualifies"));
printf("C runs A's init closure: %s\n", call_function("PathC::runInit", LibA::State::init_closure));
