%modern

# StaleA's initialization loads StaleC by its path, outside the module path (STALE_C_PATH): StaleA's own closure
# qualifies, the closure of StaleC's initialization does not (qore#7335)

%requires StaleA

# StaleC, which StaleA's initialization loaded, is applied to this Program too
load_module(ENV.STALE_C_PATH);

printf("A init closure qualifies: %y\n", is_module_init_closure(StaleA::State::init_closure));
printf("C init closure qualifies: %y\n", call_function("StaleC::ownQualifies"));
