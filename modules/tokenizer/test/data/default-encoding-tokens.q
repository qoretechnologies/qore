#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*  default-encoding-tokens.q Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License

    The child script of default-encoding-tokens.qtest, run with a default character encoding given by the test; it
    has only ASCII characters, so that it means the same in any default encoding, and prints each result as
    "<name>=<hex bytes>".

    usage: default-encoding-tokens.q
*/

%modern
%requires tokenizer

# an e acute and a euro sign in UTF-8
string e_acute = binary_to_string(<c3a9>, "UTF-8");
string euro = binary_to_string(<e282ac>, "UTF-8");

sub out(string name, data value) {
    printf("%s=%s\n", name, make_hex_string(binary(value)));
}

# prints the error code of the call as a hex string, or "none"
sub err(string name, code c) {
    try {
        c();
        out(name, "none");
    } catch (hash<ExceptionInfo> ex) {
        out(name, ex.err);
    }
}

out("default", get_default_encoding());

Tokenizer::HFTokenizer tok({
    "model": {
        "type": "WordPiece",
        "vocab": {"[UNK]": 0, "x": 1},
        "unk_token": "[UNK]",
        "continuing_subword_prefix": "##",
    },
    "added_tokens": (
        {"id": 2, "content": e_acute, "special": False},
    ),
});

string token = tok.idToToken(2);
out("id-to-token", token);
out("id-to-token-encoding", token.encoding());

# "x" is in the default encoding, and the text is converted to UTF-8 to be tokenized
hash<auto> enc = tok.encode("x" + e_acute);
out("tokens", enc.tokens.last());
out("tokens-encoding", enc.tokens.last().encoding());

string decoded = tok.decode(enc.input_ids);
out("decoded", decoded);
out("decoded-encoding", decoded.encoding());

hash<string, int> vocab = tok.getVocab();
foreach hash<auto> i in (vocab.pairIterator()) {
    if (i.value == 2) {
        out("vocab-key", i.key);
        out("vocab-key-encoding", i.key.encoding());
    }
}

# a token that the default encoding may not represent
tok.addTokens((euro,));
err("vocab-euro", sub () { tok.getVocab(); });
