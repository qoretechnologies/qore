%modern

class IntegerLabel {
%ifdef WIDE_RETURN
    *string label(int value) { return NOTHING; }
%else
    string label(int value) { return sprintf("%d", value); }
%endif
}

class Labels {
    abstract string label(bool value);
    abstract *string label(int value);
}

class BoolLabel inherits Labels {
    string label(bool value) { return "bool"; }
}

class OptionalLabel {
    string label(*int value) { return exists value ? sprintf("%d", value) : "missing"; }
}

%ifdef INHERIT_RESOLVED
# The promoted variant first competes with OptionalLabel in a descendant.
class Marker {}
class MarkerLabel { string label(Marker value) { return "marker"; } }
class Resolved inherits MarkerLabel, IntegerLabel, BoolLabel {}
class Combined inherits Resolved, OptionalLabel {}
%else
# Abstract resolution puts IntegerLabel::label(int) on Combined, ahead of OptionalLabel::label(*int).
class Combined inherits OptionalLabel, IntegerLabel, BoolLabel {}
%endif

OptionalLabel labeler = new Combined();
*int value = 1;
string label = labeler.label(value);
printf("%s %s\n", label, labeler.label(NOTHING));
