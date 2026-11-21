# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of the LazyModuleActions-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase())


#! Verifies that the provider modules generating actions from REST schemas build them on first use
/** Loading such a module registers its apps and builds no action set, and no type built from its schema; the first
    read of an app's actions builds them, once, even when several threads read them at once.  See
    @ref DataProvider::DataProviderActionCatalog::registerLazyAppActions() "registerLazyAppActions()".
*/
class LazyModuleActionsBase inherits QUnit::Test {
    public {
        #! The modules registering their actions lazily, and their apps
        const Modules = {
            "AssemblyAiDataProvider": ("AssemblyAI",),
            "AwsTextractDataProvider": ("AwsTextract",),
            "AzureDocumentIntelligenceDataProvider": ("AzureDocumentIntelligence",),
            "AzureOpenAiDataProvider": ("AzureOpenAI",),
            "BedrockDataProvider": ("AWSBedrock",),
            "BflDataProvider": ("BlackForestLabs",),
            "BitbucketDataProvider": ("Bitbucket",),
            "CohereDataProvider": ("Cohere",),
            "ConfluenceDataProvider": ("Confluence",),
            "EasyPostDataProvider": ("EasyPost",),
            "ExaDataProvider": ("Exa",),
            "FireworksDataProvider": ("FireworksAi",),
            "GitHubDataProvider": ("Github",),
            "GitLabDataProvider": ("Gitlab",),
            "GoogleDocumentAiDataProvider": ("GoogleDocumentAi",),
            "IntercomDataProvider": ("Intercom",),
            "JinaDataProvider": ("JinaAi",),
            "KlaviyoDataProvider": ("Klaviyo",),
            "LlamaParseDataProvider": ("LlamaParse",),
            "MistralDataProvider": ("MistralAi",),
            "PaddleDataProvider": ("Paddle",),
            "PineconeDataProvider": ("PineconeAdmin", "Pinecone"),
            "StabilityAiDataProvider": ("StabilityAi",),
            "StripeDataProvider": ("Stripe",),
            "TogetherDataProvider": ("TogetherAi",),
            "UnstructuredDataProvider": ("UnstructuredIo",),
            "WooCommerceDataProvider": ("WooCommerce",),
            "XeroDataProvider": ("Xero",),
        };

        #! Modules whose lazy registration builds no action set
        /** \c AwsTextractDataProvider generates its actions from its OpenAPI types directly, and the other two
            register hand-written actions; their first read is checked by the actions it registers only
        */
        const NoActionSets = {
            "AwsTextractDataProvider": True,
            "EasyPostDataProvider": True,
            "WooCommerceDataProvider": True,
        };

        #! The module whose actions are read by concurrent threads
        const ConcurrentModule = "ExaDataProvider";

        #! The modules whose cases are in part 2; the others are in part 1 (see addPartCase())
        /** The parts take about the same time: the modules are assigned by the time that building their actions
            takes
        */
        const Part2Modules = {
            "AssemblyAiDataProvider": True,
            "AwsTextractDataProvider": True,
            "AzureDocumentIntelligenceDataProvider": True,
            "AzureOpenAiDataProvider": True,
            "BedrockDataProvider": True,
            "CohereDataProvider": True,
            "EasyPostDataProvider": True,
            "GitLabDataProvider": True,
            "IntercomDataProvider": True,
            "JinaDataProvider": True,
            "KlaviyoDataProvider": True,
            "LlamaParseDataProvider": True,
            "MistralDataProvider": True,
            "XeroDataProvider": True,
        };
    }

