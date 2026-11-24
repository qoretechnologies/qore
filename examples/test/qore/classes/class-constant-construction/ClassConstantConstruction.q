%modern

class IntMethod {
    string label(int value) { return "int"; }
}

class AbstractMethods {
    abstract string label(bool value);
    abstract string label(int value);
}

class BoolMethod inherits AbstractMethods {
    string label(bool value) { return "bool"; }
}

class Combined inherits IntMethod, BoolMethod {
    public {
        const Name = (new Combined()).className();
    }
}

%ifdef EARLY_CASE
const CaseName = Combined::Name + "!";
sub show(string value) {
    switch (value) {
        case CaseName: printf("matched\n"); break;
    }
}
show("Combined!");
%else
printf("%s\n", Combined::Name);
%endif
