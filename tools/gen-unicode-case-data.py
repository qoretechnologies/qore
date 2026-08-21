#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: nil -*-
# Qore Programming Language
# Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
# Released under a choice of MIT, LGPL 2+, or GPL 2+; see README-LICENSE
"""Generate include/qore/intern/unicode-case-data.h from the Unicode Character Database.

The generated header holds the complete Unicode simple and full (1-to-many) case
mappings used by <string>::lwr()/upr(), <char>::lwr()/upr(), tolower(), toupper(),
QoreString::tolwr()/toupr() and the regex substitution case operators, plus the
Cased and Case_Ignorable property ranges needed to evaluate the language-independent
Final_Sigma condition when lowercasing.

Usage:
    tools/gen-unicode-case-data.py [--ucd <dir>] [--output <file>]

<dir> must contain UnicodeData.txt, SpecialCasing.txt and DerivedCoreProperties.txt
from a single UCD release (on Fedora these are shipped in /usr/share/unicode/ucd by
the "unicode-ucd" package; otherwise download them from
https://www.unicode.org/Public/UCD/latest/ucd/).

The generated header is committed to the repository; the build never runs this script.
"""

import argparse
import os
import re
import sys

DEFAULT_UCD_DIRS = [
    "/usr/share/unicode/ucd",
    "/usr/share/unicode",
]

FINAL_SIGMA_CP = 0x03A3
FINAL_SIGMA_LOWER = 0x03C2


def find_ucd_dir(explicit):
    if explicit:
        return explicit
    for d in DEFAULT_UCD_DIRS:
        if os.path.isfile(os.path.join(d, "UnicodeData.txt")):
            return d
    sys.exit("cannot find UnicodeData.txt; pass --ucd <dir>")


def read_lines(path):
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                yield line


def parse_cps(field):
    field = field.strip()
    if not field:
        return []
    return [int(x, 16) for x in field.split()]


def parse_unicode_data(path):
    """returns (simple_lower, simple_upper) dicts"""
    lower = {}
    upper = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            fields = line.split(";")
            if len(fields) < 15:
                continue
            cp = int(fields[0], 16)
            uc = fields[12].strip()
            lc = fields[13].strip()
            if uc:
                upper[cp] = int(uc, 16)
            if lc:
                lower[cp] = int(lc, 16)
    return lower, upper


def parse_special_casing(path):
    """returns (full_lower, full_upper) dicts of cp -> [cps]

    only unconditional (language- and context-independent) entries are used
    """
    full_lower = {}
    full_upper = {}
    for line in read_lines(path):
        fields = [x.strip() for x in line.split(";")]
        # <code>; <lower>; <title>; <upper>; (<condition_list>;)?
        if len(fields) < 4:
            continue
        # a condition list makes the entry language- or context-dependent
        if len(fields) >= 5 and fields[4]:
            continue
        cp = int(fields[0], 16)
        lc = parse_cps(fields[1])
        uc = parse_cps(fields[3])
        if lc:
            full_lower[cp] = lc
        if uc:
            full_upper[cp] = uc
    return full_lower, full_upper


def parse_derived_property(path, prop):
    """returns a sorted, coalesced list of (first, last) ranges for the given property"""
    pat = re.compile(r"^([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*(\S+)\s*$")
    ranges = []
    for line in read_lines(path):
        m = pat.match(line)
        if not m or m.group(3) != prop:
            continue
        first = int(m.group(1), 16)
        last = int(m.group(2), 16) if m.group(2) else first
        ranges.append((first, last))
    ranges.sort()
    out = []
    for first, last in ranges:
        if out and first <= out[-1][1] + 1:
            out[-1] = (out[-1][0], max(out[-1][1], last))
        else:
            out.append((first, last))
    return out


def ucd_version(ucd_dir):
    """extract the UCD version from the first line of SpecialCasing.txt"""
    with open(os.path.join(ucd_dir, "SpecialCasing.txt"), "r", encoding="utf-8") as f:
        first = f.readline()
    m = re.search(r"SpecialCasing-([0-9.]+)\.txt", first)
    return m.group(1) if m else "unknown"


def emit_simple(out, name, mapping):
    keys = sorted(mapping)
    out.append("static const q_simple_case_map_t %s[] = {" % name)
    for cp in keys:
        out.append("    { 0x%04x, 0x%04x }," % (cp, mapping[cp]))
    out.append("};")
    out.append("")


def emit_full(out, name, mapping, maxlen):
    keys = sorted(mapping)
    out.append("static const q_full_case_map_t %s[] = {" % name)
    for cp in keys:
        seq = mapping[cp]
        cps = ", ".join("0x%04x" % c for c in seq)
        pad = ", ".join(["0"] * (maxlen - len(seq)))
        if pad:
            cps = cps + ", " + pad
        out.append("    { 0x%04x, %d, { %s } }," % (cp, len(seq), cps))
    out.append("};")
    out.append("")


