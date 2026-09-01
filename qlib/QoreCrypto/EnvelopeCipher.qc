# -*- mode: qore; indent-tabs-mode: nil -*-

/*  EnvelopeCipher.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see EnvelopeCipher
public namespace QoreCrypto {

#! Current ciphertext envelope version
public const ENVELOPE_VERSION = 1;

#! Content-encryption algorithm used by this module
public const ENVELOPE_ALGORITHM = "AES-256-GCM";

#! Key-wrapping algorithm used by the built-in providers
public const KEY_WRAP_ALGORITHM = "AES-256-GCM";

#! Versioned ciphertext envelope produced by @ref EnvelopeCipher
/** Binary values are stored as base64 strings so that the envelope can be
    serialized directly as YAML or JSON by callers.

    @ref key_id "key_id" identifies the KEK generation that wrapped the DEK.
    Ciphertext written by a provider that always reports the same key id can
    never be rotated, because nothing in the stored envelope distinguishes one
    KEK generation from the next; providers must issue a distinct key id per
    KEK generation.  See @ref qorecrypto_rotation.
*/
public hashdecl CiphertextEnvelope {
    #! Envelope format version
    int version = ENVELOPE_VERSION;
    #! Content encryption algorithm
    string algorithm = ENVELOPE_ALGORITHM;
    #! Key-provider identifier
    string key_provider;
    #! Provider key identifier; identifies the KEK generation that wrapped the DEK
    string key_id;
    #! Algorithm used to wrap the DEK
    string key_wrap_algorithm = KEY_WRAP_ALGORITHM;
    #! Wrapped DEK, base64 encoded
    string wrapped_dek;
    #! IV used for DEK wrapping, base64 encoded
    string wrap_iv;
    #! Authentication tag for DEK wrapping, base64 encoded
    string wrap_tag;
    #! IV used for content encryption, base64 encoded
    string iv;
    #! Authentication tag for content encryption, base64 encoded
    string tag;
    #! Ciphertext, base64 encoded
    string ciphertext;
    #! SHA-256 digest of canonical AAD, hex encoded
    string aad_digest;
    #! Optional non-sensitive AAD descriptor for migration/audit tooling
    *string aad_descriptor;
    #! Caller-owned non-sensitive metadata
    *hash<auto> metadata;
}

#! Options for @ref EnvelopeCipher::encrypt()
public hashdecl EnvelopeEncryptOptions {
    #! Optional provider key identifier; if omitted, the provider's current key is used
    *string key_id;
    #! Optional non-sensitive AAD descriptor for migration/audit tooling
    *string aad_descriptor;
    #! Caller-owned non-sensitive metadata stored in the envelope
    *hash<auto> metadata;
}

#! Wrapped-key payload returned by key providers
public hashdecl WrappedKeyInfo {
    #! Key-provider identifier
    string key_provider;
    #! Provider key identifier
    string key_id;
    #! Key wrapping algorithm
    string algorithm = KEY_WRAP_ALGORITHM;
    #! Wrapped key, base64 encoded
    string wrapped_key;
    #! IV used for key wrapping, base64 encoded
    string iv;
    #! Authentication tag for key wrapping, base64 encoded
    string tag;
}

#! Abstract key-provider boundary for envelope encryption
/** Key providers own KEKs and wrap/unwrap per-record DEKs.  Implementations
    must fail closed: they must never return plaintext, substitute a different
    key silently, or downgrade encryption on errors.

    @par Key identifiers and rotation
    getCurrentKeyId() must return a distinct identifier for each KEK
    generation.  A provider that reports a constant key id across KEK changes
    makes every envelope it has ever written unrotatable; see
    @ref qorecrypto_rotation.  A provider that can still unwrap earlier
    generations must accept those key ids in unwrapKey(), which is what makes
    @ref EnvelopeCipher::rewrap() possible.

    @par Zeroization
    %Qore is garbage-collected and plaintext DEK buffers cannot be reliably
    zeroized from %Qore code; see @ref qorecrypto_zeroization.  Implementations
    must never log, serialize, or long-lived-cache plaintext key material.

    @par Identifier stability
    getProviderId() must be a pure local accessor: it must not perform I/O and
    must not fail, because it is used to build wrapping AAD and to label
    resilience state.
*/
public class AbstractKeyProvider {
    #! Returns the provider identifier recorded in ciphertext envelopes
    /** Must be a pure local accessor: no I/O, no failure modes.
    */
    abstract string getProviderId();

