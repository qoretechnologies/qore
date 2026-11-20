%modern
# a return out of a foreach on a map expression nested in a reference foreach (qore#6643)
list<string> log;
on_exit printf("%s\n", log.join(" | "));
list o = (7, 8);
foreach auto x in (\o) {
    on_exit log += "outer " + $#;
    foreach auto y in (map $1 * 10, (1, 2, 3)) {
        on_exit log += "inner " + $# + " " + y;
        if ($# == 1) {
            return;
        }
    }
}
