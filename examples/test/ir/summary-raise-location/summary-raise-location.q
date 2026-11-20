%modern
# an exception raised by a call that a summary of the callee replaces is located at the statement that raised it
# (qore#6673)
int sub g(int x) {
    int y = x;
    y += 1;
    return y;
}
# one statement can raise, which is not the first statement nor the return
int sub oneStatement(int x) {
    int z = 1;
    z += g(x)
        + x / (x - x);
    return z;
}
# two statements can raise: the second one raises
int sub twoStatements(int x, int d) {
    int a = x / d;
    int b = x % (d - 1);
    return a + b;
}
# a hash projection that raises, after a statement that cannot
int sub hashKey(hash<auto> h) {
    int n = 2;
    return h.k + n;
}
float sub floatDiv(float x) {
    float f = 1.5;
    return f + x / (x - x);
}
sub report(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s %d-%d\n", label, ex.err, ex.line, ex.endline);
}
try { printf("one statement: %y\n", oneStatement(2)); } catch (hash<ExceptionInfo> ex) { report("one statement", ex); }
try { printf("two statements, first: %y\n", twoStatements(2, 0)); } catch (hash<ExceptionInfo> ex) { report("two statements, first", ex); }
try { printf("two statements, second: %y\n", twoStatements(2, 1)); } catch (hash<ExceptionInfo> ex) { report("two statements, second", ex); }
try { printf("two statements, none: %y\n", twoStatements(4, 2)); } catch (hash<ExceptionInfo> ex) { report("two statements, none", ex); }
try { printf("hash key: %y\n", hashKey({"k": 3})); } catch (hash<ExceptionInfo> ex) { report("hash key", ex); }
try { printf("float division: %y\n", floatDiv(1.0)); } catch (hash<ExceptionInfo> ex) { report("float division", ex); }
