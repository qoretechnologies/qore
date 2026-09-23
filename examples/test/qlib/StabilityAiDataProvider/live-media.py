#!/usr/bin/env python3
"""Explicit one-submission live runner with persistent credit reservations.

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
Source ~/.profile.d/stabilityai.sh privately before invoking; never pass a key on argv.
The named stabilityai connection must match that credential. A failed explicit lookup is fatal.
"""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid

PLAN = {
    'generate-image': (3, '/v2beta/stable-image/generate/core', 'core'),
    'media-facade': (3, '/v2beta/stable-image/generate/core', 'core'),
    'remove-background': (5, '/v2beta/stable-image/edit/remove-background', None),
    'upscale-fast': (2, '/v2beta/stable-image/upscale/fast', None),
    'start-background-relight': (8, '/v2beta/stable-image/edit/replace-background-and-relight', None),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=[*PLAN, 'balance', 'wait-for-image'])
    parser.add_argument('--run-live', action='store_true', help='explicitly enable this single live operation')
    parser.add_argument('--job-id', help='existing job for a free wait; never starts another job')
    parser.add_argument('--retry-after-reconcile', action='store_true',
                        help='allow a new reserved attempt only after a prior accepted output was demonstrably lost locally')
    parser.add_argument('--ledger', type=Path, default=Path.home() / '.local/state/qore-5422/credits.json')
    parser.add_argument('--output', type=Path, default=Path('/tmp/qore-5422-live-media'))
    args = parser.parse_args()
    if not args.run_live:
        parser.error('--run-live is required; offline and execution-mode suites must not call this runner')
    root = Path(__file__).resolve().parents[4]
    key = os.environ.get('STABILITYAI_APIKEY')
    if not key:
        parser.error('privately source the intended profile in the child shell first')
    query = subprocess.run(['qrest', '-j', 'connections/stabilityai?with_password=true'], capture_output=True,
                           text=True, timeout=30, check=False)
    if query.returncode:
        raise RuntimeError('explicit named connection lookup failed')
    config = json.loads(query.stdout)
    if config.get('name') != 'stabilityai' or config.get('type') != 'stability-ai' or \
            config['opts'].get('apikey', config['opts'].get('token')) != key:
        raise RuntimeError('named connection and private profile do not match')
    env = dict(os.environ, QORE_STABILITYAI_LIVE_MEDIA='1', QORE_MODULE_DIR_ONLY='1',
               QORE_STABILITYAI_LIVE_ORGANIZATION=config['opts'].get('organization', ''),
               QORE_MODULE_DIR=':'.join(str(root / path) for path in
                   ['build/qlib-qmod', 'build/modules/json', 'build/modules/i18n', 'build/modules/logger_bin',
                    'build/modules/reflection', '/usr/lib/x86_64-linux-gnu/qore-modules']),
               LD_LIBRARY_PATH=str(root / 'build'))
    args.output.mkdir(mode=0o700, parents=True, exist_ok=True)

    def run(action, reservation=None):
        local_env = dict(env, QORE_STABILITYAI_RESERVATION=reservation or '')
        command = [str(root / 'build/qore'), '-penable-debug', str(Path(__file__).with_name('LiveMedia.q')),
                   action, str(args.output)]
        if args.job_id:
            command.append(args.job_id)
        response = subprocess.run(command, env=local_env, cwd=root, capture_output=True, text=True, timeout=360)
        safe_output = (response.stdout + response.stderr).replace(key, '<redacted>')
        (args.output / (action + '.log')).write_text(safe_output)
        if response.returncode:
            raise RuntimeError('Qore operation failed; reservation retained. Read the redacted local log.')
        return json.loads(response.stdout)

    def now():
        return datetime.datetime.now(datetime.timezone.utc).isoformat()

    with args.ledger.with_suffix('.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        ledger = json.loads(args.ledger.read_text())

        def save():
            temporary = args.ledger.with_suffix('.new')
            with temporary.open('w') as out:
                os.chmod(temporary, 0o600)
                out.write(json.dumps(ledger, indent=2) + '\n')
                out.flush()
                os.fsync(out.fileno())
            temporary.replace(args.ledger)

        def observe():
            balance = run('balance')['credits']
            ledger['balance_observations'].append({'utc': now(), 'credits': balance, 'source': 'Qore typed get-balance'})
            save()
            return balance

        balance = observe()
        if args.action == 'balance':
            print(json.dumps({'credits': balance, 'reserved': ledger['reserved_credits']}))
            return
        if args.action == 'wait-for-image':
            submission = next((entry for entry in ledger['submissions'] if entry.get('result', {}).get('job_id') == args.job_id), None)
            if not submission:
                raise RuntimeError('job is not owned by this ledger')
            submission['completion'] = run(args.action, submission['reservation'])
            save()
            print(json.dumps(submission['completion']))
            return
        previous = [entry for entry in ledger['submissions'] if entry['action'] == args.action]
        if previous and not (args.retry_after_reconcile and previous[-1]['state'] == 'accepted-output-lost'):
            raise RuntimeError('this action was already reserved or submitted; reconcile it instead of retrying')
        cost, endpoint, model = PLAN[args.action]
        if ledger['attributed_estimated_spend'] + ledger['reserved_credits'] + cost > min(50, ledger['starting_balance']) \
                or cost + ledger['reserved_credits'] > balance:
            raise RuntimeError('insufficient unreserved task credits')
        reservation = uuid.uuid4().hex
        entry = {'reservation': reservation, 'utc': now(), 'action': args.action, 'method': 'POST', 'endpoint': endpoint,
                 'model': model, 'reserved_maximum': cost, 'state': 'reserved-before-submission',
                 'pricing_source': 'https://platform.stability.ai/pricing', 'balance_before': balance}
        ledger['submissions'].append(entry)
        ledger['reserved_credits'] += cost
        save()
        entry['result'] = run(args.action, reservation)
        entry['state'] = 'accepted'
        ledger['reserved_credits'] -= cost
        ledger['attributed_estimated_spend'] += cost
        artifact = args.output / (args.action + '.png')
        if artifact.exists():
            entry['output_sha256'] = hashlib.sha256(artifact.read_bytes()).hexdigest()
        source = args.output / ('upscale-input.png' if args.action == 'upscale-fast' else 'generate-image.png')
        if args.action in ('remove-background', 'upscale-fast', 'start-background-relight'):
            entry['input_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
        save()
        entry['balance_after'] = observe()
        entry['measured_balance_delta'] = entry['balance_before'] - entry['balance_after']
        save()
        print(json.dumps(entry))


if __name__ == '__main__':
    main()
