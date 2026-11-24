%modern

string log;

sub run() {
    list<auto> values = (11, 12, 13, 14);
    foreach int outer in ((90, 91, 92)) {
        if (outer == 92) {
            on_exit log += "x";
            foreach auto value in (\values) {
                if (value == 13) {
                    break;
                }
                value += 10;
            }
        }
    }
    printf("%s %y\n", log, values);
}

run();
