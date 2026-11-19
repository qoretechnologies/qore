%modern
# a return out of a value foreach nested in a reference foreach (qore#6643)
list<string> log;
on_exit printf("%s\n", log.join(" | "));
list o = (7, 8);
foreach auto x in (\o) {
    on_exit log += "outer " + $#;
    foreach auto y in ((1, 2, 3)) {
        log += sprintf("inner %d", $#);
        if ($# == 1) {
            x = 70;
            return;
        }
    }
}
