# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT

# The cases of the ProviderExamples-<n>.qtest tests: each runs one part of them, so that each takes a short time,
# and together they run every case (see addPartCase()); the modules are assigned to the parts by the time that
# building their actions takes, as in RestSchemaActions/LazyModuleActions.qc


#! Offline coverage of every option of the built-in schema-backed applications
class ProviderExamplesBase inherits QUnit::Test {
    private {
        #! the part of the cases that this test runs (see addPartCase())
        int part;
    }

    constructor(string name, int part) : Test(name, "1.0", \ARGV) {
        self.part = part;
        addPartCase(2, "AssemblyAiDataProvider", \checkApp(), AssemblyAiDataProvider::AppName);
        addPartCase(2, "AwsTextractDataProvider", \checkApp(), AwsTextractDataProvider::AppName);
        addPartCase(2, "AzureDocumentIntelligenceDataProvider", \checkApp(), AzureDocumentIntelligenceDataProvider::AppName);
        addPartCase(2, "AzureOpenAiDataProvider", \checkApp(), AzureOpenAiDataProvider::AppName);
        addPartCase(2, "BedrockDataProvider", \checkApp(), BedrockDataProvider::AppName);
        addPartCase(1, "BflDataProvider", \checkApp(), BflDataProvider::AppName);
        addPartCase(1, "BitbucketDataProvider", \checkApp(), BitbucketDataProvider::AppName);
        addPartCase(2, "CohereDataProvider", \checkApp(), CohereDataProvider::AppName);
        addPartCase(1, "ConfluenceDataProvider", \checkApp(), ConfluenceDataProvider::AppName);
        addPartCase(1, "ExaDataProvider", \checkApp(), ExaDataProvider::AppName);
        addPartCase(1, "FireworksDataProvider", \checkApp(), FireworksDataProvider::AppName);
        addPartCase(1, "GitHubDataProvider", \checkApp(), GitHubDataProvider::AppName);
        addPartCase(2, "GitLabDataProvider", \checkApp(), GitLabDataProvider::AppName);
        addPartCase(1, "GoogleDocumentAiDataProvider", \checkApp(), GoogleDocumentAiDataProvider::AppName);
        addPartCase(2, "IntercomDataProvider", \checkApp(), IntercomDataProvider::AppName);
        addPartCase(2, "JinaDataProvider", \checkApp(), JinaDataProvider::AppName);
        addPartCase(2, "KlaviyoDataProvider", \checkApp(), KlaviyoDataProvider::AppName);
        addPartCase(2, "LlamaParseDataProvider", \checkApp(), LlamaParseDataProvider::AppName);
        addPartCase(1, "MewsRestDataProvider", \checkApp(), "Mews");
        addPartCase(2, "MistralDataProvider", \checkApp(), MistralDataProvider::AppName);
        addPartCase(1, "PaddleDataProvider", \checkApp(), PaddleDataProvider::AppName);
        addPartCase(1, "PineconeDataProvider", \checkApp(), PineconeDataProvider::AppName);
        addPartCase(1, "StabilityAiDataProvider", \checkApp(), StabilityAiDataProvider::AppName);
        addPartCase(1, "StripeDataProvider", \checkApp(), StripeDataProvider::AppName);
        addPartCase(1, "TogetherDataProvider", \checkApp(), TogetherDataProvider::AppName);
        addPartCase(1, "UnstructuredDataProvider", \checkApp(), UnstructuredDataProvider::AppName);
        addPartCase(2, "XeroDataProvider", \checkApp(), XeroDataProvider::AppName);
        set_return_value(main());
    }

    private addPartCase(int case_part, string name, code call, *softlist<auto> args) {
        if (case_part == part) {
            addTestCase(name, call, args);
        }
    }

    checkApp(string app) {
        list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActions(app, False);
        assertTrue(actions.size() > 0, app + " has actions");
        int count = 0;
        foreach hash<DataProviderActionInfo> action in (actions) {
            foreach hash<auto> entry in ((action.options ?? {}).pairIterator()) {
                hash<auto> option = entry.value;
                string ctx = app + "/" + action.action + "/" + entry.key;
                on_error rethrow $1.err, ctx + ": " + $1.desc;
                QoreDataField field(option + {"name": entry.key});
                if (exists option.example_value) {
                    assertEq(field.getExampleValue(), field.getType().acceptsValue(option.example_value), ctx);
                }
                if (app == "Paddle" && action.action == "create_price" && entry.key == "product_id") {
                    assertEq("pro_01gsz97mq9pa4fkyy0wqenepkz", option.example_value,
                        "presentation overrides preserve the schema's product identifier example");
                }
                ++count;
            }
        }
        assertTrue(count > 0, app + " has action options");
    }
}