    private {
        #! the qlib source directory
        string qlib = normalize_dir(get_script_dir() + "/../../../../qlib");

        #! the number of action sets constructed by loading each module
        hash<string, int> load_sets = {};

        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : Test(name, "1.0") {
        self.part = part;
        addPartCase(1, "the module list names every module with a lazy action registration", \moduleListTest());
        addPartCase(1, "the modules of part 2 have lazy action registrations", \partModulesTest());
        addPartCase(1, "concurrent first reads build the action sets once", \concurrentTest());
        # every other module is loaded before any app is read, so that a read cannot load a module; each part
        # loads them all
        foreach string mod in (keys Modules) {
            if (mod == ConcurrentModule) {
                continue;
            }
            int before = RestSchemaActionSet::getConstructionCount();
            load_module(mod);
            load_sets{mod} = RestSchemaActionSet::getConstructionCount() - before;
            if (Part2Modules{mod}) {
                addPartCase(2, mod, \moduleTest(), mod);
            } else {
                addPartCase(1, mod, \moduleTest(), mod);
            }
        }
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    #! every module of part 2 is a module of the list, so that every module's case runs in one part
    partModulesTest() {
        assertEq((), keys (Part2Modules - keys Modules));
    }

    moduleListTest() {
        hash<string, bool> lazy = {};
        Dir top();
        top.chdir(qlib);
        foreach string name in (top.listDirs()) {
            string path = qlib + DirSep + name + DirSep + name + ".qm";
            # a call in the module's code, not a mention in its documentation
            if (is_file(path) && ReadOnlyFile::readTextFile(path)
                    =~ /\n\s+DataProviderActionCatalog::registerLazyAppActions\(/) {
                lazy{name} = True;
            }
        }
        assertEq(sort(keys Modules), sort(keys lazy));
    }

    concurrentTest() {
        string mod = ConcurrentModule;
        int before = RestSchemaActionSet::getConstructionCount();
        load_module(mod);
        assertEq(before, RestSchemaActionSet::getConstructionCount(), "loading builds no action set");
        string app = Modules{mod}[0];
        assertTrue(DataProviderActionCatalog::hasLazyAppActions(app));

        int threads = 8;
        Counter ready(threads);
        Counter go(1);
        Counter done(threads);
        Queue results();
        for (int i = 0; i < threads; ++i) {
            background LazyModuleActionsBase::readActions(app, ready, go, done, results);
        }
        ready.waitForZero();
        go.dec();
        done.waitForZero();

        # the module builds one action set
        assertEq(before + 1, RestSchemaActionSet::getConstructionCount(), "the action set is built once");
        list<string> expected = map $1.action, DataProviderActionCatalog::getActions(app, False);
        assertTrue(expected.size() > 1);
        assertEq(threads, results.size());
        while (results.size()) {
            assertEq(expected, results.get());
        }
        assertEq((), DataProviderActionCatalog::getInitializationFailures((app,)));
    }

    moduleTest(string mod) {
        assertEq(0, load_sets{mod}, "loading builds no action set");
        foreach string app in (Modules{mod}) {
            assertTrue(DataProviderActionCatalog::hasLazyAppActions(app), app + " is lazy after loading");
            int before = RestSchemaActionSet::getConstructionCount();
            list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActions(app, False);
            assertFalse(DataProviderActionCatalog::hasLazyAppActions(app), app + " is complete after reading");
            assertTrue(actions.size() > 1, app + " has its actions");
            if (!NoActionSets{mod}) {
                assertTrue(RestSchemaActionSet::getConstructionCount() > before,
                    app + ": the first read builds the action sets");
            }
            assertEq((), DataProviderActionCatalog::getInitializationFailures((app,)));
            int after = RestSchemaActionSet::getConstructionCount();
            DataProviderActionCatalog::getActions(app, False);
            assertEq(after, RestSchemaActionSet::getConstructionCount(), app + ": a second read builds nothing");
        }
    }

    #! Reads the actions of the app once every thread is ready; runs in a thread of the test
    private static readActions(string app, Counter ready, Counter go, Counter done, Queue results) {
        on_exit done.dec();
        try {
            ready.dec();
            go.waitForZero();
            results.push(map $1.action, DataProviderActionCatalog::getActions(app, False));
        } catch (hash<ExceptionInfo> ex) {
            results.push(ex);
        }
    }
}
