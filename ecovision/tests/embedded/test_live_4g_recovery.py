"""Explicit bench-only EC200U fault injection. Importing never touches hardware."""
import argparse
import json
import re
import threading
import time
from pathlib import Path


def summarize(records):
    recovery = next(x['elapsed_s'] for x in records if x['kind'] == 'RECOVERY_START')
    acks = [x for x in records if x['elapsed_s'] >= recovery and x['kind'] == 'PUBACK']
    result = dict(first_puback_after_restore_s=round(acks[0]['elapsed_s']-recovery, 3) if acks else None,
                  recovery_pubacks=len(acks), phases={}, maximum_sample_gap_s={},
                  unexpected_boot_banners=sum(x['kind'] == 'BOOT' for x in records),
                  health_stalls=sum(x['kind'] == 'NET' and '[HEALTH]' in x.get('text', '')
                                   and 'stalled' in x.get('text', '') for x in records))
    for phase in ('BASELINE', 'OUTAGE', 'RECOVERY'):
        part = [x for x in records if x['phase'] == phase]
        result['phases'][phase] = {k: sum(x['kind'] == k for x in part) for k in ('VISION', 'ENV', 'PUBACK')}
    for kind in ('VISION', 'ENV'):
        times = [0, *(x['elapsed_s'] for x in records if x['kind'] == kind), records[-1]['elapsed_s']]
        result['maximum_sample_gap_s'][kind] = round(max(b-a for a, b in zip(times, times[1:])), 3)
    result['passed'] = (len(acks) >= 3 and not result['unexpected_boot_banners'] and not result['health_stalls']
                        and result['maximum_sample_gap_s']['VISION'] < 5
                        and result['maximum_sample_gap_s']['ENV'] < 12
                        and result['phases']['BASELINE']['PUBACK'] >= 2)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-hardware', action='store_true')
    parser.add_argument('--console', required=True)
    parser.add_argument('--modem', required=True)
    parser.add_argument('--case', choices=('rf', 'pdp', 'mqtt'), required=True)
    parser.add_argument('--outage-seconds', type=int, default=15)
    parser.add_argument('--observe-seconds', type=int, default=150)
    parser.add_argument('--output', type=Path, default=Path('tests/embedded/evidence/2026-11-28-4g'))
    args = parser.parse_args()
    if not args.run_hardware:
        parser.error('Fault injection requires --run-hardware')
    if not 1 <= args.outage_seconds <= 600 or not 30 <= args.observe_seconds <= 600:
        parser.error('Outage must be 1..600 seconds; observation 30..600 seconds')
    if args.console.casefold() == args.modem.casefold():
        parser.error('Console and modem must use different ports')
    prefix = f'{args.case}-{args.outage_seconds}s' if args.case == 'rf' else args.case
    destination = args.output/(prefix+'.json')
    if destination.exists():
        parser.error('Evidence exists; select a new --output directory')
    import serial
    args.output.mkdir(parents=True, exist_ok=True)
    records, listener_errors = [], []
    stop = threading.Event()
    started = time.monotonic()
    phase, error = 'BASELINE', None

    def record(kind, **fields):
        records.append(dict(elapsed_s=round(time.monotonic()-started, 3), phase=phase, kind=kind, **fields))

    def listen(port):
        buf = b''
        try:
            while not stop.is_set():
                buf += port.read(port.in_waiting or 1)
                while b'\n' in buf:
                    raw, buf = buf.split(b'\n', 1)
                    line = raw.decode('utf-8', 'replace').strip()
                    if line.startswith('[VISION] RX seq='):
                        match = re.search(r'seq=(\d+).*valid=(\d+)', line)
                        if match: record('VISION', seq=int(match[1]), valid=int(match[2]))
                    elif line.startswith('[ENV]'): record('ENV', text=line[:250])
                    elif line.startswith('[NET] PUBACK seq='): record('PUBACK', text=line[:250])
                    elif line.startswith('EcoVision F413ZH'): record('BOOT', text=line[:250])
                    elif line.startswith(('[NET] state=', '[NET] stage=', '[HEALTH]')):
                        record('NET', text=line[:250]); print(line[:250], flush=True)
                if len(buf) > 8192: raise RuntimeError('Console line exceeds safe capture limit')
        except Exception as exc:
            listener_errors.append(type(exc).__name__)

    def command(port, text, seconds=10):
        port.reset_input_buffer()
        port.write((text+'\r').encode('ascii'))
        buf, until = b'', time.monotonic()+seconds
        while time.monotonic() < until:
            buf += port.read(port.in_waiting or 1)
            if re.search(rb'(?:^|\r?\n)(?:OK|ERROR|\+CME ERROR[^\r\n]*)\r?\n', buf): break
        ok = bool(re.search(rb'(?:^|\r?\n)OK\r?\n', buf))
        # USB responses can contain private addresses; keep only command and final status.
        record('AT', command=text, ok=ok)
        print(text, 'OK' if ok else 'NO_OK', flush=True)
        return ok

    def wait(seconds):
        end = time.monotonic()+seconds
        while time.monotonic() < end: time.sleep(min(0.2, max(0, end-time.monotonic())))

    radio_changed = False
    with serial.Serial(args.console, 115200, timeout=0.2) as console, serial.Serial(args.modem, 115200, timeout=0.2, write_timeout=1) as modem:
        thread = threading.Thread(target=listen, args=(console,), daemon=True)
        thread.start()
        try:
            if not command(modem, 'AT'): raise RuntimeError('Modem not responding')
            wait(15)
            if sum(x['kind'] == 'PUBACK' for x in records) < 2:
                raise RuntimeError('No stable baseline; injection skipped')
            phase = 'OUTAGE'; record('INJECTION_START', case=args.case)
            if args.case == 'rf':
                radio_changed = True
                if not command(modem, 'AT+CFUN=0'): raise RuntimeError('RF-off not confirmed')
                wait(args.outage_seconds)
                if not command(modem, 'AT+CFUN=1'): raise RuntimeError('RF restore not confirmed')
                radio_changed = False
            elif not command(modem, 'AT+QIDEACT=1' if args.case == 'pdp' else 'AT+QMTCLOSE=0', 45):
                raise RuntimeError('Fault injection not confirmed')
            phase = 'RECOVERY'; record('RECOVERY_START')
            remaining = args.observe_seconds
            while remaining:
                interval = min(30, remaining); wait(interval); remaining -= interval
                print('OBSERVE', args.observe_seconds-remaining, 'seconds; records', len(records), flush=True)
        except (Exception, KeyboardInterrupt) as exc:
            error = type(exc).__name__+': '+str(exc)
        finally:
            if radio_changed:
                try:
                    if not command(modem, 'AT+CFUN=1'): error = 'RF restoration needs attention'
                except Exception as exc:
                    error = 'RF restoration failed: '+type(exc).__name__
            stop.set(); thread.join(timeout=2); record('CAPTURE_END')
    result = summarize(records) if any(x['kind'] == 'RECOVERY_START' for x in records) else dict(passed=False)
    result.update(error=error, listener_errors=listener_errors, case=args.case,
                  outage_seconds=args.outage_seconds if args.case == 'rf' else None,
                  observation_seconds=args.observe_seconds)
    result['passed'] = result['passed'] and not error and not listener_errors
    destination.write_text(json.dumps(dict(summary=result, records=records), ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
