# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Code that is sandboxed or acts for sandboxed code cannot serialize an object that trusted code marked as shared
# with SandboxManager::shareObject(), also when it is held by another value, whatever its classes declare
# (serializeMembers() included), also when its class lets sandboxed code serialize its non-public state (it inherits
# SandboxSerializable); trusted code serializes as before, and a deserialized object is not shared (qore#7673); see
# shared-object-serialize.qtest.  The objects that are not shared that the tenant serializes are of a trusted class
# that inherits SandboxSerializable, without which sandboxed code cannot serialize the non-public state of a trusted
# class (qore#7146)

%modern
%enable-all-warnings
%no-child-restrictions
%allow-weak-references

#! guards a private member with isSharedObject(self) and declares no serializeMembers() method
public class GuardedNoHook inherits Serializable {
    private {
        string secret = "secret";
    }

    #! the secret, which the class gives only for an object that is not shared
    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : secret;
    }

    #! serializes the object in the code of its own class
    binary serializeSelf() {
        return serialize();
    }
}

#! guards a private member with isSharedObject(self) and lets sandboxed code serialize its non-public state
/** The objects that are not shared that the tenant serializes; a shared object of the class is refused all the same
*/
public class GuardedOptedIn inherits SandboxSerializable {
    private {
        string secret = "secret";
    }

    #! the secret, which the class gives only for an object that is not shared
    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : secret;
    }
}

#! guards a private member with isSharedObject(self) and leaves it out in its own serializeMembers() method, which
#! does not let sandboxed code serialize a shared object
public class GuardedHook inherits Serializable {
    public {
        string name = "hook";
    }

    private {
        *string secret = "secret";
    }

    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : (secret ?? "no secret");
    }

    #! shared-object: leaves out secret
    private:internal *hash<auto> serializeMembers(*hash<auto> members) {
        remove members.secret;
        return members;
    }
}

#! guards a private hash and a private object, which its own serializeMembers() method leaves out (from the members
#! of the class; their data is serialized before it is called)
public class GuardedHookContainers inherits Serializable {
    public {
        string name = "containers";
    }

    private {
        *hash<auto> secret_hash = {"key": "hidden-hash-value"};
        *Box secret_obj = new Box("hidden-object-value");
    }

    string useSecret() {
        return SandboxManager::isSharedObject(self) ? "refused" : sprintf("%s %s", secret_hash.key ?? "no hash",
            exists secret_obj ? secret_obj.v : "no object");
    }

    #! shared-object: leaves out secret_hash, secret_obj
    private:internal *hash<auto> serializeMembers(*hash<auto> members) {
        remove members.secret_hash;
        remove members.secret_obj;
        return members;
    }
}

#! serializes a member of its own without a serializeMembers() method of its own
public class GuardedHookChild inherits GuardedHook {
    public {
        string extra = "extra";
    }
}

#! has only transient members of its own
public class GuardedHookTransientChild inherits GuardedHook {
    private {
        transient string cache = "cache";
    }
}

#! inherits a builtin class with a serializer
public class SharedTimeZone inherits TimeZone {
    constructor() : TimeZone("UTC") {
    }

    string useSecret() {
        return "none";
    }
}

#! an object that is not shared that holds another value
public class Box inherits Serializable {
    public {
        auto v;
    }

    constructor(auto v) {
        self.v = v;
    }
}

#! holds a value in a static class variable
public class StaticHolder inherits Serializable {
    public {
        static auto held;
    }
}

#! holds a weak reference to an object
public class WeakBox inherits Serializable {
    public {
        auto v;
    }

