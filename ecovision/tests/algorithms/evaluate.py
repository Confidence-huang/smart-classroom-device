"""Deterministic SYNTHETIC data only. Runs the actual C core, not a Python clone.
The separate filter experiment is an exploratory comparison, not validation of
real sensor noise or a proposal to alter the frozen AUTO policy.
"""
import csv
import hashlib
import io
import json
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tests/algorithms/evidence'
EXE = ROOT / 'tests/algorithms/build/replay.exe'


def row(t, temp=2900, count=1, tv=1, vv=1, mode=0, fan=-1, led=-1):
    t &= 0xffffffff
    return [t, t, temp, tv, t, 5000, 1, t, 15000, 1, t, count, vv,
            mode, fan, led, 1, 1]


def replay(rows):
    data = '\n'.join(','.join(map(str, r)) for r in rows) + '\n'
    out = subprocess.run([str(EXE)], input=data, text=True, capture_output=True, check=True).stdout
    again = subprocess.run([str(EXE)], input=data, text=True, capture_output=True, check=True).stdout
    assert out == again, 'nondeterministic replay'
    return data, out, [{k: int(v) for k, v in x.items()} for x in csv.DictReader(io.StringIO(out))]


def transitions(values):
    return sum(a != b for a, b in zip(values, values[1:]))