def emit_ranges(out, name, ranges):
    out.append("static const q_cp_range_t %s[] = {" % name)
    for first, last in ranges:
        out.append("    { 0x%04x, 0x%04x }," % (first, last))
    out.append("};")
    out.append("")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ucd", help="directory holding the UCD text files")
    ap.add_argument("--output", help="output header path")
    args = ap.parse_args()

    ucd = find_ucd_dir(args.ucd)
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output = args.output or os.path.join(root, "include", "qore", "intern", "unicode-case-data.h")

    version = ucd_version(ucd)
    simple_lower, simple_upper = parse_unicode_data(os.path.join(ucd, "UnicodeData.txt"))
    full_lower, full_upper = parse_special_casing(os.path.join(ucd, "SpecialCasing.txt"))

    # drop full mappings that are identical to the simple mapping (or to the identity
    # mapping where there is no simple mapping); they add nothing but table size
    full_lower = {cp: seq for cp, seq in full_lower.items()
                  if seq != [simple_lower.get(cp, cp)]}
    full_upper = {cp: seq for cp, seq in full_upper.items()
                  if seq != [simple_upper.get(cp, cp)]}

    maxlen = max([len(s) for s in full_lower.values()] + [len(s) for s in full_upper.values()])

    cased = parse_derived_property(os.path.join(ucd, "DerivedCoreProperties.txt"), "Cased")
    case_ignorable = parse_derived_property(os.path.join(ucd, "DerivedCoreProperties.txt"),
                                            "Case_Ignorable")

    out = []
    out.append("/* -*- mode: c++; indent-tabs-mode: nil -*- */")
    out.append("/*")
    out.append("    unicode-case-data.h")
    out.append("")
    out.append("    Qore Programming Language")
    out.append("")
    out.append("    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.")
    out.append("")
    out.append("    Permission is hereby granted, free of charge, to any person obtaining a")
    out.append("    copy of this software and associated documentation files (the \"Software\"),")
    out.append("    to deal in the Software without restriction, including without limitation")
    out.append("    the rights to use, copy, modify, merge, publish, distribute, sublicense,")
    out.append("    and/or sell copies of the Software, and to permit persons to whom the")
    out.append("    Software is furnished to do so, subject to the following conditions:")
    out.append("")
    out.append("    The above copyright notice and this permission notice shall be included in")
    out.append("    all copies or substantial portions of the Software.")
    out.append("")
    out.append("    THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR")
    out.append("    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,")
    out.append("    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE")
    out.append("    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER")
    out.append("    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING")
    out.append("    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER")
    out.append("    DEALINGS IN THE SOFTWARE.")
    out.append("")
    out.append("    Note that the Qore library is released under a choice of three open-source")
    out.append("    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more")
    out.append("    information.")
    out.append("")
    out.append("    GENERATED FILE - DO NOT EDIT")
    out.append("")
    out.append("    generated by tools/gen-unicode-case-data.py from the Unicode Character")
    out.append("    Database; case mapping data derived from UnicodeData.txt, SpecialCasing.txt")
    out.append("    and DerivedCoreProperties.txt, Copyright (C) 1991-2025 Unicode, Inc.")
    out.append("*/")
    out.append("")
    out.append("#ifndef _QORE_INTERN_UNICODE_CASE_DATA_H")
    out.append("#define _QORE_INTERN_UNICODE_CASE_DATA_H")
    out.append("")
    out.append("//! the version of the Unicode Character Database used to generate this file")
    out.append("#define QORE_UNICODE_VERSION \"%s\"" % version)
    out.append("")
    out.append("//! the maximum number of codepoints in a full (1-to-many) case mapping")
    out.append("#define QORE_MAX_FULL_CASE_MAP %d" % maxlen)
    out.append("")
    out.append("//! a 1-to-1 (simple) case mapping entry")
    out.append("struct q_simple_case_map_t {")
    out.append("    unsigned cp;      //!< the source codepoint")
    out.append("    unsigned mapped;  //!< the mapped codepoint")
    out.append("};")
    out.append("")
    out.append("//! a 1-to-many (full) case mapping entry")
    out.append("struct q_full_case_map_t {")
    out.append("    unsigned cp;                            //!< the source codepoint")
    out.append("    unsigned len;                           //!< the number of mapped codepoints")
    out.append("    unsigned mapped[QORE_MAX_FULL_CASE_MAP];  //!< the mapped codepoints")
    out.append("};")
    out.append("")
    out.append("//! an inclusive codepoint range")
    out.append("struct q_cp_range_t {")
    out.append("    unsigned first;  //!< the first codepoint in the range")
    out.append("    unsigned last;   //!< the last codepoint in the range")
    out.append("};")
    out.append("")
    out.append("//! the codepoint lowercased by the language-independent Final_Sigma condition")
    out.append("#define QORE_FINAL_SIGMA_CP 0x%04x" % FINAL_SIGMA_CP)
    out.append("//! the Final_Sigma lowercase mapping of QORE_FINAL_SIGMA_CP")
    out.append("#define QORE_FINAL_SIGMA_LOWER 0x%04x" % FINAL_SIGMA_LOWER)
    out.append("")
    out.append("//! simple (1-to-1) lowercase mappings, sorted by codepoint")
    emit_simple(out, "q_simple_lower_map", simple_lower)
    out.append("//! simple (1-to-1) uppercase mappings, sorted by codepoint")
    emit_simple(out, "q_simple_upper_map", simple_upper)
    out.append("//! unconditional full lowercase mappings that differ from the simple mapping")
    emit_full(out, "q_full_lower_map", full_lower, maxlen)
    out.append("//! unconditional full uppercase mappings that differ from the simple mapping")
    emit_full(out, "q_full_upper_map", full_upper, maxlen)
    out.append("//! codepoints with the Cased property, sorted and coalesced")
    emit_ranges(out, "q_cased_ranges", cased)
    out.append("//! codepoints with the Case_Ignorable property, sorted and coalesced")
    emit_ranges(out, "q_case_ignorable_ranges", case_ignorable)
    out.append("#endif")

    with open(output, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")

    sys.stderr.write(
        "wrote %s: UCD %s, %d simple lower, %d simple upper, %d full lower, %d full upper, "
        "%d cased ranges, %d case-ignorable ranges\n"
        % (output, version, len(simple_lower), len(simple_upper), len(full_lower),
           len(full_upper), len(cased), len(case_ignorable)))


if __name__ == "__main__":
    main()
