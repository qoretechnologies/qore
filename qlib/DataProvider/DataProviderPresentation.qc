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
        #! Translation locales required for the standard provider catalog set
        /** This policy is shared by Qore and independently released provider
            modules so their CI checks cannot silently diverge. Locale tags are
            canonical BCP 47 identities; they never encode language-specific
            matching or fallback behavior.

            @since %DataProvider 3.7
        */
        const StandardTranslationLocales = ("cs", "de", "es", "fr", "it",
            "ja", "ko", "pl", "sk", "uk", "zh-Hant", "zh-TW");
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

    #! Resolves the source-app identity used for action presentation message IDs
    /** This identity affects catalog lookup only.  The action's @c app member
        remains its exact executable application identity.

        @param app provider application metadata
        @param action provider action metadata
        @return the action-level source-app override, or the executable app
            identity

        @throw DATA-PROVIDER-PRESENTATION-ERROR if the selected identity is
            empty

        @par Example
        A remote projection of a locally owned action can retain executable
        app @c QorusRemote while using @c Qorus action message IDs.

        @note Authorization, routing, and execution must use @c action.app;
            this value is presentation provenance only.
    */
    static string actionCatalogApp(hash<auto> app, hash<auto> action) {
        *string explicit = action.presentation_catalog_app;
        string catalog_app = explicit.val() ? explicit : app.name.toString();
        # Reuse the stable identity validation boundary without exposing an
        # encoded segment as part of this API.
        appMessageId(catalog_app, "display_name");
        return catalog_app;
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

    #! Reports missing or invalid producer presentation locales
    /** This is the shared validation boundary used by source-catalog checks
        and runtime provider-index consumers.  Stable issue codes and exact
        technical identities let callers fail comprehensively without parsing
        localized exception prose.

        @param app provider application metadata
        @param actions registered action metadata for @p app
        @param include_app if @ref True "True", validate the app presentation
            as well as every action
        @return ordered issues containing @c scope, @c app, optional
            @c action, and stable @c reason; an empty list means that every
            requested presentation has a valid, non-@c und BCP-47 locale

        @par Example
        An English application with actions inheriting its declared @c en
        locale returns no issues.  An undeclared application and action return
        @c source_locale_required issues for both exact identities.

        @note This method never infers a locale from presentation text,
            identifiers, scripts, or cognates.
    */
    static list<hash<auto>> inspectSourceLocales(hash<auto> app,
            list<hash<auto>> actions, bool include_app = True) {
        list<hash<auto>> issues = ();
        if (include_app) {
            try {
                string locale = appSourceLocale(app);
                if (locale == UnknownLocale) {
                    issues += {
                        "scope": "app",
                        "app": app.name,
                        "reason": "source_locale_required",
                    };
                }
            } catch (hash<ExceptionInfo> ex) {
                issues += {
                    "scope": "app",
                    "app": app.name,
                    "reason": "invalid_source_locale",
                    "error": ex.err,
                    "detail": ex.desc,
                };
            }
        }

        foreach hash<auto> action in (actions) {
            try {
                string locale = actionSourceLocale(app, action);
                if (locale == UnknownLocale) {
                    issues += {
                        "scope": "action",
                        "app": app.name,
                        "action": action.action,
                        "reason": "source_locale_required",
                    };
                }
            } catch (hash<ExceptionInfo> ex) {
                issues += {
                    "scope": "action",
                    "app": app.name,
                    "action": action.action,
                    "reason": "invalid_source_locale",
                    "error": ex.err,
                    "detail": ex.desc,
                };
            }
        }
        return issues;
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

    #! Returns a stable presentation message id for a field nested in an action option's type
    /** Every field name is an exact technical identity. Repeated \c field segments describe the semantic data path;
        lists do not add a segment, while each union contributes the stable technical identity of its alternative.
        This lets repeated appearances of the same union branch coalesce while distinct branch semantics remain
        translatable and preserves every existing top-level option id.

        @param app exact technical app identifier
        @param action exact technical action identifier
        @param option exact technical option identifier
        @param field_path non-empty exact technical field path
        @param field presentation field name
        @param union_path ordered stable technical identities of union alternatives crossed on the way to the field;
            omitted when the path crosses no union
        @return stable symbolic message id

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported

        @since %DataProvider 3.8
    */
    static string nestedFieldMessageId(string app, string action,
            string option, list<string> field_path, string field,
            list<string> union_path = ()) {
        checkPresentationField(field);
        if (!field_path) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR",
                "a nested option field path cannot be empty";
        }
        string rv = "app." + identitySegment(app) + ".action."
            + identitySegment(action) + ".option."
            + identitySegment(option);
        foreach string union_type in (union_path) {
            rv += ".union." + identitySegment(union_type);
        }
        foreach string field_name in (field_path) {
            rv += ".field." + identitySegment(field_name);
        }
        return rv + "." + field;
    }

    #! Returns a stable allowed-value message id for a field nested in an action option's type
    /** @param app exact technical app identifier
        @param action exact technical action identifier
        @param option exact technical option identifier
        @param field_path non-empty exact technical field path
        @param value exact technical allowed value
        @param field presentation field name
        @param element_value if @ref True, identify an \c element_allowed_values entry; otherwise identify an
            \c allowed_values entry
        @param union_path ordered stable technical identities of union alternatives crossed on the way to the field
        @return stable symbolic message id, or @ref nothing for a complex value

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported

        @since %DataProvider 3.8
    */
    static *string nestedAllowedValueMessageId(string app, string action,
            string option, list<string> field_path, auto value, string field,
            bool element_value = False, list<string> union_path = ()) {
        *string value_identity = scalarValueIdentity(value);
        if (!value_identity) {
            return NOTHING;
        }
        string field_id = nestedFieldMessageId(app, action, option,
            field_path, field, union_path);
        # Replace the presentation-field suffix with the allowed-value path.
        return field_id.substr(0, field_id.length() - field.length())
            + (element_value ? "element-allowed-value." : "allowed-value.")
            + identitySegment(value_identity) + "." + field;
    }

    #! Returns the stable presentation identity for a provider type
    /** Provider type names can intentionally expose only their underlying Qore type (for example several generated
        schema types can all be named \c hash<auto> or \c *data). The identity therefore combines class and type
        name with a deterministic digest of the producer's stable technical schema path, when available, and the
        immediate technical field/type shape. Presentation text is deliberately excluded, so changing a label does
        not change its message ID. Distinct objects with the same complete technical identity are still compared for
        conflicting source text during extraction.

        @param type provider type to identify
        @return stable class, type-name, and technical-shape identity

        @since %DataProvider 3.8
    */
    static string typePresentationIdentity(AbstractDataProviderType type) {
        string base_identity = type.className() + ":" + type.getName();
        list<string> technical_shape = (base_identity,);
        if (*string type_path = type.getPath()) {
            technical_shape += "path:" + identitySegment(type_path);
        }
        if (*hash<string, AbstractDataField> fields = type.getFields()) {
            foreach string field_name in (sort(keys fields)) {
                AbstractDataProviderType field_type
                    = fields{field_name}.getDeclaredType();
                technical_shape += sprintf("field:%s:%s",
                    identitySegment(field_name), identitySegment(
                        field_type.className() + ":" + field_type.getName()));
            }
        }
        if (*AbstractDataProviderType element_type = type.getElementType()) {
            technical_shape += "element:" + identitySegment(
                element_type.className() + ":" + element_type.getName());
        }
        return base_identity + "#" + digest("sha256",
            technical_shape.join("|")).toHex();
    }

    #! Returns the stable action-scoped presentation identity for a reusable provider type
    /** Type names and shapes are not globally semantic: two actions can legitimately use equal-shaped anonymous
        objects for unrelated concepts. Scoping the reusable identity to the exact technical action prevents those
        objects from sharing translations while still coalescing a type DAG within one action.

        @param action exact technical action identifier
        @param type provider type to identify
        @return stable action and provider-type identity

        @throw DATA-PROVIDER-PRESENTATION-ERROR if \a action is empty

        @since %DataProvider 3.8
    */
    static string actionTypePresentationIdentity(string action,
            AbstractDataProviderType type) {
        return "action:" + identitySegment(action) + ":"
            + typePresentationIdentity(type);
    }

    #! Returns the stable occurrence identity for a reusable type below an action option
    /** The exact technical option, field, and union paths distinguish separate concrete type objects that happen to
        share a name and field shape. The identity remains independent of all presentation prose.

        @param action exact technical action identifier
        @param option exact technical option identifier
        @param type provider type to identify
        @param field_path exact technical field path leading to the type
        @param union_path ordered technical union-alternative identities crossed on the way to the type
        @return stable action-option occurrence and provider-type identity

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty

        @since %DataProvider 3.8
    */
    static string occurrenceTypePresentationIdentity(string action,
            string option, AbstractDataProviderType type,
            list<string> field_path, list<string> union_path = ()) {
        string rv = actionTypePresentationIdentity(action, type)
            + ":option:" + identitySegment(option);
        foreach string union_type in (union_path) {
            rv += ":union:" + identitySegment(union_type);
        }
        foreach string field_name in (field_path) {
            rv += ":field:" + identitySegment(field_name);
        }
        return rv;
    }

    #! Returns a stable presentation message id for a field in a reusable provider-type occurrence
    /** Deeper schema traversal uses an occurrence identity that includes action, option, technical field/union path,
        and type identity. The traversal assigns one canonical occurrence to each concrete type object per
        action/option, keeping work proportional to distinct schema objects rather than the potentially exponential
        number of paths through a shared type DAG.

        @param app exact technical app identifier
        @param type_identity stable identity returned by @ref occurrenceTypePresentationIdentity()
        @param field_name exact technical field identifier
        @param field presentation field name
        @return stable symbolic message id

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported

        @since %DataProvider 3.8
    */
    static string typeFieldMessageId(string app, string type_identity,
            string field_name, string field) {
        checkPresentationField(field);
        return "app." + identitySegment(app) + ".type."
            + identitySegment(type_identity) + ".field."
            + identitySegment(field_name) + "." + field;
    }

    #! Returns a stable allowed-value message id for a field in a reusable provider-type occurrence
    /** @param app exact technical app identifier
        @param type_identity stable identity returned by @ref occurrenceTypePresentationIdentity()
        @param field_name exact technical field identifier
        @param value exact technical allowed value
        @param field presentation field name
        @param element_value if @ref True, identify an \c element_allowed_values entry; otherwise identify an
            \c allowed_values entry
        @return stable symbolic message id, or @ref nothing for a complex value

        @throw DATA-PROVIDER-PRESENTATION-ERROR if an identity is empty or \a field is unsupported

        @since %DataProvider 3.8
    */
    static *string typeAllowedValueMessageId(string app,
            string type_identity, string field_name, auto value,
            string field, bool element_value = False) {
        *string value_identity = scalarValueIdentity(value);
        if (!value_identity) {
            return NOTHING;
        }
        string field_id = typeFieldMessageId(app, type_identity,
            field_name, field);
        return field_id.substr(0, field_id.length() - field.length())
            + (element_value ? "element-allowed-value." : "allowed-value.")
            + identitySegment(value_identity) + "." + field;
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
    /** The returned hash is keyed by catalog domain. Every message id is
        derived from an exact source-owned technical identity and every source
        string remains producer-owned. Action-level catalog-domain and
        source-app overrides are honored without changing executable identity.

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
        # domain -> action/option scope -> concrete type object -> seen
        #
        # Repeated paths to the same object are bounded, while distinct objects
        # claiming the same stable presentation identity are still traversed so
        # addRecordMessages() can reject conflicting source text.
        hash<string, hash<string, hash<string, bool>>> domain_type_seen = {};
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
            string catalog_app = actionCatalogApp(app, action);
            addRecordMessages(\domain_messages{domain}, action,
                *string sub (string field) {
                    return actionMessageId(catalog_app, action.action, field);
                });
            addOptionMapMessages(\domain_messages{domain}, catalog_app,
                action, action.path_vars, \domain_type_seen{domain});
            addOptionMapMessages(\domain_messages{domain}, catalog_app,
                action, action.options, \domain_type_seen{domain});
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
            [I18n::get_catalog_search_path()](../../i18n/html/index.html)
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
            [I18n::get_catalog_search_path()](../../i18n/html/index.html)
        @return a hash with \c translation_count, an \c issues list, and
            \c translation_message_ids_by_locale; each issue contains
            \c path, \c locale, \c message_id (empty for a file-level issue),
            and \c reason, while the message-id map contains the sorted union
            of exact identities supplied by each discovered translation locale

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
        hash<string, hash<string, bool>> message_ids_by_locale;
        list<string> search_path =
            I18n::get_catalog_search_path(discovery_opts);
        foreach string locale in (
                discoverCatalogLocales(domain, discovery_opts)) {
            if (locale == RootLocale) {
                continue;
            }
            message_ids_by_locale{locale} = {};
            foreach string search_root in (search_path) {
                list<string> paths = (
                    search_root + DirSep + domain + DirSep + locale
                        + ".json",
                    search_root + DirSep + domain + "." + locale
                        + ".json"
                );
                *list<string> fragments = glob(search_root + DirSep
                    + domain + DirSep + locale + DirSep + "*.json");
                if (fragments) {
                    paths += sort(fragments);
                }
                foreach string path in (paths) {
                    if (!is_file(path)) {
                        continue;
                    }
                    # Parse once, then validate the in-memory native catalog.
                    # Provider prose is opaque: braces are literal rather
                    # than format placeholders. Exact-locale enumeration is
                    # intentional: native discovery also returns root and
                    # parent-locale fallbacks, which are not translations for
                    # the locale currently being inspected.
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
                        message_ids_by_locale{locale}{entry.key} = True;
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
        hash<string, list<string>> translation_message_ids_by_locale;
        foreach string locale in (sort(keys message_ids_by_locale)) {
            translation_message_ids_by_locale{locale} =
                sort(keys message_ids_by_locale{locale});
        }
        return {
            "translation_count": translation_count,
            "issues": issues,
            "translation_message_ids_by_locale":
                translation_message_ids_by_locale,
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
            string catalog_app, hash<auto> action, auto option_map,
            reference<hash<string, hash<string, bool>>> type_seen) {
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
                    return optionMessageId(catalog_app, action.action,
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
                            return allowedValueMessageId(catalog_app,
                                action.action, option_name, value.value,
                                field);
                        });
                }
            }
            if (option.type instanceof AbstractDataProviderType) {
                addTypeMessages(\messages, catalog_app, action.action,
                    option_name, option.type, (), (), {}, True, \type_seen);
            } else if (option.type.typeCode() == NT_LIST) {
                # DataProviderOptionInfo supports a list of alternative types.
                # ActionOptionInfo normally contains one type, but accepting
                # both shapes keeps extraction at the canonical metadata
                # boundary rather than in each producer.
                foreach auto option_type in (option.type) {
                    if (option_type instanceof AbstractDataProviderType) {
                        addTypeMessages(\messages, catalog_app,
                            action.action, option_name, option_type, (), (),
                            {}, True, \type_seen);
                    }
                }
            }
        }
    }

    #! Adds presentation reachable through one option type
    /** \a type_path is passed by value deliberately and cuts a genuinely recursive root edge. Below the first
        field layer, \a type_seen makes traversal proportional to distinct type objects rather than paths through the
        type DAG. Distinct objects sharing a stable presentation identity are compared so conflicting source text
        remains a hard extraction error.
    */
    private static addTypeMessages(reference<hash<auto>> messages,
            string catalog_app, string action, string option,
            AbstractDataProviderType type, list<string> field_path,
            list<string> union_path, hash<string, bool> type_path,
            bool option_scoped,
            reference<hash<string, hash<string, bool>>> type_seen) {
        string object_identity = type.uniqueHash();
        if (type_path{object_identity}) {
            return;
        }
        type_path{object_identity} = True;

        if (type instanceof QoreUnionDataType) {
            foreach AbstractDataProviderType alternative in (
                    cast<QoreUnionDataType>(type).getUnionTypes()) {
                addTypeMessages(\messages, catalog_app, action, option,
                    alternative, field_path,
                    union_path + (typePresentationIdentity(alternative),),
                    type_path, option_scoped, \type_seen);
            }
            return;
        }

        string presentation_type = option_scoped
            ? typePresentationIdentity(type)
            : occurrenceTypePresentationIdentity(action, option, type,
                field_path, union_path);
        if (!option_scoped) {
            string seen_key = identitySegment(catalog_app) + ":"
                + identitySegment(action) + ":" + identitySegment(option);
            if (type_seen{seen_key}{object_identity}) {
                return;
            }
            type_seen{seen_key}{object_identity} = True;
        }

        if (*hash<string, AbstractDataField> fields = type.getFields()) {
            foreach string field_name in (sort(keys fields)) {
                AbstractDataField data_field = fields{field_name};
                list<string> nested_path = field_path + (field_name,);
                hash<auto> field_info = data_field.getPresentationInfo();
                addRecordMessages(\messages, field_info,
                    *string sub (string presentation_field) {
                        return option_scoped
                            ? nestedFieldMessageId(catalog_app, action,
                                option, nested_path, presentation_field,
                                union_path)
                            : typeFieldMessageId(catalog_app,
                                presentation_type, field_name,
                                presentation_field);
                    });
                foreach string values_key in (
                        ("allowed_values", "element_allowed_values")) {
                    if (field_info{values_key}.typeCode() != NT_LIST) {
                        continue;
                    }
                    bool element_value = values_key ==
                        "element_allowed_values";
                    foreach auto allowed_value in (field_info{values_key}) {
                        if (allowed_value.typeCode() != NT_HASH
                                || !exists allowed_value.value) {
                            continue;
                        }
                        hash<auto> value = allowed_value;
                        addRecordMessages(\messages, value,
                            *string sub (string presentation_field) {
                                return option_scoped
                                    ? nestedAllowedValueMessageId(catalog_app,
                                        action, option, nested_path,
                                        value.value, presentation_field,
                                        element_value, union_path)
                                    : typeAllowedValueMessageId(catalog_app,
                                        presentation_type, field_name,
                                        value.value, presentation_field,
                                        element_value);
                            });
                    }
                }
                addTypeMessages(\messages, catalog_app, action, option,
                    data_field.getDeclaredType(), nested_path, union_path,
                    type_path,
                    False, \type_seen);
            }
        }
        if (*AbstractDataProviderType element_type = type.getElementType()) {
            addTypeMessages(\messages, catalog_app, action, option,
                element_type, field_path, union_path, type_path,
                option_scoped, \type_seen);
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

    #! Selects the effective copy of each logical provider catalog layer
    /** A module owner can be installed below more than one native catalog
        search root during upgrades or development.  Such copies are the same
        logical contribution, not independent owners.  Native catalog search
        paths are ordered from low to high precedence, so retain only the last
        direct locale catalog and the last @c owner.json fragment for each
        locale.  Distinct owner filenames remain independent contributions and
        are still checked for collisions by
        @ref composeCatalog().

        The nested @c domain/locale.json and flat @c domain.locale.json layouts
        are alternative representations of the same direct locale layer.
    */
    private static list<string> selectEffectiveComposableCatalogFiles(
            string domain, list<string> files) {
        hash<string, string> effective;
        foreach string path in (files) {
            effective{composableCatalogFileIdentity(domain, path)} = path;
        }

        hash<string, bool> emitted;
        list<string> rv = ();
        foreach string path in (files) {
            string identity = composableCatalogFileIdentity(domain, path);
            if (!emitted{identity} && effective{identity} == path) {
                emitted{identity} = True;
                rv += path;
            }
        }
        return rv;
    }

    private static string composableCatalogFileIdentity(string domain,
            string path) {
        string name = basename(path);
        string parent = basename(dirname(path));
        if (parent == domain) {
            # domain/locale.json
            return "direct\x1f" + name.substr(0, name.length() - 5);
        }

        if (basename(dirname(dirname(path))) == domain) {
            # domain/locale/owner.json
            return "owner\x1f" + parent + "\x1f" + name;
        }

        # domain.locale.json; discovery has already validated the domain and
        # filename, so the suffix arithmetic is bounded.
        string prefix = domain + ".";
        return "direct\x1f" + name.substr(prefix.length(),
            name.length() - prefix.length() - 5);
    }

    #! Composes provider catalog layers, rejecting order-dependent message collisions
    /** Generic native catalogs intentionally use later-file precedence. Source-
        owned provider fragments have a stricter contract: independently owned
        contributions to one app domain must be disjoint or exactly identical.
        This check keeps catalog search-path and filename order from selecting
        user-facing provider presentation.

        The check is performed by
        [I18n::MessageCatalog](../../i18n/html/index.html) while it composes
        the layers (its @c disjoint_layers option), which also remains the
        schema and placeholder-validation authority. Layers for different
        locales, such as @c root.json and @c ja.json, cannot collide, so the
        check costs no separate pass over the messages.

        @param files effective catalog file paths in load order
        @param native_catalogs the parsed catalog for each entry of \a files,
            in the same order

        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if two files define the
            same locale and message id differently
    */
    private static I18n::MessageCatalog composeCatalog(list<string> files,
            list<hash<auto>> native_catalogs) {
        if (!files) {
            return new I18n::MessageCatalog({
                "schema_version": I18n::get_catalog_schema_version(),
                "locales": {"root": {"messages": {}}},
            }, {"literal_messages": True});
        }
        try {
            return new I18n::MessageCatalog(native_catalogs, {
                "literal_messages": True,
                "disjoint_layers": True,
            });
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "I18N-CATALOG-CONFLICT") {
                rethrow;
            }
            string first = files[ex.arg.layers[0]];
            string second = files[ex.arg.layers[1]];
            string desc = first == second
                ? sprintf("provider catalog file %y defines locale %y "
                    "message id %y more than once with different content",
                    first, ex.arg.locale, ex.arg.id)
                : sprintf("provider catalog files %y and %y conflict for "
                    "locale %y message id %y", first, second, ex.arg.locale,
                    ex.arg.id);
            throw "DATA-PROVIDER-PRESENTATION-CATALOG-ERROR", desc, {
                "locale": ex.arg.locale,
                "id": ex.arg.id,
                "files": (first, second),
            };
        }
    }

    #! Returns the canonical form of a requested projection locale
    private static string canonicalRequestedLocale(string requested_locale) {
        return requested_locale == DataProviderPresentation::RootLocale
            ? requested_locale : I18n::canonicalize_locale(requested_locale);
    }

    #! Returns the effective catalog files for one domain and canonical locale
    private static list<string> discoverEffectiveCatalogFiles(string domain,
            string locale, *hash<auto> discovery_opts,
            *I18n::CatalogDiscoveryIndex discovery_index) {
        list<string> files = discovery_index
            ? discovery_index.discover(domain, locale, discovery_opts)
            : I18n::discover_catalog_files(domain, locale, discovery_opts);
        return files
            ? selectEffectiveComposableCatalogFiles(domain, files)
            : files;
    }

    #! Creates a projection catalog
    /** @param domain native i18n catalog domain
        @param requested_locale requested BCP 47 locale or \c root for producer-source presentation
        @param discovery_opts options passed to [I18n::discover_catalog_files()](../../i18n/html/index.html)
        @param discovery_index optional discovery index reused by every catalog
            built for one operation; catalog discovery for many domains and
            locales otherwise re-reads the same catalog directories for each
            catalog

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

        @note A discovery index reports the catalog tree as it was when the
            index first read each directory, so it must be created for one
            operation and discarded with it; see
            \c I18n::CatalogDiscoveryIndex.

        @note To build catalogs for several locales of one domain, use
            @ref buildDomainCatalogs(), which parses the files the locales
            share, such as @c root.json, only once.
    */
    constructor(string domain, string requested_locale,
            *hash<auto> discovery_opts = NOTHING,
            *I18n::CatalogDiscoveryIndex discovery_index = NOTHING) {
        locale = canonicalRequestedLocale(requested_locale);
        list<string> files = discoverEffectiveCatalogFiles(domain, locale,
            discovery_opts, discovery_index);
        list<hash<auto>> native_catalogs = ();
        foreach string path in (files) {
            native_catalogs += I18n::load_native_catalog(path);
        }
        catalog = composeCatalog(files, native_catalogs);
    }

    #! Creates a projection catalog from an already composed message catalog
    private:internal constructor(string locale, I18n::MessageCatalog catalog) {
        self.locale = locale;
        self.catalog = catalog;
    }

    #! Creates the projection catalogs for several locales of one domain
    /** The result is the same as constructing one
        DataProviderPresentationCatalog per locale, but every catalog file is
        parsed at most once for the call: the catalogs of all locales of a
        domain share its @c root.json, and regional locales such as @c pt-BR
        also share their base-language file. A parsed file is released as soon
        as the last catalog that uses it has been built, and nothing but the
        returned catalogs is retained after the call.

        @param domain native i18n catalog domain
        @param requested_locales requested BCP 47 locales or \c root for
            producer-source presentation
        @param discovery_opts options passed to [I18n::discover_catalog_files()](../../i18n/html/index.html)
        @param discovery_index optional discovery index reused by every catalog
            built for one operation; if not given, one is created for this
            call

        @return one catalog for each entry of \a requested_locales, in the
            same order

        @throw I18N-CATALOG-ERROR if catalog discovery or parsing fails
        @throw I18N-LOCALE-ERROR if a requested locale is invalid
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if discovered owner
            fragments conflict for the same locale and message id

        @par Example
        @code{.py}
        list<DataProviderPresentationCatalog> catalogs =
            DataProviderPresentationCatalog::buildDomainCatalogs("data-provider.UW9yZQ",
                DataProviderPresentation::discoverCatalogLocales("data-provider.UW9yZQ"));
        @endcode

        @note All returned catalogs are held at once; a caller that uses each
            catalog only briefly, for example to validate a domain, holds one
            at a time with @ref forEachDomainCatalog()

        @since %DataProvider 3.7
    */
    static list<DataProviderPresentationCatalog> buildDomainCatalogs(
            string domain, list<string> requested_locales,
            *hash<auto> discovery_opts = NOTHING,
            *I18n::CatalogDiscoveryIndex discovery_index = NOTHING) {
        list<DataProviderPresentationCatalog> rv = ();
        forEachDomainCatalog(domain, requested_locales,
            sub (DataProviderPresentationCatalog catalog) {
                rv += catalog;
            }, discovery_opts, discovery_index);
        return rv;
    }

    #! Builds the projection catalogs for several locales of one domain one at a time
    /** Each catalog is built exactly as by @ref buildDomainCatalogs(), with
        every catalog file parsed at most once for the call, but is passed to
        \a consumer as soon as it is built instead of being collected. A
        catalog that \a consumer does not retain is released before the next
        one is built, so at most the shared parsed files and one catalog are
        held at a time.

        @param domain native i18n catalog domain
        @param requested_locales requested BCP 47 locales or \c root for
            producer-source presentation
        @param consumer called with each catalog in the order of
            \a requested_locales; an exception it throws stops the build and
            is propagated
        @param discovery_opts options passed to [I18n::discover_catalog_files()](../../i18n/html/index.html)
        @param discovery_index optional discovery index reused by every catalog
            built for one operation; if not given, one is created for this
            call

        @throw I18N-CATALOG-ERROR if catalog discovery or parsing fails
        @throw I18N-LOCALE-ERROR if a requested locale is invalid
        @throw DATA-PROVIDER-PRESENTATION-CATALOG-ERROR if discovered owner
            fragments conflict for the same locale and message id

        @par Example
        Rejecting conflicting provider catalogs of every installed locale
        without holding all of them at once:
        @code{.py}
        string domain = "data-provider.UW9yZQ";
        DataProviderPresentationCatalog::forEachDomainCatalog(domain,
            DataProviderPresentation::discoverCatalogLocales(domain),
            sub (DataProviderPresentationCatalog catalog) {
                printf("%s: valid\n", catalog.getLocale());
            });
        @endcode

        @since %DataProvider 3.7
    */
    static forEachDomainCatalog(string domain, list<string> requested_locales,
            code<nothing(DataProviderPresentationCatalog)> consumer,
            *hash<auto> discovery_opts = NOTHING,
            *I18n::CatalogDiscoveryIndex discovery_index = NOTHING) {
        if (!discovery_index) {
            discovery_index = new I18n::CatalogDiscoveryIndex();
        }

        # Discover every locale first so that each file's remaining uses are
        # known before any of them is parsed.
        list<string> locales = ();
        list<list<string>> files_by_locale = ();
        hash<string, int> remaining_uses = {};
        foreach string requested_locale in (requested_locales) {
            string locale = canonicalRequestedLocale(requested_locale);
            list<string> files = discoverEffectiveCatalogFiles(domain, locale,
                discovery_opts, discovery_index);
            locales += locale;
            files_by_locale += (files,);
            foreach string path in (files) {
                remaining_uses{path} = (remaining_uses{path} ?? 0) + 1;
            }
        }

        hash<string, hash<auto>> parsed = {};
        foreach string locale in (locales) {
            list<string> files = files_by_locale[$#];
            list<hash<auto>> native_catalogs = ();
            foreach string path in (files) {
                hash<auto> native_catalog = parsed{path}
                    ?? I18n::load_native_catalog(path);
                native_catalogs += native_catalog;
                if (--remaining_uses{path}) {
                    parsed{path} = native_catalog;
                } else {
                    remove parsed{path};
                }
            }
            consumer(new DataProviderPresentationCatalog(locale,
                composeCatalog(files, native_catalogs)));
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
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(action, action.action,
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::actionMessageId(catalog_app,
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
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(option, option_name,
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::optionMessageId(catalog_app,
                    action.action, option_name, field);
            });
    }

    #! Projects a localized field nested directly below an action option
    /** The caller supplies the same exact technical field and union paths used during source extraction.

        @param app provider application metadata
        @param action provider action metadata
        @param option_name exact technical option identifier
        @param field_path non-empty exact technical field path
        @param union_path ordered stable technical union-alternative identities
        @param field_info static field presentation metadata
        @return a copy with available localized presentation fields

        @throw DATA-PROVIDER-PRESENTATION-ERROR if \a field_path is empty

        @since %DataProvider 3.8
    */
    hash<auto> projectNestedField(hash<auto> app, hash<auto> action,
            string option_name, list<string> field_path,
            list<string> union_path, hash<auto> field_info) {
        if (!field_path) {
            throw "DATA-PROVIDER-PRESENTATION-ERROR",
                "a nested option field path cannot be empty";
        }
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(field_info, field_path.last(),
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::nestedFieldMessageId(
                    catalog_app, action.action, option_name, field_path,
                    field, union_path);
            });
    }

    #! Projects a localized field belonging to a stable reusable provider-type occurrence
    /** @param app provider application metadata
        @param action provider action metadata
        @param type_identity stable identity returned by
            @ref DataProviderPresentation::occurrenceTypePresentationIdentity()
        @param field_name exact technical field identifier
        @param field_info static field presentation metadata
        @return a copy with available localized presentation fields

        @since %DataProvider 3.8
    */
    hash<auto> projectTypeField(hash<auto> app, hash<auto> action,
            string type_identity, string field_name,
            hash<auto> field_info) {
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(field_info, field_name,
            DataProviderPresentation::actionSourceLocale(app, action),
            string sub (string field) {
                return DataProviderPresentation::typeFieldMessageId(
                    catalog_app, type_identity, field_name, field);
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
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(allowed_value, allowed_value.value.toString(),
            source_locale,
            string sub (string field) {
                return DataProviderPresentation::allowedValueMessageId(
                    catalog_app, action.action, option_name,
                    allowed_value.value, field);
            });
    }

    #! Projects an allowed value on a field nested directly below an action option
    /** @param app provider application metadata
        @param action provider action metadata
        @param option_name exact technical option identifier
        @param field_path non-empty exact technical field path
        @param union_path ordered stable technical union-alternative identities
        @param allowed_value allowed-value metadata to project
        @param element_value if @ref True, use the element-allowed-value identity
        @return a copy with available localized presentation fields; complex technical values preserve producer text

        @since %DataProvider 3.8
    */
    hash<auto> projectNestedAllowedValue(hash<auto> app,
            hash<auto> action, string option_name,
            list<string> field_path, list<string> union_path,
            hash<auto> allowed_value, bool element_value = False) {
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
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(allowed_value, allowed_value.value.toString(),
            source_locale, string sub (string field) {
                return DataProviderPresentation::nestedAllowedValueMessageId(
                    catalog_app, action.action, option_name, field_path,
                    allowed_value.value, field, element_value, union_path);
            });
    }

    #! Projects an allowed value on a field in a stable reusable provider-type occurrence
    /** @param app provider application metadata
        @param action provider action metadata
        @param type_identity stable identity returned by
            @ref DataProviderPresentation::occurrenceTypePresentationIdentity()
        @param field_name exact technical field identifier
        @param allowed_value allowed-value metadata to project
        @param element_value if @ref True, use the element-allowed-value identity
        @return a copy with available localized presentation fields; complex technical values preserve producer text

        @since %DataProvider 3.8
    */
    hash<auto> projectTypeAllowedValue(hash<auto> app, hash<auto> action,
            string type_identity, string field_name,
            hash<auto> allowed_value, bool element_value = False) {
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
        string catalog_app =
            DataProviderPresentation::actionCatalogApp(app, action);
        return project(allowed_value, allowed_value.value.toString(),
            source_locale, string sub (string field) {
                return DataProviderPresentation::typeAllowedValueMessageId(
                    catalog_app, type_identity, field_name,
                    allowed_value.value, field, element_value);
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
        DataProviderPresentation::actionMessageId(
            DataProviderPresentation::actionCatalogApp(app, action),
            action.action, "display_name");

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
        # Every locale of one domain resolves against the same catalog
        # directories and shares its root catalog; building them together
        # reads each directory and parses each file once. Nothing is retained
        # after the call, so a later population sees catalogs installed in the
        # meantime.
        foreach DataProviderPresentationCatalog catalog in (
                DataProviderPresentationCatalog::buildDomainCatalogs(domain,
                    DataProviderPresentation::discoverCatalogLocales(domain,
                        discovery_opts), discovery_opts)) {
            # Root source text is already present in the canonical provider
            # metadata and must not be duplicated in the embedding document.
            # Its catalog is built anyway so owner-fragment conflicts fail
            # population instead of remaining dependent on a later request.
            if (catalog.getLocale() != DataProviderPresentation::RootLocale) {
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
