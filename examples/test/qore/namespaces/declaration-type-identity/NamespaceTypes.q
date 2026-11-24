%modern

namespace Left { enum Status { Ok = 1 } hashdecl Record { int x = 1; } }
namespace Right { enum Status { Ok = 1 } hashdecl Record { int x = 1; } }

string sub label(Left::Status s) { return "left enum"; }
string sub label(Right::Status s) { return "right enum"; }
string sub label(hash<Left::Record> h) { return "left hash"; }
string sub label(hash<Right::Record> h) { return "right hash"; }

string sub dynamicLabel(auto value) { return label(value); }
printf("%s / %s / %s / %s\n", label(Left::Status::Ok), label(Right::Status::Ok),
    label(<Left::Record>{}), label(<Right::Record>{}));
foreach auto value in ((Left::Status::Ok, Right::Status::Ok, <Left::Record>{}, <Right::Record>{})) {
    printf("%s\n", dynamicLabel(value));
}

# Matching declarations parsed independently by another Program retain their structural compatibility.
Program child(PO_NEW_STYLE);
child.parse("namespace Right { enum Status { Ok = 1 } hashdecl Record { int x = 1; } } "
    "auto sub status() { return Right::Status::Ok; } auto sub record() { return <Right::Record>{}; }", "child");
printf("child: %s / %s\n", dynamicLabel(child.callFunction("status")), dynamicLabel(child.callFunction("record")));

Program wrong(PO_NEW_STYLE);
try {
    wrong.parse("namespace Left { hashdecl Record { int x = 1; } } "
        "namespace Right { hashdecl Record { int x = 1; } } "
        "hash<Left::Record> h = <Right::Record>{};", "assignment");
    printf("cross-namespace assignment accepted\n");
} catch (hash<ExceptionInfo> ex) {
    printf("cross-namespace assignment: %s\n", ex.err);
}
