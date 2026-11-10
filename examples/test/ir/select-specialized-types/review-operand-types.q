%modern
# cases from the review of qore#5654 (operand types of the specialized map opcodes, zero constants of select
# predicates, the exception edge of the map opcodes, gating); see select-specialized-types.qtest
hashdecl FM { *float v; }
hashdecl SM { *string v; }

string sub show(auto l) {
    if (l.typeCode() != NT_LIST) { return sprintf("%s %y", l.fullType(), l); }
    return sprintf("%s %y", l.fullType(), l);
}

# map: multiplication by a float constant; parse type says float whenever one side is float
auto sub scale_float_untyped(list<auto> l) { return map $1 * 2.0, l; }
auto sub scale_float_number(list<number> l) { return map $1 * 2.0, l; }
auto sub scale_float_star_number(list<*number> l) { return map $1 * 2.0, l; }
auto sub scale_float_string(list<string> l) { return map $1 * 2.0, l; }
auto sub scale_float_const_first(list<auto> l) { return map 2.0 * $1, l; }
# map: + with an int constant; parse type says int whenever one side is int and the other is typed
auto sub offset_int_star_float(list<*float> l) { return map $1 + 1, l; }
auto sub offset_int_star_string(list<*string> l) { return map $1 + 1, l; }
auto sub offset_int_star_number(list<*number> l) { return map $1 + 1, l; }
auto sub offset_int_star_date(list<*date> l) { return map $1 + 1, l; }
auto sub offset_float_star_string(list<*string> l) { return map $1 + 1.5, l; }
auto sub offset_int_star_int(list<*int> l) { return map $1 + 1, l; }
auto sub offset_int_bool(list<bool> l) { return map $1 + 1, l; }
auto sub offset_float_star_float(list<*float> l) { return map $1 + 1.5, l; }
auto sub offset_int_star_float_try(list<*float> l) { try { return map $1 + 1, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
# map: hash key offset
auto sub key_offset_star_float(list<hash<FM>> l) { return map $1.v + 1, l; }
auto sub key_offset_star_string(list<hash<SM>> l) { return map $1.v + 1, l; }
# fused
auto sub fused_star_float(list<*float> l) { return map $1 + 1, select l, $1 > 0; }
auto sub fused_scale_number(list<number> l) { return map $1 * 2.0, select l, $1 > 0; }
# map: *int square / scale
auto sub square_star_int(list<*int> l) { return map $1 * $1, l; }
auto sub scale_star_int(list<*int> l) { return map $1 * 2, l; }
auto sub scale_float_star_int(list<*int> l) { return map $1 * 1.5, l; }
auto sub square_bool(list<bool> l) { return map $1 * $1, l; }
auto sub scale_int_bool(list<bool> l) { return map $1 * 2, l; }
auto sub scale_int_overflow(list<int> l) { return map $1 * 4, l; }
auto sub offset_int_overflow(list<int> l) { return map $1 + 1, l; }
# select: non-zero constants that truncate to 0
auto sub sel_gt_neg_half_int(list<int> l) { return select l, $1 > -0.5; }
auto sub sel_gt_half_float(list<float> l) { return select l, $1 > 0.5; }
auto sub sel_ne_half_int(list<int> l) { return select l, $1 != 0.5; }
auto sub sel_ne_half_float(list<float> l) { return select l, $1 != 0.5; }
auto sub sel_gt_nothing_int(list<int> l) { return select l, $1 > NOTHING; }
auto sub sel_gt_false_int(list<int> l) { return select l, $1 > False; }
# select: NaN, softint, list gaps
auto sub sel_ne_float(list<float> l) { return select l, $1 != 0; }
auto sub sel_gt_float(list<float> l) { return select l, $1 > 0; }
auto sub sel_gt_softint(list<softint> l) { return select l, $1 > 0; }
auto sub sel_gt_softfloat(list<softfloat> l) { return select l, $1 > 0; }
auto sub sel_ne_int(list<int> l) { return select l, $1 != 0; }
auto sub sel_gt_int(list<int> l) { return select l, $1 > 0; }
auto sub sel_gt_star_list(*list<int> l) { return select l, $1 > 0; }
auto sub sel_gt_softlist(softlist<int> l) { return select l, $1 > 0; }
# select: local narrowed
auto sub sel_local_reassigned() { auto l = (1, 2); l += "3"; l[0] = "-1"; return select l, $1 > 0; }
auto sub sel_local_reassigned2() { auto l = (1, -2); l = ("x", 2.5); return select l, $1 != 0; }
auto sub sel_local_list_int_gap() { list<int> l = (1,); l[3] = 2; return select l, $1 != 0; }
auto sub foldl_star_float(list<*float> l) { return foldl $1 + $2, l; }
auto sub foldl_float_number(list<auto> l) { return foldl $1 * $2, l; }

sub tr(string n, code c) { try { printf("%s: %s\n", n, show(c())); } catch (hash<ExceptionInfo> ex) { printf("%s: EXC %s: %s\n", n, ex.err, ex.desc.substr(0, 60)); } }
for (int i = 0; i < 3; ++i) {
    tr("scale_float_untyped", auto sub () { return scale_float_untyped((2n, 3, "4", 1.5, 2D)); });
    tr("scale_float_number", auto sub () { return scale_float_number((2n, 1.5n)); });
    tr("scale_float_star_number", auto sub () { return scale_float_star_number((2n, NOTHING)); });
    tr("scale_float_string", auto sub () { return scale_float_string(("2", "1.5", "x")); });
    tr("scale_float_const_first", auto sub () { return scale_float_const_first((2n, 3)); });
    tr("offset_int_star_float", auto sub () { return offset_int_star_float((1.5, NOTHING, -0.5)); });
    tr("offset_int_star_string", auto sub () { return offset_int_star_string(("a", NOTHING, "2")); });
    tr("offset_int_star_number", auto sub () { return offset_int_star_number((1.5n, NOTHING)); });
    tr("offset_int_star_date", auto sub () { return offset_int_star_date((2026-01-01, NOTHING)); });
    tr("offset_float_star_string", auto sub () { return offset_float_star_string(("a", NOTHING)); });
    tr("offset_int_star_int", auto sub () { return offset_int_star_int((1, NOTHING)); });
    tr("offset_int_bool", auto sub () { return offset_int_bool((True, False)); });
    tr("offset_float_star_float", auto sub () { return offset_float_star_float((1.0, NOTHING)); });
    tr("offset_int_star_float_try", auto sub () { return offset_int_star_float_try((1.5, NOTHING)); });
    tr("key_offset_star_float", auto sub () { return key_offset_star_float((<FM>{"v": 1.5}, <FM>{})); });
    tr("key_offset_star_string", auto sub () { return key_offset_star_string((<SM>{"v": "a"}, <SM>{})); });
    tr("fused_star_float", auto sub () { return fused_star_float((1.5, NOTHING, 0.5)); });
    tr("fused_scale_number", auto sub () { return fused_scale_number((1.5n, -1n)); });
    tr("square_star_int", auto sub () { return square_star_int((3, NOTHING)); });
    tr("scale_star_int", auto sub () { return scale_star_int((3, NOTHING)); });
    tr("scale_float_star_int", auto sub () { return scale_float_star_int((3, NOTHING)); });
    tr("square_bool", auto sub () { return square_bool((True, False)); });
    tr("scale_int_bool", auto sub () { return scale_int_bool((True, False)); });
    tr("scale_int_overflow", auto sub () { return scale_int_overflow((MAXINT,)); });
    tr("offset_int_overflow", auto sub () { return offset_int_overflow((MAXINT,)); });
    tr("sel_gt_neg_half_int", auto sub () { return sel_gt_neg_half_int((0, 1, -1)); });
    tr("sel_gt_half_float", auto sub () { return sel_gt_half_float((0.3, 0.7, 1.0)); });
    tr("sel_ne_half_int", auto sub () { return sel_ne_half_int((0, 1)); });
    tr("sel_ne_half_float", auto sub () { return sel_ne_half_float((0.0, 0.5, 1.0)); });
    tr("sel_gt_nothing_int", auto sub () { return sel_gt_nothing_int((0, 1, -1)); });
    tr("sel_gt_false_int", auto sub () { return sel_gt_false_int((0, 1, -1)); });
    tr("sel_ne_float", auto sub () { return sel_ne_float((0.0, -0.0, @nan@, 1.0)); });
    tr("sel_gt_float", auto sub () { return sel_gt_float((@nan@, -0.0, 1e-300, @inf@)); });
    tr("sel_gt_softint", auto sub () { return sel_gt_softint(("2", 0, 3.7)); });
    tr("sel_gt_softfloat", auto sub () { return sel_gt_softfloat(("2.5", 0, 3)); });
    tr("sel_ne_int", auto sub () { return sel_ne_int((0, 1, MININT)); });
    tr("sel_gt_int", auto sub () { return sel_gt_int((0, 1, MAXINT, MININT)); });
    tr("sel_gt_star_list", auto sub () { return sel_gt_star_list(); });
    tr("sel_gt_star_list2", auto sub () { return sel_gt_star_list((1, -1)); });
    tr("sel_gt_softlist", auto sub () { return sel_gt_softlist(5); });
    tr("sel_local_reassigned", auto sub () { return sel_local_reassigned(); });
    tr("sel_local_reassigned2", auto sub () { return sel_local_reassigned2(); });
    tr("sel_local_list_int_gap", auto sub () { return sel_local_list_int_gap(); });
    tr("foldl_star_float", auto sub () { return foldl_star_float((1.5, 2.5)); });
    tr("foldl_float_number", auto sub () { return foldl_float_number((1.5, 2n)); });
}
