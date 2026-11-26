# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Code that is sandboxed or acts for sandboxed code cannot serialize an object that trusted code marked as shared
# with SandboxManager::shareObject(), also when it is held by another value, unless each class of the object that
# serializes data declares its own serializeMembers() method, which leaves out what the class guards; trusted code
# serializes as before, and a deserialized object is not shared (qore#7673); see shared-object-serialize.qtest

%modern
%enable-all-warnings
%no-child-restrictions

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

#! guards a private member with isSharedObject(self) and leaves it out in its own serializeMembers() method
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
    printf("tenant %s: shared without serializeMembers(): %s; with its own: %s; not shared: %s\n", name,
        p.callFunction("serialize_one", name, shared(new GuardedNoHook())),
        p.callFunction("serialize_one", name, shared(new GuardedHook())),
        p.callFunction("serialize_one", name, new GuardedNoHook()));
}
printf("tenant has the class serialize it: %s\n", p.callFunction("serialize_self", shared(new GuardedNoHook())));
printf("trusted deputy serializes it for the tenant: %s\n", p.callFunction("serialize_deputy", deputy,
    shared(new GuardedNoHook())));
printf("trusted deputy serializes one with its own serializeMembers() for the tenant: %s\n",
    p.callFunction("serialize_deputy", deputy, shared(new GuardedHook())));
printf("tenant round trip without serializeMembers(): %s\n", p.callFunction("round_trip",
    shared(new GuardedNoHook())));
printf("tenant round trip with its own serializeMembers(): %s\n", p.callFunction("round_trip",
    shared(new GuardedHook())));
printf("tenant round trip of one that is not shared: %s\n", p.callFunction("round_trip", new GuardedNoHook()));
printf("tenant serializes a subclass that serializes a member without its own serializeMembers(): %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new GuardedHookChild())));
printf("tenant serializes a subclass with only transient members: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new GuardedHookTransientChild())));
printf("tenant serializes a shared object of a builtin class with a serializer: %s\n",
    p.callFunction("serialize_one", "Serializable::serialize()", shared(new SharedTimeZone())));
{
    GuardedNoHook o = shared(new GuardedNoHook());
    GuardedNoHook c = Serializable::deserialize(o.serialize());
    printf("trusted code serializes it: deserialized shared: %y, secret: %s\n", SandboxManager::isSharedObject(c),
        c.useSecret());
    Box b = Serializable::deserialize(Serializable::serialize(new Box(o)));
    printf("trusted code serializes it in an object: %s\n", b.v.useSecret());
}
