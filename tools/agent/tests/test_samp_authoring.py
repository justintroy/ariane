"""Focused tests for SA-MP plan resolution, layout math, and atomic adapters."""

import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

AGENT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AGENT_DIR))

import samp_authoring as authoring  # noqa: E402
from samp_authoring_common import _hash  # noqa: E402
from ariane_ipc import ArianeClient  # noqa: E402


class FakeService:
    def __init__(self, document=None):
        self.document = document or {
            "schema": 1,
            "objects": [{
                "id": 1, "model": 19379, "position": [10.0, 20.0, 3.0],
                "rotation": [0.0, 0.0, 0.0], "world": 0, "interior": 1,
                "player": -1, "stream": 300.0, "draw": 0.0, "area": -1,
                "priority": 0, "group": r"Interior\North Hall", "materials": {},
            }],
            "removals": [], "groups": [r"Interior\North Hall"], "next_id": 2,
            "preview": {"world": 0, "interior": 1},
        }
        self.revision = 7
        self.patch_calls = []
        self.reject_patch = False
        self.texture_calls = []
        self.model_calls = []
        self.capture_calls = []
        self.ambiguous_mode = None
        self.client = self
        self._history = []

    def model_info(self, model):
        return {
            "model": model, "name": "sample", "txd": "sample",
            "provenance": {"defined": True, "installed": True, "renderable": True},
            "visual_bounds": {"available": True, "min": [0.0, 0.0, 0.0],
                              "max": [4.0, 1.0, 3.0], "source": "render_geometry"},
            "collision_bounds": None,
            "material_slots": [{"slot": 0, "surfaces": [{"submesh": 0,
                               "submesh_name": "wall", "geometry_slot": 0,
                               "texture": "wall", "txd": "sample"}], "submeshes": [0]}],
            "diagnostics": [], "revision": self.revision,
        }

    def _samp_engine(self, operation, **params):
        if operation == "inspect":
            result = {"document": copy.deepcopy(self.document), "revision": self.revision}
            if params.get("include_history"):
                result["history"] = copy.deepcopy(self._history)
            return result
        if operation == "model_info":
            self.model_calls.append(params["model"])
            return self.model_info(params["model"])
        if operation == "textures":
            self.texture_calls.append(params)
            return {"complete": True, "next_cursor": None, "textures": [{
                "model": params.get("model", 19379), "txd": "sample", "texture": "wall",
                "available": True, "usable": True, "source_valid": True,
            }]}
        raise AssertionError(f"unexpected SA-MP operation: {operation}")

    def engine(self, command, fields=None):
        if command == "session_status":
            return {"active": True}
        if command == "camera_context":
            return {"camera": {"camera_revision": 3}}
        if command == "samp":
            request = json.loads(fields[0])
            self.patch_calls.append(request)
            if self.reject_patch:
                error = RuntimeError("engine rejected atomic patch")
                error.response = {"ok": False}
                raise error
            if self.ambiguous_mode == "precommit_once" and len(self.patch_calls) == 1:
                raise RuntimeError("reply lost before commit")
            if self.ambiguous_mode == "unrelated_revision" and len(self.patch_calls) == 1:
                self.document["preview"]["world"] = 9
                self.revision += 1
                self._history.append({"revision": self.revision, "label": "unrelated edit"})
                raise RuntimeError("reply lost during unrelated edit")
            updated, refs = authoring._expected_document(
                self.document, request["operations"], request.get("groups", []))
            self.document = updated
            self.revision += 1
            self._history.append({"revision": self.revision, "label": "SA-MP patch"})
            if self.ambiguous_mode == "committed_timeout_once" and len(self.patch_calls) == 1:
                raise RuntimeError("reply lost after commit")
            return {"samp": {"revision": self.revision, "document": copy.deepcopy(self.document),
                              "references": refs}}
        raise AssertionError(f"unexpected engine command: {command}")

    def capture_at_pose(self, path, **params):
        self.capture_calls.append(params)
        return {"path": str(path), "camera_revision": 3, "restored": True,
                "document_revision": params["expected_document_revision"],
                "filters": params["samp_filters"]}


def assert_matrix_close(test, left, right, tolerance=1e-7):
    for row in range(3):
        for column in range(3):
            test.assertAlmostEqual(left[row][column], right[row][column], delta=tolerance)


