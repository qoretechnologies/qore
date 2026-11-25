%modern

class P0 {
    public {
        int a;
        *string b;
    }
    private {
        hash<auto> h;
    }
    private:internal {
        list<auto> l;
    }
    hash<auto> dump() {
        return {
            "keys": keys self,
            "h": exists h, "l": exists l,
            "internal": l.typeCode(),
        };
    }
}

class C0 inherits P0 {
    public {
        float c;
        auto d;
    }
    private:internal {
        list<auto> l;
    }
    hash<auto> dumpC() {
        return dump() + {"cl": exists l, "ckeys": keys self};
    }
}

class PI {
    public {
        int x = 1;
    }
}

class CI inherits PI {
    public {
        int y;
    }
}

C0 c();
printf("%y\n", c.dumpC());
printf("%y\n", c.a);
c.a = 2;
c.c = 1.5;
printf("%y\n", c.dumpC());
CI ci();
printf("%y %y %y\n", ci.x, ci.y, keys ci);
