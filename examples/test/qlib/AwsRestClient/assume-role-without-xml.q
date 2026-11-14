#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*  Copyright 2026 Qore Technologies, s.r.o.

    Helper of AwsRestClient.qtest (qore#5833): calls AwsRestClientBase::assumeRole() in a process without the xml
    module.  The module is blacklisted before AwsRestClient is loaded, so RestClient's "%try-module xml" fails as on a
    system without the module, and RestClient cannot deserialize the text/xml response of STS.

    usage: assume-role-without-xml.q <sts endpoint URL>
    prints "xml=False" and then either "keyid=... secret=... token=... expires=YYYY-MM-DD" or "error=<code>"
*/

%modern

qore_add_module_to_blacklist_ex("xml", "the test runs without the xml module");
# loaded at run time, after the blacklist entry
load_module("AwsRestClient");

printf("xml=%y\n", exists get_module_hash().xml);
try {
    hash<auto> creds = call_static_method("AwsRestClient::AwsRestClientBase", "assumeRole", {
        "aws_keyid": "AKIA-BASE",
        "aws_secret": "baseSecret",
        "aws_role_arn": "arn:aws:iam::123456789012:role/MyRole",
        "aws_sts_endpoint": ARGV[0],
    });
    printf("keyid=%s secret=%s token=%s expires=%s\n", creds.aws_keyid, creds.aws_secret, creds.aws_token,
        creds.expires.format("YYYY-MM-DD"));
} catch (hash<ExceptionInfo> ex) {
    printf("error=%s\n", ex.err);
    printf("desc=%s\n", ex.desc);
}
