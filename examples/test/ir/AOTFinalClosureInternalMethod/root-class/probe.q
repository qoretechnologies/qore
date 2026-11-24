# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern
%requires GateProbe
class Gate { int marker() { return 91; } }
::Gate consumer();
printf("%d/%d\n", GateProbe::probe(), consumer.marker());
