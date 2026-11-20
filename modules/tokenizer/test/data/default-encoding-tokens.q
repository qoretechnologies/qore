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
%requires json

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

# vocabulary keys in the default encoding match UTF-8 text
hash<auto> vocab_keys = {"[UNK]": 0, "x": 1};
vocab_keys{e_acute} = 4;
Tokenizer::HFTokenizer wp({
    "model": {
        "type": "WordPiece",
        "vocab": vocab_keys,
        "unk_token": "[UNK]",
    },
});
out("wordpiece-vocab-id", join(",", wp.encode(e_acute).input_ids));

# a configuration parsed from JSON text has its keys in the default encoding too
string json = "{\"model\": {\"type\": \"WordPiece\", \"unk_token\": \"[UNK]\", \"vocab\": {\"[UNK]\": 0, \""
    + e_acute + "\": 4}}}";
Tokenizer::HFTokenizer jt(parse_json(json));
out("json-vocab-id", join(",", jt.encode(e_acute).input_ids));

hash<auto> bpe_vocab = {"[UNK]": 0, "c": 1};
bpe_vocab{"c" + e_acute} = 2;
bpe_vocab{e_acute} = 3;
Tokenizer::HFTokenizer bpe({
    "model": {
        "type": "BPE",
        "vocab": bpe_vocab,
        "merges": ("c " + e_acute,),
        "unk_token": "[UNK]",
    },
});
out("bpe-vocab-id", join(",", bpe.encode("c" + e_acute).input_ids));

# a special token key of a template
string st = "<" + e_acute + ">";
hash<auto> special = {};
special{st} = {"id": st, "ids": (5,), "tokens": (st,)};
Tokenizer::HFTokenizer tt({
    "model": {
        "type": "WordPiece",
        "vocab": vocab_keys,
        "unk_token": "[UNK]",
    },
    "post_processor": {
        "type": "TemplateProcessing",
        "single": ({"SpecialToken": {"id": st, "type_id": 0}}, {"Sequence": {"id": "A", "type_id": 0}}),
        "special_tokens": special,
    },
});
out("special-token-key-ids", join(",", tt.encode(e_acute).input_ids));
