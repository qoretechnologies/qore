%modern

# Load the recipient only after the child Program has its restrictions.
Program loader(PO_NEW_STYLE | PO_NO_TERMINAL_IO);
loader.setSandboxManager(new SandboxManager());
loader.loadModule("DiagnosticRecipient");
loader.loadModule("ProviderIndexUtil");
loader.parse("auto sub state() { return (DiagnosticRecipient::report(), "
    "exists ProviderIndexUtil::getModuleDiagnosticSink()); }", "restricted-first");
printf("%y\n", loader.callFunction("state"));

Program trusted(PO_NEW_STYLE);
trusted.loadModule("DiagnosticRecipient");
trusted.parse("bool sub report() { return DiagnosticRecipient::report(); }", "later-trusted");
printf("%y\n", trusted.callFunction("report"));
