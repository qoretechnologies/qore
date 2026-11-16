%modern

# closures created in the member defaults of generic hashdecls (qore#6269), run by
# hashdecl-default-closure-typeparam.qtest

hashdecl HdcBox<X> {
    X val;
}

hashdecl Hdc<X> {
    # the parameter and return types name the type parameter
    code mk = list<X> sub (X v) { return (v,); };
    # the body makes values of types that name it
    code body = hash<string, string> sub (X v) {
        X x = v;
        hash<HdcBox<X>> b = new hash<HdcBox<X>>({"val": x});
        list<X> l = (x, x);
        auto cl = cast<list<X>>((x,));
        return {"x": x.type(), "b": b.fullType(), "l": l.fullType(), "cl": cl.fullType()};
    };
    # a cast to a hashdecl with the type parameter
    code hcast = auto sub (X v) { return cast<hash<HdcBox<X>>>({"val": v}); };
    # a closure made by a closure
    code outer = code sub () { return list<X> sub (X v) { return (v, v); }; };
    # a closure that makes another hash of the instantiation
    code again = code sub () { hash<Hdc<X>> h2(); return h2.mk; };
}

#! a generic hashdecl that inherits the closures of Hdc with its type parameter in the type argument
hashdecl HdcQ<Y> inherits Hdc<list<Y>> {
    code own = list<Y> sub (Y v) { return (v, v); };
}

#! a non-generic hashdecl as a control
hashdecl HdcPlain {
    code mk = list<string> sub (string v) { return (v,); };
}

string sub hdc_show(code c) {
    try {
        auto v = c();
        return sprintf("%y %s", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        # the description of an error that a type of the closure cannot be resolved names the type
        return ex.err == "RUNTIME-CAST-ERROR" || ex.err == "HASHDECL-ERROR"
            ? sprintf("%s: %s", ex.err, ex.desc)
            : ex.err;
    }
}

list<auto> sub hdc_generic_function<T>(T v) {
    hash<Hdc<T>> h();
    return h.mk(v);
}

class HdcK<T> {
    code get() {
        hash<Hdc<T>> h();
        return h.mk;
    }
}

list<string> sub probes() {
    hash<Hdc<string>> hs();
    hash<Hdc<int>> hi();
    list<string> rv;
    rv += "string mk: " + hdc_show(auto sub () { return hs.mk("a"); });
    rv += "int mk: " + hdc_show(auto sub () { return hi.mk(1); });
    rv += "string mk int: " + hdc_show(auto sub () { return hs.mk(1); });
    rv += "int mk string: " + hdc_show(auto sub () { return hi.mk("a"); });
    rv += "string body: " + hdc_show(auto sub () { return hs.body("b"); });
    rv += "int body: " + hdc_show(auto sub () { return hi.body(2); });
    rv += "string cast: " + hdc_show(auto sub () { return hs.hcast("h"); });
    rv += "int cast: " + hdc_show(auto sub () { return hi.hcast(11); });
    rv += "string nested: " + hdc_show(auto sub () { return hs.outer()("n"); });
    rv += "int nested: " + hdc_show(auto sub () { return hi.outer()(3); });
    rv += "int again: " + hdc_show(auto sub () { return hi.again()(4); });
    # copies and values passed around
    hash<Hdc<string>> hc = hs;
    code m = hc.mk;
    rv += "copy: " + hdc_show(auto sub () { return m("c"); });
    code pass = code sub (hash<Hdc<int>> h) { return h.mk; };
    rv += "passed: " + hdc_show(auto sub () { return pass(hi)(5); });
    list<code> l = (hs.mk, hi.mk);
    rv += "list: " + hdc_show(auto sub () { return map $1.fullType(), (l[0]("s"), l[1](6)); });
    rv += "hash: " + hdc_show(auto sub () { hash<auto> h = hi; return h.mk(7); });
    # made in a generic function and in a method of a generic class
    rv += "generic function: " + hdc_show(auto sub () { return hdc_generic_function<string>("g"); });
    HdcK<int> k();
    rv += "generic method: " + hdc_show(auto sub () { return k.get()(8); });
    # threads: called in other threads, and made in another thread
    Queue q();
    background q.push(hdc_show(auto sub () { return hs.mk("t"); }));
    background q.push(hdc_show(auto sub () { return hi.mk(9); }));
    list<string> tl = (q.get(), q.get());
    rv += "threads: " + hdc_show(auto sub () { return sort(tl); });
    code make_in_thread = sub () { hash<Hdc<int>> h(); q.push(h.mk); };
    background make_in_thread();
    code tc = q.get();
    rv += "made in a thread: " + hdc_show(auto sub () { return tc(10); });
    # inherited closures
    hash<HdcQ<int>> hq();
    rv += "inherited mk: " + hdc_show(auto sub () { return hq.mk((12,)); });
    rv += "inherited cast: " + hdc_show(auto sub () { return hq.hcast((13,)); });
    rv += "inherited own: " + hdc_show(auto sub () { return hq.own(14); });
    # the control
    hash<HdcPlain> hp();
    rv += "plain: " + hdc_show(auto sub () { return hp.mk("p"); });
    return rv;
}