    #! Returns the provider's current key identifier
    /** Must return a distinct identifier per KEK generation.
    */
    abstract string getCurrentKeyId();

    #! Wraps a data-encryption key
    /** @param dek the data-encryption key to wrap
        @param key_id optional provider key identifier; if omitted the current key is used
        @param aad canonical wrapping AAD supplied by @ref EnvelopeCipher
        @return wrapped key information suitable for inclusion in the envelope
    */
    abstract hash<WrappedKeyInfo> wrapKey(binary dek, *string key_id, string aad);

    #! Unwraps a data-encryption key
    /** @param info wrapped key information from the envelope
        @param aad canonical wrapping AAD supplied by @ref EnvelopeCipher
        @return plaintext DEK
    */
    abstract binary unwrapKey(hash<WrappedKeyInfo> info, string aad);
}

#! Static in-memory key provider holding one or more KEK generations
/** This provider is intended for tests, local development, and simple local
    deployments.  Every KEK must be exactly 32 bytes.

    A keyring lets earlier KEK generations remain available for unwrapping
    while new envelopes are written under the current generation, which is what
    @ref EnvelopeCipher::rewrap() needs to rotate stored ciphertext without a
    readability gap.

    @par Thread safety
    Safe to share between threads; immutable after construction.
*/
public class StaticKeyProvider inherits AbstractKeyProvider {
    private {
        #! KEK generations by key id
        hash<string, binary> keyring;
        #! provider identifier
        string provider_id;
        #! current KEK generation
        string current_key_id;
    }

    #! Creates a single-generation provider
    /** @param key 32-byte AES-256 KEK
        @param provider_id provider identifier; defaults to "static"
        @param key_id key identifier; defaults to "current"

        @throw QORE-CRYPTO-KEY-ERROR the key is not a 32-byte AES-256 key, or an
        identifier is invalid
    */
    constructor(data key, string provider_id = "static", string key_id = "current") {
        EnvelopeCipher::validateIdentifier(provider_id, "provider_id");
        EnvelopeCipher::validateIdentifier(key_id, "key_id");
        self.keyring = {key_id: EnvelopeCipher::normalizeAes256Key(key, "key")};
        self.provider_id = provider_id;
        self.current_key_id = key_id;
    }

    #! Creates a multi-generation provider from a keyring
    /** @param keyring KEK generations keyed by key id; each value must be a
        32-byte AES-256 key
        @param current_key_id the generation used to wrap new DEKs; must be
        present in \a keyring
        @param provider_id provider identifier; defaults to "static"

        @throw QORE-CRYPTO-KEY-ERROR the keyring is empty, a key is not a
        32-byte AES-256 key, an identifier is invalid, or \a current_key_id is
        not present in the keyring
    */
    constructor(hash<string, data> keyring, string current_key_id, string provider_id = "static") {
        EnvelopeCipher::validateIdentifier(provider_id, "provider_id");
        EnvelopeCipher::validateIdentifier(current_key_id, "current_key_id");
        if (!keyring) {
            throw "QORE-CRYPTO-KEY-ERROR", "keyring must contain at least one key";
        }

        hash<string, binary> normalized;
        foreach hash<auto> i in (keyring.pairIterator()) {
            EnvelopeCipher::validateIdentifier(i.key, "keyring key id");
            normalized{i.key} = EnvelopeCipher::normalizeAes256Key(i.value,
                sprintf("keyring key %y", i.key));
        }
        if (!exists normalized{current_key_id}) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("current key id %y is not present in the keyring; known key ids: %y",
                    current_key_id, sort(normalized.keys()));
        }

