# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ActionIntentClassifier.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ActionIntentClassifier
public namespace QoreLlmUtils {

#! LLM-driven classifier for whether a user message requests a state-changing action
/** Any LLM-backed agent that dispatches destructive tools (create / update /
    delete / etc.) needs to decide whether the current user turn actually asked
    for such an action.  A word-list / regex approach is fragile:

    - English-only, biased toward verb vocabulary the author remembers
    - Semantics-blind: "how do I create an SFTP connection?" contains
      "create" mid-sentence but is unambiguously a how-to question
    - Misses idiomatic constructions the regex can't anticipate
      ("I wonder if we could go ahead and delete the broken one")

    This class delegates the decision to an LLM and exposes a single
    canonical taxonomy (@ref IntentReadOnly, @ref IntentActionRequested,
    @ref IntentConfirmationRequired) plus the prompt / parser pieces callers
    need to integrate it:

    - @ref getPromptFragment() returns the instruction block a caller can
      embed inside their own classification prompt (e.g., alongside a
      complexity / plan classification) so the classifier shares the LLM
      round-trip rather than paying for a second one.
    - @ref parseIntentFromResponse() extracts the \c "INTENT: …" line out
      of an arbitrary response body.
    - The instance @ref classify() method runs a standalone dedicated
      classifier call against an @ref LlmBackend for callers that don't
      have an existing analysis prompt to piggyback on.

    The labels are stable strings (not integers) so they can round-trip
    through REST bodies, guardrail metadata, and conversation logs
    unchanged.

    @par Example: inline in a caller's combined prompt (zero extra LLM calls)
    @code{.py}
%modern
%requires QoreLlmUtils

string prompt = "Analyze this query and output:\n"
    + ActionIntentClassifier::getPromptFragment()
    + "\nCOMPLEXITY: simple|moderate|complex\nPLAN: ...\n";
# ... send prompt + user message to your LLM, get response ...
*string intent = ActionIntentClassifier::parseIntentFromResponse(response);
if (intent == ActionIntentClassifier::IntentActionRequested) {
    # user wants a state change
}
    @endcode

    @par Example: standalone classification call
    @code{.py}
%modern
%requires QoreLlmUtils

LlmBackend llm(<LlmBackendConfig>{"mode": "http", "url": "http://localhost:8080",
    "model": "gpt-4o-mini"});
ActionIntentClassifier classifier(llm);
*string intent = classifier.classify("please delete the broken workflow");
# intent == "action-requested"
    @endcode
*/
public class ActionIntentClassifier {
    public {
        #! The user's turn is asking for information / guidance / status
        /** How-to, conceptual, data-lookup, and status-check phrasings all
            map to this label regardless of language.  Any LLM-backed
            safety gate should REFUSE destructive tool dispatches when the
            current turn classifies as read-only.
        */
        const IntentReadOnly = "read-only";

        #! The user's turn unambiguously requests a state change
        /** Imperative mood in the user's native language, confirmed
            continuations ("yes, go ahead"), and one-shot execution
            requests all map here.  A destructive tool dispatch is
            permitted when the current turn classifies as
            action-requested.
        */
        const IntentActionRequested = "action-requested";

        #! The user's turn is ambiguously phrased between informational and action
        /** The model cannot tell from the text alone whether the user
            wants the system to take an action or is merely exploring
            the option.  Typical triggers: meta-questions about whether
            to act (\c "should I delete X?"), exploratory phrasings
            (\c "would it be a good idea to create a workflow?"),
            hypothetical framings (\c "if I were to update the job,
            what would happen?").

            Callers must NOT dispatch destructive tools when the current
            turn classifies here; the canonical handling is to surface
            the ambiguity to the user and request an explicit
            imperative before acting.  Treating this label as "refuse
            destructive dispatch" is the safe baseline; richer UIs can
            pause the pipeline and ask the user to confirm.
        */
        const IntentConfirmationRequired = "confirmation-required";

        #! Canonical output prefix — clients parse response text for this
        const IntentOutputPrefix = "INTENT:";
    }

    private {
        #! Backend used by @ref classify() for standalone round-trips
        LlmBackend backend;

        #! System prompt prepended to the single-turn classification call
        const StandaloneSystemPrompt = "You are an intent classifier for an "
            "LLM-driven agent.  Output ONLY a single line starting with "
            "`INTENT: ` followed by one of the three labels defined below.  Do "
            "NOT include any other text, explanation, reasoning, or "
            "formatting.\n\n";
    }

    #! Creates a standalone classifier that uses the given backend for @ref classify()
    /** @param backend the @ref LlmBackend to call for classification
    */
    constructor(LlmBackend backend) {
        self.backend = backend;
    }

