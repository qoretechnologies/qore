%new-style
%require-types
%strict-args
%enable-all-warnings

# Top of the 3-deep chain — Top → Mid → Base.  Empty-body
# constructor that delegates args to Mid via explicit BCA.
public namespace BatchInherit {
    public class Top inherits Mid {
        constructor(reference<list<string>> p_argv) : Mid(\p_argv) {
        }
    }
}
