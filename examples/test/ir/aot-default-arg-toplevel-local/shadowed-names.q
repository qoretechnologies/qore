%modern
%disable-warning duplicate-local-vars
%disable-warning unreferenced-variable
string s = "first";
string sub f(string x = s) { return x; }
{
    string s = "inner";
    printf("%s\n", f());
}
string s2 = "second";
string sub g(string x = s2) { return x; }
s2 = "changed";
printf("%s %s\n", f(), g());