    #! Returns the prompt fragment callers embed into their own combined prompt
    /** Describes the taxonomy and output format to the LLM without
        assuming any surrounding prompt structure — safe to concatenate
        with other classification instructions (complexity, tool
        selection, plan generation) in the same response.

        The returned fragment tells the model:

        - Which three labels are valid (and that anything else is a bug)
        - What each label means with typical phrasings in both English
          and abstract language-agnostic terms
        - That how-to and status-check phrasings map to read-only even
          when they contain destructive verb words mid-sentence
        @return prompt fragment as a raw string (no leading/trailing
            newlines — caller controls whitespace)
    */
    public static string getPromptFragment() {
        return IntentOutputPrefix + " " + IntentReadOnly + "|"
            + IntentActionRequested + "|" + IntentConfirmationRequired
            + "\n\n"
            + "INTENT semantics:\n"
            + "- `" + IntentReadOnly + "` — the user is asking for "
            + "information, guidance, or current system state; no state "
            + "changes, even if tool calls are needed to gather data.  "
            + "Applies to every how-to / design / conceptual / "
            + "status-check / data-lookup phrasing in any language.  "
            + "Prefer this label when the user is clearly asking a "
            + "question rather than issuing a command.\n"
            + "- `" + IntentActionRequested + "` — the user "
            + "unambiguously wants the system to change state.  "
            + "Imperative mood in the user's own language (English: "
            + "'create X', 'please delete Y', 'set Z to 30s'; equivalent "
            + "constructions in other languages); requests for one-shot "
            + "execution; confirmed continuations from a prior action "
            + "('yes, go ahead and delete it').  The safety gate uses "
            + "this label as its primary signal for whether destructive "
            + "tools may dispatch, so an incorrect `"
            + IntentActionRequested + "` label on a how-to question is "
            + "a real bug — be conservative.\n"
            + "- `" + IntentConfirmationRequired + "` — the phrasing is "
            + "ambiguous between asking about acting and asking for the "
            + "action to be taken.  Typical triggers: meta-questions "
            + "about whether to act ('should I delete X?'), exploratory "
            + "phrasings ('would it be a good idea to create a "
            + "workflow?'), hypothetical framings ('if I update the "
            + "job, what happens?').  Use this ONLY when you cannot "
            + "confidently choose between the other two labels — an "
            + "over-use blocks legitimate commands and frustrates the "
            + "user.\n";
    }

    #! Extracts the intent label from an arbitrary LLM response body
    /** Scans the text for a line matching the canonical
        \c "INTENT: <label>" format and returns the exact matched label.
        The marker and label are case-sensitive closed protocol values;
        translated, case-altered, embedded, and otherwise malformed variants
        return NOTHING. Callers then apply their own fail-closed policy.

        @param response_text the LLM's full response body
        @return one of \c "read-only", \c "action-requested", or
            \c "confirmation-required" on a successful parse; NOTHING
            when the line is missing or the label is unrecognized
    */
    public static *string parseIntentFromResponse(string response_text) {
        *list<auto> match = (response_text =~
            x/^INTENT:[ \t]*(read-only|action-requested|confirmation-required)[ \t]*\r?$/m);
        if (!match[0].val()) {
            return;
        }
        return match[0];
    }

    #! Classifies a single user message via a dedicated LLM round-trip
    /** Sends the user message to @ref backend with the standalone
        classification prompt and parses the response via
        @ref parseIntentFromResponse().  Use this when you don't have an
        existing prompt to piggyback on.  Each call costs one LLM round
        trip — prefer the combined-prompt path via
        @ref getPromptFragment() + @ref parseIntentFromResponse() when a
        caller-driven classification prompt already exists.

        @param user_message the raw user turn to classify

        @return the parsed intent label (\c "read-only",
            \c "action-requested", or \c "confirmation-required") on
            success; NOTHING when the backend call fails, the response
            is empty, or the LLM emitted something the parser couldn't
            interpret.  Callers should treat NOTHING as "classification
            unavailable" and fall back to a safer default.
    */
    public *string classify(string user_message) {
        string user_prompt = StandaloneSystemPrompt
            + getPromptFragment()
            + "\nClassify this message:\n\n" + user_message;
        hash<LlmCompletionResult> result;
        try {
            # Intent classification is a compact label decision that needs no
            # extended hidden reasoning; disabling the reasoning budget keeps
            # the output budget available for the label so a "thinking" model
            # (e.g. Gemini 2.5) cannot drain it and return empty content.
            # Providers without a reasoning budget ignore the option.
            result = backend.complete(StandaloneSystemPrompt, user_prompt,
                NOTHING, <LlmRequestOptions>{"reasoning_token_budget": 0});
        } catch () {
            # Backend unavailable / malformed response — caller must decide
            # how to proceed without a classification
            return;
        }
        if (!result.content.val()) {
            return;
        }
        return parseIntentFromResponse(result.content);
    }
}

} # namespace QoreLlmUtils
