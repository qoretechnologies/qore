# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of qore#6265, which AotSectionBounds-<part>.qtest run in parts so that each test takes a short time
# (qore#6544): part 1 runs the hand-picked cases and every part a quarter of the loads of the systematic case (see
# addPartCase())

#! the section of the metadata of an AOT module
hashdecl AotSection {
    #! the section type
    int type;
    #! the offset of the section's size field in the module file
    int size_pos;
    #! the offset of the section's data in the module file
    int data_pos;
    #! the size of the section
    int size;
}

#! qore#6265: the readers of the sections of an AOT module check their reads against the section bounds
/** A truncated or malformed AOT module is refused with an error, without reading out of its sections or allocating
    for a count that its data cannot hold.  The test compiles a small module with a section of every kind with qcc,
    with uncompressed metadata, and loads patched copies of it, each in a child process:
    - hand-picked cases, in every execution mode: a namespace, class, hashdecl, enum, typedef and constant whose name
      reference gives an empty string, and a namespace whose name reference gives the path of a nested namespace or
      whose path reference gives a string that is not a path (qore#7358), which must be refused; a namespace and an
      embedded source section cut short, their counts set to 0xFFFFFFFF and a string pool without its terminator,
      which must give their errors;
    - the required sections cut in half, in every execution mode, which must be refused;
    - every section truncated at evenly spaced sizes (at every size for a small section), and 0xFFFFFFFF and
      0x7FFFFFFF (the largest count that a signed 32-bit integer holds) written at every offset of every section,
      which sets each count (also those of nested records) in turn to them (at evenly spaced offsets of the large
      optional symbol index and lazily read debug IR): a load must end normally, without a sanitizer report or a
      failed assertion, and with a peak resident memory (on Linux) within a small bound of that of the unpatched
      module; it may load, as some sections are optional.

    The metadata is followed by its checksum (qore#6451), which the reader checks before it reads anything else: the
    checksum of each patched copy is set again (qore-aot-reseal) so that the readers behind it are checked, and one
    case checks that a copy patched without it is refused at the checksum.

    The size of a section is patched in the section directory, so its bytes stay in the file: the test cannot tell
    a read past the end of a section from a valid read, only its consequences (a crash, an assertion, an allocation
    for a count that the data cannot hold).

    On macOS every patched copy is signed again ad hoc before it is loaded, which makes a load several times as
    expensive: there the systematic case takes a sample of every section instead, still deterministic, with at most
    DarwinMaxTruncations truncated sizes and DarwinMaxCountOffsets evenly spaced offsets for each count value per
    section (about 950 loads instead of about 5400); the Linux jobs run the full case.

    The systematic case is divided between the parts (qore#6544): each part loads every SystematicParts-th variant,
    and part 1 also runs the hand-picked cases.
*/
class AotSectionBoundsBase inherits QUnit::Test {
    public {
        #! the magic number of the metadata ("QORD")
        const Magic = binary("QORD");
        #! the size of the header of the metadata
        const HeaderSize = 60;
        #! the size of an entry of the section directory
        const SectionEntrySize = 12;
        #! the section types of the hand-picked cases
        const NamespacesSection = 2;
        const FuncSourcesSection = 13;
        #! the required sections that are cut in half: CLASSES, HASHDECLS, ENUMS, FUNCTIONS, METHODS
        const RequiredSections = (3, 4, 5, 9, 10);
        #! the sections whose first record starts with its name reference, after the count: CLASSES, HASHDECLS,
        #! ENUMS, TYPEDEFS, CONSTANTS
        const NamedSections = (3, 4, 5, 6, 7);
        #! the size of a namespace record: name and path references, parent index, depth, flags
        const NamespaceRecordSize = 18;
        #! the sections whose counts are patched at evenly spaced offsets: SYMBOL_INDEX, DEBUG_IR
        const LargeOptionalSections = (26, 28);
        #! the most truncated sizes tested per section
        const MaxTruncations = 32;
        #! the most offsets patched in a large optional section
        const MaxCountOffsets = 128;
        #! the most memory in kB that a patched load may use beyond the unpatched one
        const MaxExtraKb = 256 * 1024;
        #! the address space in kB that a load may have beyond the unpatched one (2 GB)
        const VmExtraKb = 2 * 1024 * 1024;
        #! the CPU time in seconds that a load may use, so that a load that does not end fails instead of the test
        const CpuLimitSecs = 120;
        #! the values written at every offset of every section
        const CountValues = (0xffffffff, 0x7fffffff);
        #! the number of loads that run at once
        const Workers = 4;
        #! the number of loads that run at once on macOS, where a load also signs the patched copy
        const DarwinWorkers = 6;
        #! the number of parts that the loads of the systematic case are divided between (see addPartCase())
        const SystematicParts = 4;
        #! the most truncated sizes tested per section on macOS
        const DarwinMaxTruncations = 8;
        #! the most offsets patched per section and count value on macOS
        const DarwinMaxCountOffsets = 24;
    }

