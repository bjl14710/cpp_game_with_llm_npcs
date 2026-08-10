"""Unit tests for the judge-endpoint plumbing in tools/eval_lines.py.

Stdlib unittest only — no new dependencies (CLAUDE.md). Run with:

  python3 -m unittest discover -s tools -p 'test_*.py'

Covers the parts that decide WHERE a judged gate sends its prompt and what
comes back, because getting either wrong is silent: a misresolved endpoint
turns gates 3-5 into an exception, and an unstripped reasoning trace turns a
1-5 fidelity score into whatever digit the model happened to think about
first.
"""

import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import eval_lines  # noqa: E402


class JudgeEndpointTest(unittest.TestCase):
    def setUp(self):
        self._root = eval_lines.ROOT
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.addCleanup(lambda: setattr(eval_lines, "ROOT", self._root))
        eval_lines.ROOT = Path(self._tmp.name)
        (eval_lines.ROOT / "config").mkdir()

    def write_cfg(self, text):
        (eval_lines.ROOT / "config" / "llm.cfg").write_text(text)

    def test_defaults_to_the_cloud_judge_when_no_base_url_is_set(self):
        self.write_cfg("provider = ollama\nmodel = qwen3:8b\n")
        base, model = eval_lines.judge_endpoint()
        self.assertEqual(base, eval_lines.DEFAULT_JUDGE_URL)
        self.assertEqual(model, eval_lines.JUDGE_MODEL)

    def test_base_url_override_selects_a_local_model_by_default(self):
        # Pointing base_url at Ollama's shim must not keep asking for
        # anthropic/claude-haiku-4.5, which no local server serves.
        self.write_cfg("base_url = http://localhost:11434/v1\n")
        base, model = eval_lines.judge_endpoint()
        self.assertEqual(base, "http://localhost:11434/v1")
        self.assertEqual(model, eval_lines.LIVE_MODEL)

    def test_judge_model_is_honoured_and_trailing_slash_trimmed(self):
        self.write_cfg("base_url = http://localhost:11434/v1/\n"
                       "judge_model = some-other-model\n")
        base, model = eval_lines.judge_endpoint()
        self.assertEqual(base, "http://localhost:11434/v1")
        self.assertEqual(model, "some-other-model")

    def test_comments_are_stripped_and_first_assignment_wins(self):
        self.write_cfg("# base_url = http://ignored/v1\n"
                       "base_url = http://localhost:11434/v1  # inline\n"
                       "base_url = http://later/v1\n")
        self.assertEqual(eval_lines.judge_endpoint()[0],
                         "http://localhost:11434/v1")

    def test_is_local_recognises_loopback_only(self):
        self.assertTrue(eval_lines._is_local("http://localhost:11434/v1"))
        self.assertTrue(eval_lines._is_local("http://127.0.0.1:11434/v1"))
        self.assertFalse(eval_lines._is_local("https://openrouter.ai/api/v1"))


class JudgeCallTest(unittest.TestCase):
    def setUp(self):
        self._root = eval_lines.ROOT
        self._post = eval_lines._post_json
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.addCleanup(lambda: setattr(eval_lines, "ROOT", self._root))
        self.addCleanup(lambda: setattr(eval_lines, "_post_json", self._post))
        eval_lines.ROOT = Path(self._tmp.name)
        (eval_lines.ROOT / "config").mkdir()
        (eval_lines.ROOT / "config" / "llm.cfg").write_text(
            "base_url = http://localhost:11434/v1\n")

    def fake_post(self, content):
        seen = {}

        def post(url, payload, headers, timeout):
            seen["url"] = url
            seen["headers"] = headers
            seen["timeout"] = timeout
            return {"choices": [{"message": {"content": content}}]}

        eval_lines._post_json = post
        return seen

    def test_local_judge_needs_no_api_key(self):
        seen = self.fake_post("4")
        result = eval_lines.judge("score this", eval_lines.TokenBudget(10_000))
        self.assertEqual(result, "4")
        self.assertEqual(seen["url"], "http://localhost:11434/v1/chat/completions")
        self.assertNotIn("Authorization", seen["headers"])

    def test_reasoning_trace_is_stripped_before_the_verdict_is_read(self):
        # qwen3 emits <think>...</think> through the OpenAI shim. Left in, the
        # "1" and "5" below would be read as the score instead of the real 4.
        self.fake_post("<think>Maybe 1, maybe 5, hard to say.</think>\n4")
        verdict = eval_lines.judge("score", eval_lines.TokenBudget(10_000))
        self.assertEqual(verdict, "4")
        self.assertEqual(eval_lines._first_int(verdict, 1, 5), 4)

    def test_a_local_judge_gets_the_patient_timeout(self):
        # A local model on CPU takes minutes per call. Timing out sooner than
        # the hardware turns a slow gate into GateFailure, which fails the
        # whole bank closed for no reason but impatience.
        seen = self.fake_post("4")
        eval_lines.judge("score", eval_lines.TokenBudget(10_000))
        self.assertEqual(seen["timeout"], eval_lines.LOCAL_TIMEOUT_S)

    def test_a_remote_judge_keeps_the_short_timeout(self):
        (eval_lines.ROOT / "config" / "llm.cfg").write_text(
            "base_url = https://openrouter.ai/api/v1\n"
            "api_key_env = A_KEY_FOR_THIS_TEST\n")
        os.environ["A_KEY_FOR_THIS_TEST"] = "x"
        self.addCleanup(os.environ.pop, "A_KEY_FOR_THIS_TEST", None)
        seen = self.fake_post("4")
        eval_lines.judge("score", eval_lines.TokenBudget(10_000))
        self.assertEqual(seen["timeout"], eval_lines.REMOTE_TIMEOUT_S)

    def test_missing_key_on_a_remote_judge_fails_closed(self):
        (eval_lines.ROOT / "config" / "llm.cfg").write_text(
            "base_url = https://openrouter.ai/api/v1\n"
            "api_key_env = A_KEY_THAT_IS_NOT_SET\n")
        self.fake_post("4")
        with self.assertRaises(eval_lines.GateFailure):
            eval_lines.judge("score", eval_lines.TokenBudget(10_000))


if __name__ == "__main__":
    unittest.main()
