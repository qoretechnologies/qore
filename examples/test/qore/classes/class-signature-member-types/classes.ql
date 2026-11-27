# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes of class-signature-member-types.qtest: included by probe.q, which qcc compiles, and by the module
# MemberTypeClasses (MemberTypeClasses.qm), which qcc -m compiles, and parsed from this file at run time in a Program
# of its own by checks.ql; an object of each class must be accepted as the class of the same declaration parsed at run
# time (the class signatures that qcc stores in the AOT image are those of the runtime parse)

public namespace MTC {
public hashdecl Rec {
    int a = 1;
    *string b;
}

public enum Color {
    Red = 1,
    Green = 2,
}

#! member and static variable types of every kind
public class Full {
    public {
        int i = 1;
        list<int> li;
        hash<string, softint> hss;
        hash<Rec> rec;
        *Later later;
        object<Later> ol;
        union<int, string> u;
        Color col = Color::Red;
        *list<hash<string, list<*float>>> deep;
        static *list<string> sls;
        static hash<Rec> srec;
    }

    private {
        *MTC::Later pl;
    }

    int count() {
        return li.size();
    }

    static *Later make() {
        return new Later();
    }
}

#! a class whose static variable initializer initializes its subclass, while the class itself is initialized
public class Base {
    public {
        static *Base inst = new Derived();
        int x = 1;
    }
}

public class Derived inherits Base {
    public {
        int y = 2;
    }
}

#! several method names, declared out of the order of their names (qore#7458)
public class Many {
    constructor() {
    }

    int zeta() {
        return 1;
    }

    string alpha(int a) {
        return "alpha " + a;
    }

    string alpha() {
        return "alpha";
    }

    *int mid(string s) {
    }

    private beta() {
    }

    static int szeta() {
        return 2;
    }

    static string salpha() {
        return "salpha";
    }

    private static smid(int i) {
    }
}

#! declared after the class whose member types refer to it
public class Later {
    public {
        *Full back;
    }
}
}
