# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketCodecs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the BitbucketDataProvider module
public namespace BitbucketDataProvider {
#! The wire codecs this application registers
/** Bitbucket names the branches of a pull request as \c "source.branch.name" and its reviewers as a list
    of user objects each carrying a \c uuid.  Those are the shapes the API takes, and they are correct -
    but asking somebody to fill in three levels of nesting to name a branch is not a usable form, and a
    dropdown cannot offer a value into the middle of a nested object.

    So the manifest presents a branch as a plain name and the reviewers as a plain list of IDs, and names
    a codec that converts each to the shape Bitbucket expects.

    @section bitbucketcodecs_why Why a Codec and Not a Request Transform

    A codec is **declarative and reversible**: the manifest names it, the option is published with the
    codec's own type, and the value converts in both directions - so a response reads back as the same flat
    value the request took, and the overlay carries no executable code.  A request transform would convert
    one way only, and would have to be written and maintained per action.

    @section bitbucketcodecs_registration Registration

    Codecs live in a process-wide registry, so they are registered before the action set that references
    them is built - see @ref BitbucketDataProvider::BitbucketSchema::getActionSet() "getActionSet()".
    Registering twice is harmless; the second registration replaces the first with the same value.
*/
public class BitbucketCodecs {
    public {
        #! Codec name: a branch name presented for Bitbucket's nested branch reference
        const BranchRef = "bitbucket-branch-ref";

        #! Codec name: a list of account IDs presented for Bitbucket's list of user objects
        const ReviewerIds = "bitbucket-reviewer-ids";

        #! The codecs this application registers
        const Codecs = {
            BranchRef: <RestSchemaWireCodecInfo>{
                "name": BranchRef,
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "sent as Bitbucket's nested branch reference, `{\"branch\": {\"name\": …}}`",
                "to_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    return {"branch": {"name": v.toString()}};
                },
                "from_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    # a response carries the whole reference; the branch name is the part that was asked
                    # for, and anything else is left alone rather than guessed at
                    if (v.typeCode() == NT_HASH) {
                        return v.branch.name ?? v.name;
                    }
                    return v;
                },
            },
            ReviewerIds: <RestSchemaWireCodecInfo>{
                "name": ReviewerIds,
                "type": AbstractDataProviderType::get(new Type("*softlist<string>")),
                "desc": "sent as Bitbucket's list of user objects, `[{\"type\": \"user\", \"uuid\": …}]`",
                "to_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    list<auto> rv = ();
                    foreach auto id in (v) {
                        # the account type declares its "type" discriminator as required, so a bare
                        # {"uuid": …} is rejected before the request is sent even though Bitbucket
                        # accepts one
                        rv += {"type": "user", "uuid": id.toString()};
                    }
                    return rv;
                },
                "from_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    list<auto> rv = ();
                    foreach auto user in (v) {
                        # a response carries whole user objects; the ID is what the request took
                        rv += user.typeCode() == NT_HASH ? (user.uuid ?? user.account_id) : user;
                    }
                    return rv;
                },
            },
        };
    }

    #! Registers this application's codecs
    /** Called before the action set that references them is built.  Safe to call more than once.
    */
    static register() {
        map RestSchemaActionCodecs::registerCodec($1), Codecs.iterator();
    }
}
}
