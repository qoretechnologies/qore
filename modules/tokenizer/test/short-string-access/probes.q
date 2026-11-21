# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The probes of short-string-access.qtest: each passes strings of 6 bytes or fewer, which a value can hold inline
# (with no QoreStringNode), to the tokenizer module in the values of its configuration, options and token lists, and
# prints what the module made of them.  A qcc-compiled executable holds the strings of a constant hash inline; in a
# debug build ss() makes them inline in every execution mode (dbg_make_short_string()).

%modern
%requires tokenizer

#! the strings of the probes: a qcc-compiled executable holds the strings of a constant hash inline
const Short = {
    "[PAD]": "[PAD]",
    "[UNK]": "[UNK]",
    "[CLS]": "[CLS]",
    "[SEP]": "[SEP]",
    "##": "##",
    "hello": "hello",
    "a": "a",
    "xyz": "xyz",
};

#! returns the string: inline in a debug build, and from the constant hash otherwise
string sub ss(string s) {
%ifdef QoreDebug
    return dbg_make_short_string(s);
%else
    return Short{s};
%endif
}

#! returns the string, inline if so requested
string sub cs(string s, bool short) {
    return short ? ss(s) : s;
}

#! returns a WordPiece tokenizer, with inline strings in its configuration if so requested
Tokenizer::HFTokenizer sub make_tokenizer(bool short = False) {
    hash<auto> config = {
        "model": {
            "type": "WordPiece",
            "vocab": {
                "[PAD]": 0, "[UNK]": 1, "[CLS]": 2, "[SEP]": 3,
                "hello": 4, "world": 5, "a": 6,
            },
            "unk_token": cs("[UNK]", short),
            "continuing_subword_prefix": cs("##", short),
        },
        "added_tokens": (
            {"id": 0, "content": cs("[PAD]", short), "special": True},
            {"id": 1, "content": cs("[UNK]", short), "special": True},
            {"id": 2, "content": cs("[CLS]", short), "special": True},
            {"id": 3, "content": cs("[SEP]", short), "special": True},
        ),
        "post_processor": {
            "type": "BertProcessing",
            "sep": (cs("[SEP]", short), 3),
            "cls": (cs("[CLS]", short), 2),
        },
    };
    return new Tokenizer::HFTokenizer(config);
}

#! the probes in the order that they run
const Probes = (
    # the strings of the configuration
    "constructor",
    # a string option
    "encodeAdvanced text_pair",
    "encodeForModel text_pair",
    # the strings of a list option
    "encodeAdvanced words",
    # the strings of a token list
    "addTokens",
);

#! runs a probe and returns its result
string sub run_probe(string name) {
    switch (name) {
        case "constructor": {
            Tokenizer::HFTokenizer tok = make_tokenizer(True);
            return sprintf("%y", tok.encode("hello zzz").input_ids);
        }
        case "encodeAdvanced text_pair":
            return sprintf("%y", make_tokenizer().encodeAdvanced("hello", {"text_pair": ss("a")}).input_ids);
        case "encodeForModel text_pair":
            return sprintf("%y", make_tokenizer().encodeForModel("hello", {"text_pair": ss("a")}).input_ids.toList());
        case "encodeAdvanced words":
            return sprintf("%y", make_tokenizer().encodeAdvanced("", {"is_pretokenized": True,
                "words": (ss("hello"), ss("a"))}).input_ids);
        case "addTokens": {
            Tokenizer::HFTokenizer tok = make_tokenizer();
            int n = tok.addTokens((ss("xyz"),));
            return sprintf("%d %d", n, tok.tokenToId("xyz"));
        }
    }
    throw "PROBE-ERROR", sprintf("unknown probe %y", name);
}

foreach string name in (ARGV ?: Probes) {
    try {
        printf("%s: %s\n", name, run_probe(name));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}
