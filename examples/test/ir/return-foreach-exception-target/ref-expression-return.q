%modern
# a return out of a reference foreach on an expression with the implicit index, nested in a value foreach (qore#6643)
list<string> log;
list mm = ((0, 0), (0, 0), (0, 0));
on_exit printf("%s | %y\n", log.join(" | "), mm);
foreach auto x in ((1, 2, 3)) {
    on_exit log += "outer " + $#;
    foreach auto y in (\mm[$#]) {
        on_exit log += "inner " + $#;
        y = x;
        if (x == 2) {
            return;
        }
    }
}
