# -*- mode: qore; indent-tabs-mode: nil -*-
# prints the methods and method variants of classes as reflection enumerates them (qore#7169); every enumeration
# must give the order in which the methods are declared, whatever the order of their names

%modern
%requires reflection

class Order {
    constructor() {}

    copy() {}

    nz() {}
    private na() {}
    private:internal nm() {}

    static string sz() {
        return "sz";
    }
    static string sa(int i) {
        return "sa int";
    }
    static string sa(string s) {
        return "sa string";
    }
    private:internal static sm() {}
    static sb() {}
    static sy() {}

    nb() {}

    destructor() {}
}

class OrderChild inherits Order {
    static sc() {}
    nk() {}
    static sa(float f) {}
    nc() {}
}

class ImplBase {
    string impl() {
        return "impl";
    }
}

class AbstractBase {
    abstract string impl();
}

# the class gets a method for the inherited concrete variant that implements the abstract method when it is committed
class Implementing inherits ImplBase, AbstractBase {
    zz() {}
    aa() {}
}

string sub names(list<auto> l) {
    return (map $1.getName(), l).join(",");
}

string sub variants(list<auto> l) {
    return (map sprintf("%s(%s)", $1.getMethod().getName(), $1.getParamTypes() ? (map $1.getName(), $1.getParamTypes()).join(",") : ""), l).join(",");
}

foreach string cn in ("Order", "OrderChild", "Implementing") {
    Class c = Class::forName(cn);
    printf("%s static: %s\n", cn, names(c.getStaticMethods()));
    printf("%s normal: %s\n", cn, names(c.getNormalMethods()));
    printf("%s methods: %s\n", cn, names(c.getMethods()));
    printf("%s all static: %s\n", cn, names(c.getAllStaticMethods()));
    printf("%s all normal: %s\n", cn, names(c.getAllNormalMethods()));
    printf("%s static variants: %s\n", cn, variants(c.getStaticVariants()));
    printf("%s variants: %s\n", cn, variants(c.getVariants()));
}

# the first static method in declaration order is called through reflection
printf("call: %s\n", Class::forName("Order").getStaticMethods()[0].call());
