# -*- mode: qore; indent-tabs-mode: nil -*-

/*  TokenCounter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see TokenCounter
public namespace QoreTokenizerUtils {

#! Token counting and context window management for LLM applications
/** Counts tokens using a HuggingFace tokenizer and provides utilities for
    managing LLM context windows — checking whether text fits, and truncating
    text to a token limit while preserving word boundaries.

    Typical use cases:
    - Checking if a prompt fits in the model's context window before sending
    - Truncating long inputs to avoid API errors
    - Estimating costs based on token counts

    @par Example: Check context window fit
    @code{.py}
hash<auto> config = parse_json(ReadOnlyFile::readTextFile("tokenizer.json"));
TokenCounter tc(config);

string prompt = build_prompt(user_question, context_docs);
if (!tc.fitsInContext(prompt, 4096)) {
    prompt = tc.truncateToTokens(prompt, 3800);  # leave room for response
}
    @endcode

    @par Example: Count tokens for cost estimation
    @code{.py}
TokenCounter tc(config);
int tokens = tc.countTokens(document);
float estimated_cost = tokens * 0.00001;  # example per-token price
printf("Document: %d tokens, estimated cost: $%.4f\n", tokens, estimated_cost);
    @endcode
*/
public class TokenCounter {
    private {
        #! The underlying HuggingFace tokenizer
        Tokenizer::HFTokenizer tokenizer;
    }

    #! Creates a TokenCounter with the given tokenizer configuration
    /** @param tokenizer_config a parsed `tokenizer.json` hash (from `parse_json()`)
    */
    constructor(hash<auto> tokenizer_config) {
        tokenizer = new Tokenizer::HFTokenizer(tokenizer_config);
    }

    #! Counts the number of tokens in the given text
    /** @param text the text to tokenize and count
        @param add_special_tokens whether to include special tokens in the count
        (default: `False` — gives the raw content token count)
        @return the number of tokens

        @par Example:
        @code{.py}
int count = tc.countTokens("Hello, world!");
printf("Token count: %d\n", count);
        @endcode
    */
    int countTokens(string text, bool add_special_tokens = False) {
        if (!text.size()) {
            return 0;
        }
        hash<auto> enc = tokenizer.encodeAdvanced(text, {
            "add_special_tokens": add_special_tokens,
        });
        return enc.input_ids.size();
    }

    #! Checks whether the text fits within a token limit
    /** @param text the text to check
        @param max_tokens the maximum number of tokens allowed
        @param add_special_tokens whether to include special tokens in the count
        @return `True` if the text fits, `False` if it exceeds the limit

        @par Example:
        @code{.py}
if (tc.fitsInContext(prompt, 4096)) {
    # safe to send to the model
}
        @endcode
    */
    bool fitsInContext(string text, int max_tokens, bool add_special_tokens = False) {
        return countTokens(text, add_special_tokens) <= max_tokens;
    }

    #! Truncates text to fit within a token limit
    /** Uses the tokenizer's truncation support to cut text at a token boundary.
        The returned text is decoded from the truncated token IDs, so it will
        end at a clean token boundary (not in the middle of a word).

        @param text the text to truncate
        @param max_tokens the maximum number of tokens in the output
        @param add_special_tokens whether to include special tokens
        @return the truncated text, or the original text if it already fits

        @par Example:
        @code{.py}
string short_text = tc.truncateToTokens(long_document, 1024);
printf("Truncated from %d to %d tokens\n",
    tc.countTokens(long_document), tc.countTokens(short_text));
        @endcode
    */
    string truncateToTokens(string text, int max_tokens, bool add_special_tokens = False) {
        if (!text.size()) {
            return "";
        }

        # Encode with truncation
        hash<auto> enc = tokenizer.encodeAdvanced(text, {
            "max_length": max_tokens,
            "truncation": "longest_first",
            "add_special_tokens": add_special_tokens,
        });

        # Decode back to text (produces clean token-boundary truncation)
        return tokenizer.decode(enc.input_ids, True);
    }

    #! Returns the tokenizer's vocabulary size
    /** @return the number of tokens in the vocabulary
    */
    int getVocabSize() {
        return tokenizer.getVocabSize();
    }
}

} # namespace QoreTokenizerUtils
