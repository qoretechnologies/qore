# Copyright 2026 Qore Technologies, s.r.o.
%modern
Program pgm(PO_NEW_STYLE);
pgm.parse(convert_encoding(sprintf("%%include %s/café.qi\n", ARGV[0]), "ISO-8859-1"), "named-include");
printf("%d\n", pgm.callFunction("fromFile"));
