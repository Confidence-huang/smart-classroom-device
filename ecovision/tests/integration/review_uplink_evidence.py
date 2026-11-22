"""Read-only checks of real captured MQTT/API evidence, not a hardware simulator."""
import argparse
from collections import defaultdict
from datetime import datetime
import importlib.util
import json
from pathlib import Path


def main():
    args = argparse.ArgumentParser()
    args.add_argument('--evidence', type=Path, required=True)
    args.add_argument('--report', type=Path, required=True)
    options = args.parse_args()
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('contract_checks', root / 'tests/integration/test_contracts.py')
    checks = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checks)
    schema = json.loads((root / 'contracts/telemetry.schema.json').read_text(encoding='utf-8'))
    captured = json.loads((options.evidence / 'subscriber.json').read_text(encoding='utf-8'))
    formal = [m for m in captured['messages'] if m['topic'].endswith('/telemetry')
              and m['payload'].get('v') == '1.0.0']
    legacy = [m['payload'] for m in captured['messages']
              if m['payload'].get('source') == 'stm32_c_hal_ec200u']
    assert formal and legacy, 'Missing live official or compatibility telemetry'
    groups = defaultdict(list)
    matched = 0
    for message in formal:
        payload = message['payload']
        checks.check_shape(payload, schema)
        assert message['retain'] is False, 'Official telemetry must not be retained'
        assert payload['time_valid'] and payload['ts'] is not None
        assert payload['net_state'] == 'ONLINE'
        if not payload['vision_valid']:
            assert payload['count'] is None
        if not payload['temp_valid']:
            assert payload['temp_c'] is None
        groups[payload['boot_id']].append(payload['seq'])
        candidates = [p for p in legacy if p['device_id'] == payload['device_id']
                      and p['seq'] == payload['ts'] - 946684800]
        for item in candidates:
            assert item['simulated'] is False and item['people_count'] == payload['count']
            assert abs(item['temperature_c'] - payload['temp_c']) < 0.0001
            assert abs(item['humidity_pct'] - payload['humidity_pct']) < 0.0001
            stamp = int(datetime.fromisoformat(item['captured_at'].replace('Z', '+00:00')).timestamp())
            assert stamp == payload['ts']
            matched += 1
            break
    for values in groups.values():
        assert all(0 <= ((b-a) & 0xffffffff) < 0x80000000 for a, b in zip(values, values[1:])), 'Out-of-order samples'
    api = json.loads((options.evidence / 'api-final.json').read_text(encoding='utf-8'))
    api_rows = api.get('history', [])
    api_matches = sum(any(row['device_id'] == item['device_id'] and row['seq'] == item['seq']
                         and row['captured_at'] == item['captured_at']
                         and row['temperature_c'] == item['temperature_c']
                         and row['humidity_pct'] == item['humidity_pct'] for row in api_rows) for item in legacy)
    assert matched and api_matches, 'Missing end-to-end sample correlation'
    reset = json.loads((options.evidence / 'reset-upload-result.json').read_text(encoding='utf-8'))
    assert reset['initial_boot_upload_ok'] and reset['mcu_reset_upload_recovered']
    result = {'formal_schema_valid_messages': len(formal), 'official_retain_false': True,
              'boot_sessions_observed': len(groups), 'formal_legacy_correlations': matched,
              'legacy_api_correlations': api_matches, 'mcu_reset_upload_recovered': True,
              'observer_published_messages': False, 'software_replay_of_real_evidence': True,
              'one_hour_acceptance_complete': False, 'eight_hour_acceptance_complete': False,
              'remote_command_ack_verified': False}
    options.report.parent.mkdir(parents=True, exist_ok=True)
    options.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
