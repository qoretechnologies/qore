#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# issue #6014: a fold over a select stage whose predicate is a constant, in a function and at top level; see
# fold-select-constant.qtest

%modern
%enable-all-warnings

# a fold over a select stage with a constant False predicate, which no element passes
auto sub foldl_false() { return foldl $1 + $2, (select (1, 2), False); }
auto sub foldr_false() { return foldr $1 + $2, (select (1, 2), False); }
auto sub foldl_map_false() { return foldl $1 + $2, (select (map ($1,), (1, 2)), False); }
auto sub foldr_map_false() { return foldr $1 + $2, (select (map ($1,), (1, 2)), False); }
auto sub foldl_false_map() { return foldl $1 + $2, (map ($1,), (select (1, 2), False)); }
auto sub foldl_iter_false() { return foldl $1 + $2, (select new ListIterator((1, 2)), False); }
auto sub foldr_iter_false() { return foldr $1 + $2, (select new ListIterator((1, 2)), False); }
auto sub foldl_iter_map_false() { return foldl $1 + $2, (select (map ($1,), new ListIterator((1, 2))), False); }
auto sub foldr_iter_map_false() { return foldr $1 + $2, (select (map ($1,), new ListIterator((1, 2))), False); }
auto sub foldl_string_false() { return foldl $1 + "," + $2, (select ("a", "b"), False); }
auto sub foldl_false_arg(list<int> l) { return foldl $1 + $2, (select l, False); }
auto sub foldl_map_false_arg(list<int> l) { return foldl $1 + $2, (select (map ($1,), l), False); }

# the same with a constant True predicate, which every element passes
auto sub foldl_true() { return foldl $1 + $2, (select (1, 2), True); }
auto sub foldr_true() { return foldr $1 + $2, (select (1, 2), True); }
auto sub foldl_map_true() { return foldl $1 + $2, (select (map ($1,), (1, 2)), True); }
auto sub foldr_map_true() { return foldr $1 + $2, (select (map ($1,), (1, 2)), True); }
auto sub foldl_true_map() { return foldl $1 + $2, (map ($1,), (select (1, 2), True)); }
auto sub foldl_iter_true() { return foldl $1 + $2, (select new ListIterator((1, 2)), True); }
auto sub foldr_iter_true() { return foldr $1 + $2, (select new ListIterator((1, 2)), True); }
auto sub foldl_iter_map_true() { return foldl $1 + $2, (select (map ($1,), new ListIterator((1, 2))), True); }
auto sub foldr_iter_map_true() { return foldr $1 + $2, (select (map ($1,), new ListIterator((1, 2))), True); }
auto sub foldl_string_true() { return foldl $1 + "," + $2, (select ("a", "b"), True); }
auto sub foldl_true_arg(list<int> l) { return foldl $1 + $2, (select l, True); }
auto sub foldl_map_true_arg(list<int> l) { return foldl $1 + $2, (select (map ($1,), l), True); }

# other operators over a map stage after a select stage with a constant predicate, and a constant predicate that is
# an expression
class C {
    constructor(int i) {
    }
}
list<C> sub list_map_false() { return map new C($1), (select (1, 2), False); }
auto sub first_map_false() { return first (map new C($1), (select (1, 2), False)); }
bool sub any_map_false() { return any (map new C($1), (select (1, 2), False)); }
auto sub foldl_map_object_false() { return foldl $1 + $2, (map (new C($1),), (select (1, 2), False)); }
auto sub foldl_object_map_false() { return foldl $1 + $2, (select (map (new C($1),), (1, 2)), False); }
auto sub foldl_ternary_false() { return foldl $1 + $2, (select (1, 2), False ? True : False); }
list<C> sub list_map_true() { return map new C($1), (select (1, 2), True); }
bool sub any_map_true() { return any (map new C($1), (select (1, 2), True)); }
auto sub foldl_map_object_true() { return foldl $1 + $2, (map (new C($1),), (select (1, 2), True)); }
auto sub foldl_ternary_true() { return foldl $1 + $2, (select (1, 2), True ? True : False); }

