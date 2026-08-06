# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderPresentation.qc language-independent data-provider presentation localization

/** DataProviderPresentation.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DataProvider {

#! Stable presentation identity and native-catalog utilities
/** Presentation message ids are derived only from exact technical identifiers.  They never depend on an English
    label, tokenization, case folding, or natural-language matching.

    @since %DataProvider 3.7
*/
public class DataProviderPresentation {
    public {
        const CatalogDomainPrefix = "data-provider.";
        const UnknownLocale = "und";
        const RootLocale = "root";
    }

    #! Returns the default catalog domain for an exact app identifier
    /** The UTF-8 app identifier is base64url encoded so all scripts have a reversible, path-safe representation.  A
        SHA-256 digest is used only when the encoded domain would exceed the native catalog domain bound.

        @param app exact technical app identifier
        @return native i18n catalog domain

        @throw DATA-PROVIDER-PRESENTATION-ERROR if \a app is empty
    */
    static string defaultCatalogDomain(string app) {
        string segment = identitySegment(app);
        string domain = CatalogDomainPrefix + segment;
        if (domain.size() <= 128) {
            return domain;
        }
        return CatalogDomainPrefix + digest("sha256",
            convert_encoding(app, "UTF-8")).toHex();
    }

    #! Resolves an app catalog domain
    /** @param app provider application metadata
        @return the explicit catalog domain, or a domain derived from the exact app identifier

        @throw DATA-PROVIDER-PRESENTATION-ERROR if no explicit domain exists and the app identity is empty
    */
    static string appCatalogDomain(hash<auto> app) {
        *string explicit = app.presentation_catalog_domain;
        return explicit.val() ? explicit : defaultCatalogDomain(app.name);
    }

    #! Resolves an action catalog domain, including an action-level override
    /** @param app provider application metadata
        @param action provider action metadata
        @return the action override, app domain, or a domain derived from the exact app identifier

        @throw DATA-PROVIDER-PRESENTATION-ERROR if no explicit domain exists and the app identity is empty
    */
    static string actionCatalogDomain(hash<auto> app, hash<auto> action) {
        *string explicit = action.presentation_catalog_domain;
        return explicit.val() ? explicit : appCatalogDomain(app);
    }

    #! Resolves the producer locale for an app without assuming a source language
    /** @param app provider application metadata
        @return canonical producer BCP 47 locale, or \c und when it is not declared

        @throw I18N-LOCALE-ERROR if a declared source locale is invalid
    */
    static string appSourceLocale(hash<auto> app) {
        return normalizeOptionalLocale(app.presentation_source_locale);
    }

    #! Resolves the producer locale for an action, including an action-level override
    /** @param app provider application metadata
        @param action provider action metadata
        @return canonical action or app producer locale, or \c und when neither is declared

        @throw I18N-LOCALE-ERROR if a declared source locale is invalid
    */
    static string actionSourceLocale(hash<auto> app, hash<auto> action) {
        *string explicit = action.presentation_source_locale;
        return explicit.val() ? normalizeOptionalLocale(explicit)
            : appSourceLocale(app);
    }

    #! Returns a stable app presentation message id
    /** @param app exact technical app identifier
        @param field presentation field name
        @return stable symbolic message id

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported
    */
    static string appMessageId(string app, string field) {
        checkPresentationField(field);
        return "app." + identitySegment(app) + "." + field;
    }

    #! Returns a stable action presentation message id
    /** @param app exact technical app identifier
        @param action exact technical action identifier
        @param field presentation field name
        @return stable symbolic message id

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported
    */
    static string actionMessageId(string app, string action, string field) {
        checkPresentationField(field);
        return "app." + identitySegment(app) + ".action."
            + identitySegment(action) + "." + field;
    }

    #! Returns a stable action-option presentation message id
    /** @param app exact technical app identifier
        @param action exact technical action identifier
        @param option exact technical option identifier
        @param field presentation field name
        @return stable symbolic message id

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported
    */
    static string optionMessageId(string app, string action, string option,
            string field) {
        checkPresentationField(field);
        return "app." + identitySegment(app) + ".action."
            + identitySegment(action) + ".option."
            + identitySegment(option) + "." + field;
    }