        self.keyring = normalized;
        self.provider_id = provider_id;
        self.current_key_id = current_key_id;
    }

    string getProviderId() {
        return provider_id;
    }

    string getCurrentKeyId() {
        return current_key_id;
    }

    #! Returns all key ids known to this provider in sorted order
    list<string> getKeyIds() {
        return sort(keyring.keys());
    }

    #! Returns @ref True if the given key generation is available for unwrapping
    bool hasKey(string key_id) {
        return exists keyring{key_id};
    }

    hash<WrappedKeyInfo> wrapKey(binary dek, *string key_id, string aad) {
        string effective_key_id = key_id ?? current_key_id;
        binary kek = getKek(effective_key_id);

        binary iv = get_random_bytes(12);
        binary tag;
        binary wrapped;
        try {
            wrapped = encrypt(CRYPTO_ALG_AES_256, dek, kek, iv, \tag, 16, aad);
        } catch (hash<ExceptionInfo> ex) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("could not wrap DEK with provider %y key %y: %s: %s",
                    provider_id, effective_key_id, ex.err, ex.desc);
        }

        return <WrappedKeyInfo>{
            "key_provider": provider_id,
            "key_id": effective_key_id,
            "algorithm": KEY_WRAP_ALGORITHM,
            "wrapped_key": make_base64_string(wrapped),
            "iv": make_base64_string(iv),
            "tag": make_base64_string(tag),
        };
    }

    binary unwrapKey(hash<WrappedKeyInfo> info, string aad) {
        if (info.key_provider != provider_id) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("envelope provider %y does not match provider %y",
                    info.key_provider, provider_id);
        }
        if (info.algorithm != KEY_WRAP_ALGORITHM) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("unsupported key wrap algorithm %y", info.algorithm);
        }
        binary kek = getKek(info.key_id);

        try {
            return decrypt_to_binary(CRYPTO_ALG_AES_256, parse_base64_string(info.wrapped_key), kek,
                parse_base64_string(info.iv), parse_base64_string(info.tag), aad);
        } catch (hash<ExceptionInfo> ex) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("could not unwrap DEK with provider %y key %y: %s: %s",
                    provider_id, info.key_id, ex.err, ex.desc);
        }
    }

    #! Returns the KEK for the given generation
    /** @throw QORE-CRYPTO-KEY-ERROR the key generation is not available
    */
    private binary getKek(string key_id) {
        *binary kek = keyring{key_id};
        if (!exists kek) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("key id %y is not available in provider %y; known key ids: %y",
                    key_id, provider_id, getKeyIds());
        }
        return kek;
    }
}

#! File-backed static key provider
/** Reads raw 32-byte AES-256 KEKs from files at construction time.
*/
public class FileKeyProvider inherits StaticKeyProvider {
    private {
        #! key file paths by key id
        hash<string, string> paths;
    }

    #! Creates a single-generation provider
    /** @param path path to a file containing exactly 32 bytes of key material
        @param provider_id provider identifier; defaults to "file"
        @param key_id key identifier; defaults to "current"
    */
    constructor(string path, string provider_id = "file", string key_id = "current")
            : StaticKeyProvider(File::readBinaryFile(path), provider_id, key_id) {
        self.paths = {key_id: path};
    }

    #! Creates a multi-generation provider from a map of key files
    /** @param key_files key file paths keyed by key id; each file must contain
        exactly 32 bytes of key material
        @param current_key_id the generation used to wrap new DEKs
        @param provider_id provider identifier; defaults to "file"
    */
    constructor(hash<string, string> key_files, string current_key_id, string provider_id = "file")
            : StaticKeyProvider(FileKeyProvider::readKeyFiles(key_files), current_key_id, provider_id) {
        self.paths = key_files;
    }

