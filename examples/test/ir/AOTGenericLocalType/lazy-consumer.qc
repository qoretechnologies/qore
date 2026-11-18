# Copyright 2026 Qore Technologies, s.r.o.

# runs the provider's code while qcc parses this file, and again at run time (qore#5885)
%modern
public const PARSE_VALUE = GenericProvider::check();

int sub main() {
    printf("parse: %s\nrun: %s\n", PARSE_VALUE, GenericProvider::check());
    return 0;
}
