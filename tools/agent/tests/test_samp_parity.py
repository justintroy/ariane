"""Parity checks for SA-MP CLI and shared service forwarding."""

from contextlib import redirect_stdout
from io import StringIO
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


AGENT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AGENT_DIR))

import arianectl  # noqa: E402
from service import ArianeService  # noqa: E402


OPERATIONS = (
	"inspect", "code", "preview", "preview_import", "import", "update", "delete",
	"duplicate", "material", "removal", "textures", "model_info", "texture_preview", "open", "save",
	"export", "clear", "undo", "redo", "patch", "place", "replace", "window",
)

ADAPTER_OPERATIONS = (
	"authoring_schema", "resolve_plan", "apply_plan", "bounds", "validate_composition",
	"capture_views", "group_inspect", "group_transform", "group_clone", "group_delete",
	"material_bulk",
)


class RecordingEngine:
	def __init__(self):
		self.calls = []

	def __call__(self, command, fields=None):
		self.calls.append((command, fields))
		request = json.loads(fields[0])
		return {"samp": {"operation": request["op"], "parameters": request}}


class FakeClient:
	calls = []

	def __init__(self, socket, timeout):
		self.socket, self.timeout = socket, timeout

	def command(self, command, payload):
		request = json.loads(payload)
		self.calls.append((command, request))
		return {"samp": {"operation": request["op"], "parameters": request}}


class FakeService:
	calls = []

	def __init__(self, *args, **kwargs):
		pass

	def samp(self, operation, **params):
		self.calls.append((operation, params))
		if operation == "resolve_plan":
			return {"kind": "resolved", "valid": True, "operations": []}
		if operation == "apply_plan":
			return {"revision": 8, "references": {"counter": 12}}
		return {"operation": operation, "parameters": params}