    #! Returns the key file path for the current key generation
    string getPath() {
        *string rv = paths{getCurrentKeyId()};
        if (!exists rv) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("no key file is recorded for current key id %y", getCurrentKeyId());
        }
        return rv;
    }

    #! Returns all key file paths by key id
    hash<string, string> getPaths() {
        return paths;
    }

    #! Reads key material for every key file in the map
    private static hash<string, data> readKeyFiles(hash<string, string> key_files) {
        if (!key_files) {
            throw "QORE-CRYPTO-KEY-ERROR", "key_files must contain at least one entry";
        }
        hash<string, data> rv;
        foreach hash<auto> i in (key_files.pairIterator()) {
            rv{i.key} = File::readBinaryFile(i.value);
        }
        return rv;
    }
}

#! Envelope encryption helper
/** @par Thread safety
    Safe to share between threads as long as the key provider is; the cipher
    itself is immutable after construction and holds no per-call state.

    @par Key material lifetime
    A plaintext DEK exists only for the duration of a single encrypt, decrypt,
    or rewrap call.  It is never logged, never written into the envelope, and
    never cached; see @ref qorecrypto_zeroization for what this module does and
    does not guarantee.
*/
public class EnvelopeCipher {
    private {
        AbstractKeyProvider provider;
    }

    #! Creates the cipher
    constructor(AbstractKeyProvider provider) {
        self.provider = provider;
    }

    #! Returns the key provider backing this cipher
    AbstractKeyProvider getProvider() {
        return provider;
    }

    #! Encrypts data with a per-record DEK
    /** @param plaintext data to encrypt
        @param aad mandatory AAD; may be a string or a structured Qore value
        @param options optional encryption options
        @return ciphertext envelope

        @throw QORE-CRYPTO-AAD-ERROR missing or unsupported AAD
        @throw QORE-CRYPTO-ENCRYPT-ERROR content encryption failed
        @throw QORE-CRYPTO-KEY-ERROR key wrapping failed
    */
    hash<CiphertextEnvelope> encrypt(data plaintext, auto aad,
            *hash<EnvelopeEncryptOptions> options) {
        string canonical_aad = canonicalizeAad(aad);
        string key_id = options ? (options.key_id ?? provider.getCurrentKeyId()) : provider.getCurrentKeyId();
        string wrap_aad = makeWrapAad(provider.getProviderId(), key_id, canonical_aad);

        binary dek = get_random_bytes(32);
        hash<WrappedKeyInfo> wrapped = provider.wrapKey(dek, key_id, wrap_aad);

        binary iv = get_random_bytes(12);
        binary tag;
        binary ciphertext;
        try {
            ciphertext = Qore::encrypt(CRYPTO_ALG_AES_256, plaintext, dek, iv, \tag, 16, canonical_aad);
        } catch (hash<ExceptionInfo> ex) {
            throw "QORE-CRYPTO-ENCRYPT-ERROR",
                sprintf("could not encrypt envelope content: %s: %s", ex.err, ex.desc);
        }

        return <CiphertextEnvelope>{
            "version": ENVELOPE_VERSION,
            "algorithm": ENVELOPE_ALGORITHM,
            "key_provider": wrapped.key_provider,
            "key_id": wrapped.key_id,
            "key_wrap_algorithm": wrapped.algorithm,
            "wrapped_dek": wrapped.wrapped_key,
            "wrap_iv": wrapped.iv,
            "wrap_tag": wrapped.tag,
            "iv": make_base64_string(iv),
            "tag": make_base64_string(tag),
            "ciphertext": make_base64_string(ciphertext),
            "aad_digest": digest("sha256", canonical_aad).toHex(),
            "aad_descriptor": options ? options.aad_descriptor : NOTHING,
            "metadata": options ? options.metadata : NOTHING,
        };
    }

