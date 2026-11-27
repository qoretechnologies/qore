#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Prints the method access errors about classes whose names other classes share: each names the class by its path, as
# the member diagnostics do (qore#7626); every execution mode and qcc executable must print the output of the AST
# interpreter (see method-access-diagnostic-names.qtest)

%modern
%no-child-restrictions

#! a class whose name a class in namespace Fake shares
class Shape {
    private string helper() {
        return "helper";
    }

    private copy() {
    }

    string pub() {
        return "pub";
    }
}

#! a class whose name the class in namespace Fake that inherits Shape privately shares
class Holder {
}

namespace Fake {
    #! the other class named Shape
    class Shape {
    }

    #! a class that inherits the root Shape privately
    class Holder inherits private ::Shape {
    }
}

#! a class whose name no other class shares: named by its short name
class Unique {
    private string helper() {
        return "helper";
    }
}

sub show(string name, code c) {
    try {
        c();
        printf("%s: no exception\n", name);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

auto shape = new Shape();
auto holder = new Fake::Holder();
auto unique = new Unique();

show("private method", sub () { shape.helper(); });
show("private method of a unique class", sub () { unique.helper(); });
show("private copy", sub () { shape.copy(); });
show("privately inherited method", sub () { holder.pub(); });
show("call reference to a private method", sub () { code c = \shape.helper(); c(); });
show("call reference to a privately inherited method", sub () { code c = \holder.pub(); c(); });
