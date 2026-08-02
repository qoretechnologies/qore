# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aCardSignature.qc @brief A2A signed agent card support

/*  A2aCardSignature.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! The agent card member holding the JWS signatures
/** @since A2aClient 1.0
*/
public const A2aCardSignaturesKey = "signatures";

#! The JWS \c typ header value the A2A specification recommends for agent card signatures
/** @since A2aClient 1.0
*/
public const A2aCardSignatureType = "JOSE";

#! The JWS algorithms supported for agent card signatures
/** \c ES* algorithms are not included: JWS requires the raw <tt>R||S</tt> signature encoding for those, which
    differs from the DER encoding available here.

    @since A2aClient 1.0
*/
public const A2aCardSignatureAlgorithms = {
    "HS256": True,
    "HS384": True,
    "HS512": True,
    "RS256": True,
    "RS384": True,
    "RS512": True,
};

#! The JWS algorithm used for agent card signatures when none is given
/** @since A2aClient 1.0
*/
public const A2aCardDefaultAlgorithm = "RS256";

#! A JWS signature over an agent card
/** @since A2aClient 1.0
*/
public hashdecl A2aCardSignature {
    #! The base64url-encoded JWS Protected Header
    string protected;

    #! The base64url-encoded signature value
    string signature;

    #! The JWS Unprotected Header, as a JSON object rather than base64url-encoded
    *hash<auto> header;
}

#! The result of verifying an agent card signature
/** @since A2aClient 1.0
*/
public hashdecl A2aCardVerification {
    #! Whether at least one signature was verified successfully
    bool verified = False;

    #! The number of signatures the card carries
    int signature_count = 0;

    #! The key IDs of the signatures that verified
    list<string> verified_key_ids = ();

    #! Why verification failed, if it did
    *string error;
}

#! Returns the canonical payload that an agent card signature covers
/** The payload is the agent card with its \c signatures member removed, canonicalized with
    @ref a2aCanonicalizeJson() "JCS".  Excluding \c signatures is what makes multiple independent signatures
    over one card possible.

    @param card the agent card

    @return the canonical JSON text that is signed

    @note The A2A specification also requires protocol-buffer field-presence semantics: an optional field that
    was never set must be absent from the canonical form.  A Qore hash has no separate notion of "present but
    unset", so a key that should be omitted must simply not be in the hash.

    @since A2aClient 1.0
*/
public string sub a2aGetCardSigningPayload(hash<auto> card) {
    return a2aCanonicalizeJson(card - A2aCardSignaturesKey);
}

#! Signs an agent card and returns it with the signature appended
/** @param card the agent card to sign; any existing signatures are preserved and the new one is appended
    @param signing_key the signing key: the shared secret for \c HS* algorithms, or an RSA private key in PEM format
    for \c RS* algorithms
    @param kid the key ID identifying the signing key to verifiers
    @param opts optional signing options:
    - \c algorithm: the JWS algorithm; see @ref A2aCardSignatureAlgorithms; defaults to
      @ref A2aCardDefaultAlgorithm
    - \c jku: a URL for the JSON Web Key Set holding the public key, added to the protected header
    - \c header: a hash of unprotected header parameters, stored alongside the signature
    - \c protected: extra parameters to merge into the protected header

    @return the agent card with the new signature appended to its \c signatures member

    @throw A2A-SIGNATURE-ERROR if the algorithm is not supported
    @throw JWT-ERROR if the key cannot be read or signing fails

    @par Example:
    @code{.py}
hash<auto> signed = a2aSignAgentCard(card, private_key_pem, "key-1", {
    "jku": "https://agent.example.com/.well-known/jwks.json",
});
    @endcode

    @since A2aClient 1.0
*/
public hash<auto> sub a2aSignAgentCard(hash<auto> card, string signing_key, string kid, *hash<auto> opts) {
    string algorithm = opts.algorithm ?? A2aCardDefaultAlgorithm;
    if (!A2aCardSignatureAlgorithms{algorithm}) {
        throw "A2A-SIGNATURE-ERROR", sprintf("algorithm %y is not supported for agent card signatures; "
            "supported algorithms: %y", algorithm, keys A2aCardSignatureAlgorithms);
    }

    hash<auto> protected_header = {
        "alg": algorithm,
        "typ": A2aCardSignatureType,
        "kid": kid,
    };
    if (opts.jku.val()) {
        protected_header.jku = opts.jku;
    }
    if (opts.protected) {
        protected_header += opts.protected;
    }

    string protected_b64 = make_base64_url_string(a2aCanonicalizeJson(protected_header));
    string payload_b64 = make_base64_url_string(a2aGetCardSigningPayload(card));
    binary sig = jws_sign(protected_b64 + "." + payload_b64, signing_key, algorithm);

    hash<A2aCardSignature> entry = <A2aCardSignature>{
        "protected": protected_b64,
        "signature": make_base64_url_string(sig),
    };
    if (opts.header) {
        entry.header = opts.header;
    }

    softlist<auto> sigs = card{A2aCardSignaturesKey} ?? ();
    return card + {A2aCardSignaturesKey: sigs + entry};
}

