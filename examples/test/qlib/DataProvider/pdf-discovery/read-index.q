# Copyright 2026 Qore Technologies, s.r.o. MIT License
%modern
%requires ProviderIndexUtil
%requires json

ProviderIndexUtil::setEnvironmentVariable(ARGV[0]);
if (!ProviderIndexUtil::tryGetAppInfo("Qore")) {
    throw "TEST-ERROR", "the regenerated index is missing the Qore app";
}
hash<auto> actions = {};
foreach string name in (("list-form-fields", "get-form-data", "fill-form", "render-page", "extract-text")) {
    if (*hash<auto> action = ProviderIndexUtil::tryGetActionMetadata("PDF", name)) {
        actions{name} = action{"app", "action", "cls", "cls_mod", "path", "desc"};
    }
}
printf("%s", make_json(actions));
