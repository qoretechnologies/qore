# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdBlankNodeGenerator.qc @brief Blank node identifier issuer

/*  JsonLdBlankNodeGenerator.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! Blank node identifier issuer per W3C spec
/** Issues unique blank node identifiers and maintains a mapping from old to new
    identifiers for consistent renaming during algorithms like flattening.

    @since json 1.12
*/
public class JsonLdBlankNodeGenerator {
    private {
        #! Identifier prefix
        string prefix;

        #! Counter for generating unique identifiers
        int counter = 0;

        #! Mapping from existing identifiers to issued identifiers
        hash<string, string> identifierMap = {};

        #! Ordered list of existing identifiers
        list<string> issuedOrder = ();
    }

    #! Creates a new blank node identifier issuer
    /** @param prefix the prefix for generated identifiers (default: "_:b")
    */
    constructor(string prefix = "_:b") {
        self.prefix = prefix;
    }

    #! Gets or creates a blank node identifier for the given old identifier
    /** If \a oldIdentifier has already been issued a new identifier, the same new identifier
        is returned. Otherwise, a new unique identifier is issued.

        @param oldIdentifier the existing blank node identifier to map
        @return the issued blank node identifier

        @since json 1.12
    */
    string getId(string oldIdentifier) {
        if (exists identifierMap{oldIdentifier}) {
            return identifierMap{oldIdentifier};
        }
        string newId = prefix + string(counter);
        ++counter;
        identifierMap{oldIdentifier} = newId;
        push issuedOrder, oldIdentifier;
        return newId;
    }

    #! Generates a fresh blank node identifier
    /** @return a new unique blank node identifier

        @since json 1.12
    */
    string generate() {
        string newId = prefix + string(counter);
        ++counter;
        return newId;
    }

    #! Returns True if the given identifier has already been issued a mapping
    /** @param oldIdentifier the identifier to check
        @return True if \a oldIdentifier has been issued a new identifier

        @since json 1.12
    */
    bool hasId(string oldIdentifier) {
        return exists identifierMap{oldIdentifier};
    }

    #! Returns a copy of the identifier map
    /** @return the mapping from old identifiers to issued identifiers

        @since json 1.12
    */
    hash<string, string> getIdentifierMap() {
        return identifierMap;
    }

    #! Returns the ordered list of issued identifiers
    /** @return the ordered list of old identifiers in issuance order

        @since json 1.12
    */
    list<string> getIssuedOrder() {
        return issuedOrder;
    }

    #! Creates a copy of this issuer with the same state
    /** @return a new JsonLdBlankNodeGenerator with the same prefix, counter, and mappings

        @since json 1.12
    */
    JsonLdBlankNodeGenerator clone() {
        JsonLdBlankNodeGenerator copy(prefix);
        copy.counter = counter;
        copy.identifierMap = identifierMap;
        copy.issuedOrder = issuedOrder;
        return copy;
    }
}

} # namespace JsonLd
