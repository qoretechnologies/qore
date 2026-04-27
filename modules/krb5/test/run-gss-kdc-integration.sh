#!/usr/bin/env bash
# Copyright (C) 2026 Qore Technologies, s.r.o.

set -euxo pipefail

module_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_dir="$(cd "$module_dir/../.." && pwd)"
build_dir="${BUILD_DIR:-$repo_dir/build}"
qore_bin="${QORE_BIN:-$build_dir/qore}"
if [[ ! -x "$qore_bin" ]]; then
    qore_bin="$(command -v qore)"
fi
realm="${KRB5_TEST_REALM:-MODULE-KRB5.TEST}"
port="${KRB5_TEST_KDC_PORT:-61088}"
client_principal="alice@$realm"
service_principal="HTTP/localhost@$realm"
client_password="alice-password"
master_password="master-password"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/module-krb5-kdc.XXXXXX")"
pidfile="$tmpdir/krb5kdc.pid"
keytab="$tmpdir/service.keytab"
krb5_conf="$tmpdir/krb5.conf"
kdc_conf="$tmpdir/kdc.conf"

cleanup() {
    if [[ -f "$pidfile" ]]; then
        kill "$(cat "$pidfile")" 2>/dev/null || true
    fi
    rm -rf "$tmpdir"
}
trap cleanup EXIT

cat >"$krb5_conf" <<EOF
[libdefaults]
    default_realm = $realm
    dns_lookup_kdc = false
    dns_lookup_realm = false
    rdns = false
    udp_preference_limit = 1
    default_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
    default_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
    permitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96

[realms]
    $realm = {
        kdc = 127.0.0.1:$port
    }

[domain_realm]
    localhost = $realm
EOF

cat >"$kdc_conf" <<EOF
[kdcdefaults]
    kdc_ports = $port

[realms]
    $realm = {
        database_name = $tmpdir/principal
        acl_file = $tmpdir/kadm5.acl
        key_stash_file = $tmpdir/stash
        kdc_ports = $port
        max_life = 1h
        max_renewable_life = 1h
        supported_enctypes = aes256-cts-hmac-sha1-96:normal aes128-cts-hmac-sha1-96:normal
    }

[logging]
    kdc = FILE:$tmpdir/krb5kdc.log
    admin_server = FILE:$tmpdir/kadmin.log
    default = FILE:$tmpdir/krb5.log
EOF

export KRB5_CONFIG="$krb5_conf"
export KRB5_KDC_PROFILE="$kdc_conf"
export KRB5_KTNAME="FILE:$keytab"
export KRB5_TRACE="$tmpdir/krb5.trace"
export MODULE_KRB5_TEST_REALM="$realm"
export MODULE_KRB5_TEST_CLIENT_PRINCIPAL="$client_principal"
export MODULE_KRB5_TEST_CLIENT_PASSWORD="$client_password"
export MODULE_KRB5_TEST_SERVICE_PRINCIPAL="$service_principal"
export MODULE_KRB5_TEST_SERVICE_KEYTAB="$keytab"
export QORE_MODULE_DIR="$repo_dir/qlib:$build_dir/modules/krb5:$build_dir/modules/logger_bin:${QORE_MODULE_DIR:-}"

kdb5_util create -s -P "$master_password" -r "$realm"
kadmin.local -r "$realm" -q "addprinc -pw $client_password $client_principal"
kadmin.local -r "$realm" -q "modprinc -maxrenewlife 1h +allow_forwardable $client_principal"
kadmin.local -r "$realm" -q "addprinc -randkey $service_principal"
kadmin.local -r "$realm" -q "modprinc +ok_to_auth_as_delegate $service_principal"
kadmin.local -r "$realm" -q "ktadd -k $keytab $service_principal"
krb5kdc -P "$pidfile" -r "$realm" -p "$port"

"$qore_bin" --enable-debug "$module_dir/test/krb5-gss-kdc.qtest" -v
