%modern
# the cases of date-saturated-utc-offset.qtest (qore#6431): the absolute dates at the limits of the seconds, from a
# number or an int out of range and from the int64 limits given to a time zone's date(), are saturated to the range
# of an absolute date, and format in the earliest and the latest year that a date can hold, with the UTC offset of the
# time zone (TZ, set by the test)

# Copyright (C) 2026 Qore Technologies, s.r.o.

sub t(string label, date d) {
    printf("%s: epoch=%d str=%s fmt=%s off=%d year=%d\n", label, get_epoch_seconds(d), d,
        format_date("YYYY-MM-DD HH:mm:SS Z", d), d.getUtcOffset(), get_years(d));
}

softdate big = 1e300n;
softdate small = -1e300n;
t("number max", big);
t("number min", small);
softdate fbig = 1e300;
softdate fsmall = -1e300;
t("float max", fbig);
t("float min", fsmall);
softdate ibig = 9223372036854775807;
softdate ismall = -9223372036854775807 - 1;
t("int max", ibig);
t("int min", ismall);
TimeZone zone = TimeZone::get();
t("zone date max", zone.date(9223372036854775807));
t("zone date min", zone.date(-9223372036854775807 - 1));
# microseconds that carry the seconds out of the years of an int
t("zone date carry max", zone.date(67767976233532799, 2000000000));
t("zone date carry min", zone.date(-67768100567971200, -2000000000));
# a carry toward the range from seconds far out of it gives the limit
t("zone date carry back max", zone.date(9223372036854775807, -2000000000));
t("zone date carry back min", zone.date(-9223372036854775807 - 1, 2000000000));
# a carry that takes the seconds past the limit of an int64
t("zone date carry overflow max", zone.date(9223372036854775807, 2000000000));
t("zone date carry overflow min", zone.date(-9223372036854775807 - 1, -2000000000));
# a date in the range keeps its seconds
t("in range", zone.date(1790000000));
