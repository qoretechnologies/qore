%modern
# cases from the review of qore#5654 (operand types of the specialized map opcodes, zero constants of select
# predicates, the exception edge of the map opcodes, gating); see select-specialized-types.qtest
hashdecl IM { int v = 0; }
hashdecl NIM { *int v; }
string sub show(auto l) { return sprintf("%s %y", l.fullType(), l); }
sub tr(string n, code c) { try { printf("%s: %s\n", n, show(c())); } catch (hash<ExceptionInfo> ex) { printf("%s: EXC %s: %s\n", n, ex.err, ex.desc.substr(0, 60)); } }

auto sub fused_int_scale_float(list<int> l) { return map $1 * 1.5, select l, $1 > 0; }
auto sub fused_int_offset_float(list<int> l) { return map $1 + 0.5, select l, $1 > 0; }
auto sub fused_int_scale_float_first(list<int> l) { return map 1.5 * $1, select l, $1 > 0; }
auto sub fused_int_scale_int(list<int> l) { return map $1 * 3, select l, $1 > 0; }
auto sub fused_float_square(list<float> l) { return map $1 * $1, select l, $1 > 0; }
auto sub fused_int_square(list<int> l) { return map $1 * $1, select l, $1 > 0; }
auto sub fused_int_scale_float_try(list<int> l) { try { return map $1 * 1.5, select l, $1 > 0; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
# map opcodes inside try (Invoke form)
auto sub map_scale_int_try(list<int> l) { try { return map $1 * 3, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub map_offset_float_try(list<float> l) { try { return map $1 + 1, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub map_square_int_try(list<int> l) { try { return map $1 * $1, l; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub fused_int_try(list<int> l) { try { return map $1 * 2, select l, $1 > 0; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub sel_int_try(list<int> l) { try { return select l, $1 > 0; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub sel_nz_float_try(list<float> l) { try { return select l, $1 != 0; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
# hash-key select
auto sub key_im(list<hash<IM>> l) { return select l, $1.v > 0; }
auto sub key_nim(list<hash<NIM>> l) { return select l, $1.v > 0; }
auto sub key_hsi(list<hash<string, int>> l) { return select l, $1.v > 0; }
auto sub key_hsi_try(list<hash<string, int>> l) { try { return select l, $1.v > 0; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub key_star_hsi(list<*hash<string, int>> l) { return select l, $1.v > 0; }
# typed list holding converted values
auto sub sel_softint_list() { list<softint> l = ("3", 0, True); return select l, $1 > 0; }
auto sub sel_star_list_int(*list<int> l) { return select l, $1 != 0; }

for (int i = 0; i < 3; ++i) {
    tr("fused_int_scale_float", auto sub () { return fused_int_scale_float((2, -1, 3)); });
    tr("fused_int_offset_float", auto sub () { return fused_int_offset_float((2, -1, 3)); });
    tr("fused_int_scale_float_first", auto sub () { return fused_int_scale_float_first((2, -1, 3)); });
    tr("fused_int_scale_int", auto sub () { return fused_int_scale_int((2, -1, 3)); });
    tr("fused_float_square", auto sub () { return fused_float_square((1.5, -1.0)); });
    tr("fused_int_square", auto sub () { return fused_int_square((3, -1)); });
    tr("fused_int_scale_float_try", auto sub () { return fused_int_scale_float_try((2, -1, 3)); });
    tr("map_scale_int_try", auto sub () { return map_scale_int_try((2, -1)); });
    tr("map_offset_float_try", auto sub () { return map_offset_float_try((2.5, -1.0)); });
    tr("map_square_int_try", auto sub () { return map_square_int_try((2, -3)); });
    tr("fused_int_try", auto sub () { return fused_int_try((2, -3)); });
    tr("sel_int_try", auto sub () { return sel_int_try((2, -3, 0)); });
    tr("sel_nz_float_try", auto sub () { return sel_nz_float_try((2.5, 0.0, -1.0)); });
    tr("key_im", auto sub () { return key_im((<IM>{"v": 1}, <IM>{}, <IM>{"v": -1})); });
    tr("key_nim", auto sub () { return key_nim((<NIM>{"v": 1}, <NIM>{}, <NIM>{"v": -1})); });
    tr("key_hsi", auto sub () { return key_hsi(({"v": 1}, {"w": 3}, {"v": -1})); });
    tr("key_hsi_try", auto sub () { return key_hsi_try(({"v": 1}, {"w": 3}, {"v": -1})); });
    tr("key_star_hsi", auto sub () { return key_star_hsi(({"v": 1}, NOTHING, {"v": -1})); });
    tr("sel_softint_list", \sel_softint_list());
    tr("sel_star_list_int", auto sub () { return sel_star_list_int((1, 0, -1)); });
    tr("sel_star_list_int_nothing", auto sub () { return sel_star_list_int(); });
}
