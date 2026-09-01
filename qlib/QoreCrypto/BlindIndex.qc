# -*- mode: qore; indent-tabs-mode: nil -*-

/*  BlindIndex.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see BlindIndex
public namespace QoreCrypto {

#! Current blind index token version
public const BLIND_INDEX_VERSION = 1;

#! Blind index algorithm
public const BLIND_INDEX_ALGORITHM = "HMAC-SHA256";

#! Minimum blind index length in bytes
public const BLIND_INDEX_MIN_BYTES = 8;

#! Maximum blind index length in bytes; the full HMAC-SHA256 output
public const BLIND_INDEX_MAX_BYTES = 32;

#! Separator between blind index token fields
public const BLIND_INDEX_SEPARATOR = ":";

#! A parsed blind index token
public hashdecl BlindIndexToken {
    #! Token format version
    int version;
    #! Blind index key generation that produced the token
    string key_id;
    #! Hex-encoded truncated HMAC
    string index;
}

#! Keyed blind index for equality lookups over encrypted columns
/** A blind index lets a caller find rows by an exact value without storing that
    value in plaintext and without exposing it to the database.  The stored
    token is a keyed HMAC of the value; an attacker with the database but not
    the index key cannot recover the value or test candidate values offline.

    @par Independent keys
    Blind index keys are separate from the KEKs held by
    @ref AbstractKeyProvider and rotate independently, because the two have
    different rotation costs: rewrapping a DEK is cheap and touches only key
    metadata, while rotating a blind index key means recomputing every stored
    token from plaintext.  Never reuse an encryption key as a blind index key.

    @par Rotation
    Every token carries the key generation that produced it, so a rotation can
    proceed one row at a time while lookups still match: search with
    @ref indexAll() to cover every live generation, then drop the retired
    generation from the keyring once no rows reference it.

    @par What a blind index leaks
    Equal plaintexts produce equal tokens under the same key, so a blind index
    reveals equality and therefore value frequency.  It is not suitable for
    low-entropy values whose distribution is itself sensitive.  Truncating the
    index shortens the stored token at the cost of collisions; collisions are
    safe for lookup (callers must still confirm each candidate row by
    decrypting), and deliberately blur frequency, but they add read cost.

    @par Not an authenticator
    @ref matches() is an equality helper for lookups, not an authentication
    check; it does not compare in constant time.  Never use a blind index token
    as a credential, a capability, or a substitute for authenticated
    encryption.

    @par Example
    @code{.py}
BlindIndex idx(index_key, "qonsole_chat_messages.author");
string token = idx.index("user@example.com");
# ... store token in an indexed column alongside the ciphertext envelope ...
# later, to look the value up again:
*hash<auto> row = find_row(idx.index("user@example.com"));
    @endcode

    @par Thread safety
    Safe to share between threads; immutable after construction.

    @see @ref qorecrypto_blind_index
*/
public class BlindIndex {
    private {
        #! blind index key generations by key id
        hash<string, binary> keyring;
        #! current blind index key generation
        string current_key_id;
        #! logical index identifier, mixed into every token
        string index_id;
        #! stored index length in bytes
        int index_bytes;
    }

    #! Creates a single-generation blind index
    /** @param key 32-byte blind index key; must not be an encryption key
        @param index_id logical index identifier, such as
        \c "qonsole_chat_messages.author"; mixed into every token so that the
        same value indexed for two different columns does not produce the same
        token
        @param key_id key identifier; defaults to "current"
        @param index_bytes stored index length in bytes; defaults to the full
        32-byte HMAC output

        @throw QORE-CRYPTO-BLIND-INDEX-ERROR an identifier or the index length
        is invalid
        @throw QORE-CRYPTO-KEY-ERROR the key is not exactly 32 bytes
    */
    constructor(data key, string index_id, string key_id = "current",
            int index_bytes = BLIND_INDEX_MAX_BYTES) {
        BlindIndex::validateIndexId(index_id);
        BlindIndex::validateKeyId(key_id);
        self.keyring = {key_id: BlindIndex::normalizeIndexKey(key, "key")};
        self.current_key_id = key_id;
        self.index_id = index_id;
        self.index_bytes = BlindIndex::validateIndexBytes(index_bytes);
    }

    #! Creates a multi-generation blind index from a keyring
    /** @param keyring blind index key generations keyed by key id
        @param current_key_id the generation used for new tokens; must be
        present in \a keyring
        @param index_id logical index identifier
        @param index_bytes stored index length in bytes; defaults to the full
        32-byte HMAC output

        @throw QORE-CRYPTO-BLIND-INDEX-ERROR the keyring is empty, an identifier
        or the index length is invalid, or \a current_key_id is not present in
        the keyring
        @throw QORE-CRYPTO-KEY-ERROR a key is not exactly 32 bytes
    */
    constructor(hash<string, data> keyring, string current_key_id, string index_id,
            int index_bytes = BLIND_INDEX_MAX_BYTES) {
        BlindIndex::validateIndexId(index_id);
        BlindIndex::validateKeyId(current_key_id);
        if (!keyring) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR", "keyring must contain at least one key";
        }