    #! Returns a stable presentation identity for a scalar technical value
    /** The type prefix prevents collisions such as integer \c 1 versus string \c "1". Complex values return
        @ref nothing because they have no general, source-independent technical identity; providers should expose a
        scalar value when its label is intended for catalog translation.

        @param value exact technical scalar value
        @return type-qualified scalar identity, or @ref nothing for a complex value
    */
    static *string scalarValueIdentity(auto value) {
        switch (value.typeCode()) {
            case NT_STRING:
                return "string:" + value.toString();
            case NT_INT:
                return "int:" + value.toString();
            case NT_FLOAT:
                return "float:" + value.toString();
            case NT_NUMBER:
                return "number:" + value.toString();
            case NT_BOOLEAN:
                return value ? "bool:true" : "bool:false";
            case NT_DATE:
                return "date:" + value.toString();
            case NT_BINARY:
                return "binary:" + value.toBase64Url();
            case NT_NULL:
                return "null";
            case NT_NOTHING:
                return "nothing";
        }
        return NOTHING;
    }

    #! Returns a stable allowed-value presentation message id when the value has a scalar technical identity
    /** @param app exact technical app identifier
        @param action exact technical action identifier
        @param option exact technical option identifier
        @param value exact technical allowed value
        @param field presentation field name
        @return stable symbolic message id, or @ref nothing for a complex value

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported
    */
    static *string allowedValueMessageId(string app, string action,
            string option, auto value, string field) {
        checkPresentationField(field);
        *string value_identity = scalarValueIdentity(value);
        if (!value_identity) {
            return NOTHING;
        }
        return "app." + identitySegment(app) + ".action."
            + identitySegment(action) + ".option."
            + identitySegment(option) + ".allowed-value."
            + identitySegment(value_identity) + "." + field;
    }

    #! Builds root native catalogs for one provider app and its actions
    /** The returned hash is keyed by catalog domain. Every message id is derived from exact technical identity and
        every source string remains producer-owned. Action-level catalog-domain overrides are honored.

        @param app provider app metadata
        @param actions provider action metadata
        @return domain to native root catalog map

        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if an action belongs to another app or two source strings
            conflict for one stable message id
        @throw DATA-PROVIDER-PRESENTATION-ERROR if a required technical identity is empty
    */
    static hash<string, hash<auto>> buildSourceCatalogs(hash<auto> app,
            list<hash<auto>> actions) {
        hash<string, hash<auto>> domain_messages = {};
        string app_domain = appCatalogDomain(app);
        addRecordMessages(\domain_messages{app_domain}, app,
            *string sub (string field) {
                return appMessageId(app.name, field);
            });
        foreach hash<auto> action in (actions) {
            if (action.app.toString() != app.name.toString()) {
                throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR",
                    sprintf("action %y belongs to app %y, not %y",
                        action.action, action.app, app.name);
            }
            string domain = actionCatalogDomain(app, action);
            addRecordMessages(\domain_messages{domain}, action,
                *string sub (string field) {
                    return actionMessageId(app.name, action.action, field);
                });
            addOptionMapMessages(\domain_messages{domain}, app, action,
                action.path_vars);
            addOptionMapMessages(\domain_messages{domain}, app, action,
                action.options);
        }

        hash<string, hash<auto>> rv = {};
        foreach hash<auto> entry in (domain_messages.pairIterator()) {
            rv{entry.key} = {
                "schema_version": I18n::get_catalog_schema_version(),
                "locales": {
                    RootLocale: {
                        "messages": entry.value,
                    },
                },
            };
        }
        return rv;
    }

    #! Returns @ref True when two locale tags identify the same base language
    /** This uses ICU-canonicalized BCP 47 fallback chains and has no language-name or language-pair allowlist.

        @param requested requested presentation locale
        @param source producer source locale
        @return @ref True when both locales resolve to the same base language

        @throw I18N-LOCALE-ERROR if a non-placeholder locale is invalid
    */
    static bool sourceLocaleMatches(string requested, string source) {
        if (requested == RootLocale) {
            return True;
        }
        if (!source.val() || source == UnknownLocale) {
            return False;
        }
        return baseLocale(requested) == baseLocale(source);
    }

