"""The AOT module metadata record of a qmod (QAMI), as qcc writes it

A shared, bounds-checked parser for the tools that read the record (tools/qore-qmod-cache.py and
tools/check-debian-artifacts.py), so that they read the same versions in the same way (qore#6526).  The record is
written by qcc and read by libqore: include/qore/intern/QoreAOTBinary.h (QoreAOTModuleFileInfo,
QORE_AOT_MODULE_INFO_VERSION) and lib/QoreAOTBinary.cpp (qoreAOTEncodeModuleInfo(), qoreAOTDecodeModuleInfo()).

The record: the magic "QAMI", the u32 format version and the u32 size of the payload (all little-endian), then the
payload.  A string is a u32 length (not 0) followed by that many bytes with no NUL; a list is a u32 count followed by
that many strings.  The payload of each version:
- version 1: the module name, its dependencies, the optional modules that were unavailable when it was compiled;
- version 2: as version 1, then the modules it was compiled with, its %prepend-module-path directories and its
  %append-module-path directories;
- version 3: as version 2, then the parse options that its directives gave its dependencies: a u32 count, then for
  each a module name, four u32 words of options and a u32 word of flags (FLAG_*) (qore#6406, qore#6386); then the
  functional domains that the module declares in its header (a u64 as two u32, low word first), the dependencies
  that it requires only after its header (a list) and the functional domains that its own code restricts (a u64 as
  two u32, low word first) (qore#6351).
"""

import struct

#: the record magic
MAGIC = b"QAMI"
#: the record version that qcc writes (QORE_AOT_MODULE_INFO_VERSION)
CURRENT_VERSION = 3
#: the versions that parse() reads
VERSIONS = (1, 2, 3)
#: the size of the record header
HEADER_SIZE = 12
#: the largest payload accepted (QORE_AOT_MODULE_INFO_MAX_PAYLOAD_SIZE)
MAX_PAYLOAD = 16 * 1024 * 1024
#: the largest number of entries of a list accepted
MAX_COUNT = 65536
#: the number of u32 words of the parse options of a dependency restriction
RESTRICTION_WORDS = 4
#: the flag of a dependency restriction: the module was requested before the header of a module that declares
#: functional domains (QMDR_BEFORE_HEADER)
FLAG_BEFORE_HEADER = 1 << 0
#: the flag of a dependency restriction: the requests before the module header could not be recorded exactly; the
#: loader relieves no dependency of the module (QMDR_INEXACT)
FLAG_INEXACT = 1 << 1
#: the flags that a record can have; any other bit makes it invalid (QMDR_ALL_FLAGS)
ALL_FLAGS = FLAG_BEFORE_HEADER | FLAG_INEXACT


class ModuleInfo:
    """The content of a record

    - version: the record version
    - name: the module name
    - dependencies: the modules that the loader loads before the module
    - optional: the optional modules that were unavailable when the module was compiled
    - compiled_with: the modules that the module was compiled with (version 2 and later; else empty)
    - prepend_paths, append_paths: the module's own module path directories (version 2 and later; else empty)
    - dependency_restrictions: (name, options, flags) for each module that a directive loaded, options being a tuple
      of four u32 words and flags a combination of FLAG_* (version 3 and later; else empty)
    - declared_domains: the functional domains that the module declares in its header (version 3 and later; else 0)
    - post_header_dependencies: the dependencies that the module requires only after its header (version 3 and
      later; else empty)
    - restricted_domains: the functional domains that the module's own code restricts (version 3 and later; else 0)
    """

    def __init__(self, version, name, dependencies, optional, compiled_with=(), prepend_paths=(), append_paths=(),
                 dependency_restrictions=(), declared_domains=0, post_header_dependencies=(), restricted_domains=0):
        self.version = version
        self.name = name
        self.dependencies = list(dependencies)
        self.optional = list(optional)
        self.compiled_with = list(compiled_with)
        self.prepend_paths = list(prepend_paths)
        self.append_paths = list(append_paths)
        self.dependency_restrictions = list(dependency_restrictions)
        self.declared_domains = declared_domains
        self.post_header_dependencies = list(post_header_dependencies)
        self.restricted_domains = restricted_domains


def header(data, offset=0):
    """Returns (version, payload size) of the record header at @p offset, or None if it is not one"""
    if offset < 0 or offset + HEADER_SIZE > len(data):
        return None
    magic, version, size = struct.unpack_from("<4sII", data, offset)
    if magic != MAGIC:
        return None
    return version, size