def baseline(rows):
    """Independent original 1.0.0 AUTO oracle for VALID, fresh, 1 s traces.
    Restricted deliberately: no claim it models faults or command arbitration.
    """
    positive = 0
    occ = 0
    zero = dwell = target = None
    fan = led = 0
    outputs = []
    for r in rows:
        now, temp, count = r[0], r[2], r[11]
        if count > 0:
            zero = None
            positive += 1
            if positive >= 3:
                occ = 1
        else:
            positive = 0
            if zero is None:
                zero = now
            if now - zero >= 30000:
                occ = 2
        if occ == 2:
            fan = led = 0
            dwell = target = None
        elif occ == 1:
            led = 1
            demand = 1 if temp >= 2800 else 0 if temp <= 2700 else None
            if demand is None or demand == fan:
                dwell = target = None
            else:
                if target != demand:
                    target, dwell = demand, now
                if now - dwell >= 5000:
                    fan = demand
                    dwell = target = None
        outputs.append((fan, led, occ))
    return outputs


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'data_source': 'SYNTHETIC; no hardware logs available', 'scenarios': {}}
    scenarios = {
        'stable': [row(t, 2750 + (10 if t % 2000 else -10)) for t in range(0, 121000, 1000)],
        'step': [row(t, 2600 if t < 20000 else 2900 if t < 50000 else 2600)
                 for t in range(0, 81000, 1000)],
        'isolated_spike': [row(t, 3500 if t == 20000 else 2600) for t in range(0, 61000, 1000)],
        'vacancy': [row(t, count=1 if t < 20000 else 0) for t in range(0, 61000, 1000)],
        'fault_recover': [row(t, tv=0 if 20000 <= t < 90000 else 1)
                          for t in range(0, 111000, 1000)],
        'dropout': [row(t, vv=-1 if 20000 <= t < 90000 else 1)
                    for t in range(0, 111000, 1000)],
        'manual': [row(t, mode=1 if 20000 <= t < 40000 else 2 if 40000 <= t < 50000 else 0,
                       fan=0 if 20000 <= t < 40000 else -1,
                       led=1 if 20000 <= t < 40000 else -1) for t in range(0, 61000, 1000)],
        'wrap': [row(0xfffff000 + t) for t in range(0, 21000, 1000)],
    }
    for name, rows in scenarios.items():
        data, out, trace = replay(rows)
        (OUT / f'{name}.input.csv').write_text(data, encoding='utf-8')
        (OUT / f'{name}.output.csv').write_text(out, encoding='utf-8')
        item = {'rows': len(rows), 'fan_actions': transitions([x['fan'] for x in trace]),
                'led_actions': transitions([x['led'] for x in trace]),
                'deterministic_sha256': hashlib.sha256(out.encode()).hexdigest()}
        if name in ('stable', 'step', 'isolated_spike', 'vacancy'):
            expected = baseline(rows)
            mismatches = sum((x['fan'], x['led'], x['occupancy']) != e for x, e in zip(trace, expected))
            assert mismatches == 0, (name, mismatches)
            item['original_policy_mismatches'] = mismatches
            item['original_fan_actions'] = transitions([e[0] for e in expected])
        if name in ('stable', 'isolated_spike'):
            assert item['fan_actions'] == 0
            item['unwanted_fan_actions'] = 0
        if name == 'step':
            on = next(x['now_ms'] for x in trace if x['fan'])
            off = next(x['now_ms'] for x in trace if x['now_ms'] >= 50000 and not x['fan'])
            item.update(on_response_ms=on-20000, off_response_ms=off-50000)
            assert on == 25000 and off == 55000
        if name == 'fault_recover':
            assert trace[79]['fan'] == 1 and trace[80]['fan'] == 0 and trace[95]['fan'] == 1
        if name == 'dropout':
            assert trace[24]['occupancy'] == 0 and trace[84]['fan'] == 0
            assert trace[92]['occupancy'] == 1
        if name == 'manual':
            assert all(x['apply_mask'] == 0 for x in trace[20:40])
            assert all(x['fan'] == x['led'] == 0 for x in trace[40:50])
        report['scenarios'][name] = item
    # Same policy at 200 ms, 1 s, 2 s and irregular intervals; no real sleeps.
    report['sampling'] = {}
    for dt in (200, 1000, 2000):
        _, _, trace = replay([row(t) for t in range(0, 30000, dt)])
        first = next(x['now_ms'] for x in trace if x['fan'])
        report['sampling'][str(dt)] = {'occupied_at_ms': 2*dt, 'fan_at_ms': first}
        assert 5000 <= first-2*dt < 5000+dt
    irregular = [0, 700, 1900, 3300, 4700, 6100, 7200, 9000]
    _, _, trace = replay([row(t) for t in irregular])
    assert next(x['now_ms'] for x in trace if x['fan']) == 7200
    report['sampling']['irregular'] = {'fan_at_ms': 7200}
    # Fixed analytic noise, one spike, known step; units 0.01 C, dt=1 s.
    truth = [2600]*30 + [2900]*31
    noise = [0, 10, -10, 5, -5]
    raw = [v+noise[i % 5] for i, v in enumerate(truth)]
    raw[15] += 800
    methods = {'raw': [], 'median3': [], 'mean3': [], 'lowpass_alpha_025': []}
    for i, v in enumerate(raw):
        window = raw[max(0, i-2):i+1]
        methods['raw'].append(v)
        methods['median3'].append(statistics.median(window) if len(window)==3 else v)
        methods['mean3'].append(sum(window)/len(window))
        prev = methods['lowpass_alpha_025'][-1] if i else v
        methods['lowpass_alpha_025'].append(prev+0.25*(v-prev))
    _, _, ctrace = replay([row(i*1000, v) for i,v in enumerate(raw)])
    assert [x['filtered0'] for x in ctrace] == methods['median3']
    envrows = [row(i*1000) for i in range(61)]
    for i,r in enumerate(envrows):
        r[5] = 5000 if i < 30 else 6000
        r[8] = 15000 if i < 30 else 45000
        if i == 15:
            r[5],r[8] = 9000,60000
    data,out,envtrace = replay(envrows)
    assert envtrace[15]['filtered1']==5000 and envtrace[15]['filtered2']==15000
    assert envtrace[31]['filtered1']==6000 and envtrace[31]['filtered2']==45000
    assert all(x['light_need']==0 for x in envtrace)
    (OUT/'environment_filter.input.csv').write_text(data,encoding='utf-8')
    (OUT/'environment_filter.output.csv').write_text(out,encoding='utf-8')
    report['environment_filter']={'isolated_spikes_removed':True,'step_delay_ms':1000,
                                  'light_need':'UNKNOWN (uncalibrated)'}
    report['filter_comparison'] = {}
    for name, values in methods.items():
        report['filter_comparison'][name] = {
            'MAE_c': round(sum(abs(v-t) for v,t in zip(values,truth))/len(truth)/100,4),
            'spike_peak_error_c': round(max(abs(values[i]-truth[i]) for i in range(15,19))/100,4),
            'step_90pct_delay_ms': next(i for i in range(30,len(values)) if values[i] >= 2870)*1000-30000,
        }
    (OUT/'metrics.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    buf=io.StringIO(); writer=csv.writer(buf);writer.writerow(['sample_s','truth_c100',*methods])
    writer.writerows([i,truth[i],*[methods[n][i] for n in methods]] for i in range(len(truth)))
    (OUT/'filter_comparison.csv').write_text(buf.getvalue(),encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
