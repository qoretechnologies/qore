# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaActionCodecs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
#! A registry of bidirectional conversions between action-facing values and their wire representations
/** A REST schema describes what goes on the wire.  It cannot say that an integer is really a point in time,
    so an action generated straight from a schema presents an integer and leaves every caller to do the
    arithmetic — and to guess whether the unit is seconds or milliseconds.  Guessing wrongly is not a
    hypothetical: getting that unit wrong is one of the defects that motivated re-deriving overlays rather
    than copying them.

    A codec closes that gap declaratively.  A field overlay names a codec; the field is then published with
    the codec's type and values are converted in both directions automatically, so the manifest never has to
    carry executable code.

    Applications may register their own codecs with @ref registerCodec().

    @par Example:
    @code{.py}
"created": <RestSchemaFieldOverlayInfo>{
    "codec": RestSchemaActionCodecs::UnixSeconds,
    "short_desc": "When the object was created",
},
    @endcode
*/
public class RestSchemaActionCodecs {
    public {
        #! Codec name: a %Qore date presented for an integer count of seconds since the UNIX epoch
        const UnixSeconds = "unix-seconds";

        #! Codec name: a %Qore date presented for an integer count of milliseconds since the UNIX epoch
        const UnixMilliseconds = "unix-milliseconds";

        #! Codec name: a %Qore date presented for an ISO-8601 UTC timestamp string
        const Iso8601Utc = "iso-8601-utc";
    }

    private {
        #! The codec registry, keyed by codec name
        static hash<string, hash<RestSchemaWireCodecInfo>> codecmap = DefaultCodecs;

        #! Serializes access to @ref codecmap
        static Mutex lck();

        #! The codecs provided by this module
        const DefaultCodecs = {
            UnixSeconds: <RestSchemaWireCodecInfo>{
                "name": UnixSeconds,
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "sent as an integer number of seconds since 1970-01-01Z",
                "to_wire": auto sub (auto v) {
                    return exists v ? get_epoch_seconds(v) : v;
                },
                "from_wire": auto sub (auto v) {
                    return exists v ? gmtime(v) : v;
                },
            },
            UnixMilliseconds: <RestSchemaWireCodecInfo>{
                "name": UnixMilliseconds,
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "sent as an integer number of milliseconds since 1970-01-01Z",
                "to_wire": auto sub (auto v) {
                    return exists v ? get_epoch_milliseconds(v) : v;
                },
                "from_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    int ms = v;
                    # gmtime() takes whole seconds plus microseconds; a negative millisecond value must
                    # floor towards -infinity so that the microsecond remainder stays non-negative
                    int secs = ms / 1000;
                    int rem = ms % 1000;
                    if (rem < 0) {
                        --secs;
                        rem += 1000;
                    }
                    return gmtime(secs, rem * 1000);
                },
            },
            Iso8601Utc: <RestSchemaWireCodecInfo>{
                "name": Iso8601Utc,
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "sent as an ISO-8601 timestamp in UTC",
                "to_wire": auto sub (auto v) {
                    if (!exists v) {
                        return v;
                    }
                    # NOTE: a "Z" in a date format string writes the UTC *offset*, not a literal "Z", so the
                    # value is converted to UTC and the zone designator appended explicitly
                    TimeZone utc(0);
                    return utc.date(v).format("YYYY-MM-DDTHH:mm:SS") + "Z";
                },
                "from_wire": auto sub (auto v) {
                    return exists v ? date(v) : v;
                },
            },
        };
    }

    #! Registers a codec, replacing any codec already registered under the same name
    /** @param codec the codec to register

        @throw REST-SCHEMA-CODEC-ERROR the codec name is empty
    */
    static registerCodec(hash<RestSchemaWireCodecInfo> codec) {
        if (!codec.name.val()) {
            throw "REST-SCHEMA-CODEC-ERROR", "cannot register a codec with an empty name";
        }
        AutoLock al(lck);
        codecmap{codec.name} = codec;
    }

    #! Removes a codec from the registry
    /** @param name the codec name

        @return the codec removed, if any
    */
    static *hash<RestSchemaWireCodecInfo> deregisterCodec(string name) {
        AutoLock al(lck);
        return remove codecmap{name};
    }

    #! Returns the codec with the given name, if any
    static *hash<RestSchemaWireCodecInfo> getCodec(string name) {
        AutoLock al(lck);
        return codecmap{name};
    }

    #! Returns the codec with the given name
    /** @param name the codec name

        @return the codec with the given name

        @throw REST-SCHEMA-CODEC-ERROR the codec is unknown
    */
    static hash<RestSchemaWireCodecInfo> getCodecEx(string name) {
        AutoLock al(lck);
        *hash<RestSchemaWireCodecInfo> rv = codecmap{name};
        if (!rv) {
            throw "REST-SCHEMA-CODEC-ERROR", sprintf("wire codec %y is unknown; known codecs: %y", name,
                sort(keys codecmap));
        }
        return rv;
    }

    #! Returns the names of all registered codecs
    static list<string> getCodecNames() {
        AutoLock al(lck);
        return sort(keys codecmap);
    }
}
}
