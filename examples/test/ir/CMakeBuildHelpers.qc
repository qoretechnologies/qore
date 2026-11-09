# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the CMakeBuildHelpers-<n>.qtest tests: each runs one part of them, so that each takes a
# short time, and together they run every case (see addPartCase()); the helpers are those of
# CMakeBuildHelpersTestBase.qm, which the generic qcc object helper scenarios
# (CMakeBuildHelpers-generic-*.qtest) and the incremental preload cases (CMakeBuildHelpers-preload.qtest) share

class CMakeBuildHelpersBase inherits CMakeBuildHelpersTestBase::CMakeBuildTest {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : CMakeBuildTest(name, "1.0", \ARGV) {
        self.part = part;

        addPartCase(1, "exported Qore CMake build helpers", \testExportedHelpers());
        addPartCase(1, "a build tree reached through another path is reported, not loaded as an SDK",
            \testMovedBuildTree());
        addPartCase(1, "downstream catalog install root",
            \testDownstreamCatalogInstallRoot());
        addPartCase(1, "catalog install reconciles obsolete owner fragments",
            \testCatalogInstallReconciliation());
        addPartCase(1, "catalog reconciliation retires orphans it owns by name",
            \testCatalogOwnedOrphanRetirement());
        addPartCase(1, "catalog reconciliation refuses unsafe manifest entries",
            \testCatalogManifestEntryValidation());
        addPartCase(1, "catalog reconciliation honours DESTDIR staging",
            \testCatalogInstallDestdirStaging());
        addPartCase(2, "managed qcc outputs prune stale artifacts safely",
            \testManagedQccOutputPruning());
        addPartCase(1, "source qmod links reject stale format artifacts",
            \testSourceQmodSymlinkFreshness());
        addPartCase(1, "source qmod link cleanup keeps other build trees' links",
            \testSourceQmodSymlinkOtherBuildTree());
        addPartCase(1, "module source globs track added and removed sources",
            \testModuleSourceGlobTracking());
        addPartCase(1, "qcc format fingerprint covers every AOT format source",
            \testFormatFingerprintCoverage());
        addPartCase(1, "qcc build inputs track libqore for an installed qore",
            \testQccDepsTrackInstalledLibqore());
        addPartCase(1, "external AOT qmods rebuild for a new Qore library and use its qcc runtime",
            \testExternalQmodsTrackQoreRuntime());
        addPartCase(2, "cyclic source components build as one atomic generation",
            \testCyclicComponentBuilds());
        addPartCase(3, "a group watches what its members read from outside it through one depfile",
            \testGroupExternalInputsDepfile());
        addPartCase(3, "a partial aggregate of a group links the contracts its members baked",
            \testPartialAggregateBodyContracts());

        set_return_value(main());
    }

    #! adds a case if it is in the part that this test runs: each case is in one part, and each part is a test
    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! The module source names the generated install script installs for \a module
    /** Read from the install script rather than from an installation so that the
        list reflects what configure decided, which is where the glob result lands.
    */
    private list<string> installedModuleSources(string build_dir, string module) {
        hash<string, bool> names;
        string marker = "/qlib/" + module + "/";
        foreach string token in (read(build_dir + "/cmake_install.cmake").split("\"")) {
            int ix = token.find(marker);
            if (ix < 0) {
                continue;
            }
            string name = token.substr(ix + marker.size());
            if (name =~ /^[-A-Za-z0-9_]+\.(qm|qc)$/) {
                names{name} = True;
            }
        }
        return sort(keys names);
    }

