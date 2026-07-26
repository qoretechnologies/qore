# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractOneRecordAuthorizer.qc @brief Abstract authorization interface for ONE Record

/*  AbstractOneRecordAuthorizer.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for ONE Record authorization
/** Provides the interface for authorizing operations on ONE Record resources.
    Implementations can use ACLs, RBAC, or any other authorization model.

    @since json 1.12
*/
public class AbstractOneRecordAuthorizer {
    #! Checks if an identity can create logistics objects
    /** @param identity the requestor identity URI
        @param object_type the type of object being created
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canCreate(string identity, string object_type);

    #! Checks if an identity can read a logistics object
    /** @param identity the requestor identity URI
        @param object_id the object ID
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canRead(string identity, string object_id);

    #! Checks if an identity can submit change requests for an object
    /** @param identity the requestor identity URI
        @param object_id the object ID
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canRequestChange(string identity, string object_id);

    #! Checks if an identity can manage action requests (accept/reject) for an object
    /** This typically checks if the identity is the data holder for the object.

        @param identity the requestor identity URI
        @param object_id the object ID
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canManageActionRequests(string identity, string object_id);

    #! Checks if an identity can create events on an object
    /** @param identity the requestor identity URI
        @param object_id the object ID
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canCreateEvent(string identity, string object_id);

    #! Checks if an identity can subscribe to notifications
    /** @param identity the requestor identity URI
        @param topic optional topic URI
        @return True if the operation is allowed

        @since json 1.12
    */
    abstract bool canSubscribe(string identity, *string topic);

    #! Extracts the identity from the request context
    /** @param cx the HTTP request context
        @param hdr the HTTP request headers
        @return the identity URI extracted from the request (e.g., from JWT claims)

        @throw ONE-RECORD-UNAUTHORIZED if no identity can be extracted

        @since json 1.12
    */
    abstract string getIdentity(hash<auto> cx, hash<auto> hdr);
}

} # namespace OneRecordServerHandler