    private static string identitySegment(string identity) {
        if (!identity.val()) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR",
                "technical presentation identities cannot be empty";
        }
        return convert_encoding(identity, "UTF-8").toBase64Url();
    }

    private static checkPresentationField(string field) {
        if (!inlist(field, ("display_name", "short_desc", "desc"))) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR",
                sprintf("unsupported presentation field %y", field);
        }
    }

    private static string normalizeOptionalLocale(auto locale) {
        if (locale.typeCode() != NT_STRING || !locale.toString().val()) {
            return UnknownLocale;
        }
        return I18n::canonicalize_locale(locale.toString());
    }

    private static string baseLocale(string locale) {
        list<string> chain = I18n::get_locale_fallback_chain(
            I18n::canonicalize_locale(locale));
        return chain.size() > 1 ? chain[chain.size() - 2] : chain[0];
    }

    private static addOptionMapMessages(reference<hash<auto>> messages,
            hash<auto> app, hash<auto> action, auto option_map) {
        if (option_map.typeCode() != NT_HASH) {
            return;
        }
        foreach hash<auto> option_entry in (
                option_map.pairIterator()) {
            if (option_entry.value.typeCode() != NT_HASH) {
                continue;
            }
            string option_name = option_entry.key;
            hash<auto> option = option_entry.value;
            addRecordMessages(\messages, option,
                *string sub (string field) {
                    return optionMessageId(app.name, action.action,
                        option_name, field);
                });
            foreach string allowed_values_key in (
                    ("allowed_values", "element_allowed_values")) {
                if (option{allowed_values_key}.typeCode() != NT_LIST) {
                    continue;
                }
                foreach auto allowed_value in (
                        option{allowed_values_key}) {
                    if (allowed_value.typeCode() != NT_HASH
                            || !exists allowed_value.value) {
                        continue;
                    }
                    hash<auto> value = allowed_value;
                    addRecordMessages(\messages, value,
                        *string sub (string field) {
                            return allowedValueMessageId(app.name,
                                action.action, option_name, value.value,
                                field);
                        });
                }
            }
        }
    }

    private static addRecordMessages(reference<hash<auto>> messages,
            hash<auto> record, code<*string(string)> id_builder) {
        foreach string field in (("display_name", "short_desc", "desc")) {
            if (record{field}.typeCode() != NT_STRING
                    || !record{field}.toString().val()) {
                continue;
            }
            *string id = id_builder(field);
            if (!id) {
                continue;
            }
            string source = record{field}.toString();
            if (exists messages{id}) {
                string existing = messages{id}.source.toString();
                if (existing != source) {
                    throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR",
                        sprintf("message id %y has conflicting source strings "
                            "%y and %y", id, existing, source);
                }
                continue;
            }
            messages{id} = {
                "source": source,
                "message": source,
            };
        }
    }
}

#! One immutable locale-aware view of a source-owned provider catalog
/** Construct one instance per catalog domain and requested locale, then reuse it while projecting app, action, and
    option records. This keeps catalog discovery and JSON parsing out of per-record hot paths.

    @since %DataProvider 3.7
*/
public class DataProviderPresentationCatalog {
    private {
        string locale;
        I18n::MessageCatalog catalog;
    }

    #! Creates a projection catalog
    /** @param domain native i18n catalog domain
        @param requested_locale requested BCP 47 locale or \c root for producer-source presentation
        @param discovery_opts options passed to @ref I18n::discover_catalog_files()

        @throw I18N-CATALOG-ERROR if catalog discovery or parsing fails
        @throw I18N-LOCALE-ERROR if \a requested_locale is invalid

        @par Example
        A design client can project one provider action without changing its exact technical identity:
        @code{.py}
        DataProviderPresentationCatalog catalog("data-provider.UW9yZQ", "pt-BR");
        hash<auto> localized = catalog.projectAction(app, action);
        @endcode
    */
    constructor(string domain, string requested_locale,
            *hash<auto> discovery_opts = NOTHING) {
        locale = requested_locale == DataProviderPresentation::RootLocale
            ? requested_locale : I18n::canonicalize_locale(requested_locale);
        list<string> files = I18n::discover_catalog_files(domain, locale,
            discovery_opts);
        if (files) {
            catalog = new I18n::MessageCatalog(files);
        } else {
            catalog = new I18n::MessageCatalog({
                "schema_version": I18n::get_catalog_schema_version(),
                "locales": {"root": {"messages": {}}},
            });
        }
    }

    #! Projects localized app presentation while preserving technical identity
    /** @param app provider application metadata
        @return localized presentation plus unchanged app identity and producer provenance

        @throw I18N-LOCALE-ERROR if the producer source locale is invalid
    */
    hash<auto> projectApp(hash<auto> app) {
        return project(app, app.name, DataProviderPresentation::appSourceLocale(
            app), string sub (string field) {
                return DataProviderPresentation::appMessageId(app.name,
                    field);
            });
    }

