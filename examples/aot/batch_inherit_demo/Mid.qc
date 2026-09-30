%modern

# Middle layer — empty-body ctor delegating args to Base via
# explicit BCA.  Matches the shape that broke on qwf where
# `AbstractQorusDistributedProcessWithDebugProgramControl`'s
# constructor body was empty and only existed to forward args.
public namespace BatchInherit {
    public class Mid inherits Base {
        constructor(reference<list<string>> p_argv) : Base(\p_argv) {
        }
    }
}