    constructor(object v) {
        self.v := v;
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# the result of a serialization, or its exception
string sub try_serialize(code c) {
    try {
        auto v = c();
        return sprintf(\"serialized (%s)\", v.type());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, code> sub serializations() {
    return {
        \"serialize()\": auto sub (object o) { return o.serialize(); },
        \"serializeToData()\": auto sub (object o) { return o.serializeToData(); },
        \"Serializable::serialize()\": auto sub (object o) { return Serializable::serialize(o); },
        \"Serializable::serializeToData()\": auto sub (object o) { return Serializable::serializeToData(o); },
        \"serialize() to a stream\": auto sub (object o) {
            BinaryOutputStream stream();
            try {
                o.serialize(stream);
            } catch (hash<ExceptionInfo> ex) {
                # nothing is written for a refused object
                if (stream.getData()) {
                    throw \"STREAM-WRITTEN\", sprintf(\"%d bytes\", stream.getData().size());
                }
                rethrow;
            }
            return stream.getData();
        },
        \"in a member of an object\": auto sub (object o) { return Serializable::serialize(new Box(o)); },
        \"in a hash\": auto sub (object o) { return Serializable::serialize({\"k\": o}); },
        \"in a list\": auto sub (object o) { return Serializable::serialize((1, o)); },
        \"in a weak reference\": auto sub (object o) { return Serializable::serialize(new WeakBox(o)); },
    };
}

list<string> sub serialization_names() {
    return keys serializations();
}

string sub serialize_one(string name, object o) {
    return try_serialize(auto sub () { return serializations(){name}(o); });
}

string sub serialize_self(GuardedNoHook o) {
    return try_serialize(auto sub () { return o.serializeSelf(); });
}

# trusted code that the tenant calls serializes the object for it
string sub serialize_deputy(code deputy, object o) {
    return try_serialize(auto sub () { return deputy(o); });
}

# serializes an object with its static class variables
string sub serialize_static(object o) {
    return try_serialize(auto sub () { return Serializable::serialize(o, QSF_INCLUDE_STATIC_CLASS_VARS); });
}

# whether the data that the tenant serializes holds a value that the class leaves out
string sub leaks(object o) {
    try {
        string data = sprintf(\"%y\", Serializable::serializeToData(o));
        return data =~ /hidden/ ? \"LEAKED\" : \"left out\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# what the tenant gets back from the data that it serializes
string sub round_trip(object o) {
    try {
        object c = Serializable::deserialize(Serializable::serialize(o));
        return sprintf(\"shared: %y, secret: %s\", SandboxManager::isSharedObject(c), c.useSecret());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

object sub shared(object o) {
    SandboxManager::shareObject(o);
    return o;
}

# trusted code that serializes the object that it is given
code deputy = binary sub (object o) { return Serializable::serialize(o); };

foreach string name in (p.callFunction("serialization_names")) {
    printf("tenant %s: shared: %s; shared with serializeMembers(): %s; shared with SandboxSerializable: %s; not "
        "shared: %s\n", name,
        p.callFunction("serialize_one", name, shared(new GuardedNoHook())),
        p.callFunction("serialize_one", name, shared(new GuardedHook())),
        p.callFunction("serialize_one", name, shared(new GuardedOptedIn())),
        p.callFunction("serialize_one", name, new GuardedOptedIn()));
}
# without SandboxSerializable, the non-public state of a trusted class is not the tenant's to serialize (qore#7146)
printf("tenant serializes one that is not shared without SandboxSerializable: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", new GuardedNoHook()));
printf("tenant has the class serialize it: %s\n", p.callFunction("serialize_self", shared(new GuardedNoHook())));
printf("trusted deputy serializes it for the tenant: %s\n", p.callFunction("serialize_deputy", deputy,
    shared(new GuardedNoHook())));
printf("trusted deputy serializes one with serializeMembers() for the tenant: %s\n",
    p.callFunction("serialize_deputy", deputy, shared(new GuardedHook())));
printf("tenant round trip without serializeMembers(): %s\n", p.callFunction("round_trip",
    shared(new GuardedNoHook())));
printf("tenant round trip with serializeMembers(): %s\n", p.callFunction("round_trip",
    shared(new GuardedHook())));
printf("tenant serializes containers that serializeMembers() leaves out: %s\n",
    p.callFunction("leaks", shared(new GuardedHookContainers())));
printf("tenant round trip with containers left out: %s\n", p.callFunction("round_trip",
    shared(new GuardedHookContainers())));
{
    GuardedHookContainers o = shared(new GuardedHookContainers());
    GuardedHookContainers c = Serializable::deserialize(o.serialize());
    printf("trusted code serializes containers: %s\n", c.useSecret());
    GuardedHook h = shared(new GuardedHook());
    GuardedHook hc = Serializable::deserialize(h.serialize());
    printf("trusted code serializes one with serializeMembers(): %s\n", hc.useSecret());
}
printf("tenant round trip of one that is not shared: %s\n", p.callFunction("round_trip", new GuardedOptedIn()));
printf("tenant serializes a subclass that serializes a member: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new GuardedHookChild())));
printf("tenant serializes a subclass with only transient members: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new GuardedHookTransientChild())));
printf("tenant serializes a shared object of a builtin class with a serializer: %s; one that is not shared: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new SharedTimeZone())),
    p.callFunction("serialize_one", "Serializable::serialize()", new SharedTimeZone()));
StaticHolder::held = shared(new GuardedNoHook());
printf("tenant serializes a shared object in a static class variable: %s\n", p.callFunction("serialize_static",
    new StaticHolder()));
StaticHolder::held = new GuardedOptedIn();
printf("tenant serializes one that is not shared in a static class variable: %s\n",
    p.callFunction("serialize_static", new StaticHolder()));
remove StaticHolder::held;
{
    GuardedNoHook o = shared(new GuardedNoHook());
    GuardedNoHook c = Serializable::deserialize(o.serialize());
    printf("trusted code serializes it: deserialized shared: %y, secret: %s\n", SandboxManager::isSharedObject(c),
        c.useSecret());
    Box b = Serializable::deserialize(Serializable::serialize(new Box(o)));
    printf("trusted code serializes it in an object: %s\n", b.v.useSecret());
}
