# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! The SharePoint data provider common base class
/** SharePoint and OneDrive are the same Microsoft Graph API reached with the same OAuth2 flow: a
    document library is the drive-item API rooted at a named drive instead of at the signed-in
    user's own drive.  This class therefore derives from
    @ref OneDriveDataProvider::OneDriveDataProviderBase "OneDriveDataProviderBase" and overrides one
    method, @ref getDriveBasePath(), so that every drive-item provider in that module can be reused
    here rather than reimplemented.

    @note Inheritance order matters where this class is combined with a OneDrive leaf provider: a
    base class repeated in a Qore hierarchy is instantiated once, and the branch named **first**
    supplies the method.  The leaf providers therefore say
    \c "inherits SharePointDataProviderBase, OneDriveGetItemDataProvider" - the other order silently
    keeps OneDrive's personal-drive path.
*/
public class SharePointDataProviderBase inherits OneDriveDataProvider::OneDriveDataProviderBase {
    public {
        #! Constructor options
        const ConstructorOptions = OneDriveDataProvider::OneDriveDataProviderBase::ConstructorOptions + {
            "site_id": <DataProviderOptionInfo>{
                "display_name": "Site ID",
                "short_desc": "The default SharePoint site",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The ID of the SharePoint site to address when a request does not name one",
            },
            "drive_id": <DataProviderOptionInfo>{
                "display_name": "Document Library ID",
                "short_desc": "The default document library",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The ID of the document library (drive) to address when a request does not name one",
            },
        };
    }