    #! Adding or removing a module source must re-run configure by itself
    /** A split-dir module's file list is a configure-time glob, and it feeds
        install(FILES), the doc/metadata rules and the AOT .qmod rule's DEPENDS.
        Without CONFIGURE_DEPENDS a newly added .qc reaches none of them: it is
        never installed -- leaving an installed source tree that cannot parse --
        and the .qmod is not rebuilt even though \c qcc \c -m compiles the whole
        directory, so the AOT artifact goes stale against a file it consumes.
    */
    testModuleSourceGlobTracking() {
        TmpDir tmp("qore-module-source-glob-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string stage_dir = tmp.path + "/stage";
        string module_dir = src_dir + "/qlib/Fixture";
        # an absolute module dir that cannot exist, so only the DESTDIR-staged
        # installation below can satisfy the assertions
        string module_install_dir = "/opt/qore-fixture-modules";
        mkdir(module_dir, 0755, True);
        write(module_dir + "/Fixture.qm", "%modern\n"
            "module Fixture {\n"
            "    version = \"1.0\";\n"
            "    desc = \"module source glob fixture\";\n"
            "    author = \"Qore Technologies, s.r.o.\";\n"
            "    license = \"MIT\";\n"
            "}\n");
        write(module_dir + "/FixtureOne.qc", "%modern\n"
            "public namespace Fixture;\n"
            "public class FixtureOne {\n"
            "    static int one() { return 1; }\n"
            "}\n");
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreModuleSourceGlob NONE)
find_package(Qore REQUIRED)
qore_user_modules(\"qlib/Fixture\")
");

        string configure = run(sprintf("cmake -S %s -B %s -DQORE_USER_MODULE_DIR=%s%s",
            shellQuote(src_dir), shellQuote(build_dir), shellQuote(module_install_dir),
            qore_dir ? " -DQore_DIR=" + shellQuote(qore_dir) : ""));
        assertTrue(is_file(build_dir + "/cmake_install.cmake"),
            "the fixture module project configures: " + configure);
        assertEq(("Fixture.qm", "FixtureOne.qc"), installedModuleSources(build_dir, "Fixture"),
            "configure installs exactly the module sources that exist: " + configure);

        # the glob must be re-checked on every build, not only on reconfigure
        string verify_globs = build_dir + "/CMakeFiles/VerifyGlobs.cmake";
        assertTrue(is_file(verify_globs),
            "the module source glob is verified by the generated build system: " + configure);
        assertTrue(read(verify_globs).find(module_dir + "/*.qc") >= 0,
            "the module's .qc glob is among the globs verified on every build");

        # adding a source must take effect without an explicit reconfigure
        write(module_dir + "/FixtureTwo.qc", "%modern\n"
            "public namespace Fixture;\n"
            "public class FixtureTwo {\n"
            "    static int two() { return 2; }\n"
            "}\n");
        string build = run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertEq(("Fixture.qm", "FixtureOne.qc", "FixtureTwo.qc"),
            installedModuleSources(build_dir, "Fixture"),
            "a source added to the module joins the install list on the next build: " + build);

        string install = run(sprintf("DESTDIR=%s cmake --install %s",
            shellQuote(stage_dir), shellQuote(build_dir)));
        assertTrue(is_file(stage_dir + module_install_dir + "/Fixture/FixtureTwo.qc"),
            "the added module source is installed with the module: " + install);
        assertEq(read(module_dir + "/FixtureTwo.qc"),
            read(stage_dir + module_install_dir + "/Fixture/FixtureTwo.qc"),
            "the installed module source preserves its content");

        # and removing one must drop it again
        unlink(module_dir + "/FixtureTwo.qc");
        string rebuild = run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertEq(("Fixture.qm", "FixtureOne.qc"), installedModuleSources(build_dir, "Fixture"),
            "a source removed from the module leaves the install list: " + rebuild);
    }

    testSourceQmodSymlinkFreshness() {
        TmpDir tmp("qore-source-qmod-links-");
        string source_qlib = tmp.path + "/qlib";
        string build_qmods = tmp.path + "/qlib-qmod";
        string source_module = source_qlib + "/Fixture";
        string build_module = build_qmods + "/Fixture";
        string source = source_module + "/Fixture.qm";
        string qmod = build_module + "/Fixture.qmod";
        string link = source_module + "/Fixture.qmod";
        string format_stamp = tmp.path + "/qcc-format.stamp";
        string script = realpath(get_script_dir()
            + "/../../../cmake/EnsureSourceQmodSymlinks.cmake");
        mkdir(source_module, 0755, True);
        mkdir(build_module, 0755, True);
        write(source, "%modern\nmodule Fixture { version = \"1.0\"; }\n");
        write(qmod, "fixture qmod\n");
        write(format_stamp, "format=1\n");
        symlink(qmod, link);

        TestFileTime::set_file_times(2026-01-01T00:00:00Z, qmod);
        TestFileTime::set_file_times(2026-01-01T00:01:00Z, format_stamp);
        assertTrue(mtime(qmod) < mtime(format_stamp), "the fixture establishes deterministic stale timestamps");
        string stale = run(sprintf(
            "cmake -DQORE_SOURCE_QLIB_DIR=%s -DQORE_BUILD_QMOD_DIR=%s "
                + "-DQORE_QCC_FORMAT_STAMP=%s -P %s",
            shellQuote(source_qlib), shellQuote(build_qmods), shellQuote(format_stamp), shellQuote(script)));
        assertFalse(is_file(link),
            "a qmod older than the format stamp is unlinked: " + stale);
        assertTrue(stale.find("1 stale artifact(s) skipped") >= 0,
            "stale qmod rejection is visible in the build diagnostic: " + stale);

        TestFileTime::set_file_times(2026-01-01T00:02:00Z, qmod);
        assertTrue(mtime(qmod) > mtime(format_stamp),
            "the fixture advances the rebuilt qmod timestamp deterministically");
        string fresh = run(sprintf(
            "cmake -DQORE_SOURCE_QLIB_DIR=%s -DQORE_BUILD_QMOD_DIR=%s "
                + "-DQORE_QCC_FORMAT_STAMP=%s -P %s",
            shellQuote(source_qlib), shellQuote(build_qmods), shellQuote(format_stamp), shellQuote(script)));
        assertEq(qmod, readlink(link),
            "a rebuilt qmod is linked after the format stamp: " + fresh);
    }

    #! Tests that one build tree's qmod link cleanup leaves the links of another build tree
    /** Regression test: the qcc format-change cleanup of one build tree removed every source qmod link, so building
        a second tree (such as a debug build that does not link source modules) silently took the AOT qmods of the
        first tree away from its tests.
    */
    testSourceQmodSymlinkOtherBuildTree() {
        TmpDir tmp("qore-source-qmod-trees-");
        string source_qlib = tmp.path + "/qlib";
        string remove_script = realpath(get_script_dir() + "/../../../cmake/RemoveSourceQmodSymlinks.cmake");
        string ensure_script = realpath(get_script_dir() + "/../../../cmake/EnsureSourceQmodSymlinks.cmake");

        # module A is linked into tree "release", module B into tree "debug"
        hash<string, string> links;
        hash<string, string> qmods;
        foreach hash<auto> h in ({"module": "ModA", "tree": "release"}, {"module": "ModB", "tree": "debug"}) {
            string source_module = source_qlib + "/" + h.module;
            string build_module = tmp.path + "/" + h.tree + "/qlib-qmod/" + h.module;
            mkdir(source_module, 0755, True);
            mkdir(build_module, 0755, True);
            write(source_module + "/" + h.module + ".qm",
                sprintf("%%modern\nmodule %s { version = \"1.0\"; }\n", h.module));
            qmods{h.module} = build_module + "/" + h.module + ".qmod";
            write(qmods{h.module}, "fixture qmod\n");
            links{h.module} = source_module + "/" + h.module + ".qmod";
        }
        # tree "debug" also built module A, but its link belongs to tree "release"
        string debug_moda = tmp.path + "/debug/qlib-qmod/ModA/ModA.qmod";
        mkdir(dirname(debug_moda), 0755, True);
        write(debug_moda, "fixture qmod\n");
        symlink(qmods.ModA, links.ModA);
        symlink(qmods.ModB, links.ModB);
        # a relative link into tree "debug" is recognized as well
        string source_modc = source_qlib + "/ModC";
        mkdir(source_modc, 0755, True);
        mkdir(tmp.path + "/debug/qlib-qmod/ModC", 0755, True);
        write(tmp.path + "/debug/qlib-qmod/ModC/ModC.qmod", "fixture qmod\n");
        symlink("../../debug/qlib-qmod/ModC/ModC.qmod", source_modc + "/ModC.qmod");

        # the format-change cleanup of tree "debug" removes only its own links
        string out = run(sprintf("cmake -DQORE_SOURCE_QLIB_DIR=%s -DQORE_BUILD_QMOD_DIR=%s -P %s",
            shellQuote(source_qlib), shellQuote(tmp.path + "/debug/qlib-qmod"), shellQuote(remove_script)));
        assertTrue(is_link(links.ModA), "the link into the other build tree is kept: " + out);
        assertEq(qmods.ModA, readlink(links.ModA), "the link into the other build tree is unchanged: " + out);
        assertFalse(is_link(links.ModB), "the link into the cleaning build tree is removed: " + out);
        assertFalse(is_link(source_modc + "/ModC.qmod"), "a relative link into the cleaning tree is removed: " + out);
        assertTrue(out.find("Removed 2 generated source qmod symlink(s)") >= 0, out);

        # the stale-artifact check of tree "debug" does not remove tree "release"'s link to a fresh qmod
        string format_stamp = tmp.path + "/debug/qcc-format.stamp";
        write(format_stamp, "format=1\n");
        TestFileTime::set_file_times(2026-01-01T00:00:00Z, debug_moda);
        TestFileTime::set_file_times(2026-01-01T00:01:00Z, format_stamp);
        assertTrue(mtime(debug_moda) < mtime(format_stamp),
            "the fixture establishes deterministic stale timestamps");
        out = run(sprintf("cmake -DQORE_SOURCE_QLIB_DIR=%s -DQORE_BUILD_QMOD_DIR=%s -DQORE_QCC_FORMAT_STAMP=%s -P %s",
            shellQuote(source_qlib), shellQuote(tmp.path + "/debug/qlib-qmod"), shellQuote(format_stamp),
            shellQuote(ensure_script)));
        assertTrue(is_link(links.ModA), "a stale artifact in another tree keeps the link: " + out);
        assertEq(qmods.ModA, readlink(links.ModA), "a stale artifact in another tree leaves the link alone: " + out);

        # the cleanup requires the build tree whose links it removes
        assertNeq(0, system(sprintf("cmake -DQORE_SOURCE_QLIB_DIR=%s -P %s > /dev/null 2>&1",
            shellQuote(source_qlib), shellQuote(remove_script))), "the cleanup refuses to run without a build tree");
        assertTrue(is_link(links.ModA), "a refused cleanup removes nothing");
    }

    #! a build tree's QoreConfig.cmake loaded from another real path says so, with both paths
    /** The build tree's config records the paths it was configured at.  Loaded from another path, as from a moved
        build tree or one mounted elsewhere in a container, those paths do not exist, and the config used to fall
        through to the installed-SDK settings, which then failed to include a QoreMacros.cmake that only an install
        has.  The config files copied to another directory stand in for a remount; a symlink to the build tree's
        cmake directory resolves to the same real path, so it keeps working.
    */
    testMovedBuildTree() {
        if (!qore_dir || !is_file(qore_dir + "/QoreBuildTree.cmake")) {
            testSkip("the build tree's QoreConfig.cmake is not available for the build under test");
        }
        TmpDir tmp("qore-moved-build-tree-");
        string src_dir = tmp.path + "/src";
        string moved_dir = tmp.path + "/moved";
        string link_dir = tmp.path + "/link";
        mkdir(src_dir);
        mkdir(moved_dir);
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreMovedBuildTree NONE)
find_package(Qore REQUIRED)
message(STATUS \"qore cmake dir: ${QORE_CMAKE_DIR}\")
");
        foreach string file in (("QoreConfig.cmake", "QoreConfigVersion.cmake", "QoreBuildTree.cmake")) {
            write(moved_dir + "/" + file, read(qore_dir + "/" + file));
        }
        symlink(qore_dir, link_dir);

        string linked = run(sprintf("cmake -S %s -B %s -DQore_DIR=%s", shellQuote(src_dir),
            shellQuote(tmp.path + "/build-link"), shellQuote(link_dir)));
        assertRegex("Configuring done", linked, "a symlink to the build tree's config configures: " + linked);
        assertRegex("qore cmake dir: ", linked, "a symlink to the build tree's config loads it: " + linked);

        string moved = run(sprintf("cmake -S %s -B %s -DQore_DIR=%s", shellQuote(src_dir),
            shellQuote(tmp.path + "/build-moved"), shellQuote(moved_dir)));
        # CMake wraps the reason given by the package, so the words are compared without the line breaks
        string flat = (moved =~ s/\s+/ /g);
        assertRegex("Configuring incomplete", moved, "a moved build tree does not configure: " + moved);
        assertNeq(-1, flat.find("the Qore build tree was moved or remounted"),
            "the reason is given: " + moved);
        assertNeq(-1, flat.find("configured at \"" + realpath(qore_dir) + "\""),
            "the path it was configured at is given: " + moved);
        assertNeq(-1, flat.find("loaded from \"" + realpath(moved_dir) + "\""),
            "the path it was loaded from is given: " + moved);
        assertNeq(-1, flat.find("configure and build Qore again at the new path"),
            "what to do is given: " + moved);
        assertEq(-1, moved.find("QoreMacros.cmake"), "the installed-SDK settings are not loaded: " + moved);
    }

    testDownstreamCatalogInstallRoot() {
        TmpDir tmp("qore-cmake-downstream-catalog-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string install_dir = tmp.path + "/install";
        string catalog_dir = src_dir + "/catalogs/provider.example";
        mkdir(src_dir);
        mkdir(src_dir + "/catalogs");
        mkdir(catalog_dir);
        write(catalog_dir + "/root.json",
            "{\"schema_version\":1,\"locales\":{\"root\":{\"messages\":{}}}}\n");
        # the record of the Qore that generated the catalogs (qore-data-provider-i18n) is beside them
        write(src_dir + "/catalogs/.qore-generator.json",
            "{\"qore-data-provider-i18n\":{\"version\":\"3.0.0\",\"git_hash\":\"0123456789abcdef\"}}\n");
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreDownstreamCatalogInstall NONE)
find_package(Qore REQUIRED)
QORE_INSTALL_USER_MODULE_CATALOGS(OutsideModule
    \"${CMAKE_SOURCE_DIR}/catalogs\" DownstreamCatalogs
    \"share/downstream/i18n\")
");

        string configure = run(sprintf(
            "cmake -S %s -B %s -DCMAKE_INSTALL_PREFIX=%s%s",
            src_dir, build_dir, install_dir,
            qore_dir ? " -DQore_DIR=" + qore_dir : ""));
        assertTrue(is_file(build_dir + "/cmake_install.cmake"),
            "downstream catalog project configures: " + configure);
        string install = run(sprintf(
            "cmake --install %s --component DownstreamCatalogs", build_dir));
        string installed = install_dir
            + "/share/downstream/i18n/provider.example/root/OutsideModule.json";
        assertTrue(is_file(installed),
            "catalog installs below the downstream prefix: " + install);
        assertEq(read(catalog_dir + "/root.json"), read(installed),
            "installed downstream catalog preserves its content");
        list<string> installed_files = filesBelow(install_dir);
        assertEq(("share/downstream/i18n/provider.example/root/OutsideModule.json",),
            installed_files,
            "the generator record is not a catalog and is not installed");

        string install_script = read(build_dir + "/cmake_install.cmake");
        assertTrue(install_script.find("share/downstream/i18n") != -1,
            "the generated install rule retains the downstream catalog root");
    }

    #! Writes a fixture project installing owner-qualified catalog fragments
    /** @param src_dir project source directory; recreated from scratch, so
            calling this again with fewer owners models a later source generation
        @param project CMake project name, which is also the catalog manifest key
        @param owners owner module -> app name -> presentation field -> source text

        The fixture points \c QORE_CATALOG_DIR at its own install prefix, so its
        catalog root is Qore-managed as far as the install rules are concerned --
        that is what makes the obsolete flat-layout sweep apply to it.
    */
    private writeCatalogProject(string src_dir, string project,
            hash<string, hash<string, hash<string, string>>> owners) {
        if (is_dir(src_dir)) {
            remove_tree(src_dir);
        }
        mkdir(src_dir, 0755, True);
        string installs;
        foreach hash<auto> owner in (owners.pairIterator()) {
            foreach hash<auto> app in (owner.value.pairIterator()) {
                hash<auto> messages = {};
                foreach hash<auto> field in (app.value.pairIterator()) {
                    messages{DataProviderPresentation::appMessageId(app.key,
                        field.key)} = field.value;
                }
                string domain_dir = src_dir + "/catalogs/" + owner.key + "/"
                    + catalogDomain(app.key);
                mkdir(domain_dir, 0755, True);
                write(domain_dir + "/root.json", make_json({
                    "schema_version": I18n::get_catalog_schema_version(),
                    "locales": {"root": {"messages": messages}},
                }) + "\n");
            }
            installs += sprintf("QORE_INSTALL_USER_MODULE_CATALOGS(%s "
                "\"${CMAKE_SOURCE_DIR}/catalogs/%s\" FixtureCatalogs)\n",
                owner.key, owner.key);
        }
        write(src_dir + "/CMakeLists.txt", sprintf(
"cmake_minimum_required(VERSION 3.16)
project(%s NONE)
set(QORE_CATALOG_DIR \"${CMAKE_INSTALL_PREFIX}/share/qore/i18n\")
find_package(Qore REQUIRED)
%sQORE_FINALIZE_CATALOG_INSTALL()
", project, installs));
    }

    #! The catalog domain of an app, derived the same way the providers derive it
    private string catalogDomain(string app) {
        return DataProviderPresentation::appCatalogDomain({"name": app});
    }

    #! Configures and installs one generation of a fixture catalog project
    /** @param src_dir project source directory
        @param build_dir build directory, reused across generations
        @param install_dir installation prefix owning the catalog root
        @param destdir when set, stage the installation below this directory

        @return the install output, so assertions can quote what the install did
    */
    private string installCatalogProject(string src_dir, string build_dir,
            string install_dir, *string destdir) {
        string configure = run(sprintf(
            "cmake -S %s -B %s -DCMAKE_INSTALL_PREFIX=%s%s",
            src_dir, build_dir, install_dir,
            qore_dir ? " -DQore_DIR=" + qore_dir : ""));
        assertTrue(is_file(build_dir + "/cmake_install.cmake"),
            "catalog fixture project configures: " + configure);
        return run(sprintf("%scmake --install %s --component FixtureCatalogs",
            destdir ? "DESTDIR=" + destdir + " " : "", build_dir));
    }

    #! The fragment paths recorded in an installed catalog manifest
    #! Returns the paths of the files below a directory, relative to it, sorted
    private list<string> filesBelow(string dir, string prefix = "") {
        list<string> rv = ();
        Dir d();
        if (!d.chdir(dir)) {
            return rv;
        }
        foreach string file in (d.listFiles()) {
            rv += prefix + file;
        }
        foreach string sub_dir in (d.listDirs()) {
            rv += filesBelow(dir + "/" + sub_dir, prefix + sub_dir + "/");
        }
        return sort(rv);
    }

    private list<string> manifestEntries(string path) {
        list<string> rv = ();
        foreach string line in (read(path).split("\n")) {
            string entry = trim(line);
            if (!entry.val() || entry =~ /^#/) {
                continue;
            }
            rv += entry;
        }
        return rv;
    }

    #! Composes every installed catalog domain below \a root
    /** This is what obsolete fragments break: discovery finds the current and
        the obsolete definition of one message id and composition refuses to
        choose between them.
    */
    private composeInstalledCatalogs(string root) {
        Dir d();
        d.chdir(root);
        list<string> domains = d.listDirs();
        assertTrue(domains.size() > 1,
            "the fixture installs more than one catalog domain");
        foreach string domain in (sort(domains)) {
            if (domain == ".qore-catalog-manifests") {
                continue;
            }
            list<string> files = I18n::discover_catalog_files(domain, "root",
                {"search_path": (root,)});
            assertTrue(files.size() > 0,
                sprintf("domain %y has discoverable fragments", domain));
            DataProviderPresentationCatalog catalog(domain, "root",
                {"search_path": (root,)});
            assertEq(DataProviderPresentation::RootLocale, catalog.getLocale(),
                sprintf("domain %y composes after install", domain));
        }
    }

    #! The composed text of one app presentation field below \a root
    /** Returns \c "<absent>" when no discovered fragment defines the field, so a
        fragment that reconciliation should have installed cannot pass unnoticed.
    */
    private string composedText(string root, string app, string field) {
        list<string> files = I18n::discover_catalog_files(catalogDomain(app),
            "root", {"search_path": (root,)});
        I18n::MessageCatalog catalog(files, {"literal_messages": True});
        return catalog.tc(DataProviderPresentation::appMessageId(app, field),
            "<absent>", "root");
    }

    testCatalogInstallReconciliation() {
        TmpDir tmp("qore-cmake-catalog-reconcile-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string other_src_dir = tmp.path + "/other-src";
        string other_build_dir = tmp.path + "/other-build";
        string install_dir = tmp.path + "/install";
        string root = install_dir + "/share/qore/i18n";
        string manifest = root
            + "/.qore-catalog-manifests/QoreCatalogFixtureGeneration.manifest";
        string example_domain = catalogDomain("Example");
        string other_domain = catalogDomain("Other");

        # residue of a pre-fragment-layout install of the same root: no current
        # generation installs <domain>/<locale>.json, so it is obsolete by
        # definition and only the bootstrap sweep can retire it
        string flat_dir = root + "/" + catalogDomain("Legacy");
        mkdir(flat_dir, 0755, True);
        write(flat_dir + "/root.json", make_json({
            "schema_version": I18n::get_catalog_schema_version(),
            "locales": {"root": {"messages": {
                DataProviderPresentation::appMessageId("Legacy", "short_desc"):
                    "retired app",
            }}},
        }) + "\n");

        # generation A: OldOwner owns both apps
        writeCatalogProject(src_dir, "QoreCatalogFixtureGeneration", {
            "OldOwner": {
                "Example": {"short_desc": "generation A Example"},
                "Other": {"short_desc": "unrelated app"},
            },
        });
        string install_a = installCatalogProject(src_dir, build_dir, install_dir);
        assertTrue(is_file(root + "/" + example_domain + "/root/OldOwner.json"),
            "generation A installs its owner fragment: " + install_a);
        assertFalse(is_file(flat_dir + "/root.json"),
            "the obsolete flat catalog is swept on the first reconcile: "
            + install_a);
        assertFalse(is_dir(flat_dir),
            "the domain directory emptied by the sweep is pruned");
        assertEq((example_domain + "/root/OldOwner.json",
            other_domain + "/root/OldOwner.json"), manifestEntries(manifest),
            "the manifest records exactly what generation A installed");

        # a different project contributes a disjoint fragment to the same domain
        # in the same root -- the case the owner-qualified layout exists for
        writeCatalogProject(other_src_dir, "QoreCatalogFixtureOtherProject", {
            "OtherOwner": {"Example": {"desc": "owned by another project"}},
        });
        string install_other = installCatalogProject(other_src_dir,
            other_build_dir, install_dir);
        string other_project_fragment =
            root + "/" + example_domain + "/root/OtherOwner.json";
        assertTrue(is_file(other_project_fragment),
            "another project installs into the same catalog root: "
            + install_other);

        # generation B: Example migrates to NewOwner with different presentation
        writeCatalogProject(src_dir, "QoreCatalogFixtureGeneration", {
            "OldOwner": {"Other": {"short_desc": "unrelated app"}},
            "NewOwner": {"Example": {"short_desc": "generation B Example"}},
        });
        string install_b = installCatalogProject(src_dir, build_dir, install_dir);
        assertFalse(is_file(root + "/" + example_domain + "/root/OldOwner.json"),
            "the previous owner's fragment is removed: " + install_b);
        assertTrue(is_file(root + "/" + example_domain + "/root/NewOwner.json"),
            "the new owner's fragment is installed: " + install_b);
        assertTrue(is_file(root + "/" + other_domain + "/root/OldOwner.json"),
            "an app still owned by OldOwner keeps its fragment");
        assertTrue(is_file(other_project_fragment),
            "reconciliation never removes another project's fragment");
        assertEq("generation B Example", composedText(root, "Example",
            "short_desc"),
            "the installed catalog composes to the current owner's text");
        assertEq("owned by another project", composedText(root, "Example",
            "desc"),
            "the other project's contribution still composes into the domain");
        composeInstalledCatalogs(root);
        assertEq((example_domain + "/root/NewOwner.json",
            other_domain + "/root/OldOwner.json"), manifestEntries(manifest),
            "the manifest tracks the new generation's fragments");

        # an unchanged source tree must reconcile to exactly the same state
        string manifest_before = read(manifest);
        string install_again = installCatalogProject(src_dir, build_dir,
            install_dir);
        assertEq(-1, install_again.find("removed"),
            "an unchanged reinstall removes nothing: " + install_again);
        assertEq(manifest_before, read(manifest),
            "an unchanged reinstall leaves the manifest byte-identical");
        assertTrue(is_file(other_project_fragment),
            "an unchanged reinstall preserves the other project's fragment");
    }

    testCatalogOwnedOrphanRetirement() {
        TmpDir tmp("qore-cmake-catalog-orphan-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string install_dir = tmp.path + "/install";
        string root = install_dir + "/share/qore/i18n";
        string orphan_domain = catalogDomain("Retired");

        # a fragment left by a generation that predates any manifest: no manifest
        # diff can see it, so only the owner name identifies it as this project's
        string orphan = root + "/" + orphan_domain + "/root/OldOwner.json";
        mkdir(dirname(orphan), 0755, True);
        write(orphan, make_json({
            "schema_version": I18n::get_catalog_schema_version(),
            "locales": {"root": {"messages": {
                DataProviderPresentation::appMessageId("Retired", "short_desc"):
                    "an app this owner no longer describes",
            }}},
        }) + "\n");

        # a fragment of the same vintage owned by a module this project does not
        # install catalogs for; it must survive, because its owner does
        string foreign = root + "/" + orphan_domain + "/root/ForeignOwner.json";
        write(foreign, make_json({
            "schema_version": I18n::get_catalog_schema_version(),
            "locales": {"root": {"messages": {
                DataProviderPresentation::appMessageId("Retired", "desc"):
                    "owned by a module from another project",
            }}},
        }) + "\n");

        writeCatalogProject(src_dir, "QoreCatalogFixtureOrphan", {
            "OldOwner": {"Example": {"short_desc": "current app"}},
        });
        string install = installCatalogProject(src_dir, build_dir, install_dir);
        assertFalse(is_file(orphan),
            "a fragment this project owns by name but no longer installs is "
            "retired even on the first reconcile: " + install);
        assertTrue(is_file(foreign),
            "a fragment owned by another project's module survives");
        assertTrue(is_file(root + "/" + catalogDomain("Example")
            + "/root/OldOwner.json"),
            "the current generation's fragment is installed");
        assertEq("owned by a module from another project",
            composedText(root, "Retired", "desc"),
            "the surviving fragment still composes");
    }

    testCatalogManifestEntryValidation() {
        TmpDir tmp("qore-cmake-catalog-manifest-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string install_dir = tmp.path + "/install";
        string root = install_dir + "/share/qore/i18n";
        string manifest = root
            + "/.qore-catalog-manifests/QoreCatalogFixtureGuarded.manifest";
        string domain = catalogDomain("Guarded");

        writeCatalogProject(src_dir, "QoreCatalogFixtureGuarded", {
            "OldOwner": {"Guarded": {"short_desc": "guarded app"}},
        });
        installCatalogProject(src_dir, build_dir, install_dir);
        assertTrue(is_file(manifest), "the first install publishes a manifest");

        # a manifest lives in the installed tree and names files the next install
        # deletes, so entries that could reach outside the catalog root must be
        # refused rather than followed
        string outside = install_dir + "/share/outside.json";
        write(outside, "{}\n");
        write(manifest, read(manifest) + "../outside.json\n" + outside + "\n");

        # generation B drops the app, so every manifest entry is a removal
        # candidate, including the two injected ones
        writeCatalogProject(src_dir, "QoreCatalogFixtureGuarded", {
            "OldOwner": {"Other": {"short_desc": "unrelated app"}},
        });
        string install_b = installCatalogProject(src_dir, build_dir, install_dir);
        assertTrue(is_file(outside),
            "a manifest entry outside the catalog root is refused: " + install_b);
        assertFalse(is_file(root + "/" + domain + "/root/OldOwner.json"),
            "the safe entry is still reconciled: " + install_b);
        assertEq((catalogDomain("Other") + "/root/OldOwner.json",),
            manifestEntries(manifest),
            "the republished manifest holds only this generation's fragments");
    }

    testCatalogInstallDestdirStaging() {
        TmpDir tmp("qore-cmake-catalog-destdir-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string install_dir = tmp.path + "/install";
        string stage_dir = tmp.path + "/stage";
        string root = install_dir + "/share/qore/i18n";
        string manifest = root
            + "/.qore-catalog-manifests/QoreCatalogFixtureStaged.manifest";
        string domain = catalogDomain("Staged");

        writeCatalogProject(src_dir, "QoreCatalogFixtureStaged", {
            "OldOwner": {"Staged": {"short_desc": "staged app"}},
        });

        # a package build must not touch the live prefix, so the file the sweep
        # would otherwise remove is placed there to prove it survives
        string flat_dir = root + "/" + catalogDomain("Legacy");
        mkdir(flat_dir, 0755, True);
        write(flat_dir + "/root.json", make_json({
            "schema_version": I18n::get_catalog_schema_version(),
            "locales": {"root": {"messages": {}}},
        }) + "\n");

        string install = installCatalogProject(src_dir, build_dir, install_dir,
            stage_dir);
        string staged_root = stage_dir + root;
        assertTrue(is_file(staged_root + "/" + domain + "/root/OldOwner.json"),
            "the fragment installs below the staged root: " + install);
        assertTrue(is_file(stage_dir + manifest),
            "the manifest is written below the staged root");
        assertFalse(is_file(manifest),
            "DESTDIR staging writes no manifest into the live prefix");
        assertTrue(is_file(flat_dir + "/root.json"),
            "DESTDIR staging reconciles the staged root only");
        assertEq((domain + "/root/OldOwner.json",),
            manifestEntries(stage_dir + manifest),
            "the staged manifest records the staged fragment");
    }

    #! Returns the files listed in a `set(<name> ...)` block of the qore CMakeLists.txt.
    private list<string> cmakeSourceList(string cmakelists, string name) {
        list<string> out = ();
        bool in_list = False;
        foreach string line in (cmakelists.split("\n")) {
            if (!in_list) {
                if (line.equalPartial("set(" + name)) {
                    in_list = True;
                }
                continue;
            }
            string trimmed = trim(line);
            if (trimmed == ")") {
                break;
            }
            # entries are ${CMAKE_SOURCE_DIR}-relative
            *list<*string> m = trimmed.regexExtract('^\$\{CMAKE_SOURCE_DIR\}/(.+)$');
            if (exists m) {
                push out, m[0];
            }
        }
        return out;
    }

    #! Every AOT/IR source relative to the qore source root.
    private list<string> aotSourceUniverse(string src) {
        list<string> out = ();
        hash<string, bool> seen = {};
        foreach string pattern in ((
                "lib/QoreAOT*.cpp", "lib/QoreIR*.cpp",
                "include/qore/intern/QoreAOT*.h", "include/qore/intern/QoreIR*.h")) {
            foreach string path in (glob(src + "/" + pattern)) {
                string rel = path.substr(src.size() + 1);
                if (!seen{rel}) {
                    seen{rel} = True;
                    push out, rel;
                }
            }
        }
        return sort(out);
    }

    testFormatFingerprintCoverage() {
        string src = normalize_dir(get_script_dir() + "/../../..");
        if (!is_file(src + "/CMakeLists.txt")) {
            testSkip("the qore source tree is not available beside the test");
        }
        string cmakelists = read(src + "/CMakeLists.txt");

        list<string> tracked = cmakeSourceList(cmakelists, "_qcc_format_sources");
        list<string> excused = cmakeSourceList(cmakelists, "_qcc_format_excluded_sources");
        assertGt(20, tracked.size(), "the curated fingerprint list was parsed");
        assertGt(0, excused.size(), "the deliberate-exclusion list was parsed");

        hash<string, bool> tracked_set = map {$1: True}, tracked;
        hash<string, bool> excused_set = map {$1: True}, excused;

        # The partition must be total.  A codegen source that is in neither list is
        # the silent-staleness bug this guards: it compiles and works, and only a
        # consumer somewhere keeps objects built by a compiler that no longer exists.
        list<string> unclassified = map $1, aotSourceUniverse(src),
            !tracked_set{$1} && !excused_set{$1};
        assertEq((), unclassified,
            "every AOT/IR source is either fingerprinted or explicitly excused");
        list<string> both = map $1, keys tracked_set, excused_set{$1};
        assertEq((), both, "no source is both fingerprinted and excused");

        # These define the ON-DISK expression-tree format: the 256-entry kind table
        # in QoreAOTExprNodeRegistry.cpp is positional, so its order IS the wire
        # format, and the handlers beside it are how a .qo is read back.  A change
        # here against an un-invalidated cache is silent misinterpretation, because
        # the load-time check only rejects a version NEWER than the reader's.
        foreach string required in ((
                "lib/QoreAOTExprNodeRegistry.cpp",
                "lib/QoreAOTExprNodeHandlers.cpp",
                "include/qore/intern/QoreAOTExprNodeRegistry.h",
                "lib/QoreAOTBinary.cpp",
                "include/qore/intern/QoreAOTBinary.h")) {
            assertTrue(tracked_set{required},
                "the AOT binary format contract is fingerprinted: " + required);
        }
        # QoreIRVerifier gates whether a body is lowered natively at all, so it
        # changes which symbols an object exports.
        assertTrue(tracked_set{"lib/QoreIRVerifier.cpp"},
            "the IR verifier gates native lowering, so it is fingerprinted");

        # The interpreter is excused only because the AOT compiler no longer calls
        # into it.  extractLValueBaseVarRef() decides outlining in QoreAOT.cpp; while
        # it lived in the interpreter, editing it changed emitted objects without
        # moving the fingerprint.
        assertTrue(excused_set{"lib/QoreIRInterpreter.cpp"},
            "the IR interpreter is excused from the fingerprint");
        assertEq(-1, read(src + "/lib/QoreIRInterpreter.cpp").find(
                "const VarRefNode* extractLValueBaseVarRef(const QoreValue& lvalue) {"),
            "the excused interpreter does not define an AOT-contract helper");
        assertNeq(-1, read(src + "/lib/QoreIRAnalysis.cpp").find(
                "const VarRefNode* extractLValueBaseVarRef(const QoreValue& lvalue) {"),
            "the outlining helper lives in a fingerprinted file");

        # ...and the list actually reaches the digest qcc publishes.
        string stamp_path = qore_build_dir + "/qcc-format.stamp";
        if (is_file(stamp_path)) {
            string stamp = read(stamp_path);
            foreach string required in ((
                    "QoreAOTExprNodeRegistry.cpp", "QoreAOTExprNodeHandlers.cpp",
                    "QoreIRVerifier.cpp")) {
                assertNeq(-1, stamp.find(required),
                    "the published fingerprint hashes " + required);
            }
            assertEq(-1, stamp.find("QoreIRInterpreter.cpp"),
                "the published fingerprint does not hash the excused interpreter");
        }
    }

    testQccDepsTrackInstalledLibqore() {
        string src = normalize_dir(get_script_dir() + "/../../..");
        string macros = src + "/cmake/QoreMacros.cmake";
        if (!is_file(macros)) {
            testSkip("QoreMacros.cmake is not available beside the test");
        }

        TmpDir tmp("qore-qcc-deps-libqore-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        mkdir(src_dir);

        # A stand-in for each tracked file; only the decision is under test.
        string stamp = src_dir + "/qcc-format.stamp";
        string library = src_dir + "/libqore.so";
        string qcc = src_dir + "/qcc";
        write(stamp, "format=1\n");
        write(library, "not really a library\n");
        write(qcc, "#!/bin/sh\n");
        chmod(qcc, 0755);

        write(src_dir + "/CMakeLists.txt", sprintf(
"cmake_minimum_required(VERSION 3.5)
project(QoreQccDepsProbe NONE)
set(QORE_QCC_EXECUTABLE \"%s\")
set(QORE_QCC_FORMAT_STAMP \"%s\")
set(QORE_LIBRARY \"%s\")
include(\"%s\")

set(QORE_IN_BUILD_TREE TRUE)
qore_get_qcc_deps(IN_TREE_DEPS QCC_COMMAND \"${QORE_QCC_EXECUTABLE}\")
set(QORE_IN_BUILD_TREE FALSE)
qore_get_qcc_deps(INSTALLED_DEPS QCC_COMMAND \"${QORE_QCC_EXECUTABLE}\")
file(WRITE \"${CMAKE_BINARY_DIR}/deps.txt\"
    \"in_tree=${IN_TREE_DEPS}\\n\"
    \"installed=${INSTALLED_DEPS}\\n\")
", qcc, stamp, library, macros));

        string configure = run(sprintf("cd %s && cmake -S . -B %s",
            shellQuote(src_dir), shellQuote(build_dir)));
        assertTrue(is_file(build_dir + "/deps.txt"),
            "the probe project configured: " + configure);

        hash<string, string> deps = {};
        foreach string line in (read(build_dir + "/deps.txt").split("\n")) {
            *list<*string> m = line.regexExtract("^([a-z_]+)=(.*)$");
            if (exists m) {
                deps{m[0]} = m[1];
            }
        }

        # Both forms track the fingerprint...
        assertNeq(-1, deps.in_tree.find(stamp),
            "an in-tree qore tracks the format fingerprint: " + deps.in_tree);
        assertNeq(-1, deps.installed.find(stamp),
            "an installed qore tracks the format fingerprint: " + deps.installed);

        # ...but only an installed qore also tracks the library.  The fingerprint is
        # a curated source list and libqore's codegen is larger than it; the branch
        # that adds the library used to sit in the same if/elseif chain as the
        # fingerprint lookup, so setting QORE_QCC_FORMAT_STAMP -- which
        # QoreConfig.cmake always does -- made it unreachable for every consumer.
        assertNeq(-1, deps.installed.find(library),
            "an installed qore tracks libqore itself: " + deps.installed);
        assertEq(-1, deps.in_tree.find(library),
            "an in-tree qore keeps the curated list alone: " + deps.in_tree);
    }

    #! AOT qmods of a module built outside the Qore tree follow the Qore library they are built for
    /** The qmods of such a build depended only on the qcc executable, which is not reinstalled when only the Qore
        library changes, so installing a new library left qmods compiled for the previous one, which the new library
        refuses.  The qcc run also inherited the configure-time LD_LIBRARY_PATH ahead of the Qore library, so a
        path to another Qore build tree made qcc compile for that tree's library.  A fake qcc stands in for the real
        one: it reports the identity in \c identity.txt and records each compilation in \c compiles.log.
    */
    testExternalQmodsTrackQoreRuntime() {
        if (!qore_dir) {
            testSkip("QoreConfig.cmake is not available for the build under test");
        }
        TmpDir tmp("qore-external-qmod-runtime-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string tool_dir = tmp.path + "/tools";
        string lib_dir = tmp.path + "/lib";
        mkdir(src_dir + "/qlib/Fixture", 0755, True);
        mkdir(tool_dir);
        mkdir(lib_dir);
        write(src_dir + "/qlib/Fixture/Fixture.qm", "%modern\n"
            "module Fixture {\n"
            "    version = \"1.0\";\n"
            "    desc = \"external qmod runtime fixture\";\n"
            "    author = \"Qore Technologies, s.r.o.\";\n"
            "    license = \"MIT\";\n"
            "}\n");
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreExternalQmodRuntime NONE)
find_package(Qore REQUIRED)
# the stand-in library, also for a build-tree QoreConfig.cmake, which names the build tree's library
set(QORE_LIBRARY \"${FIXTURE_QORE_LIBRARY}\")
qore_external_user_module(\"qlib/Fixture\" \"\")
");
        string library = lib_dir + "/libqore.so";
        write(library, "a stand-in for the Qore library\n");
        string identity_file = tool_dir + "/identity.txt";
        string log = tool_dir + "/compiles.log";
        string qcc = tool_dir + "/qcc";
        write(qcc, sprintf("#!/bin/sh\n"
            "if [ \"$1\" = --version ]; then\n"
            "    echo 'qcc (Qore Code Compiler) v1.0'\n"
            "    id=`cat %s`\n"
            "    [ -n \"$id\" ] && echo \"AOT runtime identity: $id\"\n"
            "    exit 0\n"
            "fi\n"
            "out=\n"
            "while [ $# -gt 0 ]; do\n"
            "    case \"$1\" in\n"
            "        -o) out=$2; shift ;;\n"
            "        --depfile=*) dep=${1#--depfile=} ;;\n"
            "    esac\n"
            "    shift\n"
            "done\n"
            "echo \"LD_LIBRARY_PATH=$LD_LIBRARY_PATH\" >> %s\n"
            "echo qmod > \"$out\"\n"
            "[ -n \"$dep\" ] && echo \"$out:\" > \"$dep\"\n"
            "exit 0\n", identity_file, log));
        chmod(qcc, 0755);
        write(identity_file, "aaaa\n");

        string other_tree = tmp.path + "/other-qore-build";
        string configure = run(sprintf("LD_LIBRARY_PATH=%s cmake -S %s -B %s -DQore_DIR=%s "
            "-DQORE_QCC_EXECUTABLE=%s -DFIXTURE_QORE_LIBRARY=%s -DQORE_BUILD_AOT_MODULES=ON",
            shellQuote(other_tree), shellQuote(src_dir), shellQuote(build_dir), shellQuote(qore_dir),
            shellQuote(qcc), shellQuote(library)));
        string qmod = build_dir + "/qlib-qmod/Fixture/Fixture.qmod";
        string stamp = build_dir + "/qcc-format.stamp";

        code<int()> compiles = int sub () {
            return is_file(log) ? read(log).split("\n").size() : 0;
        };
        string build = run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertTrue(is_file(qmod), "the fixture qmod is compiled: " + configure + build);
        assertEq(1, compiles());
        assertRegex("aot-runtime-identity=aaaa", read(stamp), "the stamp records the identity qcc reports");

        # qcc runs with the Qore library's directory ahead of the configure-time library path
        string ld_path = read(log).split("\n")[0];
        int lib_pos = ld_path.find(lib_dir);
        int other_pos = ld_path.find(other_tree);
        assertTrue(lib_pos >= 0 && other_pos > lib_pos,
            "the Qore library directory precedes the configure-time LD_LIBRARY_PATH: " + ld_path);

        # nothing changed: nothing is recompiled
        run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertEq(1, compiles(), "an unchanged build recompiles nothing");

        # a new Qore library is installed and qcc is not: the qmods are recompiled
        TestFileTime::set_file_times(now() + 2s, library);
        build = run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertEq(2, compiles(), "a new Qore library recompiles the qmods: " + build);

        # a new qcc reporting a new identity: recompiled, and the stamp names the new identity
        write(identity_file, "bbbb\n");
        TestFileTime::set_file_times(now() + 4s, qcc);
        build = run(sprintf("cmake --build %s", shellQuote(build_dir)));
        assertEq(3, compiles(), "a new qcc recompiles the qmods: " + build);
        assertRegex("aot-runtime-identity=bbbb", read(stamp));

        # a qcc that reports no identity fails the build before any module is compiled
        write(identity_file, "");
        TestFileTime::set_file_times(now() + 6s, qcc);
        build = run(sprintf("cmake --build %s; echo RC=$?", shellQuote(build_dir)));
        assertRegex("does\\s+not\\s+report\\s+an\\s+AOT\\s+runtime\\s+identity", build);
        assertFalse(build =~ /RC=0/, "the build fails: " + build);
        assertEq(3, compiles(), "no module is compiled with a qcc that reports no identity");
    }

    testExportedHelpers() {
        TmpDir tmp("qore-cmake-build-helpers-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        mkdir(src_dir);

        string input = src_dir + "/input.txt";
        string cmakelists = src_dir + "/CMakeLists.txt";
        write(input, "stable content\n");
        write(cmakelists,
"cmake_minimum_required(VERSION 3.5)
project(QoreCMakeHelperSmoke NONE)
find_package(Qore REQUIRED)

set(INPUT_FILE \"${CMAKE_SOURCE_DIR}/input.txt\")
set(DIGEST_FILE \"${CMAKE_BINARY_DIR}/input.sha256\")
qore_content_digest_target(input-digest
    OUTPUT \"${DIGEST_FILE}\"
    INPUTS \"${INPUT_FILE}\")

qore_get_qcc_command(QCC_CMD)
qore_get_qcc_deps(QCC_DEPS QCC_COMMAND \"${QCC_CMD}\")
qore_qcc_sidecar_paths(\"${CMAKE_BINARY_DIR}/sample.qo\"
    DEPFILE SAMPLE_DEPFILE
    INDEX_JSON SAMPLE_INDEX
    STATUS_JSON SAMPLE_STATUS
    SUCCESS_STAMP SAMPLE_STAMP
    CONTENT_STAMP SAMPLE_CONTENT_STAMP
    COMPILE_CONTRACT_STAMP SAMPLE_COMPILE_CONTRACT_STAMP
    AGGREGATE_CONTRACT_STAMP SAMPLE_AGGREGATE_CONTRACT_STAMP
    MANIFEST_JSON SAMPLE_MANIFEST)
qore_qcc_sidecar_flags(SAMPLE_FLAGS \"${CMAKE_BINARY_DIR}/sample.qo\" ALL)
file(WRITE \"${CMAKE_BINARY_DIR}/helper-vars.txt\"
    \"qcc=${QCC_CMD}\\n\"
    \"deps=${QCC_DEPS}\\n\"
    \"depfile=${SAMPLE_DEPFILE}\\n\"
    \"status=${SAMPLE_STATUS}\\n\"
    \"compile_contract_stamp=${SAMPLE_COMPILE_CONTRACT_STAMP}\\n\"
    \"aggregate_contract_stamp=${SAMPLE_AGGREGATE_CONTRACT_STAMP}\\n\"
    \"flags=${SAMPLE_FLAGS}\\n\")
");

        string configure = run(sprintf("cd %s && cmake -S . -B %s%s",
            src_dir, build_dir, qore_dir ? " -DQore_DIR=" + qore_dir : ""));
        assertTrue(is_file(build_dir + "/helper-vars.txt"),
            "configured with Qore package: " + configure);

        string vars = read(build_dir + "/helper-vars.txt");
        assertTrue(vars.find("qcc=") != -1, "qcc command exported: " + vars);
        assertTrue(vars.find("qcc-format.stamp") != -1,
            "qcc deps use semantic format stamp: " + vars);
        string qcc_format = read(qore_build_dir + "/qcc-format.stamp");
        assertTrue(qcc_format.find("format=1\n") == 0
                && qcc_format.find("QoreAOT.cpp") != -1,
            "qcc format stamp contains a deterministic codegen fingerprint: " + qcc_format);
        assertEq(-1, vars.find("deps=" + qore_build_dir + "/qcc\n"),
            "qcc deps do not mtime-track qcc when format stamp is available: " + vars);
        assertTrue(vars.find("depfile=" + build_dir + "/sample.qo.d") != -1,
            "sidecar depfile path exported: " + vars);
        assertTrue(vars.find("--content-stamp=" + build_dir + "/sample.qo.content.stamp") != -1,
            "sidecar content-stamp flag exported: " + vars);
        assertTrue(vars.find("--compile-contract-stamp="
                + build_dir + "/sample.qo.compile-contract.stamp") != -1,
            "sidecar compile-contract flag exported: " + vars);
        assertTrue(vars.find("--aggregate-contract-stamp="
                + build_dir + "/sample.qo.aggregate-contract.stamp") != -1,
            "sidecar aggregate-contract flag exported: " + vars);
        assertTrue(vars.find("--depfile-compile-contract-stamps") != -1,
            "sidecar compile-contract dependency flag exported: " + vars);
        assertTrue(vars.find("--skip-if-manifest-current") != -1,
            "sidecar manifest skip flag exported: " + vars);

        string build1 = run(sprintf("cmake --build %s --target input-digest", build_dir));
        assertTrue(is_file(build_dir + "/input.sha256"),
            "content digest target built: " + build1);
        string digest1 = read(build_dir + "/input.sha256");
        assertTrue(digest1.find("format=1\n") == 0, "digest has stable format header: " + digest1);
        assertTrue(digest1.find("input.txt") != -1, "digest names input: " + digest1);

        write(input, "stable content\n");
        string build2 = run(sprintf("cmake --build %s --target input-digest", build_dir));
        assertEq(digest1, read(build_dir + "/input.sha256"),
            "same-content rebuild does not change digest content: " + build2);

        write(input, "changed content\n");
        string build3 = run(sprintf("cmake --build %s --target input-digest", build_dir));
        assertNeq(digest1, read(build_dir + "/input.sha256"),
            "changed content updates digest: " + build3);
    }

    #! A source whose body takes a call reference to each named function.
    /** A call reference to a unique free function is a required edge and the
        smallest cycle the build can actually compile, so a ring of them is a
        strongly connected component that no build order can resolve.
    */
    private string cycleSource(string name, softlist<string> targets) {
        string body = "%new-style\nstring sub " + name + "Func() {\n";
        foreach string target in (targets) {
            body += sprintf("    code %s_ref = \\%sFunc();\n", target, target);
        }
        body += "    return \"" + name + "\";\n}\n";
        return body;
    }

    #! A group's object recipes carry no depfile; one depfile watches what the group reads from outside.
    /** Every object recipe used to have its own DEPFILE.  Behind the coordinator those decided
        nothing -- a recipe checks only group sources, so one that ran because a module moved found
        its component current -- and with the Makefile generators before CMake 4.0 every rewritten
        depfile was appended to the consolidated dependencies of every target consuming the object
        stamps, never replacing its previous entry: 1.7 GB per target on an 895-source group, and 50
        seconds of CMake dependency scanning per incremental build.

        What a member reads from outside the group is watched through one depfile instead, and that
        closes a gap the per-object depfiles had: a module that a member started requiring after the
        group's last shared parse is recorded only by that member's standalone compile, not by the
        shared parse's depfile, so an edit to it rebuilt nothing.
    */
    testGroupExternalInputsDepfile() {
        TmpDir tmp("qore-cmake-qcc-external-inputs-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string snippets_dir = src_dir + "/srcs";
        string module_dir = tmp.path + "/modules";
        mkdir(src_dir);
        mkdir(snippets_dir);
        mkdir(module_dir);

        string module_path = module_dir + "/ExtInput.qm";
        string module_source = "%new-style
module ExtInput {
    version = \"1.0\";
    desc = \"an input from outside the object group\";
    author = \"Qore Technologies, s.r.o.\";
    url = \"https://qore.org\";
    license = \"MIT\";
}
public namespace ExtInput {
    public string sub extInputName() {
        return \"NAME\";
    }
}
";
        write(module_path, replace(module_source, "NAME", "one"));
        write(snippets_dir + "/alpha.qc", "%new-style\nstring sub alphaName() {\n    return \"alpha\";\n}\n");
        write(snippets_dir + "/beta.qc", "%new-style\nstring sub betaName() {\n    return \"beta\";\n}\n");
        write(snippets_dir + "/gamma.qc", "%new-style\nstring sub gammaName() {\n    return \"gamma\";\n}\n");

        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.20)
project(QoreQoExternalInputs NONE)
find_package(Qore REQUIRED)

qore_qcc_compile_objects(QO_OBJECTS
    GROUP extinputs
    OUTPUT_DIR \"${CMAKE_BINARY_DIR}/qo\"
    SCRIPT_DIR \"${CMAKE_BINARY_DIR}/qo-scripts\"
    MODULE_DIR \"" + module_dir + "\"
    SOURCES
        \"${CMAKE_SOURCE_DIR}/srcs/alpha.qc\"
        \"${CMAKE_SOURCE_DIR}/srcs/beta.qc\"
        \"${CMAKE_SOURCE_DIR}/srcs/gamma.qc\"
    PARSE_OPTIONS PO_MODERN
    WARNINGS_ARE_ERRORS
    STAMPS_VAR QO_STAMPS
    ORDER_TARGETS_VAR QO_ORDER_TARGETS
    GENERATION_TARGET_VAR QO_GENERATION_TARGET)
add_custom_target(qo-objects ALL DEPENDS ${QO_STAMPS})
add_dependencies(qo-objects ${QO_ORDER_TARGETS})
file(WRITE \"${CMAKE_BINARY_DIR}/extinputs-vars.txt\"
    \"generation_target=${QO_GENERATION_TARGET}\\n\")
");

        string configure = run(sprintf("cmake -S %s -B %s%s", src_dir, build_dir,
            qore_dir ? " -DQore_DIR=" + qore_dir : ""));
        assertTrue(is_file(build_dir + "/qo-scripts/scc-map.cmake"),
            "configured the external-input group: " + configure);
        string generation_target = trim(read(build_dir + "/extinputs-vars.txt")
            .regexExtract("generation_target=(.*)")[0]);
        string generation_dir = build_dir + "/CMakeFiles/" + generation_target + ".dir";

        # No object recipe hands the build tool its own depfile any more.
        string depend_info = read(generation_dir + "/DependInfo.cmake");
        assertTrue(depend_info.size() > 0, "the generation target has dependency info");
        assertEq(-1, depend_info.find(".qo.d"),
            "no member depfile is consolidated into a target consuming the object stamps: "
                + depend_info);

        string clean = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(3, occurrences(clean, "qcc: script-context .qo"),
            "the clean build compiles every member: " + clean);
        string external_depfile = build_dir + "/qo/.qcc-scc/.qcc-external-inputs.stamp.d";
        assertTrue(is_file(external_depfile), "the coordinator records the group's external inputs");
        assertEq(-1, read(external_depfile).find(module_path),
            "no member reads the module yet: " + read(external_depfile));

        # beta starts requiring the module; only its standalone compile records it.
        write(snippets_dir + "/beta.qc", "%new-style\n%requires ExtInput\n"
            "string sub betaName() {\n    return extInputName();\n}\n");
        string edited = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(1, occurrences(edited, "qcc: script-context .qo"),
            "only the edited member is recompiled: " + edited);
        assertEq(1, occurrences(edited, "one at a time"),
            "on the standalone path, so the shared parse's depfile never names the module: " + edited);
        assertEq(-1, read(build_dir + "/qo/.qcc-scc/.qcc-batch-bootstrap.stamp.d").find(module_path),
            "the shared parse's depfile really does not name the module");
        assertTrue(read(external_depfile).find(module_path) >= 0,
            "the group's external inputs now name the module: " + read(external_depfile));
        date recorded = mtime(external_depfile);

        string settled = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(0, occurrences(settled, "qcc: script-context .qo"),
            "the next build compiles nothing: " + settled);
        assertEq(recorded, mtime(external_depfile),
            "and leaves an unchanged external-input set unwritten");

        # An edit to the module rebuilds the member that reads it, and nothing else.
        write(module_path, replace(module_source, "NAME", "two"));
        string module_edit = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(1, occurrences(module_edit, "qcc: script-context .qo"),
            "a module edit rebuilds the one member that reads it: " + module_edit);
        assertTrue(module_edit.find("beta") >= 0,
            "and that member is beta: " + module_edit);
        string after_module = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(0, occurrences(after_module, "qcc: script-context .qo"),
            "the build after the module edit converges: " + after_module);

        # Dependencies an earlier configuration consolidated are rebuilt from scratch, together
        # with what an interrupted rewrite left behind.
        write(generation_dir + "/compiler_depend.internal", "stale\n");
        write(generation_dir + "/compiler_depend.make.tmp12345", "left behind\n");
        string reconfigure = run(sprintf("cmake %s", build_dir));
        assertFalse(is_file(generation_dir + "/compiler_depend.internal"),
            "configuring discards the consolidated dependencies: " + reconfigure);
        assertFalse(is_file(generation_dir + "/compiler_depend.make.tmp12345"),
            "and an interrupted rewrite's temporary file");
        assertEq(0, read(generation_dir + "/compiler_depend.make").find("# Empty compiler generated"),
            "and leaves the file the target's makefile includes in CMake's initial form");
        string after_reset = run(sprintf("cmake --build %s -- -j9", build_dir));
        assertEq(0, occurrences(after_reset, "qcc: script-context .qo"),
            "a build after the reset compiles nothing: " + after_reset);
    }

    testCyclicComponentBuilds() {
        TmpDir tmp("qore-cmake-qcc-cycle-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string snippets_dir = src_dir + "/srcs";
        mkdir(src_dir);
        mkdir(snippets_dir);

        # PAIR is a two-source mutual dependency; RING has more members than the
        # 16-attempt budget the component build unit replaces, so a per-file
        # convergence loop could not have finished it.
        int ring_size = 20;
        list<string> names = ("pairone", "pairtwo");
        write(snippets_dir + "/pairone.qc", cycleSource("pairone", "pairtwo"));
        write(snippets_dir + "/pairtwo.qc", cycleSource("pairtwo", "pairone"));
        # A consumer of the pair, in its own component, plus an unrelated source.
        write(snippets_dir + "/pairuser.qc", cycleSource("pairuser", "pairone"));
        write(snippets_dir + "/solo.qc", cycleSource("solo", ()));
        push names, "pairuser";
        push names, "solo";
        for (int index = 0; index < ring_size; ++index) {
            string name = sprintf("ring%02d", index);
            write(snippets_dir + "/" + name + ".qc",
                cycleSource(name, sprintf("ring%02d", (index + 1) % ring_size)));
            push names, name;
        }

        string source_list = "";
        foreach string name in (names) {
            source_list += sprintf("    \"${CMAKE_SOURCE_DIR}/srcs/%s.qc\"\n", name);
        }
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreQoCycleSmoke NONE)
find_package(Qore REQUIRED)

set(QO_SOURCES
" + source_list + ")

qore_qcc_compile_objects(QO_OBJECTS
    GROUP cycle
    OUTPUT_DIR \"${CMAKE_BINARY_DIR}/qo\"
    SCRIPT_DIR \"${CMAKE_BINARY_DIR}/qo-scripts\"
    SOURCES ${QO_SOURCES}
    PARSE_OPTIONS PO_MODERN
    WARNINGS_ARE_ERRORS
    STAMPS_VAR QO_STAMPS
    CONTENT_STAMPS_VAR QO_CONTENT_STAMPS
    ORDER_TARGETS_VAR QO_ORDER_TARGETS
    GENERATION_MAP_VAR QO_GENERATION_MAP
    GENERATION_TARGET_VAR QO_GENERATION_TARGET)
add_custom_target(qo-objects ALL DEPENDS ${QO_STAMPS})
add_dependencies(qo-objects ${QO_ORDER_TARGETS})

qore_qcc_link_objects(QO_LINK
    OUTPUT \"${CMAKE_BINARY_DIR}/linked.qo\"
    AGGREGATE_SYMBOL QORE_CMAKE_CYCLE_SMOKE
    INPUTS ${QO_OBJECTS}
    INPUT_CONTENT_STAMPS ${QO_CONTENT_STAMPS}
    INPUT_ORDER_TARGETS ${QO_ORDER_TARGETS}
    OBJECT_GENERATION_MAPS ${QO_GENERATION_MAP}
    OBJECT_GENERATION_TARGETS ${QO_GENERATION_TARGET}
    WARNINGS_ARE_ERRORS
    STAMP_VAR QO_LINK_STAMP)
add_custom_target(qo-link ALL DEPENDS ${QO_LINK_STAMP})

qore_qcc_finalize_managed_outputs(
    OUTPUT_ROOT \"${CMAKE_BINARY_DIR}/qo\"
    SCRIPT_ROOT \"${CMAKE_BINARY_DIR}/qo-scripts\")
file(WRITE \"${CMAKE_BINARY_DIR}/cycle-vars.txt\"
    \"generation_map=${QO_GENERATION_MAP}\\n\")
");

        string configure = run(sprintf("cmake -S %s -B %s%s", src_dir, build_dir,
            qore_dir ? " -DQore_DIR=" + qore_dir : ""));
        assertTrue(is_file(build_dir + "/qo-scripts/scc-map.cmake"),
            "configured the cyclic object group: " + configure);

        # The decomposition must find the pair, the ring, and the singletons.
        string scc_map = read(build_dir + "/qo-scripts/scc-map.cmake");
        *list<*string> count_groups = scc_map.regexExtract("_COUNT \"([0-9]+)\"");
        assertTrue(exists count_groups, "the component map declares a count");
        # 2-member pair + pairuser + solo + 20-member ring = 4 components.
        assertEq("4", count_groups[0],
            "a 20-member ring and a 2-member cycle condense into components: "
                + scc_map);
        list<int> sizes = ();
        for (int index = 0; index < count_groups[0].toInt(); ++index) {
            *list<*string> outputs = scc_map.regexExtract(
                sprintf("_OUTPUTS_%d \"([^\"]*)\"", index));
            push sizes, outputs[0].split(";").size();
        }
        assertEq((1, 1, 2, 20), sort(sizes),
            "the components are the ring, the mutual pair, and two singletons: "
                + scc_map);

        string clean = run(buildCommand(build_dir));
        assertTrue(is_file(build_dir + "/linked.qo"),
            "clean cyclic build links the object set: " + clean);
        assertEq(0, occurrences(clean, "retrying ("),
            "a clean cyclic build emits no input-change retry: " + clean);
        assertEq(0, occurrences(clean, "moved during"),
            "a clean cyclic build publishes without a rebuild: " + clean);

        string no_op = run(buildCommand(build_dir));
        assertEq(0, occurrences(no_op, "qcc: script-context .qo"),
            "a no-op cyclic rebuild recompiles nothing: " + no_op);
        assertEq(0, occurrences(no_op, "one shared parse"),
            "a no-op cyclic rebuild runs no shared parse: " + no_op);

        # Changing EITHER member of the mutual pair performs exactly one
        # component compilation, at high parallelism and serially.
        foreach hash<auto> attempt in ((("member": "pairone", "jobs": buildJobs()),
                ("member": "pairtwo", "jobs": 1))) {
            string member = snippets_dir + "/" + attempt.member + ".qc";
            write(member, read(member)
                + sprintf("# %s edit at -j%d\n", attempt.member, attempt.jobs));
            string edited = run(sprintf("cmake --build %s -- -j%d", build_dir,
                attempt.jobs));
            assertEq(1, occurrences(edited, "one shared parse"),
                sprintf("editing %s performs exactly one component compilation "
                    "at -j%d: %s", attempt.member, attempt.jobs, edited));
            assertEq(0, occurrences(edited, "retrying ("),
                sprintf("editing %s emits no input-change retry: %s",
                    attempt.member, edited));
            assertEq(0, occurrences(edited, "moved during"),
                sprintf("editing %s publishes without a rebuild: %s",
                    attempt.member, edited));
            string converged = run(sprintf("cmake --build %s -- -j%d", build_dir,
                attempt.jobs));
            assertEq(0, occurrences(converged, "qcc: script-context .qo"),
                sprintf("the build after editing %s converges: %s",
                    attempt.member, converged));
        }

        # A ring larger than the old retry limit is still one build unit.
        string ring_member = snippets_dir + "/ring07.qc";
        write(ring_member, read(ring_member) + "# ring edit\n");
        string ring_edit = run(buildCommand(build_dir));
        assertEq(1, occurrences(ring_edit, "one shared parse"),
            sprintf("editing a %d-member ring performs exactly one component "
                "compilation: %s", ring_size, ring_edit));
        assertEq(0, occurrences(ring_edit, "retrying ("),
            "a ring larger than the old retry limit needs no retry: " + ring_edit);
        assertEq(0, occurrences(run(buildCommand(build_dir)),
                "qcc: script-context .qo"),
            "the build after a ring edit converges");

        # Concurrent requests for different members of one component: exactly one
        # builder publishes the generation.
        list<string> ring_targets = ();
        foreach string name in (names) {
            if (name.startsWith("ring0")) {
                push ring_targets, name;
            }
        }
        write(snippets_dir + "/ring03.qc",
            read(snippets_dir + "/ring03.qc") + "# concurrent ring edit\n");
        string concurrent = run(buildCommand(build_dir));
        assertEq(1, occurrences(concurrent, "one shared parse"),
            "concurrent requests for different members publish exactly one "
                "generation: " + concurrent);
        assertEq(0, occurrences(concurrent, "retrying ("),
            "concurrent member requests need no retry: " + concurrent);

        # Every component has a published generation record, and the generation
        # map the linker validates names all of them.
        string generation_map = trim(read(build_dir + "/cycle-vars.txt")).substr(15);
        assertTrue(is_file(generation_map),
            "the cyclic group publishes a generation map: " + generation_map);
        assertEq(4, occurrences(read(generation_map), "generation="),
            "the generation map names every component: " + read(generation_map));
        assertTrue(read(build_dir + "/linked.qo.qolink.manifest.json").find(
                generation_map) != -1,
            "the qo-link aggregate validates the cyclic group's generation map");

        # Removing the object directory is the recovery a developer reaches for,
        # and it used to end the next build with a message about strongly
        # connected components.  The two trees were not invalidated together: the
        # generation records live under OUTPUT_DIR while the stamps asserting they
        # are current lived under SCRIPT_DIR, so wiping the first left the build
        # tool with no reason to reach the coordinator and every reason to run the
        # object recipes behind it -- each of which then found its component stale
        # behind a claim that the group had been planned.  Recovering took a wipe
        # of both trees and a `cmake` re-run, neither of which the error named.
        #
        # The stamps now live in the tree they describe, so removing it is a
        # complete invalidation: the coordinator is reached again and plans the
        # rebuild.  No `cmake` re-run here, deliberately -- needing one was the
        # part of the old recovery that could not be derived from the failure.
        assertTrue(is_file(build_dir + "/qo/.qcc-scc/.qcc-incremental-plan.stamp"),
            "the plan stamp lives in the tree whose currency it asserts");
        assertTrue(is_file(build_dir + "/qo/.qcc-scc/.qcc-batch-bootstrap.stamp"),
            "and so does the batch bootstrap stamp");
        assertFalse(is_file(build_dir + "/qo-scripts/.qcc-incremental-plan.stamp"),
            "neither is left behind in the script directory");

        remove_tree(build_dir + "/qo");
        assertFalse(is_dir(build_dir + "/qo"), "the object tree is removed");
        assertTrue(is_file(build_dir + "/qo-scripts/qcc-batch.sh"),
            "while the configure output beside it survives");

        string recovered = run(buildCommand(build_dir));
        assertTrue(is_file(build_dir + "/linked.qo"),
            "the build recovers from a wiped object tree: " + recovered);
        assertTrue(occurrences(recovered, "scheduling against dependency graph") > 0,
            "the coordinator is reached again rather than bypassed: " + recovered);
        assertEq(0, occurrences(recovered, "cannot be started from a single object"),
            "and no recipe is left holding a component it cannot build: "
                + recovered);
        assertEq(4, occurrences(read(generation_map), "generation="),
            "every component is published again: " + read(generation_map));

        assertEq(0, occurrences(run(buildCommand(build_dir)),
                "qcc: script-context .qo"),
            "and the build after the recovery converges");

        # A shared parse that fails must leave the incremental state exactly as it
        # found it.  The ordering token the bootstrap advances before compiling
        # marks the whole group stale by construction -- currency treats a member
        # whose stamp is older than the token as belonging to a previous bootstrap
        # -- so a parse error in one source, the ordinary case while editing,
        # converted a one-component rebuild into a whole-group one: the next build
        # found every member older than the token and recompiled all of them.
        string bootstrap_stamp = build_dir + "/qo/.qcc-scc/.qcc-batch-bootstrap.stamp";
        list<string> member_stamps = glob(build_dir + "/qo/*.qo.stamp");
        assertEq(names.size(), member_stamps.size(),
            "the group publishes one success stamp per member");
        date token_before = mtime(bootstrap_stamp);
        assertEq(0, membersOlderThan(member_stamps, token_before),
            "a published tree leaves no member older than the ordering token");

        string broken = snippets_dir + "/ring05.qc";
        string broken_text = read(broken);
        write(broken, broken_text + "\nthis is not valid qore source\n");
        string failed = run(buildCommand(build_dir));
        assertEq(1, occurrences(failed, "one shared parse"),
            "a broken member of a multi-source component reaches the shared "
                "parse: " + failed);
        # Not an equality: the token is put back with touch -r, and busybox --
        # /bin/sh on Alpine -- restores whole seconds where coreutils restores
        # nanoseconds.  Rounding down is safe and is what must hold: the token
        # must not have ADVANCED, because what it dominates is every member
        # stamp.
        assertTrue(mtime(bootstrap_stamp) <= token_before,
            sprintf("a failed shared parse does not advance the group's ordering "
                "token (%y vs %y): %s", mtime(bootstrap_stamp), token_before,
                failed));
        assertEq(0, membersOlderThan(glob(build_dir + "/qo/*.qo.stamp"),
                mtime(bootstrap_stamp)),
            "and so leaves no member of the group looking stale: " + failed);

        write(broken, broken_text);
        string repaired = run(buildCommand(build_dir));
        assertEq(0, occurrences(run(buildCommand(build_dir)),
                "qcc: script-context .qo"),
            "the build after the repair converges: " + repaired);
    }

    #! How many of \a stamps predate \a token, which is how many look stale.
    private int membersOlderThan(list<string> stamps, date token) {
        int older = 0;
        foreach string stamp in (stamps) {
            if (mtime(stamp) < token) {
                ++older;
            }
        }
        return older;
    }

    #! A link aggregate of part of an object group, as Qorus links its process groups
    /** The provider's static method is a speculative fast entry that discovery demotes after its contract was
        first hashed.  Its caller's dependency rows kept that hash, so the consumer imported a contract the
        provider never published and the aggregate failed with "body_contract=<hash> actual=".
    */
    testPartialAggregateBodyContracts() {
        TmpDir tmp("qore-cmake-qcc-partial-agg-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string snippets_dir = src_dir + "/srcs";
        mkdir(src_dir);
        mkdir(snippets_dir);
        write(snippets_dir + "/provider.qc", "%modern\nnamespace AggDeps {\nour auto Config;\n"
            "class Resource {\n"
            "    static bool enabled() {\n"
            "        try {\n"
            "            return (Config.options.get(\"enabled\") ?? True).toBool();\n"
            "        } catch (hash<ExceptionInfo> ex) {\n"
            "            return False;\n"
            "        }\n"
            "    }\n"
            "    int user() { return Resource::enabled() ? 1 : 0; }\n"
            "}\n}\n");
        write(snippets_dir + "/holder.qc", "%modern\nclass AggOptions { auto get(string key) { return NOTHING; } }\n"
            "class AggHolder { public { AggOptions options(); } }\n");
        write(snippets_dir + "/consumer.qc", "%modern\npublic int sub aggUseResource(AggDeps::Resource r) {\n"
            "    return r.user() + 1;\n}\n"
            "public int sub aggRun() { AggDeps::Config = new AggHolder(); AggDeps::Resource r(); "
            "return aggUseResource(r); }\n");
        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreQoPartialAggregate NONE)
find_package(Qore REQUIRED)

qore_qcc_compile_objects(AGG_OBJECTS
    GROUP AGG
    OUTPUT_DIR \"${CMAKE_BINARY_DIR}/qo/AGG\"
    SCRIPT_DIR \"${CMAKE_BINARY_DIR}/qo-scripts/AGG\"
    SOURCES \"${CMAKE_SOURCE_DIR}/srcs/provider.qc\" \"${CMAKE_SOURCE_DIR}/srcs/holder.qc\"
        \"${CMAKE_SOURCE_DIR}/srcs/consumer.qc\"
    PARSE_OPTIONS PO_MODERN
    WARNINGS_ARE_ERRORS
    STAMPS_VAR AGG_STAMPS
    CONTENT_STAMPS_VAR AGG_CONTENT_STAMPS
    ORDER_TARGETS_VAR AGG_ORDER_TARGETS)
list(GET AGG_OBJECTS 0 PROVIDER_QO)
list(GET AGG_OBJECTS 2 CONSUMER_QO)

# the provider and its consumer only: the holder's declarations stay unresolved imports
qore_qcc_link_objects(AGG_PARTIAL
    OUTPUT \"${CMAKE_BINARY_DIR}/qo/AGG_link/AGG_PARTIAL.qo\"
    AGGREGATE_SYMBOL QORE_AGG_PARTIAL
    INPUTS ${PROVIDER_QO} ${CONSUMER_QO}
    INPUT_CONTENT_STAMPS ${AGG_CONTENT_STAMPS}
    INPUT_ORDER_TARGETS ${AGG_ORDER_TARGETS}
    ALLOW_UNRESOLVED_IMPORTS
    WARNINGS_ARE_ERRORS
    STAMP_VAR AGG_PARTIAL_STAMP)
qore_qcc_link_objects(AGG_FULL
    OUTPUT \"${CMAKE_BINARY_DIR}/qo/AGG_link/AGG_FULL.qo\"
    AGGREGATE_SYMBOL QORE_AGG_FULL
    INPUTS ${AGG_OBJECTS}
    INPUT_CONTENT_STAMPS ${AGG_CONTENT_STAMPS}
    INPUT_ORDER_TARGETS ${AGG_ORDER_TARGETS}
    WARNINGS_ARE_ERRORS
    STAMP_VAR AGG_FULL_STAMP)

qore_qcc_finalize_managed_outputs(
    OUTPUT_ROOT \"${CMAKE_BINARY_DIR}/qo\"
    SCRIPT_ROOT \"${CMAKE_BINARY_DIR}/qo-scripts\")
add_custom_target(all-qo ALL DEPENDS ${AGG_STAMPS} ${AGG_PARTIAL_STAMP} ${AGG_FULL_STAMP})
");
        string configure = run(sprintf("cmake -S %s -B %s%s", shellQuote(src_dir), shellQuote(build_dir),
            qore_dir ? " -DQore_DIR=" + shellQuote(qore_dir) : ""));
        assertTrue(is_file(build_dir + "/Makefile"), "configured the object group: " + configure);
        string built = run(sprintf("cmake --build %s -- -j4", shellQuote(build_dir)));
        assertTrue(is_file(build_dir + "/qo/AGG_link/AGG_PARTIAL.qo"),
            "the partial aggregate links the contracts its members baked: " + built);
        assertTrue(is_file(build_dir + "/qo/AGG_link/AGG_FULL.qo"), "the complete aggregate links: " + built);
        assertEq(-1, built.find("hash mismatch"), "no contract mismatch is reported: " + built);
        assertEq(-1, built.find("unresolved import"),
            "the partial aggregate's allowed unresolved imports are not reported: " + built);

        hash<auto> link_map = parse_json(read(build_dir + "/qo/AGG_link/AGG_PARTIAL.qo.qolink.json"));
        assertGt(0, link_map.unresolved_imports.size(),
            "the partial aggregate leaves the holder's declarations unresolved");
    }

    testManagedQccOutputPruning() {
        TmpDir tmp("qore-cmake-qcc-prune-");
        string src_dir = tmp.path + "/src";
        string build_dir = tmp.path + "/build";
        string snippets_dir = src_dir + "/srcs";
        mkdir(src_dir);
        mkdir(snippets_dir);

        string retained_source = snippets_dir + "/retained.qc";
        string removed_source = snippets_dir + "/removed.qc";
        string replacement_source = snippets_dir + "/replacement.qc";
        string group_b_source = snippets_dir + "/group-b.qc";
        write(retained_source, "%modern\npublic string sub retainedValue() { return \"retained\"; }\n");
        write(removed_source, "%modern\npublic string sub removedValue() { return \"removed\"; }\n");
        write(replacement_source,
            "%modern\npublic string sub replacementValue() { return \"replacement\"; }\n");
        write(group_b_source, "%modern\npublic string sub groupBValue() { return \"group-b\"; }\n");

        write(src_dir + "/CMakeLists.txt",
"cmake_minimum_required(VERSION 3.16)
project(QoreQoPruneSmoke NONE)
find_package(Qore REQUIRED)

set(PRUNE_SOURCE_MODE initial CACHE STRING \"source transition\")
option(ENABLE_GROUP_B \"build the second object group\" ON)
option(ENABLE_AGGREGATES \"build link and script aggregates\" ON)

set(A_SOURCES \"${CMAKE_SOURCE_DIR}/srcs/retained.qc\")
if(PRUNE_SOURCE_MODE STREQUAL \"initial\")
    list(APPEND A_SOURCES \"${CMAKE_SOURCE_DIR}/srcs/removed.qc\")
elseif(PRUNE_SOURCE_MODE STREQUAL \"replace\")
    list(APPEND A_SOURCES \"${CMAKE_SOURCE_DIR}/srcs/replacement.qc\")
elseif(NOT PRUNE_SOURCE_MODE STREQUAL \"single\")
    message(FATAL_ERROR \"unknown PRUNE_SOURCE_MODE=${PRUNE_SOURCE_MODE}\")
endif()

qore_qcc_compile_objects(A_OBJECTS
    GROUP GROUP_A
    OUTPUT_DIR \"${CMAKE_BINARY_DIR}/qo/GROUP_A\"
    SCRIPT_DIR \"${CMAKE_BINARY_DIR}/qo-scripts/GROUP_A\"
    SOURCES ${A_SOURCES}
    PARSE_OPTIONS PO_MODERN
    WARNINGS_ARE_ERRORS
    STAMPS_VAR A_STAMPS
    CONTENT_STAMPS_VAR A_CONTENT_STAMPS
    ORDER_TARGETS_VAR A_ORDER_TARGETS)
set(ALL_QO_STAMPS ${A_STAMPS})

if(ENABLE_GROUP_B)
    qore_qcc_compile_objects(B_OBJECTS
        GROUP GROUP_B
        OUTPUT_DIR \"${CMAKE_BINARY_DIR}/qo/GROUP_B\"
        SCRIPT_DIR \"${CMAKE_BINARY_DIR}/qo-scripts/GROUP_B\"
        SOURCES \"${CMAKE_SOURCE_DIR}/srcs/group-b.qc\"
        PARSE_OPTIONS PO_MODERN
        WARNINGS_ARE_ERRORS
        STAMPS_VAR B_STAMPS)
    list(APPEND ALL_QO_STAMPS ${B_STAMPS})
endif()

if(ENABLE_AGGREGATES)
    qore_qcc_link_objects(A_LINK
        OUTPUT \"${CMAKE_BINARY_DIR}/qo/GROUP_A_link/A_LINK.qo\"
        AGGREGATE_SYMBOL QORE_PRUNE_LINK
        INPUTS ${A_OBJECTS}
        INPUT_CONTENT_STAMPS ${A_CONTENT_STAMPS}
        INPUT_ORDER_TARGETS ${A_ORDER_TARGETS}
        WARNINGS_ARE_ERRORS
        STAMP_VAR A_LINK_STAMP)
    qore_qcc_script_aggregate(A_SCRIPT
        OUTPUT \"${CMAKE_BINARY_DIR}/qo/GROUP_A_script/A_SCRIPT.qo\"
        AGGREGATE QORE_PRUNE_SCRIPT
        SOURCES ${A_SOURCES}
        INPUT_CONTENT_STAMPS ${A_CONTENT_STAMPS}
        INPUT_ORDER_TARGETS ${A_ORDER_TARGETS}
        PARSE_OPTIONS PO_MODERN
        WARNINGS_ARE_ERRORS
        STAMP_VAR A_SCRIPT_STAMP)
    list(APPEND ALL_QO_STAMPS ${A_LINK_STAMP} ${A_SCRIPT_STAMP})
endif()

qore_qcc_finalize_managed_outputs(
    OUTPUT_ROOT \"${CMAKE_BINARY_DIR}/qo\"
    SCRIPT_ROOT \"${CMAKE_BINARY_DIR}/qo-scripts\")
add_custom_target(all-qo ALL DEPENDS ${ALL_QO_STAMPS})

string(REPLACE \";\" \"\\n\" A_OBJECT_LINES \"${A_OBJECTS}\")
file(WRITE \"${CMAKE_BINARY_DIR}/a-objects.txt\" \"${A_OBJECT_LINES}\\n\")
if(ENABLE_GROUP_B)
    string(REPLACE \";\" \"\\n\" B_OBJECT_LINES \"${B_OBJECTS}\")
    file(WRITE \"${CMAKE_BINARY_DIR}/b-objects.txt\" \"${B_OBJECT_LINES}\\n\")
endif()
");

        mkdir(build_dir);
        mkdir(build_dir + "/qo");
        mkdir(build_dir + "/qo/LEGACY_GROUP");
        write(build_dir + "/qo/LEGACY_GROUP/legacy.qo", "legacy\n");
        write(build_dir + "/qo/LEGACY_GROUP/legacy.qo.tmp.12345", "interrupted qcc\n");
        write(build_dir + "/qo/LEGACY_GROUP/legacy.qo.tmp.12345.p0.o",
            "interrupted split codegen\n");
        write(build_dir + "/qo/LEGACY_GROUP/legacy.qo.tmp.12345.loc.12345",
            "interrupted ELF localization\n");
        write(build_dir + "/qo/LEGACY_GROUP/legacy.qo.tmp.12345.globals.12345",
            "interrupted Mach-O localization\n");
        mkdir(build_dir + "/qo-scripts");
        mkdir(build_dir + "/qo-scripts/LEGACY_GROUP");
        write(build_dir + "/qo-scripts/LEGACY_GROUP/qcc-single.sh", "#!/bin/sh\n");
        write(build_dir
            + "/qo-scripts/LEGACY_GROUP/.qcc-batch-bootstrap.stamp.lock", "12345\n");
        write(build_dir
            + "/qo-scripts/LEGACY_GROUP/.qcc-batch-bootstrap.stamp.lock.12345", "12345\n");
        write(build_dir
            + "/qo-scripts/LEGACY_GROUP/.qcc-incremental-plan.stamp", "");

        string qore_arg = qore_dir ? " -DQore_DIR=" + qore_dir : "";
        string configure = run(sprintf("cmake -S %s -B %s%s", src_dir, build_dir, qore_arg));
        assertTrue(is_file(build_dir + "/.qore-qcc-managed-outputs.manifest"),
            "initial configure writes the managed-output manifest: " + configure);
        assertFalse(is_dir(build_dir + "/qo/LEGACY_GROUP")
                || is_dir(build_dir + "/qo-scripts/LEGACY_GROUP"),
            "first managed configure prunes recognized pre-manifest groups");
        string initial_build = run(sprintf("cmake --build %s -- -j17", build_dir));
        list<string> initial_a = trim(read(build_dir + "/a-objects.txt")).split("\n");
        list<string> initial_b = trim(read(build_dir + "/b-objects.txt")).split("\n");
        assertEq(2, initial_a.size(), "initial group A contains two outputs");
        assertEq(1, initial_b.size(), "initial group B contains one output");
        assertTrue(is_file(initial_a[1] + ".source-parse-defines"),
            "initial build writes the complete object sidecar family: " + initial_build);
        assertTrue(is_file(build_dir + "/qo/GROUP_A_link/A_LINK.qo.qolink-context"),
            "initial build registers link aggregate sidecars");
        assertTrue(is_file(build_dir + "/qo/GROUP_A_script/A_SCRIPT.qo.script-aggregate-context"),
            "initial build registers script aggregate sidecars");
        assertTrue(is_file(build_dir
                + "/qo/GROUP_A/.qcc-scc/.qcc-incremental-plan.stamp"),
            "initial multi-source build publishes its incremental-plan barrier");

        date retained_mtime = mtime(initial_a[0]);
        string replace_configure = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=replace%s",
            src_dir, build_dir, qore_arg));
        list<string> replacement_a = trim(read(build_dir + "/a-objects.txt")).split("\n");
        assertEq(2, replacement_a.size(), "replacement keeps the object count unchanged");
        assertEq(initial_a[0], replacement_a[0], "replacement retains the first output identity");
        assertNeq(initial_a[1], replacement_a[1], "replacement changes the second output identity");
        foreach string suffix in ("", ".d", ".idx.json", ".status.json", ".content.stamp",
                ".compile-contract.stamp", ".aggregate-contract.stamp", ".stamp",
                ".source.manifest.json",
                ".source-parse-defines", ".lock") {
            assertFalse(is_file(initial_a[1] + suffix),
                "source replacement prunes stale artifact " + suffix + ": " + replace_configure);
        }
        assertEq(retained_mtime, mtime(initial_a[0]),
            "source replacement configuration preserves retained object timestamps");
        string replacement_build = run(sprintf("cmake --build %s -- -j17", build_dir));
        assertTrue(is_file(replacement_a[1] + ".source.manifest.json"),
            "parallel build creates the replacement object: " + replacement_build);

        date retained_after_replace = mtime(replacement_a[0]);
        string single_configure = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single%s",
            src_dir, build_dir, qore_arg));
        foreach string suffix in ("", ".d", ".idx.json", ".status.json", ".content.stamp",
                ".compile-contract.stamp", ".aggregate-contract.stamp", ".stamp",
                ".source.manifest.json",
                ".source-parse-defines", ".lock") {
            assertFalse(is_file(replacement_a[1] + suffix),
                "source removal prunes stale artifact " + suffix + ": " + single_configure);
        }
        assertFalse(is_file(build_dir + "/qo-scripts/GROUP_A/qcc-batch.sh"),
            "single-source transition prunes the obsolete batch script");
        assertFalse(is_file(build_dir + "/qo/GROUP_A/.qcc-scc/.qcc-batch-bootstrap.stamp"),
            "single-source transition prunes the obsolete bootstrap stamp");
        assertFalse(is_file(build_dir + "/qo/GROUP_A/.qcc-scc/.qcc-batch-bootstrap.stamp.d"),
            "single-source transition prunes the obsolete bootstrap depfile");
        assertFalse(is_file(build_dir
                + "/qo/GROUP_A/.qcc-scc/.qcc-incremental-plan.stamp"),
            "single-source transition prunes the obsolete incremental-plan stamp");
        assertEq(retained_after_replace, mtime(replacement_a[0]),
            "source removal configuration preserves the retained object timestamp");
        string single_build = run(sprintf("cmake --build %s -- -j17", build_dir));
        assertTrue(is_file(replacement_a[0] + ".stamp"),
            "parallel build after source removal succeeds: " + single_build);

        string remove_group = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single -DENABLE_GROUP_B=OFF"
                + " -DENABLE_AGGREGATES=OFF%s", src_dir, build_dir, qore_arg));
        assertFalse(is_dir(build_dir + "/qo/GROUP_B"),
            "removed object group output directory is pruned: " + remove_group);
        assertFalse(is_dir(build_dir + "/qo-scripts/GROUP_B"),
            "removed object group script directory is pruned");
        assertFalse(is_dir(build_dir + "/qo/GROUP_A_link"),
            "removed link aggregate directory is pruned");
        assertFalse(is_dir(build_dir + "/qo/GROUP_A_script"),
            "removed script aggregate directory is pruned");

        string state_path = build_dir + "/.qore-qcc-managed-outputs.manifest";
        string state_before_error = read(state_path);
        string orphan_dir = build_dir + "/qo/ORPHAN";
        mkdir(orphan_dir);
        write(orphan_dir + "/stale.qo", "stale\n");
        write(orphan_dir + "/keep.txt", "unexpected\n");
        string rejected = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single -DENABLE_GROUP_B=OFF"
                + " -DENABLE_AGGREGATES=OFF%s", src_dir, build_dir, qore_arg));
        assertTrue(rejected.find("refusing to prune unexpected files or directories") != -1,
            "unexpected files make configuration fail safely: " + rejected);
        assertTrue(is_file(orphan_dir + "/stale.qo") && is_file(orphan_dir + "/keep.txt"),
            "validation failure removes no files");
        assertEq(state_before_error, read(state_path),
            "validation failure does not advance the managed-output manifest");

        unlink(orphan_dir + "/keep.txt");
        string orphan_cleanup = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single -DENABLE_GROUP_B=OFF"
                + " -DENABLE_AGGREGATES=OFF%s", src_dir, build_dir, qore_arg));
        assertFalse(is_dir(orphan_dir),
            "recognized orphan artifacts are pruned after safety validation: " + orphan_cleanup);

        date final_object_mtime = mtime(replacement_a[0]);
        date state_mtime = mtime(state_path);
        string state_content = read(state_path);
        string noop1 = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single -DENABLE_GROUP_B=OFF"
                + " -DENABLE_AGGREGATES=OFF%s", src_dir, build_dir, qore_arg));
        string noop2 = run(sprintf(
            "cmake -S %s -B %s -DPRUNE_SOURCE_MODE=single -DENABLE_GROUP_B=OFF"
                + " -DENABLE_AGGREGATES=OFF%s", src_dir, build_dir, qore_arg));
        assertEq(final_object_mtime, mtime(replacement_a[0]),
            "idempotent configurations preserve current object timestamps: " + noop1 + noop2);
        assertEq(state_mtime, mtime(state_path),
            "idempotent configurations do not rewrite the state manifest");
        assertEq(state_content, read(state_path),
            "idempotent configurations preserve the exact state manifest");
    }
}