    private {
        string qore_bin = TestInterpreter::qore_interpreter();
        string qcc_bin;
        string libqore_dir;
        TmpDir tmp("aot-section-bounds-");
        binary qmod;
        int meta_pos;
        #! the length of the metadata of the unpatched module, without its checksum
        int meta_len;
        #! the tool that sets the checksum of patched metadata again
        string reseal_bin;
        int pool_pos;
        int pool_size;
        list<hash<AotSection>> sections = ();
        #! the peak resident memory of the unpatched load, or -1 if unknown
        int base_hwm = -1;
        #! the address space limit of a load in kB, or 0 for none
        int vm_limit = 0;
        int variant_id = 0;
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : QUnit::Test(name, "1.0", \ARGV) {
        self.part = part;
        libqore_dir = ENV.QORE_LIBDIR ?? dirname(qore_bin);
        qcc_bin = TestInterpreter::qcc_compiler() ?? "qcc";
        reseal_bin = ENV.QORE_AOT_RESEAL ?? dirname(qore_bin) + "/qore-aot-reseal";
        addPartCase(1, "hand-picked malformed sections", \handPicked());
        # each part loads every SystematicParts-th variant, so that the parts take about as long
        addPartCase(1, "every section truncated and with an excessive count (part 1)", \systematic(), 0);
        addPartCase(2, "every section truncated and with an excessive count (part 2)", \systematic(), 1);
        addPartCase(3, "every section truncated and with an excessive count (part 3)", \systematic(), 2);
        addPartCase(4, "every section truncated and with an excessive count (part 4)", \systematic(), 3);
        set_return_value(main());
    }

    #! adds the case if it is in the part that this test runs
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    globalSetUp() {
        if (!is_file(qcc_bin)) {
            testSkip("qcc is not available at " + qcc_bin);
        }
        if (!is_file(reseal_bin)) {
            testSkip("qore-aot-reseal is not available at " + reseal_bin);
        }
        string src_dir = tmp.path + "/src";
        mkdir(src_dir);
        copy_file(get_script_dir() + "/AotSectionBoundsMod.qm", src_dir + "/AotSectionBoundsMod.qm");
        string out_path = tmp.path + "/AotSectionBoundsMod.qmod";
        int rc;
        string out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s QORE_MODULE_DIR=%s %s -m "
            "--aot-metadata-compression=none --include-source -o %s %s/AotSectionBoundsMod.qm 2>&1", libqore_dir,
            libqore_dir, src_dir, qcc_bin, out_path, src_dir), \rc);
        if (rc || !is_file(out_path)) {
            throw "QCC-ERROR", sprintf("qcc could not compile the module: %s", out);
        }
        qmod = ReadOnlyFile::readBinaryFile(out_path);
        findMetadata();
        hash<auto> r = load(qmod);
        if (r.out !~ /^loaded\n/) {
            throw "LOAD-ERROR", sprintf("the unpatched module does not load: %s", r.out);
        }
        base_hwm = r.hwm ?? -1;
        # the address space of a load is limited, so that a load that allocates for a count that its data cannot
        # hold fails in the child process instead of exhausting the memory of the machine; sanitizers reserve address
        # space of their own, so it is not limited under them
        if (r.vm && !ENV.ASAN_OPTIONS && !ENV.TSAN_OPTIONS && !ENV.MSAN_OPTIONS) {
            vm_limit = r.vm + VmExtraKb;
        }
    }

    #! finds the metadata in the module: the first "QORD" whose header, section directory and string pool fit
    private findMetadata() {
        int pos = 0;
        while (True) {
            pos = qmod.find(Magic, pos);
            if (pos < 0) {
                throw "NO-METADATA", "no AOT metadata found in the module";
            }
            if (tryMetadata(pos)) {
                return;
            }
            ++pos;
        }
    }

    #! returns True and sets the layout if the metadata at pos is consistent
    private bool tryMetadata(int pos) {
        if (pos + HeaderSize > qmod.size()) {
            return False;
        }
        int count = u32(pos + 16);
        # uncompressed metadata and string pool only
        if (qmod[pos + 34] != 0 || qmod[pos + 35] != 0 || count < 1 || count > 1000) {
            return False;
        }
        int dir_pos = pos + HeaderSize;
        int pool_size_pos = dir_pos + count * SectionEntrySize;
        if (pool_size_pos + 4 > qmod.size()) {
            return False;
        }
        int psize = u32(pool_size_pos);
        int data_pos = pool_size_pos + 4 + psize;
        if (data_pos > qmod.size()) {
            return False;
        }
        list<hash<AotSection>> secs = ();
        int data_end = data_pos;
        for (int i = 0; i < count; ++i) {
            int e = dir_pos + i * SectionEntrySize;
            int offset = u32(e + 4);
            int size = u32(e + 8);
            if (data_pos + offset + size > qmod.size()) {
                return False;
            }
            data_end = max(data_end, data_pos + offset + size);
            secs += <AotSection>{
                "type": u16(e),
                "size_pos": e + 8,
                "data_pos": data_pos + offset,
                "size": size,
            };
        }
        meta_pos = pos;
        meta_len = data_end - pos;
        pool_pos = pool_size_pos + 4;
        pool_size = psize;
        sections = secs;
        return True;
    }

    private int u16(int pos) {
        return qmod[pos] | (qmod[pos + 1] << 8);
    }

    private int u32(int pos) {
        return qmod[pos] | (qmod[pos + 1] << 8) | (qmod[pos + 2] << 16) | (qmod[pos + 3] << 24);
    }

    #! returns the string of the pool with the given reference
    private string poolString(int ref) {
        int start = pool_pos + ref;
        int end = start;
        while (qmod[end]) {
            ++end;
        }
        return qmod.substr(start, end - start).toString();
    }

    #! returns the position of the record of the first namespace of the section whose path has two names or more
    private int nestedNamespaceRecord(hash<AotSection> ns) {
        int count = u32(ns.data_pos);
        for (int i = 0; i < count; ++i) {
            int rec = ns.data_pos + 4 + i * NamespaceRecordSize;
            string path = poolString(u32(rec + 4));
            if (path =~ /^::[^:]+::[^:]+/) {
                return rec;
            }
        }
        throw "NO-NESTED-NAMESPACE", "the module has no nested namespace";
    }

    #! returns a copy of the module with the 4 bytes at pos set to v (little-endian)
    private static binary patch32(binary data, int pos, int v) {
        binary b = data.substr(0, pos);
        b += binary(chr(v & 0xff) + chr((v >> 8) & 0xff) + chr((v >> 16) & 0xff) + chr((v >> 24) & 0xff));
        b += data.substr(pos + 4);
        return b;
    }

    #! returns a copy of the module with the byte at pos set to v
    private static binary patch8(binary data, int pos, int v) {
        return data.substr(0, pos) + binary(chr(v)) + data.substr(pos + 1);
    }

    #! returns a new variant id; the variants of the systematic case get theirs before the loads run in threads
    private int nextId() {
        return ++variant_id;
    }

    #! loads a patched module in a child process and returns its output, exit status and peak memory in kB
    private hash<auto> load(binary data, *string mode, *int id, bool reseal = True, bool with_source = False) {
        string dir = sprintf("%s/v%d", tmp.path, id ?? nextId());
        mkdir(dir);
        if (with_source) {
            copy_file(get_script_dir() + "/AotSectionBoundsMod.qm", dir + "/AotSectionBoundsMod.qm");
        }
        string path = dir + "/AotSectionBoundsMod.qmod";
        File f();
        f.open2(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        f.write(data);
        f.close();
        if (reseal) {
            # the checksum of the patched metadata is set again, so that the readers behind it are checked
            int rc;
            string out = backquote(sprintf("%s %s %d %d 2>&1", reseal_bin, path, meta_pos, meta_len), \rc);
            if (rc) {
                throw "RESEAL-ERROR", sprintf("%s: %s", path, out);
            }
        }
        if (PlatformOS == "Darwin") {
            # a patched module loses its code signature: it is signed again ad hoc
            system(sprintf("codesign -f -s - %s >/dev/null 2>&1", path));
        }
        int rc;
        string limit = sprintf("ulimit -t %d 2>/dev/null; ", CpuLimitSecs)
            + (vm_limit ? sprintf("ulimit -v %d 2>/dev/null; ", vm_limit) : "");
        string out = backquote(sprintf("%sLD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s QORE_MODULE_DIR=%s %s -b %s%s 2>&1",
            limit, libqore_dir, libqore_dir, dir, qore_bin, mode ? "--exec-mode=" + mode + " " : "",
            get_script_dir() + "/loader.q"), \rc);
        *list<*string> m = (out =~ x/hwm: ([0-9]+)/);
        *list<*string> v = (out =~ x/vm: ([0-9]+)/);
        return {"out": out, "rc": rc, "hwm": m ? m[0].toInt() : NOTHING, "vm": v ? v[0].toInt() : NOTHING};
    }

    #! checks that a patched load ended cleanly
    private checkClean(string label, hash<auto> r) {
        # the loader ends normally whether the module loads or not
        assertEq(0, r.rc, sprintf("%s: exit status: %s", label, r.out));
        assertEq(-1, r.out.find("Sanitizer"), label + ": no sanitizer report: " + r.out);
        assertEq(-1, r.out.find("Assertion"), label + ": no failed assertion: " + r.out);
        assertTrue(r.out =~ /(^|\n)(loaded|error: )/, label + ": loaded or refused: " + r.out);
        if (base_hwm > 0 && r.hwm) {
            assertTrue(r.hwm < base_hwm + MaxExtraKb, sprintf("%s: peak memory %d kB (unpatched %d kB)", label,
                r.hwm, base_hwm));
        }
    }

    #! returns the section of the given type
    private hash<AotSection> section(int type) {
        foreach hash<AotSection> sec in (sections) {
            if (sec.type == type) {
                return sec;
            }
        }
        throw "NO-SECTION", sprintf("the module has no section of type %d", type);
    }

    handPicked() {
        hash<AotSection> ns = section(NamespacesSection);
        hash<AotSection> src = section(FuncSourcesSection);
        hash<string, hash<auto>> cases = {
            "namespaces cut short": {"data": patch32(qmod, ns.size_pos, 2), "err": "truncated NAMESPACES"},
            "namespace count": {"data": patch32(qmod, ns.data_pos, 0xffffffff), "err": "truncated NAMESPACES"},
            "embedded source cut short": {"data": patch32(qmod, src.size_pos, 6), "err": "truncated FUNC_SOURCES"},
            "function name count": {"data": patch32(qmod, src.data_pos + 4, 0xffffffff),
                "err": "truncated FUNC_SOURCES"},
            "string pool without its terminator": {"data": patch8(qmod, pool_pos + pool_size - 1, 0x41),
                "err": "the string pool is not terminated"},
        };
        foreach int type in (RequiredSections) {
            hash<AotSection> sec = section(type);
            cases{sprintf("section %d cut in half", type)} = {"data": patch32(qmod, sec.size_pos, sec.size / 2)};
        }
        # a name reference to an empty string of the pool: its last byte, the terminator, which every pool has; the
        # reference of another string can give one too, depending on the strings of the pool, which depend on the
        # platform and on the path of the source (qore#6265)
        int empty_ref = pool_size - 1;
        cases."namespace without a name" = {
            "data": patch32(qmod, ns.data_pos + 4 + NamespaceRecordSize, empty_ref),
            "err": "has no name",
        };
        # a name or path reference into a string of the pool that is not a name or path of a namespace, such as the
        # path of a nested namespace, from which the namespace cannot be made (qore#7358); a mutation of a reference
        # gives one depending on the strings of the pool, so the case uses the path of the module's nested namespace
        {
            int rec = nestedNamespaceRecord(ns);
            int path_ref = u32(rec + 4);
            cases."namespace name with its path" = {
                "data": patch32(qmod, rec, path_ref),
                "err": "has a name that is not valid",
            };
            cases."namespace path that is not valid" = {
                "data": patch32(qmod, rec + 4, path_ref + 1),
                "err": "has a path that is not valid",
            };
        }
        foreach int type in (NamedSections) {
            hash<AotSection> sec = section(type);
            cases{sprintf("section %d record without a name", type)} = {
                "data": patch32(qmod, sec.data_pos + 4, empty_ref),
                "err": "has no name",
            };
        }
        # metadata patched without its checksum set again is refused at the checksum, before it is read (qore#6451)
        foreach string mode in (ExecModeHarness::ExecModes) {
            string label = sprintf("patched without its checksum (%s)", mode);
            hash<auto> r = load(patch32(qmod, ns.data_pos, 0xffffffff), mode, NOTHING, False);
            checkClean(label, r);
            assertTrue(r.out.find("does not match its checksum") >= 0, label + ": error: " + r.out);
        }
        # with the source module beside it, the source module is loaded instead, with a warning that names the cause,
        # as for an AOT module that cannot be loaded
        {
            hash<auto> r = load(patch32(qmod, ns.data_pos, 0xffffffff), NOTHING, NOTHING, False, True);
            checkClean("patched without its checksum, with the source", r);
            assertTrue(r.out =~ /(^|\n)loaded\n/, "the source module is loaded: " + r.out);
            assertTrue(r.out.find("does not match its checksum") >= 0, "the warning names the checksum: " + r.out);
        }
        # qcc, which reads the metadata of a file without loading it, refuses it at the checksum too
        {
            string path = tmp.path + "/unsealed.qmod";
            File f();
            f.open2(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            f.write(patch32(qmod, ns.data_pos, 0xffffffff));
            f.close();
            string out = backquote(sprintf("LD_LIBRARY_PATH=%s DYLD_LIBRARY_PATH=%s %s --dump-info %s 2>&1",
                libqore_dir, libqore_dir, qcc_bin, path));
            assertTrue(out =~ /invalid: the AOT metadata does not match its checksum/,
                "qcc --dump-info of metadata patched without its checksum: " + out);
        }
        foreach hash<auto> c in (cases.pairIterator()) {
            foreach string mode in (ExecModeHarness::ExecModes) {
                string label = sprintf("%s (%s)", c.key, mode);
                hash<auto> r = load(c.value.data, mode);
                checkClean(label, r);
                assertTrue(r.out =~ /(^|\n)error: /, label + ": refused: " + r.out);
                if (c.value.err) {
                    assertTrue(r.out.find(c.value.err) >= 0, label + ": error: " + r.out);
                }
            }
        }
    }

    #! loads the variants whose index modulo SystematicParts is the given slice
    systematic(int slice) {
        # macOS takes a sample of every section (see the class description)
        bool sample = PlatformOS == "Darwin";
        int max_truncations = sample ? DarwinMaxTruncations : MaxTruncations;
        int workers = sample ? DarwinWorkers : Workers;
        # each variant is a 4-byte patch of the module, made in the worker that loads it
        list<hash<auto>> variants = ();
        foreach hash<AotSection> sec in (sections) {
            # evenly spaced truncated sizes, every size for a small section
            int step = max(1, sec.size / max_truncations);
            for (int size = 0; size < sec.size; size += step) {
                variants += {"label": sprintf("section %d truncated to %d of %d bytes", sec.type, size, sec.size),
                    "pos": sec.size_pos, "value": size, "id": nextId()};
            }
            # each count value at every offset, which sets every count in turn to it; evenly spaced offsets in a
            # large optional section, and in every section on macOS
            int last = sec.size - 4;
            int max_offsets = sample ? DarwinMaxCountOffsets
                : (inlist(sec.type, LargeOptionalSections) ? MaxCountOffsets : 0);
            step = (max_offsets && last >= max_offsets) ? last / max_offsets : 1;
            foreach int value in (CountValues) {
                for (int off = 0; off <= last; off += step) {
                    variants += {"label": sprintf("section %d with 0x%X at offset %d", sec.type, value, off),
                        "pos": sec.data_pos + off, "value": value, "id": nextId()};
                }
            }
        }
        # this part's share of the variants, every SystematicParts-th of the whole list
        variants = select variants, $# % SystematicParts == slice;
        # the loads run in worker threads, each in a child process
        Queue q();
        map q.push($1), variants;
        # a NOTHING ends a worker
        for (int i = 0; i < workers; ++i) {
            q.push(NOTHING);
        }
        Mutex m();
        list<hash<auto>> results = ();
        Counter done(workers);
        for (int i = 0; i < workers; ++i) {
            background sub () {
                on_exit done.dec();
                while (True) {
                    *hash<auto> v = q.get();
                    if (!v) {
                        break;
                    }
                    hash<auto> r = load(patch32(qmod, v.pos, v.value), NOTHING, v.id);
                    m.lock();
                    on_exit m.unlock();
                    results += {"label": v.label, "r": r};
                }
            }();
        }
        done.waitForZero();
        assertEq(variants.size(), results.size(), "every variant was loaded");
        foreach hash<auto> res in (results) {
            checkClean(res.label, res.r);
        }
    }
}
