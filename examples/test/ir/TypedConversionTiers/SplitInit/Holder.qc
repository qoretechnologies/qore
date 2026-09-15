# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

public namespace SplitInit {
    public class Holder {
        public {
            static softlist<string> strings = "s";
            softstring text = 3;
        }

        static string describe() {
            return sprintf("%y %y", strings, (new Holder()).text);
        }
    }
}