class SampParityTests(unittest.TestCase):
	def setUp(self):
		self.service = ArianeService.__new__(ArianeService)
		self.engine = RecordingEngine()
		self.service.engine = self.engine

	def test_service_and_dispatch_forward_every_operation_without_schema_drift(self):
		for operation in OPERATIONS:
			direct = self.service.samp(operation, marker=operation)
			dispatched = self.service.dispatch(f"samp.{operation}", {"marker": operation})
			self.assertEqual(operation, direct["operation"])
			self.assertEqual(direct, dispatched)
			self.assertEqual(operation, direct["parameters"]["marker"])
		self.assertEqual(len(OPERATIONS) * 2, len(self.engine.calls))
		self.assertTrue(all(command == "samp" for command, _ in self.engine.calls))

	def test_schema_is_static_discovery_and_does_not_call_engine(self):
		result = self.service.samp("authoring_schema")
		self.assertEqual("ariane.samp.authoring-schema", result["kind"])
		self.assertIn("radial", result["plan"]["layouts"])
		self.assertIn("model_info", result["operations"])
		self.assertEqual([], self.engine.calls)

	def test_cli_routes_python_adapters_and_keeps_bare_service_shape(self):
		FakeService.calls.clear()
		resolved = {"kind": "resolved", "valid": True, "operations": []}
		with patch.object(arianectl, "ArianeService", FakeService):
			output = StringIO()
			with redirect_stdout(output):
				code = arianectl.main(["samp", "resolve_plan", "--params",
				                       json.dumps({"plan": {"expected_revision": 3}})])
			self.assertEqual(0, code)
			self.assertEqual(resolved, json.loads(output.getvalue()))

			output = StringIO()
			with redirect_stdout(output):
				code = arianectl.main(["samp", "apply_plan", "--params",
				                       json.dumps({"resolved_plan": resolved, "expected_revision": 3})])
			self.assertEqual(0, code)
			self.assertEqual({"revision": 8, "references": {"counter": 12}},
			                 json.loads(output.getvalue()))
		self.assertEqual("resolve_plan", FakeService.calls[0][0])
		self.assertEqual("apply_plan", FakeService.calls[1][0])
		self.assertEqual(resolved, FakeService.calls[1][1]["resolved_plan"])

	def test_cli_forwards_every_operation_and_preserves_parameters(self):
		FakeClient.calls.clear()
		with patch.object(arianectl, "ArianeClient", FakeClient):
			for operation in OPERATIONS:
				output = StringIO()
				with redirect_stdout(output):
					result = arianectl.main(["samp", operation, "--params", json.dumps({"marker": operation})])
				self.assertEqual(0, result)
				payload = json.loads(output.getvalue())
				self.assertEqual(operation, payload["samp"]["operation"])
		self.assertEqual(OPERATIONS, tuple(request["op"] for _, request in FakeClient.calls))
		self.assertTrue(all(command == "samp" for command, _ in FakeClient.calls))

	def test_cli_file_payload_and_operation_override_share_same_contract(self):
		FakeClient.calls.clear()
		with tempfile.TemporaryDirectory() as temporary:
			request = Path(temporary) / "request.json"
			request.write_text(json.dumps({"op": "clear", "expected_revision": 7}), encoding="utf-8")
			with patch.object(arianectl, "ArianeClient", FakeClient), redirect_stdout(StringIO()):
				result = arianectl.main(["samp", "inspect", "--file", str(request)])
		self.assertEqual(0, result)
		self.assertEqual({"op": "inspect", "expected_revision": 7}, FakeClient.calls[0][1])

	def test_cli_rejects_non_object_parameters_before_transport(self):
		FakeClient.calls.clear()
		output = StringIO()
		with patch.object(arianectl, "ArianeClient", FakeClient), redirect_stdout(output):
			result = arianectl.main(["samp", "inspect", "--params", "[]"])
		self.assertEqual(1, result)
		self.assertIn("parameters must be a JSON object", output.getvalue())
		self.assertEqual([], FakeClient.calls)

	def test_authoring_adapter_binding_errors_are_structured_and_do_not_mask_body_errors(self):
		invalid_calls = (
			{"group": "Interior", "expected_revision": 7},
			{"group": "Interior", "expected_revision": 7, "pivot": [0, 0, 0], "unexpected": True},
		)
		for params in invalid_calls:
			with self.subTest(params=params):
				with self.assertRaisesRegex(ValueError, "invalid group_transform parameters"):
					self.service.samp("group_transform", **params)
				with self.assertRaisesRegex(ValueError, "invalid group_transform parameters"):
					self.service.dispatch("samp.group_transform", params)
		self.assertEqual([], self.engine.calls)

		import samp_authoring
		with patch.object(samp_authoring, "group_transform", side_effect=TypeError("adapter implementation bug")):
			with self.assertRaisesRegex(TypeError, "adapter implementation bug"):
				self.service.samp("group_transform", group="Interior", expected_revision=7,
								  pivot=[0, 0, 0])

	def test_cli_emits_json_for_missing_and_unknown_adapter_parameters(self):
		class BoundService:
			def __init__(self, *args, **kwargs):
				self.inner = ArianeService.__new__(ArianeService)

			def samp(self, operation, **params):
				return ArianeService.samp(self.inner, operation, **params)

		calls = (
			{"group": "Interior", "expected_revision": 7},
			{"group": "Interior", "expected_revision": 7, "pivot": [0, 0, 0], "unexpected": True},
		)
		with patch.object(arianectl, "ArianeService", BoundService):
			for params in calls:
				with self.subTest(params=params):
					output = StringIO()
					with redirect_stdout(output):
						result = arianectl.main(["samp", "group_transform", "--params", json.dumps(params)])
					self.assertEqual(1, result)
					payload = json.loads(output.getvalue())
					self.assertFalse(payload["ok"])
					self.assertIn("invalid group_transform parameters", payload["error"])
					self.assertNotIn("Traceback", output.getvalue())

	def test_preview_and_window_operations_explicit_semantics(self):
		# Preview operation: document filter mutation forwarding (e.g. world, interior)
		preview_res = self.service.samp("preview", world=1, interior=0)
		self.assertEqual("preview", preview_res["operation"])
		self.assertEqual(1, preview_res["parameters"]["world"])
		self.assertEqual(0, preview_res["parameters"]["interior"])

		# Window operation: GUI visibility inspection / toggle forwarding (non-document-mutating)
		window_res = self.service.samp("window", show=True)
		self.assertEqual("window", window_res["operation"])
		self.assertTrue(window_res["parameters"]["show"])

		window_query = self.service.samp("window")
		self.assertEqual("window", window_query["operation"])

		# CLI parity for preview (document preview filter mutation) and window (UI visibility)
		FakeClient.calls.clear()
		with patch.object(arianectl, "ArianeClient", FakeClient):
			out = StringIO()
			with redirect_stdout(out):
				code = arianectl.main(["samp", "preview", "--params", json.dumps({"world": 2, "interior": 1})])
			self.assertEqual(0, code)
			self.assertEqual({"op": "preview", "world": 2, "interior": 1}, FakeClient.calls[-1][1])

			out = StringIO()
			with redirect_stdout(out):
				code = arianectl.main(["samp", "window", "--params", json.dumps({"show": False})])
			self.assertEqual(0, code)
			self.assertEqual({"op": "window", "show": False}, FakeClient.calls[-1][1])


if __name__ == "__main__":
	unittest.main()