        hash<string, binary> normalized;
        foreach hash<auto> i in (keyring.pairIterator()) {
            BlindIndex::validateKeyId(i.key);
            normalized{i.key} = BlindIndex::normalizeIndexKey(i.value,
                sprintf("keyring key %y", i.key));
        }
        if (!exists normalized{current_key_id}) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("current key id %y is not present in the keyring; known key ids: %y",
                    current_key_id, sort(normalized.keys()));
        }

        self.keyring = normalized;
        self.current_key_id = current_key_id;
        self.index_id = index_id;
        self.index_bytes = BlindIndex::validateIndexBytes(index_bytes);
    }

    #! Returns the logical index identifier
    string getIndexId() {
        return index_id;
    }

    #! Returns the current blind index key generation
    string getCurrentKeyId() {
        return current_key_id;
    }

    #! Returns all blind index key generations in sorted order
    list<string> getKeyIds() {
        return sort(keyring.keys());
    }

    #! Returns @ref True if the given key generation is available
    bool hasKey(string key_id) {
        return exists keyring{key_id};
    }

    #! Returns the stored index length in bytes
    int getIndexBytes() {
        return index_bytes;
    }

    #! Computes the blind index token for a value
    /** @param value the value to index; canonicalized the same way AAD is, so
        structured values index deterministically
        @param key_id the key generation to use; defaults to the current
        generation

        @return a token of the form \c "<version>:<key_id>:<hex>"

        @throw QORE-CRYPTO-BLIND-INDEX-ERROR the key generation is unavailable
        @throw QORE-CRYPTO-AAD-ERROR the value is missing or cannot be canonicalized
    */
    string index(auto value, *string key_id) {
        string effective_key_id = key_id ?? current_key_id;
        binary index_key = getIndexKey(effective_key_id);
        string message = sprintf("QoreCrypto:blind-index:v%d:%s:%s", BLIND_INDEX_VERSION,
            index_id, EnvelopeCipher::canonicalizeAad(value));
        string hex = SHA256_hmac(message, index_key);
        return sprintf("%d%s%s%s%s", BLIND_INDEX_VERSION, BLIND_INDEX_SEPARATOR, effective_key_id,
            BLIND_INDEX_SEPARATOR, hex.substr(0, index_bytes * 2));
    }

    #! Computes the blind index token for a value under every key generation
    /** Use this to build a lookup that still matches rows written under a
        retired key generation while a blind index rotation is in progress.

        @param value the value to index

        @return one token per key generation, in sorted key id order
    */
    list<string> indexAll(auto value) {
        list<string> rv = ();
        foreach string key_id in (getKeyIds()) {
            push rv, index(value, key_id);
        }
        return rv;
    }

    #! Returns @ref True if a stored token matches a value
    /** The token's own key generation is used, so a token written under a
        retired generation still matches while that generation remains in the
        keyring.  A token naming an unknown generation does not match.

        @param token a stored blind index token
        @param value the candidate value

        @return @ref True if the token was produced from \a value

        @throw QORE-CRYPTO-BLIND-INDEX-ERROR the token is malformed

        @note this is an equality helper for lookups, not an authentication
        check; it does not compare in constant time
    */
    bool matches(string token, auto value) {
        hash<BlindIndexToken> parsed = BlindIndex::parseToken(token);
        if (!hasKey(parsed.key_id)) {
            return False;
        }
        return index(value, parsed.key_id) == token;
    }

    #! Parses a blind index token
    /** @param token the token to parse

        @return the parsed token

        @throw QORE-CRYPTO-BLIND-INDEX-ERROR the token is malformed or has an
        unsupported version
    */
    static hash<BlindIndexToken> parseToken(string token) {
        list<auto> parts = split(BLIND_INDEX_SEPARATOR, token);
        if (parts.size() != 3) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index token %y is malformed; expecting "
                    "\"<version>:<key_id>:<hex>\"", token);
        }

        string version_str = parts[0];
        if (!(version_str =~ /^[0-9]+$/)) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index token %y has a non-numeric version", token);
        }
        int version = version_str.toInt();
        if (version != BLIND_INDEX_VERSION) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("unsupported blind index token version %d in token %y", version, token);
        }

        string key_id = parts[1];
        BlindIndex::validateKeyId(key_id);

        string index_hex = parts[2];
        if (!(index_hex =~ /^[0-9a-f]+$/)) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index token %y does not carry a lowercase hex index", token);
        }
        if (index_hex.size() % 2
            || index_hex.size() < BLIND_INDEX_MIN_BYTES * 2
            || index_hex.size() > BLIND_INDEX_MAX_BYTES * 2) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index token %y has an invalid index length %d", token,
                    index_hex.size());
        }

        return <BlindIndexToken>{
            "version": version,
            "key_id": key_id,
            "index": index_hex,
        };
    }

    #! Returns the blind index key for the given generation
    private binary getIndexKey(string key_id) {
        *binary rv = keyring{key_id};
        if (!exists rv) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index key id %y is not available for index %y; known key ids: %y",
                    key_id, index_id, getKeyIds());
        }
        return rv;
    }

    #! Validates a blind index key identifier
    private static validateKeyId(string key_id) {
        EnvelopeCipher::validateIdentifier(key_id, "blind index key id");
        if (key_id.find(BLIND_INDEX_SEPARATOR) != -1) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("blind index key id %y must not contain %y, which separates token fields",
                    key_id, BLIND_INDEX_SEPARATOR);
        }
    }

    #! Validates a logical index identifier
    private static validateIndexId(string index_id) {
        if (!index_id.val()) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR", "index_id must not be empty";
        }
        if (index_id =~ /[\r\n]/) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                "index_id must not contain newline characters";
        }
    }

    #! Validates the stored index length
    private static int validateIndexBytes(int index_bytes) {
        if (index_bytes < BLIND_INDEX_MIN_BYTES || index_bytes > BLIND_INDEX_MAX_BYTES) {
            throw "QORE-CRYPTO-BLIND-INDEX-ERROR",
                sprintf("index_bytes must be between %d and %d, got %d",
                    BLIND_INDEX_MIN_BYTES, BLIND_INDEX_MAX_BYTES, index_bytes);
        }
        return index_bytes;
    }

    #! Validates blind index key material
    private static binary normalizeIndexKey(data key, string arg_name) {
        return EnvelopeCipher::normalizeAes256Key(key, arg_name);
    }
}

}
