%modern
string sub chr(int i) { return "ROOTCHR"; }
string sub substr(string s, int i) { return "ROOTSUBSTR"; }
namespace N {
    public string sub chr(int i) { return "NCHR"; }
}
printf("%s\n", ::chr(65));
printf("%s\n", chr(65));
printf("%s\n", ::substr("abc", 1));
printf("%s\n", N::chr(65));
printf("%s\n", Qore::chr(65));
code c = \::chr();
printf("%s\n", c(66));
code d = \chr();
printf("%s\n", d(66));
printf("%s\n", call_function("chr", 67));
printf("%s\n", map ::chr($1), (1, 2)[0]);