def parse(data, offset=0, versions=VERSIONS, exact=False):
    """Parses the record at @p offset of @p data; returns a ModuleInfo, or None if it is not a valid record

    @param versions the record versions accepted
    @param exact if True, the record must end where @p data ends (a record extracted on its own)
    """
    hdr = header(data, offset)
    if hdr is None:
        return None
    version, size = hdr
    if version not in versions or version not in VERSIONS or size > MAX_PAYLOAD:
        return None
    pos = offset + HEADER_SIZE
    end = pos + size
    if end > len(data) or (exact and end != len(data)):
        return None

    def read_u32(p):
        if p + 4 > end:
            raise ValueError
        return struct.unpack_from("<I", data, p)[0], p + 4

    def read_str(p):
        length, p = read_u32(p)
        if not length or p + length > end:
            raise ValueError
        s = bytes(data[p:p + length])
        if b"\0" in s:
            raise ValueError
        return s.decode("utf-8"), p + length

    def read_count(p, entry_min):
        count, p = read_u32(p)
        # every entry needs at least entry_min bytes: a damaged count cannot make the loop run far past the data
        if count > MAX_COUNT or count > (end - p) // entry_min:
            raise ValueError
        return count, p

    def read_list(p):
        count, p = read_count(p, 5)
        items = []
        for _ in range(count):
            s, p = read_str(p)
            items.append(s)
        return items, p

    def read_u64(p):
        lo, p = read_u32(p)
        hi, p = read_u32(p)
        return lo | (hi << 32), p

    try:
        cur = pos
        name, cur = read_str(cur)
        deps, cur = read_list(cur)
        optional, cur = read_list(cur)
        compiled_with, prepend, append, restrictions = [], [], [], []
        declared_domains, post_header, restricted_domains = 0, [], 0
        if version >= 2:
            compiled_with, cur = read_list(cur)
            prepend, cur = read_list(cur)
            append, cur = read_list(cur)
        if version >= 3:
            count, cur = read_count(cur, 5 + 4 * RESTRICTION_WORDS + 4)
            for _ in range(count):
                dep, cur = read_str(cur)
                if cur + 4 * RESTRICTION_WORDS + 4 > end:
                    raise ValueError
                words = struct.unpack_from("<%dI" % RESTRICTION_WORDS, data, cur)
                cur += 4 * RESTRICTION_WORDS
                (flags,) = struct.unpack_from("<I", data, cur)
                cur += 4
                if flags & ~ALL_FLAGS:
                    raise ValueError
                restrictions.append((dep, words, flags))
            declared_domains, cur = read_u64(cur)
            post_header, cur = read_list(cur)
            restricted_domains, cur = read_u64(cur)
    except (ValueError, UnicodeDecodeError):
        return None
    if cur != end:
        return None
    return ModuleInfo(version, name, deps, optional, compiled_with, prepend, append, restrictions, declared_domains,
                      post_header, restricted_domains)


def find(data, name=None, versions=VERSIONS):
    """Returns the valid records in @p data (found by their magic), of module @p name if given"""
    found = []
    pos = data.find(MAGIC)
    while pos >= 0:
        rec = parse(data, pos, versions)
        if rec is not None and (name is None or rec.name == name):
            found.append(rec)
        pos = data.find(MAGIC, pos + 1)
    return found


def encode(version, name, dependencies=(), optional=(), compiled_with=(), prepend_paths=(), append_paths=(),
           dependency_restrictions=(), declared_domains=0, post_header_dependencies=(), restricted_domains=0):
    """Encodes a record of @p version (for tests)"""
    def enc_str(s):
        b = s.encode("utf-8")
        return struct.pack("<I", len(b)) + b

    def enc_list(items):
        return struct.pack("<I", len(items)) + b"".join(enc_str(s) for s in items)

    payload = enc_str(name) + enc_list(list(dependencies)) + enc_list(list(optional))
    if version >= 2:
        payload += enc_list(list(compiled_with)) + enc_list(list(prepend_paths)) + enc_list(list(append_paths))
    if version >= 3:
        payload += struct.pack("<I", len(dependency_restrictions))
        for entry in dependency_restrictions:
            dep, words = entry[0], entry[1]
            flags = entry[2] if len(entry) > 2 else 0
            payload += enc_str(dep) + struct.pack("<%dI" % RESTRICTION_WORDS, *words) + struct.pack("<I", flags)
        payload += struct.pack("<II", declared_domains & 0xffffffff, declared_domains >> 32)
        payload += enc_list(list(post_header_dependencies))
        payload += struct.pack("<II", restricted_domains & 0xffffffff, restricted_domains >> 32)
    return MAGIC + struct.pack("<II", version, len(payload)) + payload
