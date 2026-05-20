# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractLlmProviderFactory.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreLlmUtils {

#! Static metadata describing a registered LLM provider
/** Returned by @ref AbstractLlmProviderFactory::getInfo() so tooling can
    discover provider capabilities without instantiating a backend.
*/
public hashdecl LlmProviderInfo {
    #! The mode string used to look up this provider (e.g. \c "gemini")
    string mode;
    #! Human-readable name for diagnostics and discovery UIs
    string display_name;
    #! Module that supplies this provider (e.g. \c "GeminiRestClient")
    *string module_name;
    #! True when the provider can stream chat completions
    bool supports_streaming = False;
    #! True when the provider can dispatch tool / function calls
    bool supports_tools = False;
    #! True when the provider supports structured output (JSON schema)
    bool supports_structured_output = False;
    #! True when the provider accepts multimodal content (images etc.)
    bool supports_multimodal = False;
}

#! Abstract factory for an LLM provider
/** Concrete factories register themselves with
    @ref LlmBackend::registerFactory() and the backend dispatches calls
    through them.  Factories are looked up by mode string and the heavy
    provider object is constructed lazily on demand via @ref create().

    @par Subclassing contract
    Concrete subclasses implement @ref getMode(), @ref getDisplayName(),
    @ref create(), and optionally @ref getInfo() to advertise capabilities.
*/
public class AbstractLlmProviderFactory {
    #! Returns the mode string associated with this factory
    /** For example, the Gemini factory returns \c "gemini"; the OpenAI-
        compatible factory returns \c "http".  The mode is matched against
        @ref LlmBackendConfig::mode at @ref LlmBackend construction.
    */
    abstract string getMode();

    #! Returns a human-readable display name
    abstract string getDisplayName();

    #! Creates a provider instance from the given backend configuration
    /** @param config the @ref LlmBackendConfig hash; the factory uses the
            relevant fields (URL, API key, retry settings, etc.) to build
            a fully initialized provider

        @return a concrete @ref AbstractLlmProvider that can service
            \c complete() / \c chat() / \c chatStream() calls
    */
    abstract AbstractLlmProvider create(hash<LlmBackendConfig> config);

    #! Returns static information about the provider this factory creates
    /** Default implementation returns the bare minimum
        (@ref LlmProviderInfo::mode and @ref LlmProviderInfo::display_name);
        concrete factories override to advertise streaming, tool, schema,
        and multimodal support.
    */
    hash<LlmProviderInfo> getInfo() {
        return <LlmProviderInfo>{
            "mode": getMode(),
            "display_name": getDisplayName(),
            "module_name": Class::getClass(self).getModuleName(),
        };
    }
}

} # namespace QoreLlmUtils