    private {
        #! The site addressed when a request does not name one
        *string default_site_id;

        #! The document library addressed when a request does not name one
        *string default_drive_id;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : OneDriveDataProvider::OneDriveDataProviderBase(rest) {
        default_site_id = options.site_id;
        default_drive_id = options.drive_id;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : OneDriveDataProvider::OneDriveDataProviderBase(
                SharePointDataProviderBase::getRestClientFromOptions(options)) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        default_site_id = copts.site_id;
        default_drive_id = copts.drive_id;
    }

    #! Returns a REST client for the given constructor options, pinned to the SharePoint API profile
    /** The client is @ref OneDriveRestClient::OneDriveRestClientIo "OneDriveRestClientIo" rather
        than a SharePoint-specific class: Graph serves both products from one host with one OAuth2
        flow, and the two differ only in scopes and ping, which is what the \c api_profile selects.
        A separate client class would mean fixing every Microsoft OAuth2 bug twice.

        The profile is a default: a caller that pinned \c api_profile itself keeps it.
    */
    static RestClientIo::RestClientIo getRestClientFromOptions(*hash<auto> options) {
        if (options.restclient) {
            return options.restclient;
        }
        if (!options.oauth2_client_id || !options.oauth2_client_secret) {
            throw "CONSTRUCTOR-ERROR", "missing 'oauth2_client_id' and 'oauth2_client_secret' arguments "
                "for SharePoint API communication";
        }
        hash<auto> opts = (options.restclient_options ?? {}) + {
            "oauth2_client_id": options.oauth2_client_id,
            "oauth2_client_secret": options.oauth2_client_secret,
        };
        if (options.tenant) {
            opts.tenant = options.tenant;
        }
        if (!opts.api_profile) {
            opts.api_profile = ApiProfile;
        }
        return new OneDriveRestClient::OneDriveRestClientIo(opts);
    }

    #! Returns the Graph path prefix for the document library the given request addresses
    /** A document library is named by the request's \c drive_id, not fixed for the life of the
        provider: one SharePoint connection reaches every library in the tenant, and a provider
        object is reused across calls and threads, so the drive cannot be carried on the instance
        between calls.  Providers built with a \c drive_id constructor option - the event providers,
        which are configured once and then run - fall back to it.

        \c "drives/{drive-id}" is used rather than \c "sites/{site-id}/drives/{drive-id}" because a
        drive ID is unique tenant-wide; the site is what the drive is *chosen* from, not part of its
        address.  That is also why the file API can name a single container option.

        @param req the request, search options, or reference-data options for the call being built

        @return the Graph path prefix for the document library, with no trailing slash

        @throw SHAREPOINT-DRIVE-ERROR no document library was named
    */
    string getDriveBasePath(*hash<auto> req) {
        *string drive = req.drive_id ?? default_drive_id;
        if (!drive.val()) {
            throw "SHAREPOINT-DRIVE-ERROR", sprintf("no document library named; set the 'drive_id' option "
                "to the ID of the library to use (the 'list-drives' action lists the libraries in a site)");
        }
        return "drives/" + drive;
    }

    #! Returns the Graph path prefix for the site the given request addresses
    /** @param req the request, search options, or reference-data options for the call being built

        @return the Graph path prefix for the site, with no trailing slash

        @throw SHAREPOINT-SITE-ERROR no site was named
    */
    private string getSiteBasePath(*hash<auto> req) {
        *string site = req.site_id ?? default_site_id;
        if (!site.val()) {
            throw "SHAREPOINT-SITE-ERROR", sprintf("no site named; set the 'site_id' option to the ID of "
                "the site to use (the 'list-sites' action lists the sites available)");
        }
        return "sites/" + site;
    }

    #! Returns the Graph path prefix for the list the given request addresses
    /** @param req the request or search options for the call being built

        @return the Graph path prefix for the list, with no trailing slash

        @throw SHAREPOINT-LIST-ERROR no list was named
    */
    private string getListBasePath(*hash<auto> req) {
        *string list_id = req.list_id;
        if (!list_id.val()) {
            throw "SHAREPOINT-LIST-ERROR", sprintf("no list named; set the 'list_id' option to the ID of "
                "the list to use (the 'list-lists' action lists the lists in a site)");
        }
        return getSiteBasePath(req) + "/lists/" + list_id;
    }

    #! Retrieves every page of a Graph collection, following \c "@odata.nextLink"
    /** @param path the initial path
        @param limit stop once this many entries have been collected, if given
        @param hdr any additional request headers, sent with every page

        @return every entry in the collection
    */
    private list<hash<auto>> getAllPages(string path, *int limit, *hash<auto> hdr) {
        list<hash<auto>> rv = ();
        while (True) {
            hash<auto> response = doRestCommand("GET", path, NOTHING, hdr);
            if (response.body.value) {
                rv += response.body.value;
            }
            if (limit && rv.size() >= limit) {
                return rv[0..limit - 1];
            }
            *string next_link = response.body."@odata.nextLink";
            if (!next_link.val()) {
                break;
            }
            path = SharePointDataProviderBase::stripGraphBaseUrl(next_link);
        }
        return rv;
    }

    #! Converts an absolute Graph URL into a path relative to the client's base path
    /** Graph returns \c "@odata.nextLink" and \c "@odata.deltaLink" as absolute URLs, but the REST
        client is already rooted at the API version, so the prefix has to come off or the version
        would appear twice in the request.

        @param url the absolute (or already relative) Graph URL

        @return the path relative to the client's configured base path
    */
    static string stripGraphBaseUrl(string url) {
        url =~ s/^https:\/\/graph\.microsoft\.com\/(v1\.0|beta)\///;
        return url;
    }

    #! Returns the reference data types supported by this provider
    *hash<string, bool> getSupportedReferenceData() {
        return OneDriveDataProvider::OneDriveDataProviderBase::getSupportedReferenceData() + {
            "sites": True,
            "lists": True,
            "list-items": True,
            "columns": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "sites": return getReferenceSites(action_opts);
            case "drives": return getReferenceSiteDrives(action_opts);
            case "lists": return getReferenceLists(action_opts);
            case "list-items": return getReferenceListItems(action_opts);
            case "columns": return getReferenceColumns(action_opts);
        }
        # "folders" and "items" are inherited: they are built from getDriveBasePath(), which this
        # class has already redirected to the named document library
        return OneDriveDataProvider::OneDriveDataProviderBase::getReferenceDataImpl(type, action_opts);
    }

    #! Returns reference data for the document libraries in a site
    /** \c "drives" must not fall through to the inherited implementation: that one lists
        \c "me/drives", the signed-in user's own OneDrive, so the document library picker would
        offer the wrong drives entirely - and silently, since it does return something.
    */
    private *list<hash<AllowedValueInfo>> getReferenceSiteDrives(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string path = getSiteBasePath(action_opts) + "/drives?$select=id,name,description,driveType,"
                "webUrl&$top=" + MaxPageSize;
            foreach hash<auto> drive in (getAllPages(path)) {
                string display = drive.name ?? drive.id;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": drive.description.val() ? drive.description : display,
                    "desc": sprintf("%s (%s)", drive.description.val() ? drive.description : display,
                        drive.driveType ?? "unknown"),
                    "value": string(drive.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceSiteDrives: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for sites
    /** The tenant's root site is always offered, and subsites are added to it.  Graph's
        \c "/sites?search=" is backed by the search index, which returns nothing at all on a tenant
        whose sites have not been indexed - the TypeScript app this replaces used only that query,
        so on such a tenant its site picker was simply empty and the app unusable.
    */
    private *list<hash<AllowedValueInfo>> getReferenceSites(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        hash<string, bool> seen;
        code add = sub (hash<auto> site) {
            if (!site.id.val() || seen{site.id}) {
                return;
            }
            seen{site.id} = True;
            string display = site.displayName ?? site.name ?? site.id;
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": display,
                "desc": site.webUrl.val() ? sprintf("%s (%s)", display, site.webUrl) : display,
                "value": string(site.id),
            };
        };
        try {
            add(doRestCommand("GET", "sites/root?$select=id,displayName,name,webUrl").body);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceSites: root: %s: %s", ex.err, ex.desc);
        }
        try {
            map add($1), getAllPages("sites/root/sites?$select=id,displayName,name,webUrl", MaxPageSize);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceSites: subsites: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for the document libraries and lists in a site
    private *list<hash<AllowedValueInfo>> getReferenceLists(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string path = getSiteBasePath(action_opts)
                + "/lists?$select=id,displayName,description,list,system&$top=" + MaxPageSize;
            foreach hash<auto> l in (getAllPages(path)) {
                # system lists are SharePoint's own bookkeeping and are not useful targets
                if (exists l.system) {
                    continue;
                }
                string display = l.displayName ?? l.id;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": l.description.val() ? l.description : display,
                    "desc": sprintf("%s (template: %s)", l.description.val() ? l.description : display,
                        l.list.template ?? "unknown"),
                    "value": string(l.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceLists: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for the items in a list
    private *list<hash<AllowedValueInfo>> getReferenceListItems(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string path = getListBasePath(action_opts) + "/items?$expand=fields&$top=" + MaxPageSize;
            foreach hash<auto> item in (getAllPages(path, MaxPageSize)) {
                string display = item.fields.Title.val() ? item.fields.Title : sprintf("Item %s", item.id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": sprintf("%s (ID %s)", display, item.id),
                    "desc": sprintf("%s (ID %s)", display, item.id),
                    "value": string(item.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceListItems: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data for the writable columns of a list
    private *list<hash<AllowedValueInfo>> getReferenceColumns(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            foreach hash<auto> col in (getListColumns(action_opts)) {
                string display = col.displayName ?? col.name;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": col.description.val() ? col.description : display,
                    "desc": col.description.val() ? col.description : display,
                    "value": string(col.name),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceColumns: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns the writable column definitions of the list the given options address
    /** Read-only columns are excluded: Graph rejects a write to them, so offering them would only
        produce failures.

        @param opts options naming the site and list

        @return the writable column definitions
    */
    private list<hash<auto>> getListColumns(*hash<auto> opts) {
        string path = getListBasePath(opts) + "/columns?$top=" + MaxPageSize;
        return map $1, getAllPages(path), !$1.readOnly && !$1.hidden && $1.name.val()
            && !ReadOnlyItemFields{$1.name};
    }

    #! Converts a Graph column definition into a data provider field description
    /** A \c columnDefinition carries its kind as the presence of a facet - \c text, \c number,
        \c choice and so on - rather than as a type name, so the kind is found by looking for the
        facet that is set.

        @param col the column definition

        @return the field description, or @ref nothing if the column's kind has no useful mapping
        (\c calculated, \c thumbnail, \c geolocation and \c term columns are all read in their raw
        form rather than being given a type here)
    */
    static *hash<auto> getColumnField(hash<auto> col) {
        hash<auto> field = {
            "display_name": col.displayName ?? col.name,
            "short_desc": col.description.val() ? col.description : (col.displayName ?? col.name),
            "desc": col.description.val() ? col.description : sprintf("The %y column",
                col.displayName ?? col.name),
            "required": col.required ?? False,
        };
        if (exists col.boolean) {
            return field + {"type": AbstractDataProviderTypeMap."*bool"};
        }
        if (exists col.text) {
            return field + {"type": AbstractDataProviderTypeMap."*string"};
        }
        if (exists col.dateTime) {
            return field + {"type": DateOrNothingType};
        }
        if (exists col.number || exists col.currency) {
            return field + {"type": AbstractDataProviderTypeMap."*number"};
        }
        if (exists col.choice) {
            *list<auto> choices = col.choice.choices;
            list<hash<AllowedValueInfo>> allowed = map <AllowedValueInfo>{
                "value": $1,
                "display_name": $1,
                "desc": $1,
            }, choices;
            # a "checkBoxes" choice column holds several values at once
            hash<auto> rv = field + (col.choice.displayAs == "checkBoxes"
                ? {"type": AbstractDataProviderTypeMap."*softlist"}
                : {"type": AbstractDataProviderTypeMap."*string"});
            if (allowed) {
                rv.allowed_values = allowed;
            }
            return rv;
        }
        if (exists col.personOrGroup || exists col.lookup) {
            # both are written as the numeric ID of the target, or a list of them for multiple values
            return field + {"type": AbstractDataProviderTypeMap."*softstring"};
        }
        if (exists col.hyperlinkOrPicture) {
            return field + {
                "type": AutoHashOrNothingType,
                "desc": field.desc + "; a hash with `Url` and `Description` keys",
            };
        }
        return;
    }

    #! Returns the writable columns of a list as data provider fields
    /** @param opts options naming the site and list

        @return the writable columns as fields, keyed by internal column name; empty if the columns
        cannot be read (the list has not been chosen yet, or the call fails), so that a request type
        can always be produced
    */
    private hash<auto> getListColumnFields(*hash<auto> opts) {
        hash<auto> rv;
        try {
            foreach hash<auto> col in (getListColumns(opts)) {
                if (*hash<auto> field = SharePointDataProviderBase::getColumnField(col)) {
                    rv{col.name} = field;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getListColumnFields: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }
}
}
