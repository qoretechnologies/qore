# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! Builds the type of a GitHub webhook payload from the vendored schema
/** A webhook payload is not an operation, and @ref RestSchemaActions::RestSchemaPruner "RestSchemaPruner"
    drops the section describing one, so an event type cannot be generated the way an action's output type
    is.  GitHub declares its webhook payloads under an \c "x-webhooks" extension rather than the OpenAPI 3
    \c webhooks section, so preserving them would be a vendor-specific pruner feature.

    The objects those payloads carry are a different matter.  An \c issues delivery carries an \c issue, a
    \c repository and a \c sender, and all three **are** component schemas in the pinned document, reachable
    because the exported operations reference them.  So each event type is composed here from the pinned
    schema's own components, and only the few shapes GitHub does not publish as reusable components — a push
    commit, a review, a commit comment — are left as open hashes.

    That is what keeps these types honest: they are the same declarations the actions are generated from, so
    they cannot drift from the API the way a hand-written second copy would.  The application this module
    replaces hand-wrote a partial shape per trigger, and those shapes had already drifted.
*/
public class GitHubEventDataType {
    #! Returns a component schema's type, or an open hash if the pinned document does not carry it
    /** @param name the component schema name, ex: \c "issue"
        @param required whether the value is always present

        @return the type
    */
    static AbstractDataProviderType getComponentType(string name, bool required = True) {
        *OpenApi3::SchemaObject so = GitHubSchema::getSchema().components.schemas{name};
        if (so) {
            return so.getDataType(required);
        }
        return AbstractDataProviderTypeMap{required ? "hash" : "*hash"};
    }

    #! Returns the type of an event payload
    /** @param name the type name
        @param fields the payload fields, in order, each a hash of \c desc and either \c component (a
            component schema name) or \c type (a type map key)
        @param optional the fields that are not always present

        @return the payload type
    */
    static AbstractDataProviderType getPayloadType(string name, list<hash<auto>> fields) {
        HashDataType rv(name);
        foreach hash<auto> f in (fields) {
            AbstractDataProviderType type = f.component.val()
                ? GitHubEventDataType::getComponentType(f.component, !f.optional)
                : AbstractDataProviderTypeMap{f.optional ? "*" + f."type" : f."type"};
            rv.addField(new QoreDataField(f.name, f.desc, type));
        }
        return rv;
    }

    #! The fields every repository-scoped delivery carries
    /** GitHub sends the repository the hook is registered on and the account that caused the delivery with
        every event, so these are appended to each payload type rather than repeated in each declaration.
    */
    static list<hash<auto>> getCommonFields() {
        return (
            {
                "name": "repository",
                "desc": "the repository the webhook is registered on",
                "component": "repository",
            },
            {
                "name": "sender",
                "desc": "the account that caused the delivery",
                "component": "simple-user",
            },
        );
    }
}
}