    #! Projects localized action presentation while preserving technical identity
    /** @param app provider application metadata
        @param action provider action metadata
        @return localized presentation plus unchanged action identity and producer provenance

        @throw I18N-LOCALE-ERROR if the producer source locale is invalid
    */
    hash<auto> projectAction(hash<auto> app, hash<auto> action) {
        return project(action, action.action,
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::actionMessageId(app.name,
                    action.action, field);
            });
    }

    #! Projects localized action-option presentation while preserving option identity
    /** @param app provider application metadata
        @param action provider action metadata
        @param option_name exact technical option identifier
        @param option provider option metadata
        @return localized presentation plus unchanged option identity and producer provenance

        @throw DATA-PROVIDER-PRESENTATION-ERROR if \a option_name is empty
        @throw I18N-LOCALE-ERROR if the producer source locale is invalid
    */
    hash<auto> projectOption(hash<auto> app, hash<auto> action,
            string option_name, hash<auto> option) {
        return project(option, option_name,
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::optionMessageId(app.name,
                    action.action, option_name, field);
            });
    }

    #! Projects localized allowed-value presentation without changing its technical value
    /** @param app provider application metadata
        @param action provider action metadata
        @param option_name exact technical option identifier
        @param allowed_value provider allowed-value metadata
        @return localized presentation plus the unchanged exact allowed value and producer provenance

        @throw DATA-PROVIDER-PRESENTATION-ERROR if \a option_name is empty
        @throw I18N-LOCALE-ERROR if the producer source locale is invalid
    */
    hash<auto> projectAllowedValue(hash<auto> app, hash<auto> action,
            string option_name, hash<auto> allowed_value) {
        if (!exists allowed_value.value) {
            return allowed_value;
        }
        *string value_identity =
            DataProviderPresentation::scalarValueIdentity(
                allowed_value.value);
        string source_locale =
            DataProviderPresentation::actionSourceLocale(app, action);
        if (!value_identity) {
            return projectUntranslatableValue(allowed_value, source_locale);
        }
        return project(allowed_value, allowed_value.value.toString(),
            source_locale,
            string sub (string field) {
                return DataProviderPresentation::allowedValueMessageId(
                    app.name, action.action, option_name,
                    allowed_value.value, field);
            });
    }

    private hash<auto> projectUntranslatableValue(hash<auto> source,
            string source_locale) {
        hash<auto!> rv = {};
        foreach hash<auto> entry in (source.pairIterator()) {
            rv{entry.key} = entry.value;
        }
        if (locale == DataProviderPresentation::RootLocale) {
            return rv;
        }
        hash<auto!> producer = {};
        foreach string field in (("display_name", "short_desc", "desc")) {
            if (source{field}.typeCode() == NT_STRING) {
                producer{field} = source{field};
                rv{field} = "";
            }
        }
        if (producer) {
            producer.language = source_locale;
            rv.producer_presentation = producer;
        }
        rv.presentation_language = locale;
        rv.presentation_incomplete = True;
        return rv;
    }

    private hash<auto> project(hash<auto> source, string technical_identity,
            string source_locale, code<*string(string)> id_builder) {
        # Copy into an open hash so typed provider records and uniform
        # producer hashes can carry projection metadata without weakening
        # their authoritative source types.
        hash<auto!> rv = {};
        foreach hash<auto> entry in (source.pairIterator()) {
            rv{entry.key} = entry.value;
        }
        hash<auto!> producer = {};
        bool incomplete;
        bool use_source = DataProviderPresentation::sourceLocaleMatches(
            locale, source_locale);

        foreach string field in (("display_name", "short_desc", "desc")) {
            if (source{field}.typeCode() != NT_STRING) {
                continue;
            }
            string source_text = source{field}.toString();
            producer{field} = source_text;
            if (!source_text.val()) {
                rv{field} = "";
                continue;
            }
            if (use_source) {
                rv{field} = source_text;
                continue;
            }
            *string id = id_builder(field);
            if (!id) {
                rv{field} = field == "display_name" ? technical_identity : "";
                incomplete = True;
                continue;
            }
            *string message_locale = catalog.getMessageLocale(id, locale);
            if (message_locale.val()
                    && message_locale != DataProviderPresentation::RootLocale
                    && catalog.getMessageSource(id, locale) == source_text) {
                rv{field} = catalog.tc(id, source_text, locale);
                continue;
            }
            rv{field} = field == "display_name" ? technical_identity : "";
            incomplete = True;
        }
        if (producer) {
            producer.language = source_locale;
            rv.producer_presentation = producer;
        }
        rv.presentation_language = use_source ? source_locale : locale;
        if (incomplete) {
            rv.presentation_incomplete = True;
        } else {
            delete rv.presentation_incomplete;
        }
        return rv;
    }
}
}
