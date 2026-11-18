%modern
int sub strlen(string s) { return -1; }
int sub length(string s) { return -2; }
int sub index(string s, string t) { return -3; }
int sub rindex(string s, string t) { return -4; }
number sub number(int i) { return 9999n; }
int sub min(int a, int b) { return -5; }
int sub max(int a, int b) { return -6; }
string sub string(int i) { return "S" + i.toString(); }
int sub int(string s) { return -8; }
float sub float(int i) { return -9.5; }
bool sub boolean(int i) { return False; }
string sub type(int i) { return "TYPE"; }
string sub typename(int i) { return "TYPENAME"; }
printf("%d\n", ::strlen("abc"));
printf("%d\n", ::length("abc"));
printf("%d\n", ::index("abc", "b"));
printf("%d\n", ::rindex("abc", "b"));
printf("%n\n", ::number(3));
printf("%d\n", ::min(1, 2));
printf("%d\n", ::max(1, 2));
printf("%s\n", ::string(3));
printf("%d\n", ::int("7"));
printf("%s\n", ::float(2).toString());
printf("%s\n", ::boolean(1).toString());
printf("%s %s\n", ::type(1), ::typename(1));
# the builtins still work through their own names
printf("%s %s\n", type(1), typename(1));
printf("%d %d %d %d\n", strlen("abc"), index("abc", "b"), min(1, 2), max(1, 2));
printf("%s %d %s\n", string(3), int("7"), float(2).toString());
printf("%y\n", map ::min($1, 2), (1, 2, 3));
printf("%y\n", map min($1, 2), (1, 2, 3));
