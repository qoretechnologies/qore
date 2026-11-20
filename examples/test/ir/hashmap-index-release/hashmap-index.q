%modern
# hash maps whose value is the implicit index ($#) of their loop (qore#6641)
list<string> l = ("a", "b", "c");
hash<auto> h1 = map {$1: $#}, l;
hash<auto> h2 = map {$1: $#}, l, $# > 0;
hash<auto> h3 = map {$1: $#}, l.iterator();
hash<auto> h4 = map {$1: $#}, l.iterator(), $1 != "b";
list<auto> l1 = map $#, l;
list<auto> l2 = map $#, l, $1 != "b";
# a header row mapped to the column positions, as CsvHelper::adjustFieldsFromHeaders() does
hash<string, int> cols = map {$1.lwr(): $#}, ("ID", "Name", "Value");
printf("%y\n%y\n%y\n%y\n%y\n%y\n%y\n", h1, h2, h3, h4, l1, l2, cols);