    #! Decrypts an envelope and returns binary plaintext
    /** @param envelope ciphertext envelope
        @param aad AAD that must canonicalize to the same value used for encryption
        @return plaintext bytes

        @throw QORE-CRYPTO-AAD-ERROR missing or mismatched AAD
        @throw QORE-CRYPTO-DECRYPT-ERROR content decryption failed
        @throw QORE-CRYPTO-KEY-ERROR key unwrap failed
    */
    binary decrypt(hash<CiphertextEnvelope> envelope, auto aad) {
        validateEnvelope(envelope);
        string canonical_aad = canonicalizeAad(aad);
        string aad_digest = digest("sha256", canonical_aad).toHex();
        if (aad_digest != envelope.aad_digest) {
            throw "QORE-CRYPTO-AAD-ERROR", "AAD does not match ciphertext envelope";
        }

        string wrap_aad = makeWrapAad(envelope.key_provider, envelope.key_id, canonical_aad);
        binary dek = provider.unwrapKey(<WrappedKeyInfo>{
            "key_provider": envelope.key_provider,
            "key_id": envelope.key_id,
            "algorithm": envelope.key_wrap_algorithm,
            "wrapped_key": envelope.wrapped_dek,
            "iv": envelope.wrap_iv,
            "tag": envelope.wrap_tag,
        }, wrap_aad);

        try {
            return decrypt_to_binary(CRYPTO_ALG_AES_256, parse_base64_string(envelope.ciphertext), dek,
                parse_base64_string(envelope.iv), parse_base64_string(envelope.tag), canonical_aad);
        } catch (hash<ExceptionInfo> ex) {
            throw "QORE-CRYPTO-DECRYPT-ERROR",
                sprintf("could not decrypt envelope content: %s: %s", ex.err, ex.desc);
        }
    }

    #! Decrypts an envelope and returns string plaintext
    /** @param envelope ciphertext envelope
        @param aad AAD that must canonicalize to the same value used for encryption
        @param encoding optional plaintext encoding
        @return plaintext string
    */
    string decryptToString(hash<CiphertextEnvelope> envelope, auto aad, *string encoding) {
        return decrypt(envelope, aad).toString(encoding);
    }

    #! Returns @ref True if the envelope is not wrapped by the given key generation
    /** @param envelope ciphertext envelope
        @param target_key_id the target key generation; defaults to the
        provider's current key

        @return @ref True if @ref rewrap() would change the envelope

        @note this compares key metadata only; it never unwraps a DEK and never
        touches ciphertext, so it is safe to call over a whole table

        @see @ref qorecrypto_rotation
    */
    bool needsRewrap(hash<CiphertextEnvelope> envelope, *string target_key_id) {
        return envelope.key_provider != provider.getProviderId()
            || envelope.key_id != (target_key_id ?? provider.getCurrentKeyId());
    }

    #! Rewraps an envelope's DEK under a different key generation
    /** This is the cheap rotation path: the DEK is unwrapped and rewrapped, and
        the content ciphertext, IV, tag, and AAD digest are carried over
        unchanged.  The record plaintext is never decrypted, and the caller does
        not need the record AAD, because the wrapping AAD is reconstructed from
        the AAD digest already stored in the envelope.

        Rewrapping is only valid when the record AAD is unchanged.  If any AAD
        field changes, the record must go through @ref reencrypt() instead;
        rewrapping across an AAD change produces an envelope whose stored AAD
        digest no longer matches the record, and it will never decrypt again.

        @param envelope ciphertext envelope to rewrap
        @param target_key_id target key generation; defaults to the provider's
        current key

        @return a new envelope wrapped under \a target_key_id

        @throw QORE-CRYPTO-ENVELOPE-ERROR unsupported envelope version or algorithm
        @throw QORE-CRYPTO-KEY-ERROR the source or target key generation is unavailable

        @see @ref qorecrypto_rotation
    */
    hash<CiphertextEnvelope> rewrap(hash<CiphertextEnvelope> envelope, *string target_key_id) {
        return EnvelopeCipher::rewrapAcross(provider, provider, envelope, target_key_id);
    }