#! Verifies the signatures on an agent card
/** A card verifies when at least one of its signatures verifies against a key the caller supplied.  A card
    with no signatures never verifies.

    @param card the agent card to verify
    @param key_info the verification keys: either a single key as a string, applied to every signature, or a hash
    mapping key ID to key.  For \c RS* algorithms these are RSA public keys in PEM format; a private key is
    also accepted, since the public key can be derived from it.
    @param opts optional verification options:
    - \c require_kid: if @ref True, a signature whose protected header has no \c kid is rejected rather than
      tried against every supplied key

    @return the verification result

    @note This function does not fetch keys over the network.  A card whose protected header carries a \c jku
    must have that key set retrieved and supplied by the caller, so that the network access, its sandboxing,
    and its trust decisions stay under the caller's control.

    @par Example:
    @code{.py}
hash<A2aCardVerification> rv = a2aVerifyAgentCard(card, {"key-1": public_key_pem});
if (!rv.verified) {
    throw "UNTRUSTED-AGENT", rv.error;
}
    @endcode

    @since A2aClient 1.0
*/
public hash<A2aCardVerification> sub a2aVerifyAgentCard(hash<auto> card, auto key_info, *hash<auto> opts) {
    softlist<auto> sigs = card{A2aCardSignaturesKey} ?? ();
    hash<A2aCardVerification> rv = <A2aCardVerification>{
        "signature_count": sigs.size(),
    };
    if (!sigs) {
        rv.error = "the agent card carries no signatures";
        return rv;
    }

    # normalize the key argument to a kid -> key hash; a bare string applies to every signature
    *hash<auto> key_map;
    *string single_key;
    if (key_info.typeCode() == NT_STRING) {
        single_key = key_info;
    } else if (key_info.typeCode() == NT_HASH) {
        key_map = key_info;
    } else {
        rv.error = sprintf("no verification key supplied (got type %y)", key_info.type());
        return rv;
    }

    string payload_b64 = make_base64_url_string(a2aGetCardSigningPayload(card));
    list<string> errors();

    foreach auto sig in (sigs) {
        if (sig.typeCode() != NT_HASH || !sig.protected.val() || !sig.signature.val()) {
            errors += "a signature entry is missing 'protected' or 'signature'";
            continue;
        }
        hash<auto> header;
        try {
            header = parse_json(parse_base64_url_string_to_string(sig.protected));
        } catch (hash<ExceptionInfo> ex) {
            errors += sprintf("the protected header could not be decoded: %s", ex.desc);
            continue;
        }
        *string alg = header.alg;
        if (!alg.val() || !A2aCardSignatureAlgorithms{alg}) {
            errors += sprintf("unsupported or missing signature algorithm %y", alg);
            continue;
        }
        *string kid = header.kid;
        if (opts.require_kid && !kid.val()) {
            errors += "the protected header has no 'kid' and require_kid is set";
            continue;
        }

        # the keys to try: the single key, the key for this kid, or every supplied key when there is no kid
        list<auto> candidates;
        if (exists single_key) {
            candidates = (single_key,);
        } else if (kid.val() && exists key_map{kid}) {
            candidates = (key_map{kid},);
        } else if (kid.val()) {
            errors += sprintf("no verification key was supplied for key ID %y", kid);
            continue;
        } else {
            candidates = (map key_map{$1}, keys key_map) ?? ();
        }

        string signing_input = sig.protected + "." + payload_b64;
        binary raw;
        try {
            raw = parse_base64_url_string(sig.signature);
        } catch (hash<ExceptionInfo> ex) {
            errors += sprintf("the signature could not be decoded: %s", ex.desc);
            continue;
        }
        bool ok = False;
        foreach auto candidate in (candidates) {
            try {
                if (jws_verify(signing_input, raw, candidate, alg)) {
                    ok = True;
                    break;
                }
            } catch (hash<ExceptionInfo> ex) {
                errors += sprintf("verification with key ID %y failed: %s: %s", kid ?? "(none)", ex.err,
                    ex.desc);
            }
        }
        if (ok) {
            rv.verified = True;
            rv.verified_key_ids += kid ?? "";
        } else {
            errors += sprintf("the signature with key ID %y did not verify", kid ?? "(none)");
        }
    }

    if (!rv.verified) {
        rv.error = errors ? errors.join("; ") : "no signature verified";
    }
    return rv;
}
}
