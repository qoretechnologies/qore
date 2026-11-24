# The public and lexical method families return labels, including through a reference to a static method.
public class ParentLabel {
    string label() { return "parent"; }
    static string kind() { return "parent static"; }
    string family(string text) { return text; }
    string pair(string left, string right) { return left + right; }
}
public class ChildLabel inherits ParentLabel {
    private:internal string label() { return "child"; }
    private:internal static string kind() { return "child static"; }

    private:internal string pair(string right, string left) { return left + right; }

    static string parentNamed(ParentLabel obj) { return obj.family(text: "P"); }
    static string childNamed(ParentLabel obj) { return obj.family(number: 7); }
    private:internal string family(int number) { return number.toString(); }

    static list labels(ParentLabel obj, auto dynamic_obj) {
        code label_ref = \obj.label();
        code kind_ref = \obj.kind();
        return (obj.label(), dynamic_obj.label(), label_ref(), obj.kind(), kind_ref(), obj.pair(right: "R", left: "L"));
    }
}
public class LeafLabel inherits ChildLabel {}
