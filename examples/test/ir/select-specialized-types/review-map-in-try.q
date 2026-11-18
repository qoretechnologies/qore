%modern
# cases from the review of qore#5654 (operand types of the specialized map opcodes, zero constants of select
# predicates, the exception edge of the map opcodes, gating); see select-specialized-types.qtest
string sub show(auto l) { return sprintf("%s %y", l.fullType(), l); }
auto sub m1(list<*int> l) { try { return map $1 + 1, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub m2(list<bool> l) { try { return map $1 * 2, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
for (int i = 0; i < 3; ++i) {
    printf("m1: %s\n", show(m1((1, NOTHING))));
    printf("m2: %s\n", show(m2((True, False))));
}
