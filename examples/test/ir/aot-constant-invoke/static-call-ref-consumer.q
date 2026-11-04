#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls the members of the AOTStaticCallRef fixture that hold call references and prints the results

%modern
%requires AOTStaticCallRef

printf("call-try: %s\n", AOTStaticCallRef::Refs::callTry());
printf("call-plain: %s\n", AOTStaticCallRef::Refs::callPlain());
printf("call-script: %s\n", AOTStaticCallRef::Refs::describe(AOTStaticCallRef::Refs::sref,
    AOTStaticCallRef::Refs::refs, AOTStaticCallRef::Refs::list_refs, new AOTStaticCallRef::Refs()));
