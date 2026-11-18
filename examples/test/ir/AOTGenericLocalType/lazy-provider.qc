# Copyright 2026 Qore Technologies, s.r.o.

# a preloaded .qo whose functions run while qcc parses another file, which builds their IR from the AOT image
# (lazy function IR); its generic locals refuse the values the instantiated types refuse (qore#5885)
%new-style
public class GenericProvider {
    public static int optLocal<T>(T x, auto y) {
        *T z = x;
        z = y;
        return 1;
    }

    public static int onExitLocal<T>(auto y) {
        {
            on_exit {
                *T z;
                z = y;
            }
        }
        return 1;
    }

    public static int forward<T>(T x, auto y) {
        return GenericProvider::optLocal<T>(x, y);
    }

    public static string check() {
        string r;
        try {
            GenericProvider::optLocal<string>("a", 2.5);
            r += "opt:accepted ";
        } catch (hash<ExceptionInfo> ex) {
            r += "opt:" + ex.err + " ";
        }
        try {
            GenericProvider::optLocal<string>("a", "b");
            r += "optok:accepted ";
        } catch (hash<ExceptionInfo> ex) {
            r += "optok:" + ex.err + " ";
        }
        try {
            GenericProvider::onExitLocal<string>(1);
            r += "oex:accepted ";
        } catch (hash<ExceptionInfo> ex) {
            r += "oex:" + ex.err + " ";
        }
        try {
            GenericProvider::forward<string>("a", 2.5);
            r += "fwd:accepted";
        } catch (hash<ExceptionInfo> ex) {
            r += "fwd:" + ex.err;
        }
        return r;
    }
}
