%modern
# a return out of a reference foreach in a block whose on_exit handler catches an exception (qore#6643)
list<string> log;
on_exit printf("%s\n", log.join(" | "));
on_exit {
    try {
        throw "A", "handler";
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
}
list m = (1, 2);
foreach auto x in (\m) {
    on_exit log += "body " + $#;
    x = 10;
    return;
}
