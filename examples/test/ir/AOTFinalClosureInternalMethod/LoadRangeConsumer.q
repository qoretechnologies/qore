%modern
# Copyright 2026 Qore Technologies, s.r.o.
%requires RangeConsumer

sub run() {
    RangeConsumer::Consumer consumer();
    printf("%s\n", consumer.run());
}
run();