    #! Rewraps an envelope's DEK from one key provider to another
    /** Used to migrate stored ciphertext to a different key provider without
        decrypting record content.  As with @ref rewrap(), this is only valid
        when the record AAD is unchanged.

        @param from provider that can unwrap the envelope's current DEK
        @param to provider that will wrap the DEK
        @param envelope ciphertext envelope to rewrap
        @param target_key_id target key generation; defaults to \a to's current key

        @return a new envelope wrapped by \a to

        @throw QORE-CRYPTO-ENVELOPE-ERROR unsupported envelope version or algorithm
        @throw QORE-CRYPTO-KEY-ERROR the source or target key generation is unavailable

        @see @ref qorecrypto_rotation
    */
    static hash<CiphertextEnvelope> rewrapAcross(AbstractKeyProvider from, AbstractKeyProvider to,
            hash<CiphertextEnvelope> envelope, *string target_key_id) {
        EnvelopeCipher::validateEnvelope(envelope);
        string new_key_id = target_key_id ?? to.getCurrentKeyId();

        string unwrap_aad = EnvelopeCipher::makeWrapAadFromDigest(envelope.key_provider,
            envelope.key_id, envelope.aad_digest);
        binary dek = from.unwrapKey(<WrappedKeyInfo>{
            "key_provider": envelope.key_provider,
            "key_id": envelope.key_id,
            "algorithm": envelope.key_wrap_algorithm,
            "wrapped_key": envelope.wrapped_dek,
            "iv": envelope.wrap_iv,
            "tag": envelope.wrap_tag,
        }, unwrap_aad);

        string wrap_aad = EnvelopeCipher::makeWrapAadFromDigest(to.getProviderId(), new_key_id,
            envelope.aad_digest);
        hash<WrappedKeyInfo> wrapped = to.wrapKey(dek, new_key_id, wrap_aad);

        return <CiphertextEnvelope>{
            "version": envelope.version,
            "algorithm": envelope.algorithm,
            "key_provider": wrapped.key_provider,
            "key_id": wrapped.key_id,
            "key_wrap_algorithm": wrapped.algorithm,
            "wrapped_dek": wrapped.wrapped_key,
            "wrap_iv": wrapped.iv,
            "wrap_tag": wrapped.tag,
            "iv": envelope.iv,
            "tag": envelope.tag,
            "ciphertext": envelope.ciphertext,
            "aad_digest": envelope.aad_digest,
            "aad_descriptor": envelope.aad_descriptor,
            "metadata": envelope.metadata,
        };
    }

    #! Decrypts and re-encrypts an envelope, optionally under new AAD
    /** This is the expensive rotation path: the record is decrypted and
        encrypted again under a fresh DEK.  It is required whenever any AAD
        field changes, and it is the only path that can change the AAD digest
        stored in the envelope.

        The AAD descriptor and metadata of the source envelope are carried over
        unless \a options supplies replacements.

        @param envelope ciphertext envelope to re-encrypt
        @param old_aad the AAD the envelope was encrypted under
        @param new_aad the AAD to encrypt under; pass the same value as
        \a old_aad to re-encrypt without an AAD change
        @param options optional encryption options for the new envelope

        @return a new envelope

        @throw QORE-CRYPTO-AAD-ERROR \a old_aad does not match the envelope
        @throw QORE-CRYPTO-DECRYPT-ERROR content decryption failed
        @throw QORE-CRYPTO-KEY-ERROR key unwrap or wrap failed

        @see @ref qorecrypto_rotation
    */
    hash<CiphertextEnvelope> reencrypt(hash<CiphertextEnvelope> envelope, auto old_aad, auto new_aad,
            *hash<EnvelopeEncryptOptions> options) {
        binary plaintext = decrypt(envelope, old_aad);
        hash<EnvelopeEncryptOptions> opts = options ?? <EnvelopeEncryptOptions>{};
        if (!exists opts.aad_descriptor) {
            opts.aad_descriptor = envelope.aad_descriptor;
        }
        if (!exists opts.metadata) {
            opts.metadata = envelope.metadata;
        }
        return encrypt(plaintext, new_aad, opts);
    }

    #! Canonicalizes structured AAD into a deterministic string
    /** Hash keys are sorted recursively.  Supported values are strings,
        binary values, booleans, integers, floats, numbers, dates, null,
        nothing, lists, and hashes.
    */
    public static string canonicalizeAad(auto aad) {
        if (!exists aad) {
            throw "QORE-CRYPTO-AAD-ERROR", "AAD is required";
        }
        return canonicalValue(aad);
    }

