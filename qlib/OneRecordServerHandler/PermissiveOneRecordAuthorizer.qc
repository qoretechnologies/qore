# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file PermissiveOneRecordAuthorizer.qc @brief Permissive authorizer for ONE Record (testing)

/*  PermissiveOneRecordAuthorizer.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %OneRecordServerHandler namespace holds all public definitions
public namespace OneRecordServerHandler {

#! Permissive ONE Record authorizer that allows all operations
/** Returns True for all authorization checks and uses a configurable default
    identity. Suitable for testing and development.

    @since json 1.12
*/
public class PermissiveOneRecordAuthorizer inherits AbstractOneRecordAuthorizer {
    private {
        #! Default identity URI
        string default_identity;
    }

    #! Creates a permissive authorizer with a default identity
    /** @param identity the default identity URI (default: "https://example.com/default-user")
    */
    constructor(string identity = "https://example.com/default-user") {
        default_identity = identity;
    }

    bool canCreate(string identity, string object_type) {
        return True;
    }

    bool canRead(string identity, string object_id) {
        return True;
    }

    bool canRequestChange(string identity, string object_id) {
        return True;
    }

    bool canManageActionRequests(string identity, string object_id) {
        return True;
    }

    bool canCreateEvent(string identity, string object_id) {
        return True;
    }

    bool canSubscribe(string identity, *string topic) {
        return True;
    }

    string getIdentity(hash<auto> cx, hash<auto> hdr) {
        # Try to extract from Authorization header (Bearer token)
        *string auth = hdr.authorization ?? hdr.Authorization;
        if (exists auth && auth =~ /^Bearer /) {
            # In a real implementation, decode JWT and extract logistics_agent_uri
            return default_identity;
        }
        return default_identity;
    }
}

} # namespace OneRecordServerHandler
