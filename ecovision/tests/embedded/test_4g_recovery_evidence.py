"""Host-only checks of archived bench evidence; never opens serial ports."""
import copy
import hashlib
import json
from pathlib import Path
import unittest

from test_live_4g_recovery import summarize

EVIDENCE = Path(__file__).parent / 'evidence' / '2026-11-28-4g'


class RecoveryEvidenceTests(unittest.TestCase):
    def load(self, name='rf-130s'):
        return json.loads((EVIDENCE / (name+'.json')).read_text(encoding='utf-8'))

    def test_archived_bytes_match_manifest(self):
        manifest = self.load('sha256')
        self.assertEqual(len(manifest), 10)
        for name, expected in manifest.items():
            with self.subTest(file=name):
                self.assertEqual(hashlib.sha256((EVIDENCE/name).read_bytes()).hexdigest(), expected)

    def test_archived_summaries_match_raw_records(self):
        for name in ('pdp', 'mqtt', 'rf-130s'):
            with self.subTest(case=name):
                report = self.load(name)
                actual = summarize(report['records'])
                for key in ('first_puback_after_restore_s', 'recovery_pubacks', 'phases',
                            'maximum_sample_gap_s', 'unexpected_boot_banners', 'passed'):
                    self.assertEqual(actual[key], report['summary'][key])
                self.assertEqual(actual['health_stalls'], 0)
                self.assertIsNone(report['summary']['error'])
                self.assertEqual(report['summary']['listener_errors'], [])

    def test_short_rf_raw_record(self):
        records = self.load('rf-outage')
        on = next(x['elapsed_s'] for x in records if x.get('command') == 'AT+CFUN=1')
        acks = [x for x in records if x['phase'] == 'RECOVERY' and 'PUBACK' in x.get('text', '')]
        self.assertEqual(len(acks), 23)
        self.assertEqual(round(acks[0]['elapsed_s']-on, 3), 36.223)
        for kind, count in (('VISION', 177), ('ENV', 34)):
            self.assertEqual(sum(x['kind'] == kind for x in records), count)

    def test_long_outage_keeps_sampling_without_successful_publish(self):
        records = [x for x in self.load()['records'] if x['phase'] == 'OUTAGE']
        for kind, count in (('VISION', 127), ('ENV', 26), ('PUBACK', 0)):
            self.assertEqual(sum(x['kind'] == kind for x in records), count)

    def test_radio_and_link_restored_at_end_of_original_session(self):
        final = self.load('final-link-state')
        for key in ('read_only', 'radio_enabled', 'registered', 'attached', 'pdp_active', 'mqtt_connected'):
            self.assertIs(final[key], True)

    def test_cloud_observer_is_not_misreported_as_acceptance(self):
        observer = self.load('mqtt-observer')
        self.assertEqual(observer['messages'], [])
        self.assertIn('SUBSCRIBE_FAILED', observer['errors'])

    def test_missing_local_samples_fail(self):
        for kind in ('VISION', 'ENV'):
            with self.subTest(missing=kind):
                records = [x for x in self.load()['records'] if x['kind'] != kind]
                self.assertFalse(summarize(records)['passed'])

    def test_stopped_task_at_capture_end_fails(self):
        records = [x for x in self.load()['records']
                   if not (x['kind'] == 'ENV' and x['elapsed_s'] > 200)]
        self.assertFalse(summarize(records)['passed'])

    def test_no_recovery_puback_fails(self):
        records = [x for x in self.load()['records']
                   if not (x['kind'] == 'PUBACK' and x['phase'] == 'RECOVERY')]
        self.assertFalse(summarize(records)['passed'])

    def test_boot_or_stall_fails_even_after_successful_reconnect(self):
        for item in ({'kind': 'BOOT'}, {'kind': 'NET', 'text': '[HEALTH] task=1 stalled; IWDG recovery pending'}):
            with self.subTest(event=item):
                records = copy.deepcopy(self.load()['records'])
                records.insert(-1, dict(records[-1], **item))
                self.assertFalse(summarize(records)['passed'])


if __name__ == '__main__':
    unittest.main()
