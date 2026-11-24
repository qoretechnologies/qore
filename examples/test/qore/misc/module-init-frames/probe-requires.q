#!/usr/bin/env qore
# the initialization of a module that the script requires (qore#7289): run by ../module-init-frames.qtest

%modern
%requires InitFramesA

printf("%s\n", InitFramesA::InitFramesA::report());