    private static string makeWrapAad(string provider_id, string key_id, string canonical_aad) {
        return EnvelopeCipher::makeWrapAadFromDigest(provider_id, key_id,
            digest("sha256", canonical_aad).toHex());
    }

    #! Builds the wrapping AAD from an AAD digest
    /** Rotation works from the digest already stored in the envelope, so
        rewrapping never needs the record AAD in plaintext form.
    */
    private static string makeWrapAadFromDigest(string provider_id, string key_id, string aad_digest) {
        return sprintf("QoreCrypto:key-wrap:v1:%s:%s:%s", provider_id, key_id, aad_digest);
    }

    private static validateEnvelope(hash<CiphertextEnvelope> envelope) {
        if (envelope.version != ENVELOPE_VERSION) {
            throw "QORE-CRYPTO-ENVELOPE-ERROR",
                sprintf("unsupported envelope version %d", envelope.version);
        }
        if (envelope.algorithm != ENVELOPE_ALGORITHM) {
            throw "QORE-CRYPTO-ENVELOPE-ERROR",
                sprintf("unsupported envelope algorithm %y", envelope.algorithm);
        }
        if (envelope.key_wrap_algorithm != KEY_WRAP_ALGORITHM) {
            throw "QORE-CRYPTO-ENVELOPE-ERROR",
                sprintf("unsupported key wrap algorithm %y", envelope.key_wrap_algorithm);
        }
    }

    private static string canonicalValue(auto val) {
        switch (val.typeCode()) {
            case NT_STRING:
                return sprintf("s:%d:%s", val.size(), val);
            case NT_BINARY:
                return sprintf("b:%s", make_base64_string(val));
            case NT_BOOLEAN:
                return val ? "t" : "f";
            case NT_INT:
                return sprintf("i:%d", val);
            case NT_FLOAT:
                return sprintf("f:%y", val);
            case NT_NUMBER:
                return sprintf("n:%y", val);
            case NT_DATE:
                return sprintf("d:%s", val.format("%Y-%m-%dT%H:%M:%S.%u%z"));
            case NT_NULL:
                return "null";
            case NT_NOTHING:
                return "nothing";
            case NT_LIST: {
                list<string> parts = ();
                foreach auto item in (val) {
                    string encoded = canonicalValue(item);
                    push parts, sprintf("%d:%s", encoded.size(), encoded);
                }
                return sprintf("l:%d:%s", parts.size(), parts.join(""));
            }
            case NT_HASH: {
                list<string> key_list = sort(val.keys());
                list<string> parts = ();
                foreach string key in (key_list) {
                    string encoded_key = canonicalValue(key);
                    string encoded_val = canonicalValue(val{key});
                    push parts, sprintf("%d:%s%d:%s", encoded_key.size(), encoded_key,
                        encoded_val.size(), encoded_val);
                }
                return sprintf("h:%d:%s", parts.size(), parts.join(""));
            }
        }
        throw "QORE-CRYPTO-AAD-ERROR",
            sprintf("unsupported AAD value type %y", val.typeName());
    }

    public static binary normalizeAes256Key(data key, string arg_name) {
        binary b;
        if (key.typeCode() == NT_BINARY) {
            b = key;
        } else if (key.typeCode() == NT_STRING) {
            b = key.toBinary();
        } else {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("%s must be a string or binary value", arg_name);
        }
        if (b.size() != 32) {
            throw "QORE-CRYPTO-KEY-ERROR",
                sprintf("%s must contain exactly 32 bytes for AES-256, got %d", arg_name, b.size());
        }
        return b;
    }

    public static validateIdentifier(string id, string arg_name) {
        if (!id.val()) {
            throw "QORE-CRYPTO-KEY-ERROR", sprintf("%s must not be empty", arg_name);
        }
        if (id =~ /[\r\n]/) {
            throw "QORE-CRYPTO-KEY-ERROR", sprintf("%s must not contain newline characters", arg_name);
        }
    }
}

}
