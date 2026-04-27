#!/usr/bin/env bash
# Copyright (C) 2026 Qore Technologies, s.r.o.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/../../.." && pwd)"
build_dir="${BUILD_DIR:-$repo_dir/build-debug}"
qore_bin="${QORE_BIN:-$build_dir/qore}"
if [[ ! -x "$qore_bin" && -x "$repo_dir/build/qore" ]]; then
    build_dir="$repo_dir/build"
    qore_bin="$build_dir/qore"
fi
if [[ ! -x "$qore_bin" ]]; then
    qore_bin="$(command -v qore || true)"
fi
if [[ -z "$qore_bin" || ! -x "$qore_bin" ]]; then
    echo "skipping auth-proxy integration test: qore binary not found"
    exit 0
fi

engine="${CONTAINER_ENGINE:-}"
if [[ -z "$engine" ]]; then
    if command -v podman >/dev/null 2>&1; then
        engine=podman
    elif command -v docker >/dev/null 2>&1; then
        engine=docker
    fi
fi
if [[ -z "$engine" ]]; then
    echo "skipping auth-proxy integration test: podman/docker not available"
    exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "skipping auth-proxy integration test: python3 not available"
    exit 0
fi
image="${QORE_KRB5_AUTH_PROXY_IMAGE:-docker.io/liggitt/auth-proxy:latest}"
container_name="qore-krb5-auth-proxy-$$"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/qore-krb5-auth-proxy.XXXXXX")"
backend_log="$tmpdir/backend-requests.jsonl"
backend_script="$tmpdir/backend.py"
krb5_conf="$tmpdir/krb5.conf"
proxy_log="$tmpdir/auth-proxy.log"

cleanup() {
    set +e
    if [[ -n "${container_name:-}" ]]; then
        "$engine" rm -f "$container_name" >/dev/null 2>&1
    fi
    if [[ -n "${PROXY_PID:-}" ]]; then
        kill "$PROXY_PID" >/dev/null 2>&1
        wait "$PROXY_PID" >/dev/null 2>&1
    fi
    if [[ -n "${BACKEND_PID:-}" ]]; then
        kill "$BACKEND_PID" >/dev/null 2>&1
        wait "$BACKEND_PID" >/dev/null 2>&1
    fi
    rm -rf "$tmpdir"
}
trap cleanup EXIT

cat >"$backend_script" <<'PY'
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import sys

log_path = sys.argv[1]

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        record = {
            "path": self.path,
            "remote_user": self.headers.get("Remote-User"),
            "authorization": self.headers.get("Authorization"),
        }
        with open(log_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(record, sort_keys=True) + "\n")
            f.flush()

        body = json.dumps(record, sort_keys=True).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        sys.stderr.write((fmt % args) + "\n")

server = ThreadingHTTPServer(("0.0.0.0", 0), Handler)
print(server.server_address[1], flush=True)
server.serve_forever()
PY

coproc BACKEND { python3 -u "$backend_script" "$backend_log"; }
if ! read -r backend_port <&"${BACKEND[0]}"; then
    echo "failed to start auth-proxy backend" >&2
    exit 1
fi

backend_host=host.containers.internal
container_args=()
case "$(basename "$engine")" in
    docker)
        backend_host=host.docker.internal
        container_args+=(--add-host=host.docker.internal:host-gateway)
        ;;
esac

"$engine" pull "$image"
coproc PROXY { "$engine" run --name "$container_name" -h auth.example.com \
    -e KRB5CCNAME=FILE:/tmp/krb5cc_proxy \
    -e BACKEND="http://$backend_host:$backend_port" \
    "${container_args[@]}" \
    -p 127.0.0.1::80 -p 127.0.0.1::88 "$image" 2>&1; }

start_timeout="${QORE_KRB5_AUTH_PROXY_START_TIMEOUT:-120}"
ready=False
end=$((SECONDS + start_timeout))
while (( SECONDS < end )); do
    remaining=$((end - SECONDS))
    if IFS= read -r -t "$remaining" line <&"${PROXY[0]}"; then
        printf "%s\n" "$line" | tee -a "$proxy_log" >&2
        if [[ "$line" == *"Go loop."* ]]; then
            ready=True
            break
        fi
    else
        break
    fi
done
if [[ "$ready" != True ]]; then
    echo "auth-proxy container did not become ready" >&2
    if [[ -s "$proxy_log" ]]; then
        cat "$proxy_log" >&2
    fi
    exit 1
fi

http_port="$("$engine" port "$container_name" 80/tcp | head -n 1)"
http_port="${http_port##*:}"
kdc_port="$("$engine" port "$container_name" 88/tcp | head -n 1)"
kdc_port="${kdc_port##*:}"
if [[ -z "$http_port" || -z "$kdc_port" ]]; then
    echo "failed to determine auth-proxy mapped ports" >&2
    "$engine" port "$container_name" >&2 || true
    exit 1
fi

cat >"$krb5_conf" <<EOF
[libdefaults]
    default_realm = AUTH.EXAMPLE.COM
    dns_lookup_kdc = false
    dns_lookup_realm = false
    rdns = false
    udp_preference_limit = 1

[realms]
    AUTH.EXAMPLE.COM = {
        kdc = 127.0.0.1:$kdc_port
        admin_server = 127.0.0.1:$kdc_port
        default_domain = auth.example.com
    }

[domain_realm]
    .auth.example.com = AUTH.EXAMPLE.COM
    auth.example.com = AUTH.EXAMPLE.COM
EOF

export LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}"
export QORE_MODULE_DIR="$repo_dir/qlib:$build_dir/modules/krb5:$build_dir/modules/logger_bin:${QORE_MODULE_DIR:-}"
export KRB5_CONFIG="$krb5_conf"
export KRB5_TRACE="$tmpdir/krb5.trace"
export QORE_KRB5_AUTH_PROXY_URL="http://127.0.0.1:$http_port/mod_auth_gssapi"
export QORE_KRB5_AUTH_PROXY_SERVICE_PRINCIPAL="HTTP/auth.example.com@AUTH.EXAMPLE.COM"
export QORE_KRB5_AUTH_PROXY_CLIENT_PRINCIPAL="user1@AUTH.EXAMPLE.COM"
export QORE_KRB5_AUTH_PROXY_CLIENT_PASSWORD="password"
export QORE_KRB5_AUTH_PROXY_CREDENTIAL_CACHE="FILE:$tmpdir/client.ccache"
export QORE_KRB5_AUTH_PROXY_BACKEND_LOG="$backend_log"

"$qore_bin" --enable-debug "$script_dir/restclientio-auth-proxy.qtest" -v
