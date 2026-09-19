#!/usr/bin/env python3
"""Run one opt-in BFL operation after a persistent, locked credit reservation.

Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
Privately source ~/.profile.d/bfl.sh in a child shell. Never put credentials on argv.
Offline/mode suites do not invoke this helper. Failed submissions stay reserved;
there is intentionally no automatic retry or budget-increase option.
"""
import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import subprocess
import uuid

PLAN = {
    'generate-image': {'maximum': 4, 'model': 'flux-pro-1.1', 'size': '512x512', 'seed': 0},
    'image-facade': {'maximum': 3, 'model': 'flux-2-klein-4b', 'size': '512x512', 'seed': 0},
    'edit-image': {'maximum': 5, 'model': 'flux-2-klein-4b', 'size': '512x512', 'references': 1, 'seed': 0},
    'video-facade': {'maximum': 30, 'model': 'flux-3-video', 'mode': 't2v', 'duration': 5,
                     'resolution': 'hd', 'draft': True, 'generate_audio': False},
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=[*PLAN, 'balance', 'inventory'])
    parser.add_argument('--run-live', action='store_true')
    parser.add_argument('--retry-after-preflight-reconcile', action='store_true',
                        help='allow a new attempt only after manual proof that the previous request never reached transport')
    parser.add_argument('--output', type=Path, default=Path('/var/tmp/qore-5423-live-media'))
    args = parser.parse_args()
    if not args.run_live or not os.environ.get('BFL_APIKEY'):
        parser.error('--run-live and a privately sourced BFL_APIKEY are required')
    os.umask(0o077)
    repo = Path(__file__).resolve().parents[4]
    query = subprocess.run(['qrest', '-j', 'connections/blackforestlabs?with_password=true'],
                           capture_output=True, text=True, timeout=30)
    if query.returncode:
        raise RuntimeError('explicit named connection lookup failed')
    config = json.loads(query.stdout)
    if config.get('name') != 'blackforestlabs' or config.get('type') != 'bfl' or \
            config.get('opts', {}).get('apikey') != os.environ['BFL_APIKEY']:
        raise RuntimeError('named connection does not match the privately selected credential')
    build = repo/'build'
    env = dict(os.environ, QORE_BFL_LIVE_MEDIA='1', QORE_BFL_LIVE_URL=config['url'],
               QORE_MODULE_DIR_ONLY='1', LD_LIBRARY_PATH=str(build),
               QORE_MODULE_DIR=':'.join([str(build/'qlib-qmod'),
                   *sorted({str(p.parent) for p in (build/'modules').rglob('*.qmod')}),
                   '/usr/lib/x86_64-linux-gnu/qore-modules']))
    args.output.mkdir(mode=0o700, parents=True, exist_ok=True)
    state = Path.home()/'.local/state/qore-5423'
    state.mkdir(mode=0o700, parents=True, exist_ok=True)
    path = state/'credits.json'

    def now(): return datetime.datetime.now(datetime.timezone.utc).isoformat()

    def run(action, reservation=''):
        result = subprocess.run([str(build/'qore'), '-penable-debug', str(Path(__file__).with_name('LiveMedia.q')),
                                 action, str(args.output)], cwd=repo,
                                env=dict(env, QORE_BFL_RESERVATION=reservation),
                                capture_output=True, text=True, timeout=1000)
        # Do not emit raw runtime diagnostics, URLs, or credential-bearing response bodies.
        try: response = json.loads(result.stdout)
        except (ValueError, TypeError):
            raise RuntimeError('Qore helper failed before safe evidence; keep the reservation and inspect privately') from None
        if result.returncode:
            raise RuntimeError('Qore helper failed; reservation retained: '+str(response.get('error')))
        return response

    with (state/'credits.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        ledger = json.loads(path.read_text()) if path.exists() else {
            'budget_credits': 50, 'starting_balance': None, 'reserved_credits': 0,
            'attributed_estimated_spend': 0, 'balance_observations': [], 'submissions': []}

        def save():
            temporary = path.with_suffix('.new')
            with temporary.open('w') as out:
                json.dump(ledger, out, indent=2); out.write('\n'); out.flush(); os.fsync(out.fileno())
            temporary.replace(path)

        def observe():
            balance = run('balance')['credits']
            ledger['balance_observations'].append({'utc': now(), 'credits': balance, 'source': 'Qore typed get-credits'})
            if ledger['starting_balance'] is None: ledger['starting_balance'] = balance
            save()
            return balance

        balance = observe()
        if args.action == 'balance':
            print(json.dumps({'credits': balance, 'reserved': ledger['reserved_credits']})); return
        if args.action == 'inventory': print(json.dumps(run('inventory'))); return
        previous = [entry for entry in ledger['submissions'] if entry['action'] == args.action]
        if previous and not (args.retry_after_preflight_reconcile
                             and previous[-1]['state'] == 'preflight-rejected-no-submission'):
            raise RuntimeError('already reserved/submitted; reconcile the original operation instead of retrying')
        plan = PLAN[args.action]
        maximum = plan['maximum']
        if ledger['attributed_estimated_spend'] + ledger['reserved_credits'] + maximum > min(50, ledger['starting_balance']) \
                or ledger['reserved_credits'] + maximum > balance:
            raise RuntimeError('insufficient unreserved credits within the 50-credit task ceiling')
        reservation = uuid.uuid4().hex
        entry = {'action': args.action, 'reservation': reservation, 'utc': now(), 'options': plan,
                 'state': 'reserved-before-submission', 'reserved_maximum': maximum, 'balance_before': balance,
                 'pricing_source': 'https://docs.bfl.ai/quick_start/pricing'}
        ledger['submissions'].append(entry); ledger['reserved_credits'] += maximum; save()
        entry['result'] = run(args.action, reservation)
        entry['state'] = 'completed'
        ledger['reserved_credits'] -= maximum
        # Keep the conservative maximum charged against the cap even if the service reports less.
        ledger['attributed_estimated_spend'] += maximum
        entry['settled_service_cost'] = entry['result'].get('cost')
        save()
        entry['balance_after'] = observe()
        entry['measured_balance_delta'] = entry['balance_before'] - entry['balance_after']
        save()
        print(json.dumps(entry))


if __name__ == '__main__':
    main()
