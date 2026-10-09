"""Unit tests for the offline parts of the dialogue evaluation harness.

Stdlib unittest only — no new dependencies (CLAUDE.md). Run with:

  python3 -m unittest discover -s tools -p 'test_*.py'

SCAFFOLD — every case is skipped until steps 7-11 land. The bodies are written
against the real interfaces so they compile and run the moment the skip comes
off.

WHAT IS TESTED HERE AND WHAT IS NOT. The harness itself needs a live model and
is never part of a test run — the rule tools/eval_lines.py already follows.
What IS testable offline is everything that decides whether a number is
trustworthy: the freeze check, the contamination check, the scorecard
arithmetic, and the baseline-establishment path. Those are the parts whose
failures are silent, so those are the parts with tests.
"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import curate_candidates  # noqa: E402
import eval_dialogue  # noqa: E402
import probe_lib  # noqa: E402

SKIP = "not implemented yet — see .claude/plans/npc-dialogue-measurement.md"


class NormalizationTest(unittest.TestCase):
    """probe_lib.normalized is the comparison basis for contamination."""

    @unittest.skip(SKIP)
    def test_case_and_punctuation_do_not_defeat_a_match(self):
        # A candidate is contaminated whether or not someone retyped the
        # question with different punctuation.
        a = probe_lib.normalized("Where were you on Tuesday evening?")
        b = probe_lib.normalized("where were you on tuesday evening")
        self.assertEqual(a, b)

    @unittest.skip(SKIP)
    def test_whitespace_is_collapsed_not_stripped_between_words(self):
        self.assertEqual(probe_lib.normalized("a   b\n c"), "a b c")


class ContaminationTest(unittest.TestCase):
    """The check that keeps the calibration standard uncontaminated."""

    @unittest.skip(SKIP)
    def test_an_exact_probe_utterance_is_refused(self):
        # The obvious case, and the one that would otherwise be caught only by
        # someone noticing a suspiciously good score months later.
        self.assertTrue(curate_candidates.is_contaminated(
            "Where were you on Tuesday evening?",
            ["Where were you on Tuesday evening?"]))

    @unittest.skip(SKIP)
    def test_a_shared_five_word_run_is_refused(self):
        # Paraphrase-level overlap still teaches the model the probe.
        self.assertTrue(curate_candidates.is_contaminated(
            "So tell me, where were you on Tuesday evening, Marge?",
            ["Where were you on Tuesday evening?"]))

    @unittest.skip(SKIP)
    def test_an_unrelated_line_is_not_refused(self):
        # A false positive costs one candidate; a false negative costs every
        # measurement. But a check that refuses everything is not a check.
        self.assertFalse(curate_candidates.is_contaminated(
            "Do you sell the sourdough on Sundays?",
            ["Where were you on Tuesday evening?"]))

    @unittest.skip(SKIP)
    def test_a_secret_token_anywhere_in_the_candidate_is_refused(self):
        self.assertTrue(curate_candidates.is_contaminated(
            "I heard something about the mill.", [], secret_tokens=["mill"]))

    @unittest.skip(SKIP)
    def test_retired_probes_from_older_versions_still_contaminate(self):
        # A probe dropped in v2 still took v1's measurements, and those
        # numbers are still in the trend line.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "golden-v1").mkdir()
            (root / "golden-v2").mkdir()
            probes = curate_candidates.load_all_probe_text(root)
            self.assertIn("golden-v1", str(probes))


class ScorecardTest(unittest.TestCase):
    """Arithmetic that must stay comparable across golden-set versions."""

    @unittest.skip(SKIP)
    def test_a_v2_run_reports_the_original_subset_score_as_well(self):
        # Reporting only the current-version total is how a version bump
        # quietly resets a trend line.
        card = eval_dialogue.scorecard(results=[], version="golden-v2",
                                       original_ids=["marge_alibi_direct"])
        self.assertIn("original_subset", card)
        self.assertIn("total", card)

    @unittest.skip(SKIP)
    def test_every_statistical_metric_carries_advisory_and_its_own_n(self):
        # role_leak_probe.py's honesty requirement: a probe that hides its own
        # sample size is worse than no probe.
        card = eval_dialogue.scorecard(results=[], version="golden-v1",
                                       original_ids=[])
        for name, metric in card["statistical"].items():
            self.assertTrue(metric["advisory"], name)
            self.assertIn("n", metric)

    @unittest.skip(SKIP)
    def test_a_first_run_establishes_a_baseline_and_succeeds(self):
        # A harness that fails when it has nothing to compare against teaches
        # people to pass --force, and then it never gates anything again.
        with tempfile.TemporaryDirectory() as tmp:
            code = eval_dialogue.apply_baseline(
                card={"statistical": {}}, baseline_dir=Path(tmp),
                version="golden-v1")
            self.assertEqual(code, 0)
            self.assertTrue((Path(tmp) / "golden-v1.json").exists())

    @unittest.skip(SKIP)
    def test_an_unreachable_model_is_cannot_evaluate_not_a_pass(self):
        # The distinction the whole harness rests on: a gate that could not
        # run has not passed.
        with self.assertRaises(probe_lib.ProbeFailure):
            probe_lib.ask_model("prompt", "utterance", "no-such-model")


class FreezeTest(unittest.TestCase):
    """The manifest is what makes the golden set a standard rather than data."""

    @unittest.skip(SKIP)
    def test_a_digest_is_stable_across_calls(self):
        with tempfile.NamedTemporaryFile("w", suffix=".probe",
                                         delete=False) as handle:
            handle.write("probe_id = x\n")
            path = Path(handle.name)
        self.assertEqual(probe_lib.file_digest(path),
                         probe_lib.file_digest(path))
        path.unlink()

    @unittest.skip(SKIP)
    def test_an_edited_file_produces_a_different_digest(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "a.probe"
            path.write_text("probe_id = x\n")
            before = probe_lib.file_digest(path)
            path.write_text("probe_id = x\n# a comment someone added\n")
            self.assertNotEqual(before, probe_lib.file_digest(path))


if __name__ == "__main__":
    unittest.main()
