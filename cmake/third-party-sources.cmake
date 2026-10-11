# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# third-party-sources.cmake: the third-party source archives that the build may download
#
# This is the only place where the build names a source archive to download.  An entry is:
#
#   qore_third_party_source(<name> <version> SHA256 <sha256> URL <upstream url>)
#
# "{version}" in the URL stands for the version.  The build verifies the SHA-256 whichever source it uses.
#
# Sources, in order:
#   1. the mirror, when QORE_THIRD_PARTY_MIRROR is set (CI sets it): <mirror>/<name>/<version>/<file name of the URL>
#   2. the upstream URL
#
# The mirror is the generic package registry of mirror/qore on git.qoretechnologies.com (internal; CI jobs read it
# with their job token):
#   https://git.qoretechnologies.com/api/v4/projects/mirror%2Fqore/packages/generic/<name>/<version>/<file>
#
# To add an archive or bump its version:
#   1. download the upstream archive and verify it against upstream's published checksum or signature, where
#      upstream publishes one
#   2. set its version and SHA-256 here (sha256sum <file>, or shasum -a 256 <file> on macOS)
#   3. upload exactly that file to the mirror (the token needs the api scope and the Developer role on mirror/qore):
#        curl --fail-with-body -H "PRIVATE-TOKEN: <token>" --upload-file <file> \
#            "https://git.qoretechnologies.com/api/v4/projects/mirror%2Fqore/packages/generic/<name>/<version>/<file>"
#   4. take the source from the manifest where the build fetches it:
#        qore_third_party_source_args(<name> <var>)
#        FetchContent_Declare(<name> ${<var>} ...)    # or ExternalProject_Add(<name> ${<var>} ...)
#   5. check it:
#        QORE_THIRD_PARTY_MIRROR_TOKEN=<token> cmake \
#            -DQORE_THIRD_PARTY_MIRROR=https://git.qoretechnologies.com/api/v4/projects/mirror%2Fqore/packages/generic \
#            -P cmake/QoreCheckThirdPartySources.cmake
#
# The check fails when a download in the build does not take its source from this manifest, when an entry is not
# used by the build and, with a mirror, when an entry is missing from the mirror or differs from its SHA-256.  CI runs
# it with the mirror in the check-third-party-sources job; examples/test/qore/misc/ThirdPartySources.qtest runs it
# without one.

# c-ares: DNS resolution, when the system has no c-ares >= this version
qore_third_party_source(c-ares 1.34.8
    SHA256 c222b6d681096f9444d2c4863d2c1174019e27cacca0a4a5c114d36dd7d7bf78
    URL "https://github.com/c-ares/c-ares/releases/download/v{version}/c-ares-{version}.tar.gz")

# nghttp2: HTTP/2, when the system has no nghttp2 >= this version
qore_third_party_source(nghttp2 1.70.0
    SHA256 e05cb1388eaca3830aded4ccf20044b6e1ac1a61411dcca11b0437c4285c8bc2
    URL "https://github.com/nghttp2/nghttp2/releases/download/v{version}/nghttp2-{version}.tar.xz")

# ngtcp2 and nghttp3: QUIC and HTTP/3, when the system has no ngtcp2 and nghttp3 >= these versions
qore_third_party_source(ngtcp2 1.25.0
    SHA256 2a34d2484ba17847a5d11965704e9dd0fac4c6d8efc75ffe1ec7de66d8c6b6fb
    URL "https://github.com/ngtcp2/ngtcp2/releases/download/v{version}/ngtcp2-{version}.tar.xz")
qore_third_party_source(nghttp3 1.18.0
    SHA256 aad782c23d3f01bd4bb52c8bac7a553b631ef8115fd1612703df6183449fef19
    URL "https://github.com/ngtcp2/nghttp3/releases/download/v{version}/nghttp3-{version}.tar.xz")

# jemalloc: the allocator linked into libqore on musl targets
qore_third_party_source(jemalloc 5.4.0
    SHA256 200776fac271093e7c2f21edd6d62657ecd2be578d9328633f2a86bfa6ef4f1d
    URL "https://github.com/jemalloc/jemalloc/releases/download/{version}/jemalloc-{version}.tar.bz2")

# tree-sitter: the parser runtime of the astparser module, when the system has no tree-sitter >= this version; upstream
# publishes no source archive, so this is GitHub's archive of the release tag
qore_third_party_source(tree-sitter 0.26.13
    SHA256 ece24c3c5e2a76384075e830c7139b59fce8fb01e4ef8436fab08bbe10444c89
    URL "https://github.com/tree-sitter/tree-sitter/archive/refs/tags/v{version}.tar.gz")

# Apache Arrow: Arrow and Parquet for the dataframe module, when they are not installed; Arrow's own build may
# download further dependencies that the system does not provide
qore_third_party_source(arrow 25.0.1
    SHA256 43d5de0a581f43cf63a2c06b4dcf13b9ff6fcd800f023324596e5781093bc500
    URL "https://archive.apache.org/dist/arrow/arrow-{version}/apache-arrow-{version}.tar.gz")
