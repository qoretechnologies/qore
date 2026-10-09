#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# calls a closure created in a static method, which uses a private constructor and a private static method of its
# class, often enough to run it on the IR tier, and prints the results

%modern

class Node {
    public {
        int count;
    }

    constructor(string value) {
        count = value.size();
    }

    private constructor(list<Node> children) {
        count = children[0].count + children[1].count;
    }

    private static int twice(int i) {
        return i * 2;
    }

    #! returns a closure that uses the class's private constructor and private static method
    static code<int(Node, Node)> factory() {
        return int sub (Node a, Node b) { return Node::twice(new Node((a, b)).count); };
    }
}

code<int(Node, Node)> f = Node::factory();
Node a("abc");
Node b("de");
int sum = 0;
# more calls than the default IR threshold, so that the closure also runs on the IR tier
for (int i = 0; i < 250; ++i) {
    sum += f(a, b);
}
printf("%d\n", sum);
