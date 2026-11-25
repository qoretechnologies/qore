%modern

# the closures that qualify as module-init closures, with the modules loaded from source or as qcc-compiled modules
# (qore#7335); then a Program with a SandboxManager loads InitA after this one, which applies the module to it

%requires InitA
%requires InitB

printf("init closure qualifies: %y\n", InitA::initQualifies());
try {
    printf("init closure run: %y\n", InitA::runInit());
} catch (hash<ExceptionInfo> ex) {
    printf("init closure run: %s\n", ex.err);
}
printf("static closure qualifies: %y\n", InitA::staticQualifies());
printf("late closure qualifies: %y\n", InitA::lateQualifies());
printf("late closure run: %s\n", InitA::runLate());
printf("closure of another module's code qualifies: %y\n", InitB::fromAQualifies());
printf("closure of the module's own initialization qualifies: %y\n", InitB::ownQualifies());
code own = string sub () { return "own"; };
printf("the loader's closure qualifies: %y\n", is_module_init_closure(own));

const ChildCode = "%requires InitA
list<string> sub run() {
    list<string> l = ();
    l += sprintf(\"child: init closure qualifies: %y\", InitA::initQualifies());
    l += sprintf(\"child: static closure qualifies: %y\", InitA::staticQualifies());
    l += sprintf(\"child: late closure qualifies: %y\", InitA::lateQualifies());
    code own = string sub () { return \"child\"; };
    l += sprintf(\"child: its closure qualifies: %y\", is_module_init_closure(own));
    return l;
}
";

SandboxManager mgr();
Program child(PO_NEW_STYLE);
child.setSandboxManager(mgr);
child.parse(ChildCode, "child");
map printf("%s\n", $1), child.callFunction("run");
