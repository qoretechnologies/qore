#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the calls of DeletedByArgsCallTargets.qi with a legacy semantic policy (%broken-range, which %modern does not
# allow): the AST interpreter, which only runs code that is not %modern, makes the calls with the policy
# (LexicalSemanticPolicy), and the output is the same
%new-style
%require-types
%strict-args
%broken-range

%include DeletedByArgsCallTargets.qi
