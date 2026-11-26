#!/usr/bin/env qore
# a class that sandboxed code declares with the name and the declarations of a class that trusted code declares has
# no access to the private:internal declarations of the objects of the trusted class (qore#6843, qore#7036); run by
# ../sandbox-trusted-private-access.qtest in every execution mode once qore#7036 is fixed

%modern
%enable-all-warnings

#! a class that trusted code declares
class Shape {
    private {
        string secret = "secret";
    }

    private:internal {
        string isecret = "isecret";
    }

    private string helper() {
        return "helper";
    }

    private:internal string ihelper() {
        return "ihelper";
    }

    auto get(string what) {
        return what;
    }
}

# an object shared with the sandboxed code
Shape shared();

# the class of the sandboxed code: the same name and declarations in another namespace, other method bodies
const Tenant = "
class Holder {
    public {
        static auto h;
    }
}

namespace Fake {
class Shape {
    private {
        string secret = \"secret\";
    }

    private:internal {
        string isecret = \"isecret\";
    }

    private string helper() {
        return \"helper\";
    }

    private:internal string ihelper() {
        return \"ihelper\";
    }

    auto get(string what) {
        auto h = Holder::h;
        switch (what) {
            case \"secret\": return h.secret;
            case \"write\": h.secret = \"changed\"; return h.secret;
            case \"isecret\": return h.isecret;
            case \"helper\": return h.helper();
            case \"ihelper\": return h.ihelper();
            case \"keys\": return keys h;
        }
    }
}
}

auto sub run(auto shared, string what) {
    Holder::h = shared;
    return new Fake::Shape().get(what);
}
";

Program p(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(Tenant, "tenant");

foreach string what in (("isecret", "ihelper", "keys")) {
    try {
        printf("%s: OK %y\n", what, p.callFunction("run", shared, what));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", what, ex.err, ex.desc);
    }
}