class SampAuthoringTests(unittest.TestCase):
    def test_sa_rotation_matrix_and_inverse_mixed_axis_and_gimbal(self):
        for rotation in ([23.0, -37.0, 71.0], [90.0, 33.0, -71.0],
                         [89.9999, -15.0, 119.0]):
            matrix = authoring._matrix(rotation)
            assert_matrix_close(self, matrix, authoring._matrix(authoring._euler(matrix)), 2e-5)

    def test_numeric_schema_rejects_bool_string_and_nonfinite_values(self):
        for invalid in (True, "3", float("nan"), float("inf")):
            with self.subTest(value=invalid):
                with self.assertRaises(ValueError):
                    authoring._number(invalid, "value")

    def test_unsupported_geometry_fields_fail_before_asset_inspection(self):
        for field in ("scale", "support", "snap", "boolean", "mesh_ops"):
            with self.subTest(field=field):
                service = FakeService()
                plan = {"expected_revision": service.revision,
                        "objects": [{"key": "bad", "model": 19379,
                                     "position": [0, 0, 0], field: True}]}
                with self.assertRaisesRegex(ValueError, f"{field} is unsupported"):
                    authoring.resolve_plan(service, plan)
                self.assertEqual(service.model_calls, [])

    def test_geometry_rejection_does_not_ban_object_or_palette_names(self):
        service = FakeService()
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": {"mesh": {"model": 19379, "position": [1, 2, 3]}},
            "palettes": {"support": {"type": "text", "text": "Support",
                                      "font": "Arial"}},
            "overrides": [{"target": "mesh", "slot": 0, "palette": "support"}],
        })
        self.assertTrue(resolved["valid"], resolved["diagnostics"])
        self.assertEqual([operation["op"] for operation in resolved["operations"]],
                         ["place", "material"])
        with self.assertRaisesRegex(ValueError, "plan.mesh_ops is unsupported"):
            authoring.resolve_plan(service, {"expected_revision": service.revision,
                                             "mesh_ops": []})

    def test_aggregate_bulk_material_expansion_stops_at_operation_limit(self):
        service = FakeService()
        tint = {"type": "texture", "model": -1, "txd": "unused",
                "texture": "unused", "color": 0xFFFFFFFF}
        plan = {
            "expected_revision": service.revision,
            "objects": [{"key": f"item.{index:04d}", "model": 19379,
                         "position": [index * 2.0, 0, 0], "group": "bulk"}
                        for index in range(1024)],
            "bulk_materials": [{"group": "bulk", "slot": 0, "material": tint}
                               for _ in range(4)],
        }
        with self.assertRaisesRegex(ValueError, "bulk material expansion exceeds"):
            authoring.resolve_plan(service, plan)
        self.assertEqual(service.patch_calls, [])

    def test_perimeter_rotation_is_rigid_full_3d_transform_and_preserves_opening(self):
        info = {"visual_bounds": {"available": True, "min": [0, 0, 0], "max": [4, 1, 3]}}
        common = {"type": "perimeter", "key_prefix": "room.wall", "model": 19379,
                  "origin": [120, -45, 8], "width": 12, "depth": 10, "spacing": 4,
                  "openings": [{"side": "south", "start": 4, "end": 8}],
                  "group": r"Interior\North Hall"}
        unrotated = authoring._layout_items(common, info, [])
        mixed_rotation = [26.0, -18.0, 43.0]
        rotated = authoring._layout_items({**common, "rotation": mixed_rotation}, info, [])
        self.assertEqual([row[0] for row in unrotated], [row[0] for row in rotated])
        pivot = common["origin"]
        layout_matrix = authoring._matrix(mixed_rotation)
        for base_row, moved_row in zip(unrotated, rotated):
            local = [base_row[1][axis] - pivot[axis] for axis in range(3)]
            expected = [pivot[axis] + authoring._mv(layout_matrix, local)[axis] for axis in range(3)]
            for actual, wanted in zip(moved_row[1], expected):
                self.assertAlmostEqual(actual, wanted, places=6)
            expected_matrix = authoring._mm(layout_matrix, authoring._matrix(base_row[2]))
            assert_matrix_close(self, authoring._matrix(moved_row[2]), expected_matrix, 2e-6)
        south = [row for row in rotated if ".south." in row[0]]
        self.assertNotIn("room.wall.south.001", {row[0] for row in south})
        self.assertTrue(any(abs(row[1][2] - pivot[2]) > 0.1 for row in rotated))

    def test_perimeter_expansion_checks_limit_before_generating_rows(self):
        info = {"visual_bounds": {"available": True, "min": [0, 0, 0], "max": [4, 1, 3]}}
        layout = {"type": "perimeter", "key_prefix": "wide", "model": 19379,
                  "origin": [0, 0, 0], "width": 1e308, "depth": 10, "spacing": 1e-308}
        with self.assertRaisesRegex(ValueError, "2048-object limit"):
            authoring._layout_items(layout, info, [])

    def test_perimeter_wall_axis_selects_measured_local_tangent(self):
        info = {"visual_bounds": {"available": True, "min": [0, 0, 0], "max": [4, 1, 3]}}
        layout = {"type": "perimeter", "key_prefix": "axis", "model": 19379,
                  "origin": [0, 0, 0], "width": 12, "depth": 10}
        y_long = authoring._layout_items({**layout, "wall_axis": 1}, info, [])
        x_long = authoring._layout_items({**layout, "wall_axis": 0}, info, [])
        self.assertEqual(sum(".south." in row[0] for row in y_long), 12)
        self.assertEqual(sum(".south." in row[0] for row in x_long), 3)

    def test_resolve_existing_id_transform_and_material_keeps_immutable_base_hash(self):
        service = FakeService()
        original = copy.deepcopy(service.document)
        texture = {"type": "texture", "model": -1, "txd": "sample", "texture": "wall", "color": 0xFFFFFFFF}
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "transforms": [{"target": 1, "translation": [3, -2, 1], "rotation_delta": [0, 0, 40]}],
            "overrides": [{"target": 1, "slot": 0, "material": texture}],
        })
        self.assertTrue(resolved["valid"], resolved["diagnostics"])
        self.assertEqual(resolved["base_document_hash"], _hash(original))
        self.assertEqual(service.document, original)
        self.assertEqual(service.texture_calls, [])
        self.assertEqual([op["op"] for op in resolved["operations"]], ["update", "material"])
        self.assertEqual(len(resolved["computed_bounds"]), 1)
        self.assertEqual(resolved["computed_bounds"][0]["id"], 1)
        applied = authoring.apply_plan(service, resolved)
        self.assertEqual(len(service.patch_calls), 1)
        self.assertEqual(applied["revision"], service.revision)
        updated = service.document["objects"][0]
        self.assertEqual(updated["position"], [13.0, 18.0, 4.0])
        self.assertEqual(updated["materials"]["0"], texture)

    def test_symbolic_plan_references_and_source_group_names_apply_as_one_patch(self):
        service = FakeService()
        plan = {
            "expected_revision": service.revision,
            "groups": [r"Interior\Front Desk"],
            "objects": [{"key": "counter", "model": 19379, "position": [20, 30, 4],
                         "group": r"Interior\Front Desk", "world": 2, "interior": 0}],
            "duplicates": [{"key": "counter.copy", "source": "counter",
                             "relative_to": "counter", "offset": [5, 0, 0],
                             "group": r"Interior\Front Desk"}],
            "transforms": [{"target": "counter.copy", "translation": [0, 2, 0]}],
        }
        resolved = authoring.resolve_plan(service, plan)
        self.assertTrue(resolved["valid"], resolved["diagnostics"])
        self.assertEqual(resolved["operations"][1]["id"], {"ref": "counter"})
        self.assertEqual(resolved["operations"][2]["id"], {"ref": "counter.copy"})
        applied = authoring.apply_plan(service, resolved)
        self.assertEqual(applied["references"], {"counter": 2, "counter.copy": 3})
        self.assertEqual(len(service.patch_calls), 1)
        self.assertIn(r"Interior\Front Desk", service.document["groups"])
        self.assertEqual(service.document["objects"][-1]["world"], 2)
        self.assertEqual(service.document["objects"][-1]["interior"], 0)
        self.assertEqual(service.document["objects"][-1]["position"], [25.0, 32.0, 4.0])

    def test_slot_and_texture_validation_fail_closed_for_plans_and_bulk(self):
        service = FakeService()
        bad_slot = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "overrides": [{"target": 1, "slot": 7,
                           "material": {"type": "texture", "model": 19379,
                                        "txd": "sample", "texture": "wall", "color": 0xFFFFFFFF}}],
        })
        self.assertFalse(bad_slot["valid"])
        self.assertIn("material_slot_unresolved", {item["code"] for item in bad_slot["diagnostics"]})
        service._samp_engine = lambda operation, **params: ({"textures": [], "complete": False}
            if operation == "textures" else FakeService._samp_engine(service, operation, **params))
        bad_texture = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "overrides": [{"target": 1, "slot": 0,
                           "material": {"type": "texture", "model": 19379,
                                        "txd": "missing", "texture": "missing", "color": 0xFFFFFFFF}}],
        })
        self.assertFalse(bad_texture["valid"])
        self.assertIn("texture_unverified", {item["code"] for item in bad_texture["diagnostics"]})
        with self.assertRaisesRegex(ValueError, "preflight failed"):
            authoring.material_bulk(service, expected_revision=service.revision, ids=[1], slot=7,
                                    material={"type": "text", "text": "Door", "font": "Arial"})
        self.assertEqual(service.patch_calls, [])

    def test_definition_only_model_is_not_silently_accepted(self):
        service = FakeService()
        base_model_info = service.model_info
        def info(model):
            result = base_model_info(model)
            result["provenance"] = {"defined": True, "installed": False, "renderable": False}
            return result
        service.model_info = info
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": [{"key": "bad", "model": 19379, "position": [0, 0, 0]}],
        })
        self.assertFalse(resolved["valid"])
        self.assertIn("model_unavailable", {item["code"] for item in resolved["diagnostics"]})

    def test_failed_atomic_apply_does_not_retry_structured_rejection(self):
        service = FakeService()
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": [{"key": "new", "model": 19379, "position": [0, 0, 0]}],
        })
        service.reject_patch = True
        before = copy.deepcopy(service.document)
        with self.assertRaisesRegex(RuntimeError, "rejected atomic patch"):
            authoring.apply_plan(service, resolved)
        self.assertEqual(service.document, before)
        self.assertEqual(service.revision, 7)
        self.assertEqual(len(service.patch_calls), 1)

    def test_expected_document_simulates_symbolic_place_duplicate_and_update(self):
        service = FakeService()
        expected, references = authoring._expected_document(service.document, [
            {"op": "place", "key": "new", "object": {
                "model": 19379, "position": [1, 2, 3], "rotation": [0, 0, 0],
                "group": r"Exterior\Gate", "materials": {}}},
            {"op": "duplicate", "key": "clone", "id": {"ref": "new"}, "kind": "objects"},
            {"op": "update", "id": {"ref": "clone"}, "changes": {"position": [7, 8, 9]}},
        ], [r"Exterior\Gate"])
        self.assertEqual(references, {"new": 2, "clone": 3})
        self.assertEqual(expected["next_id"], 4)
        self.assertEqual(expected["objects"][-1]["id"], 3)
        self.assertEqual(expected["objects"][-1]["position"], [7, 8, 9])
        self.assertIn(r"Exterior\Gate", expected["groups"])

    def test_committed_timeout_recovers_references_without_resending_patch(self):
        service = FakeService()
        service.ambiguous_mode = "committed_timeout_once"
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": [{"key": "new", "model": 19379, "position": [1, 2, 3]}],
        })
        applied = authoring.apply_plan(service, resolved)
        self.assertTrue(applied["recovered"])
        self.assertEqual(applied["references"], {"new": 2})
        self.assertEqual(len(service.patch_calls), 1)
        self.assertEqual(service.document["objects"][-1]["id"], 2)

    def test_precommit_timeout_retries_once_only_while_base_is_unchanged(self):
        service = FakeService()
        service.ambiguous_mode = "precommit_once"
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": [{"key": "new", "model": 19379, "position": [1, 2, 3]}],
        })
        applied = authoring.apply_plan(service, resolved)
        self.assertTrue(applied["retried"])
        self.assertFalse(applied["recovered"])
        self.assertEqual(len(service.patch_calls), 2)
        self.assertEqual(service.document["objects"][-1]["id"], 2)

    def test_unrelated_revision_change_is_never_retried(self):
        service = FakeService()
        service.ambiguous_mode = "unrelated_revision"
        resolved = authoring.resolve_plan(service, {
            "expected_revision": service.revision,
            "objects": [{"key": "new", "model": 19379, "position": [1, 2, 3]}],
        })
        with self.assertRaisesRegex(RuntimeError, "result is uncertain"):
            authoring.apply_plan(service, resolved)
        self.assertEqual(len(service.patch_calls), 1)
        self.assertEqual(service.revision, 8)
        self.assertEqual(service.document["preview"]["world"], 9)

    def test_diagnostic_and_pair_scan_limits_report_coverage(self):
        bounds = {"diagnostics": [], "records": [
            {"id": index + 1, "available": True, "min": [0, 0, 0], "max": [2, 2, 2]}
            for index in range(6)]}
        records = [{"id": index + 1} for index in range(6)]
        issues = authoring._composition_issues(bounds, records, {
            "max_pair_checks": 3, "diagnostic_limit": 4,
        })
        codes = [item["code"] for item in issues]
        self.assertIn("pair_scan_truncated", codes)
        self.assertIn("diagnostics_truncated", codes)
        scan = next(item for item in issues if item["code"] == "pair_scan_truncated")
        self.assertEqual(scan["pairs_checked"], 3)
        self.assertEqual(scan["pairs_total"], 15)
        self.assertLessEqual(len(issues), 4)

    def test_capture_views_pins_world_interior_and_document_revision(self):
        service = FakeService()
        with tempfile.TemporaryDirectory() as temporary:
            captured = authoring.capture_views(service, output_dir=temporary, ids=[1],
                                               world=0, interior=1)
            self.assertEqual(captured["revision"], service.revision)
            self.assertEqual(len(service.capture_calls), 3)
            self.assertTrue(all(call["samp_filters"] == (0, 1) for call in service.capture_calls))
            self.assertTrue(all(call["expected_document_revision"] == service.revision
                                for call in service.capture_calls))
            self.assertTrue(captured["camera_restored"])
            self.assertTrue(Path(captured["manifest_path"]).is_file())

    def test_minus_one_capture_filters_are_wildcards_and_specific_filters_include_global(self):
        service = FakeService()
        global_row = copy.deepcopy(service.document["objects"][0])
        global_row.update({"id": 2, "world": -1, "interior": -1})
        world_row = copy.deepcopy(service.document["objects"][0])
        world_row.update({"id": 3, "world": 42, "interior": 1})
        other_row = copy.deepcopy(service.document["objects"][0])
        other_row.update({"id": 4, "world": 43, "interior": 2})
        service.document["objects"] = [global_row, world_row, other_row]
        service.document["preview"] = {"world": -1, "interior": -1}
        all_bounds = authoring.bounds(service)
        self.assertEqual([row["id"] for row in all_bounds["records"]], [2, 3, 4])
        scoped_bounds = authoring.bounds(service, world=42, interior=1)
        self.assertEqual([row["id"] for row in scoped_bounds["records"]], [2, 3])
        with tempfile.TemporaryDirectory() as temporary:
            captured = authoring.capture_views(service, output_dir=temporary)
        self.assertEqual(captured["bounds"]["visual"]["record_count"], 3)
        self.assertTrue(all(call["samp_filters"] == (-1, -1) for call in service.capture_calls))

    def test_capture_at_pose_places_filters_after_camera_revision_fields(self):
        client = ArianeClient.__new__(ArianeClient)
        calls = []
        client.command = lambda command, *fields: calls.append((command, fields)) or {"ok": True}
        with tempfile.TemporaryDirectory() as temporary:
            client.capture_at_pose(Path(temporary) / "view.png", position=[1, 2, 3],
                                   target=[4, 5, 6], expected_camera_revision=9,
                                   samp_filters=(2, 4), expected_document_revision=17)
        command, fields = calls[0]
        self.assertEqual(command, "capture_pose")
        self.assertEqual(fields[12:16], (9, 2, 4, 17))
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "camera and document revisions"):
                client.capture_at_pose(Path(temporary) / "bad.png", position=[1, 2, 3],
                                       target=[4, 5, 6], samp_filters=(2, 4),
                                       expected_document_revision=17)


if __name__ == "__main__":
    unittest.main()
