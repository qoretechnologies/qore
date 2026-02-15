# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactSchemaRegistry.qc EDIFACT schema registry

/*  EdifactSchemaRegistry.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! Registry for EDIFACT schemas by release
    public class EdifactSchemaRegistry {
        private {
            static hash<string, EdifactSchema> registry;
        }

        #! Loads a schema for a release, using cache or a flat-file directory source
        /** @param release the EDIFACT release (ex: "D01B")
            @param opts options:
                - reload: ignore in-memory registry and reload
                - skip_cache_read: do not read from on-disk cache
                - cache_only: load from cache only
                - cache_error: how to handle cache parse/type errors (ignore, warn, error)
                - cache_warnings: list reference to receive warnings when cache_error is warn
                - dir: directory containing flat files for the release
                - loader_opts: options passed to EdifactDirectoryLoader
        */
        public static EdifactSchema loadRelease(string release, *hash<auto> opts) {
            if (!registry) {
                registry = new hash<string, EdifactSchema>();
            }
            bool reload = opts.reload ?? False;
            if (!reload && registry{release}) {
                return registry{release};
            }

            EdifactDirectoryCache cache();
            *EdifactSchema schema;
            if (!opts.skip_cache_read) {
                hash cache_opts = {
                    "cache_error": opts.cache_error ?? "warn",
                    "cache_warnings": opts.cache_warnings,
                };
                schema = cache.loadSchema(release, cache_opts);
                if (schema) {
                    registry{release} = schema;
                    return schema;
                }
            }

            if (opts.cache_only) {
                throw "EDIFACT-CACHE-MISS", sprintf("no cached schema for release %y", release);
            }

            if (!opts.dir) {
                throw "EDIFACT-DIR-ERROR", sprintf("no directory provided for release %y", release);
            }

            EdifactDirectoryLoader loader();
            schema = loader.loadFromFlatDir(opts.dir, release, opts.loader_opts ?? {});
            cache.writeSchema(schema);
            registry{release} = schema;
            return schema;
        }

        #! Returns the schema for a release if loaded
        public static *EdifactSchema getRelease(string release) {
            if (!registry) {
                return NOTHING;
            }
            return registry{release};
        }

        #! Returns a list of loaded releases
        public static list<string> listReleases() {
            if (!registry) {
                return ();
            }
            return keys registry;
        }
    }
}
