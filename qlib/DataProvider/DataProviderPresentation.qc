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
        @param include_app include app-level presentation messages; set to \c False for an action-only module
            contributing to an app owned by another module
        @return domain to native root catalog map

        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if an action belongs to another app or two source strings
            conflict for one stable message id
        @throw DATA-PROVIDER-PRESENTATION-ERROR if a required technical identity is empty
    */
    static hash<string, hash<auto>> buildSourceCatalogs(hash<auto> app,
            list<hash<auto>> actions, bool include_app = True) {
        hash<string, hash<auto>> domain_messages = {};
        string app_domain = appCatalogDomain(app);
        if (include_app) {
            addRecordMessages(\domain_messages{app_domain}, app,
                *string sub (string field) {
                    return appMessageId(app.name, field);
                });
        }
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

    #! Discovers every native locale file available for one catalog domain
    /** Both supported native layouts are inspected: @c domain/locale.json and
        @c domain.locale.json. Locale identities come from the catalog files
        themselves; no language list or language-family heuristic is used.

        @param domain native catalog domain
        @param discovery_opts options accepted by
            @ref I18n::get_catalog_search_path()
        @return canonical locale tags in stable order, with @c root first when
            it is present

        @throw I18N-CATALOG-ERROR if @p domain, discovery options, or a locale
            filename is invalid
        @throw I18N-LOCALE-ERROR if a locale filename is not a valid BCP-47 tag

        @par Example
        If @c data-provider.UW9yZQ contains @c root.json, @c ja.json, and
        @c pt.json, this method returns @c root, @c ja, and @c pt without the
        caller naming those languages.
        @note Only regular JSON files in the native catalog search path are
            considered. Locale filenames must already use their canonical
            BCP-47 spelling so subsequent native discovery resolves the same
            file.

        @since %DataProvider 3.7
    */
    static list<string> discoverCatalogLocales(string domain,
            *hash<auto> discovery_opts = NOTHING) {
        # Reuse the native domain-validation authority without performing any
        # filesystem access.
        list<string> unused = I18n::discover_catalog_files(domain,
            RootLocale, {"search_path": ()});
        hash<string, bool> locales;
        foreach string search_root in (
                I18n::get_catalog_search_path(discovery_opts)) {
            string nested_dir = search_root + DirSep + domain;
            foreach string file in (glob(nested_dir + DirSep + "*.json")) {
                if (!is_file(file)) {
                    continue;
                }
                string name = basename(file);
                addDiscoveredCatalogLocale(\locales,
                    name.substr(0, name.length() - 5), file);
            }

            # Installed source catalogs can contain independently composable
            # owner fragments at domain/locale/owner.json.
            foreach string file in (glob(nested_dir + DirSep + "*"
                    + DirSep + "*.json")) {
                if (!is_file(file)) {
                    continue;
                }
                addDiscoveredCatalogLocale(\locales,
                    basename(dirname(file)), file);
            }

            string flat_prefix = domain + ".";
            foreach string file in (glob(search_root + DirSep
                    + flat_prefix + "*.json")) {
                if (!is_file(file)) {
                    continue;
                }
                string name = basename(file);
                addDiscoveredCatalogLocale(\locales,
                    name.substr(flat_prefix.length(),
                        name.length() - flat_prefix.length() - 5), file);
            }
        }
        list<string> rv = ();
        if (locales{RootLocale}) {
            rv += RootLocale;
        }
        foreach string locale in (sort(keys locales)) {
            if (locale != RootLocale) {
                rv += locale;
            }
        }
        return rv;
    }

    #! Inspects source-owned translations against a generated root catalog
    /** Every native translation file for \a domain in the configured search
        path is schema-validated and checked against the current producer
        messages. Partial catalogs and translations identical to their source
        are valid. This method does not infer a language from translated text
        and does not contain a locale or language-pair allowlist.

        @param domain native catalog domain
        @param root_messages generated root-catalog message map
        @param discovery_opts options accepted by
            @ref I18n::get_catalog_search_path()
        @return a hash with \c translation_count and an \c issues list; each
            issue contains \c path, \c locale, \c message_id (empty for a
            file-level issue), and \c reason

        @throw I18N-CATALOG-ERROR if a catalog is malformed or its filename is
            invalid
        @throw I18N-LOCALE-ERROR if a locale filename is not a valid BCP-47
            tag

        @par Example
        @code{.qore}
        hash<auto> result =
            DataProviderPresentation::inspectTranslationCatalogs(
                domain, generated.locales.root.messages,
                {"search_path": (catalog_root,)});
        if (result.issues) {
            throw "STALE-CATALOG", make_json(result.issues);
        }
        @endcode

        @note The complete search path is inspected rather than only the
            effective locale fallback, so shadowed stale source-owned files
            are also reported.

        @since %DataProvider 3.7
    */
    static hash<auto> inspectTranslationCatalogs(string domain,
            hash<auto> root_messages,
            *hash<auto> discovery_opts = NOTHING) {
        int translation_count = 0;
        list<hash<auto>> issues = ();
        list<string> search_path =
            I18n::get_catalog_search_path(discovery_opts);
        foreach string locale in (
                discoverCatalogLocales(domain, discovery_opts)) {
            if (locale == RootLocale) {
                continue;
            }
            foreach string search_root in (search_path) {
                foreach string path in ((
                        search_root + DirSep + domain + DirSep + locale
                            + ".json",
                        search_root + DirSep + domain + "." + locale
                            + ".json")) {
                    if (!is_file(path)) {
                        continue;
                    }
                    # Parse once, then validate the in-memory native catalog.
                    # Provider prose is opaque: braces are literal rather
                    # than format placeholders.
                    hash<auto> catalog = I18n::load_native_catalog(path);
                    I18n::MessageCatalog validated(catalog,
                        {"literal_messages": True});
                    if (sort(keys catalog.locales) != (locale,)) {
                        issues += translationCatalogIssue(path, locale, "",
                            sprintf("catalog must contain only locale %y",
                                locale));
                        continue;
                    }
                    foreach hash<auto> entry in (
                            catalog.locales{locale}.messages.pairIterator()) {
                        ++translation_count;
                        string reason = translationEntryProblem(entry.key,
                            entry.value, root_messages);
                        if (reason.val()) {
                            issues += translationCatalogIssue(path, locale,
                                entry.key, reason);
                        }
                    }
                }
            }
        }
        return {
            "translation_count": translation_count,
            "issues": issues,
        };
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

    private static addDiscoveredCatalogLocale(
            reference<hash<string, bool>> locales, string raw_locale,
            string file) {
        if (raw_locale == RootLocale) {
            locales{RootLocale} = True;
            return;
        }
        string locale = I18n::canonicalize_locale(raw_locale);
        if (locale != raw_locale) {
            throw "I18N-CATALOG-ERROR", sprintf(
                "catalog locale filename %y is not canonical; use %y",
                file, locale + ".json");
        }
        locales{locale} = True;
    }

    private static hash<auto> translationCatalogIssue(string path,
            string locale, string message_id, string reason) {
        return {
            "path": path,
            "locale": locale,
            "message_id": message_id,
            "reason": reason,
        };
    }

    private static string translationEntryProblem(string id, auto entry,
            hash<auto> root_messages) {
        if (!exists root_messages{id}) {
            return "message id is not present in the generated root catalog";
        }
        if (entry.typeCode() != NT_HASH) {
            return "provider translations must carry their producer source";
        }
        if (entry.source.typeCode() != NT_STRING) {
            return "translation source must be a string";
        }
        if (entry.source != root_messages{id}.source) {
            return "translation source no longer matches provider metadata";
        }
        if (entry.message.typeCode() != NT_STRING || !entry.message.val()) {
            return "translation message must be a non-empty string";
        }
        return "";
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
            # Erase a provider-owned option hashdecl into an open hash before
            # probing optional presentation members.  Path variables and
            # action options use different hashdecls, and direct dynamic
            # access to a member absent from either declaration raises
            # INVALID-MEMBER.
            hash<auto!> option = {};
            foreach hash<auto> field in (
                    option_entry.value.pairIterator()) {
                option{field.key} = field.value;
            }
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
        # Keep optional presentation-field discovery independent of the
        # producer's hashdecl.  Some valid records intentionally declare only
        # a subset of these fields.
        hash<auto!> presentation = {};
        foreach hash<auto> entry in (record.pairIterator()) {
            presentation{entry.key} = entry.value;
        }
        foreach string field in (("display_name", "short_desc", "desc")) {
            if (presentation{field}.typeCode() != NT_STRING
                    || !presentation{field}.toString().val()) {
                continue;
            }
            *string id = id_builder(field);
            if (!id) {
                continue;
            }
            string source = presentation{field}.toString();
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

    #! Rejects order-dependent message collisions across provider catalog files
    /** Generic native catalogs intentionally use later-file precedence. Source-
        owned provider fragments have a stricter contract: independently owned
        contributions to one app domain must be disjoint or exactly identical.
        This check keeps catalog search-path and filename order from selecting
        user-facing provider presentation.

        Malformed native catalogs are left to @ref I18n::MessageCatalog, which
        remains the schema and placeholder-validation authority.
    */
    private static validateComposableCatalogFiles(list<string> files) {
        hash<string, hash<auto>> messages_by_locale = {};
        hash<string, hash<string, string>> paths_by_locale = {};
        foreach string path in (files) {
            hash<auto> native_catalog = I18n::load_native_catalog(path);
            if (native_catalog.locales.typeCode() != NT_HASH) {
                continue;
            }
            foreach hash<auto> locale_entry in (
                    native_catalog.locales.pairIterator()) {
                if (locale_entry.value.typeCode() != NT_HASH
                        || locale_entry.value.messages.typeCode() != NT_HASH) {
                    continue;
                }
                string catalog_locale = locale_entry.key;
                if (!exists messages_by_locale{catalog_locale}) {
                    messages_by_locale{catalog_locale} = {};
                    paths_by_locale{catalog_locale} = {};
                }
                foreach hash<auto> message_entry in (
                        locale_entry.value.messages.pairIterator()) {
                    string message_id = message_entry.key;
                    if (exists messages_by_locale{catalog_locale}{message_id}
                            && messages_by_locale{catalog_locale}{message_id}
                                != message_entry.value) {
                        throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR",
                            sprintf("provider catalog files %y and %y conflict "
                                "for locale %y message id %y",
                                paths_by_locale{catalog_locale}{message_id},
                                path, catalog_locale, message_id);
                    }
                    messages_by_locale{catalog_locale}{message_id} =
                        message_entry.value;
                    paths_by_locale{catalog_locale}{message_id} = path;
                }
            }
        }
    }

    #! Creates a projection catalog
    /** @param domain native i18n catalog domain
        @param requested_locale requested BCP 47 locale or \c root for producer-source presentation
        @param discovery_opts options passed to @ref I18n::discover_catalog_files()

        @throw I18N-CATALOG-ERROR if catalog discovery or parsing fails
        @throw I18N-LOCALE-ERROR if \a requested_locale is invalid
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if discovered owner
            fragments conflict for the same locale and message id

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
            validateComposableCatalogFiles(files);
            catalog = new I18n::MessageCatalog(files,
                {"literal_messages": True});
        } else {
            catalog = new I18n::MessageCatalog({
                "schema_version": I18n::get_catalog_schema_version(),
                "locales": {"root": {"messages": {}}},
            }, {"literal_messages": True});
        }
    }

    #! Returns the exact locale represented by this projection catalog
    /** @return canonical requested BCP-47 locale or \c root
        @throw no exceptions are expected
    */
    string getLocale() {
        return locale;
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

#! Cached multilingual embedding projection for one provider application
/** This class discovers every locale shipped by a provider and constructs at
    most one immutable @ref DataProviderPresentationCatalog per catalog domain
    and locale. Canonical provider metadata remains separate; returned text is
    suitable only for semantic retrieval documents.

    @since %DataProvider 3.7
*/
public class DataProviderPresentationEmbeddingCatalog {
    private {
        hash<auto> app;
        *hash<auto> discovery_opts;
        Mutex cache_lock();
        hash<string, list<DataProviderPresentationCatalog>> catalog_cache;
    }

    #! Creates a cached application embedding projector
    /** @param app registered provider application metadata
        @param discovery_opts native catalog discovery options

        @throw DATA-PROVIDER-PRESENTATION-ERROR if the app identity is empty
        @throw I18N-CATALOG-ERROR if discovery options are invalid

        @par Example
        @code{.py}
        DataProviderPresentationEmbeddingCatalog embedding(app);
        hash<auto> info = embedding.projectAction(action);
        @endcode
    */
    constructor(hash<auto> app, *hash<auto> discovery_opts = NOTHING) {
        # Validate the identity before retaining the record.
        DataProviderPresentation::appCatalogDomain(app);
        self.app = app;
        self.discovery_opts = discovery_opts;
    }

    #! Builds a bounded multilingual embedding document for one action
    /** The document contains exact app/action identities, producer app/action
        presentation, and every requested-locale app/action presentation found
        in source-owned catalogs. It is the compatibility combination of
        @ref projectActionDocuments(). It never reconstructs, classifies,
        tokenizes, or translates producer prose.

        @param action registered provider action metadata
        @param max_description_chars maximum Unicode characters retained from
            each long @c desc field; @c 0 omits long descriptions
        @return a hash containing @c embedding_text and
            @c embedding_language; the latter is @c mul when at least one
            additional locale document contributes presentation, otherwise it
            is the declared producer locale or @c und

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an exact identity is empty
        @throw DATA-PROVIDER-PRESENTATION-EMBEDDING-ERROR if
            @p max_description_chars is negative or @p action belongs to a
            different app
        @throw I18N-CATALOG-ERROR if a discovered catalog is invalid
        @throw I18N-LOCALE-ERROR if a declared or discovered locale is invalid
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if discovered owner
            fragments conflict for the same locale and message id

        @par Example
        A Qore @c histogram record with Japanese and Portuguese catalogs keeps
        @c Qore and @c histogram in the document alongside all three producer
        and translated descriptions while returning @c mul.
        @note The result contains retrieval context only. Routing,
            authorization, schema selection, and execution must continue to use
            the original exact @p action record.
    */
    hash<auto> projectAction(hash<auto> action,
            int max_description_chars = 200) {
        list<hash<auto>> documents = projectActionDocuments(action,
            max_description_chars);
        list<string> lines = ();
        hash<string, bool> seen;
        foreach hash<auto> document in (documents) {
            foreach string line in (
                    document.embedding_text.toString().split("\n", False)) {
                if (line.val() && !seen{line}) {
                    seen{line} = True;
                    lines += line;
                }
            }
        }
        return {
            "embedding_text": lines.join("\n"),
            "embedding_language": documents.size() > 1 ? "mul"
                : documents[0].embedding_language,
        };
    }

    #! Builds one bounded embedding document per source-owned locale
    /** The first document contains the producer presentation and exact
        app/action identities. Each additional document contains the same
        identities plus presentation contributed by exactly one discovered
        locale. Empty locale catalogs and producer-locale aliases do not create
        redundant documents.

        This representation avoids semantic dilution as more translations are
        installed while keeping @ref projectAction() as the backwards-
        compatible combined multilingual projection.

        @param action registered provider action metadata
        @param max_description_chars maximum Unicode characters retained from
            each long @c desc field; @c 0 omits long descriptions
        @return ordered embedding documents with @c embedding_text and exact
            BCP-47 @c embedding_language; producer presentation is first

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an exact identity is empty
        @throw DATA-PROVIDER-PRESENTATION-EMBEDDING-ERROR if
            @p max_description_chars is negative or @p action belongs to a
            different app
        @throw I18N-CATALOG-ERROR if a discovered catalog is invalid
        @throw I18N-LOCALE-ERROR if a declared or discovered locale is invalid
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if discovered owner
            fragments conflict for the same locale and message id

        @par Example
        A Czech producer action with Japanese and Arabic catalogs returns three
        documents. Indexers can store the locale documents separately and map
        every hit back to the same exact provider action identity.
        @note The result contains retrieval context only. Routing,
            authorization, schema selection, and execution must continue to use
            the original exact @p action record.
    */
    list<hash<auto>> projectActionDocuments(hash<auto> action,
            int max_description_chars = 200) {
        if (max_description_chars < 0) {
            throw "DATA-PROVIDER-PRESENTATION-EMBEDDING-ERROR",
                "max_description_chars cannot be negative";
        }
        if (action.app.toString() != app.name.toString()) {
            throw "DATA-PROVIDER-PRESENTATION-EMBEDDING-ERROR", sprintf(
                "action %y belongs to app %y, not %y", action.action,
                action.app, app.name);
        }
        # Validate the exact action identity even when no locale catalog is
        # installed.
        DataProviderPresentation::actionMessageId(app.name, action.action,
            "display_name");

        list<hash<auto>> rv = ();
        list<string> lines = ();
        hash<string, bool> seen;
        addEmbeddingIdentity(\lines, \seen, app.name);
        addEmbeddingPresentation(\lines, \seen, app,
            max_description_chars);
        addEmbeddingIdentity(\lines, \seen, action.action);
        addEmbeddingPresentation(\lines, \seen, action,
            max_description_chars);
        string source_locale =
            DataProviderPresentation::actionSourceLocale(app, action);
        rv += {
            "embedding_text": lines.join("\n"),
            "embedding_language": source_locale,
        };

        string domain = DataProviderPresentation::actionCatalogDomain(app,
            action);
        foreach DataProviderPresentationCatalog catalog in (
                getCatalogs(domain)) {
            if (DataProviderPresentation::sourceLocaleMatches(
                    catalog.getLocale(), source_locale)) {
                continue;
            }
            lines = ();
            seen = {};
            addEmbeddingIdentity(\lines, \seen, app.name);
            addEmbeddingIdentity(\lines, \seen, action.action);
            hash<auto> localized_app = catalog.projectApp(app);
            hash<auto> localized_action = catalog.projectAction(app, action);
            bool app_text = addEmbeddingPresentation(\lines, \seen,
                localized_app, max_description_chars);
            bool action_text = addEmbeddingPresentation(\lines, \seen,
                localized_action, max_description_chars);
            if (!app_text && !action_text) {
                continue;
            }
            rv += {
                "embedding_text": lines.join("\n"),
                "embedding_language": catalog.getLocale(),
            };
        }
        return rv;
    }

    private list<DataProviderPresentationCatalog> getCatalogs(string domain) {
        cache_lock.lock();
        on_exit cache_lock.unlock();
        if (exists catalog_cache{domain}) {
            return catalog_cache{domain};
        }
        list<DataProviderPresentationCatalog> catalogs = ();
        foreach string locale in (
                DataProviderPresentation::discoverCatalogLocales(domain,
                    discovery_opts)) {
            DataProviderPresentationCatalog catalog(domain, locale,
                discovery_opts);
            # Root source text is already present in the canonical provider
            # metadata and must not be duplicated in the embedding document.
            # Construct its catalog anyway so owner-fragment conflicts fail
            # population instead of remaining dependent on a later request.
            if (locale != DataProviderPresentation::RootLocale) {
                catalogs += catalog;
            }
        }
        catalog_cache{domain} = catalogs;
        return catalogs;
    }

    private static addEmbeddingIdentity(reference<list<string>> lines,
            reference<hash<string, bool>> seen, auto value) {
        if (value.typeCode() != NT_STRING) {
            return;
        }
        string text = value.toString();
        if (text.val() && !seen{text}) {
            seen{text} = True;
            lines += text;
        }
    }

    private static bool addEmbeddingPresentation(
            reference<list<string>> lines,
            reference<hash<string, bool>> seen, hash<auto> record,
            int max_description_chars) {
        bool added = False;
        foreach string field in (("display_name", "short_desc", "desc")) {
            if (record{field}.typeCode() != NT_STRING) {
                continue;
            }
            string text = record{field}.toString();
            if (field == "desc") {
                if (!max_description_chars) {
                    continue;
                }
                if (text.length() > max_description_chars) {
                    text = text.substr(0, max_description_chars);
                }
            }
            if (text.val() && !seen{text}) {
                seen{text} = True;
                lines += text;
                added = True;
            }
        }
        return added;
    }
}
}