printf("function foldl false: %y\n", foldl_false());
printf("function foldr false: %y\n", foldr_false());
printf("function foldl map false: %y\n", foldl_map_false());
printf("function foldr map false: %y\n", foldr_map_false());
printf("function foldl false map: %y\n", foldl_false_map());
printf("function foldl iterator false: %y\n", foldl_iter_false());
printf("function foldr iterator false: %y\n", foldr_iter_false());
printf("function foldl iterator map false: %y\n", foldl_iter_map_false());
printf("function foldr iterator map false: %y\n", foldr_iter_map_false());
printf("function foldl string false: %y\n", foldl_string_false());
printf("function foldl argument false: %y\n", foldl_false_arg((1, 2)));
printf("function foldl argument map false: %y\n", foldl_map_false_arg((1, 2)));
printf("function foldl true: %y\n", foldl_true());
printf("function foldr true: %y\n", foldr_true());
printf("function foldl map true: %y\n", foldl_map_true());
printf("function foldr map true: %y\n", foldr_map_true());
printf("function foldl true map: %y\n", foldl_true_map());
printf("function foldl iterator true: %y\n", foldl_iter_true());
printf("function foldr iterator true: %y\n", foldr_iter_true());
printf("function foldl iterator map true: %y\n", foldl_iter_map_true());
printf("function foldr iterator map true: %y\n", foldr_iter_map_true());
printf("function foldl string true: %y\n", foldl_string_true());
printf("function foldl argument true: %y\n", foldl_true_arg((1, 2)));
printf("function foldl argument map true: %y\n", foldl_map_true_arg((1, 2)));
printf("function list map false: %y\n", list_map_false().size());
printf("function first map false: %y\n", first_map_false());
printf("function any map false: %y\n", any_map_false());
printf("function foldl map object false: %y\n", foldl_map_object_false());
printf("function foldl object map false: %y\n", foldl_object_map_false());
printf("function foldl ternary false: %y\n", foldl_ternary_false());
printf("function list map true: %y\n", list_map_true().size());
printf("function any map true: %y\n", any_map_true());
printf("function foldl map object true: %y\n", foldl_map_object_true().size());
printf("function foldl ternary true: %y\n", foldl_ternary_true());

# top-level code
printf("top level foldl false: %y\n", foldl $1 + $2, (select (1, 2), False));
printf("top level foldr false: %y\n", foldr $1 + $2, (select (1, 2), False));
printf("top level foldl map false: %y\n", foldl $1 + $2, (select (map ($1,), (1, 2)), False));
printf("top level foldr map false: %y\n", foldr $1 + $2, (select (map ($1,), (1, 2)), False));
printf("top level foldl iterator false: %y\n", foldl $1 + $2, (select new ListIterator((1, 2)), False));
printf("top level foldr iterator false: %y\n", foldr $1 + $2, (select new ListIterator((1, 2)), False));
printf("top level foldl iterator map false: %y\n",
    foldl $1 + $2, (select (map ($1,), new ListIterator((1, 2))), False));
printf("top level foldl true: %y\n", foldl $1 + $2, (select (1, 2), True));
printf("top level foldr true: %y\n", foldr $1 + $2, (select (1, 2), True));
printf("top level foldl map true: %y\n", foldl $1 + $2, (select (map ($1,), (1, 2)), True));
printf("top level foldr map true: %y\n", foldr $1 + $2, (select (map ($1,), (1, 2)), True));
printf("top level foldl iterator true: %y\n", foldl $1 + $2, (select new ListIterator((1, 2)), True));
printf("top level foldr iterator true: %y\n", foldr $1 + $2, (select new ListIterator((1, 2)), True));
printf("top level foldl iterator map true: %y\n",
    foldl $1 + $2, (select (map ($1,), new ListIterator((1, 2))), True));
