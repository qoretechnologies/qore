%modern

%requires QUnit
%requires DataProvider

class DebugTest inherits QUnit::QUnit {
    constructor() : QUnit::QUnit("Debug Test", "1.0") {
        addTestCase("test", \testMethod());
    }

    testMethod() {
        QoreListDataType t(new Type("softlist<int>"));
        printf("Test 2: t.acceptsValue((NULL,))\n");
        int caught = 0;
        try {
            auto result = t.acceptsValue((NULL,));
            printf("  No exception, result: %y\n", result);
        } catch (hash<ExceptionInfo> ex) {
            caught = 1;
            printf("  Exception caught: %s\n", ex.err);
            printf("  Description: %s\n", ex.desc);
            printf("  'NULL' in desc: %d\n", ex.desc =~ /NULL/);
        }
        
        if (!caught) {
            throw "TEST-ERROR", "Expected exception not thrown";
        }
    }
}

DebugTest test();
