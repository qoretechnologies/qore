# -*- mode: qore; indent-tabs-mode: nil -*-
# prints the methods of a module's class as reflection enumerates them in the Program that loads the module
# (qore#7169)

%modern
%requires reflection
%requires MethodOrderMod

string sub names(list<auto> l) {
    return (map $1.getName(), l).join(",");
}

Class c = Class::forName("MethodOrderMod::Ordered");
printf("static: %s\n", names(c.getStaticMethods()));
printf("methods: %s\n", names(c.getMethods()));
printf("call: %s\n", c.getStaticMethods()[0].call());
