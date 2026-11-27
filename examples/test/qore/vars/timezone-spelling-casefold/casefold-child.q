#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# run by timezone-spelling-casefold.qtest in a user namespace of its own, on a tmpfs mounted with case folding at
# ARGV[0]; ARGV[1] is chattr(1).  It makes a zoneinfo directory that folds case (chattr +F) with copies of a zoneinfo
# file as AA/Rab0 .. AA/Rab9, makes the runtime read regions from it, and prints the region names that case variants
# of them get (qore#7783)

%modern

string mnt = ARGV[0];
string chattr = ARGV[1];
string root = mnt + "/zoneinfo";
mkdir(root);
# the directory folds the case of names; directories made in it after this do too
if (system(sprintf("%s +F %s", chattr, root))) {
    printf("SKIP: chattr +F failed\n");
    exit(0);
}
mkdir(root + "/AA");

# a zoneinfo file of the system, copied as the regions
binary data;
foreach string zi in (("/usr/share/zoneinfo/UTC", "/usr/lib/zoneinfo/UTC", "/usr/share/lib/zoneinfo/UTC")) {
    if (is_file(zi)) {
        data = ReadOnlyFile::readBinaryFile(zi);
        break;
    }
}
if (!data) {
    printf("SKIP: no zoneinfo file\n");
    exit(0);
}
for (int i = 0; i < 10; ++i) {
    File f();
    f.open2(sprintf("%s/AA/Rab%d", root, i), O_CREAT | O_WRONLY | O_TRUNC);
    f.write(data);
    f.close();
}
# the directory folds case: a case variant reaches the entry
if (!is_file(root + "/aa/rAB0")) {
    printf("SKIP: the directory does not fold case\n");
    exit(0);
}

call_function("dbg_tz_set_zoneinfo_root", root);
hash<auto> st0 = call_function("dbg_tz_region_cache");
int limit = st0.spelling_zones + 4;
call_function("dbg_tz_set_spelling_limits", limit, 2);

printf("entry AA/Rab0: %s\n", (new TimeZone("AA/Rab0")).region());
for (int i = 1; i <= 4; ++i) {
    string sp = sprintf("aa/Rab%d", i);
    printf("kept %s: %s\n", sp, (new TimeZone(sp)).region());
}
for (int i = 5; i <= 7; ++i) {
    string sp = sprintf("aa/Rab%d", i);
    printf("bounded %s: %s\n", sp, (new TimeZone(sp)).region());
}
hash<auto> st1 = call_function("dbg_tz_region_cache");
printf("zones at the limit: %s\n", st1.spelling_zones == limit ? "yes" : sprintf("no (%d, limit %d)",
    st1.spelling_zones, limit));
# the stored name of the directory too: the variant of the last component only is the entry's case
printf("stored dir name: %s\n", (new TimeZone("aA/Rab8")).region());
call_function("dbg_tz_set_spelling_limits", 0, 0);

# restricted code gets the zone of the stored name of the entry whatever case it uses, and adds one region name
Program p(PO_NEW_STYLE | PO_NO_FILESYSTEM);
p.parse("string sub tzr(string r) { return (new TimeZone(r)).region(); }", "tenant");
int rn0 = call_function("dbg_tz_region_cache").region_names;
list<string> rnames = map p.callFunction("tzr", $1), ("aa/rab9", "AA/rAB9", "Aa/RaB9");
printf("restricted: %s\n", rnames.join(","));
printf("restricted added: %d\n", call_function("dbg_tz_region_cache").region_names - rn0);
