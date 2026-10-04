"""SA-MP visual bounds, composition diagnostics, and capture adapters."""

from __future__ import annotations

import json
import math
from pathlib import Path
import uuid

if __package__:
	from .samp_authoring_common import (MAX_LAYOUT_ITEMS, MAX_OPERATIONS, MAX_SELECTED_RECORDS, PLAN_SCHEMA, _engine_samp, _group, _hash, _inspect, _int, _issue, _matrix, _model_info, _mv, _number, _select, _transformed_bounds, _vec)
else:
	from samp_authoring_common import (MAX_LAYOUT_ITEMS, MAX_OPERATIONS, MAX_SELECTED_RECORDS, PLAN_SCHEMA, _engine_samp, _group, _hash, _inspect, _int, _issue, _matrix, _model_info, _mv, _number, _select, _transformed_bounds, _vec)

def _bounded_diagnostics(items, limit):
	limit = _int(limit, "diagnostic_limit", 1, 4096)
	if len(items) <= limit:
		return items
	kept = items[:max(0, limit - 1)]
	kept.append(_issue("diagnostics_truncated", "Diagnostic output was capped; omitted findings remain unreported.",
					"warning", omitted_count=len(items) - len(kept)))
	return kept


def bounds(service, *, ids=None, groups=None, world=None, interior=None, include_records=True, diagnostic_limit=512):
	"""Aggregate measured visual and collision bounds for selected document objects."""
	inspection = _inspect(service)
	document, revision = inspection["document"], int(inspection["revision"])
	objects = _select(document, ids=ids, groups=groups, world=world, interior=interior)
	cache, visual_rows, collision_rows, diagnostics = {}, [], [], []
	selected_ids = {int(row["id"]) for row in objects}
	for row in objects:
		ident, model = int(row["id"]), int(row["model"])
		info = _model_info(service, model, cache)
		visual = _transformed_bounds(info.get("visual_bounds", {}), row["position"], row["rotation"])
		if visual["available"]:
			visual_rows.append({"id": ident, "model": model, "group": row.get("group"), **visual})
		else:
			diagnostics.append(_issue("visual_bounds_unavailable", "Visual geometry bounds are unavailable; dimensions were not inferred.", id=ident, model=model))
		collision = info.get("collision_bounds")
		if isinstance(collision, dict) and collision.get("available"):
			collision_rows.append({"id": ident, "model": model, "group": row.get("group"),
								  **_transformed_bounds(collision, row["position"], row["rotation"])})
		else:
			diagnostics.append(_issue("collision_bounds_unavailable", "Collision bounds are absent or unavailable; walkability is unknown.", "info", id=ident, model=model))
		for item in info.get("diagnostics", []):
			diagnostics.append({**item, "id": ident, "model": model})
		available_slots = {int(item["slot"]) for item in info.get("material_slots", []) if isinstance(item, dict) and "slot" in item}
		for slot_key in row.get("materials", {}):
			try:
				slot = int(slot_key)
			except (TypeError, ValueError):
				diagnostics.append(_issue("invalid_material_slot", "Stored material slot is not numeric.", "error", id=ident, slot=slot_key))
				continue
			if available_slots and slot not in available_slots:
				diagnostics.append(_issue("material_slot_unresolved", "Override targets no inspected surface slot on this model.", "error", id=ident, model=model, slot=slot))
			elif not available_slots:
				diagnostics.append(_issue("material_slot_mapping_unavailable", "Slot mapping is unavailable; override could not be verified.", "error", id=ident, model=model, slot=slot))
	for item in inspection.get("asset_diagnostics", []):
		if item.get("id") in selected_ids:
			diagnostics.append(item)
	def aggregate(rows):
		if not rows:
			return {"available": False, "min": None, "max": None, "center": None, "size": None, "record_count": 0}
		minimum = [min(row["min"][axis] for row in rows) for axis in range(3)]
		maximum = [max(row["max"][axis] for row in rows) for axis in range(3)]
		return {"available": True, "min": minimum, "max": maximum,
				"center": [(minimum[i] + maximum[i]) / 2 for i in range(3)],
				"size": [maximum[i] - minimum[i] for i in range(3)], "record_count": len(rows)}
	if int(_inspect(service)["revision"]) != revision:
		raise ValueError("SA-MP document changed while calculating bounds")
	return {"revision": revision, "filters": {"world": world, "interior": interior},
			"selection": {"ids": ids, "groups": groups}, "visual": aggregate(visual_rows),
			"collision": aggregate(collision_rows), "diagnostics": _bounded_diagnostics(diagnostics, diagnostic_limit),
			"records": visual_rows if include_records else None}

def authoring_schema():
	"""Machine-readable contract shared by `samp authoring_schema` and service calls."""
	return {
		"schema": PLAN_SCHEMA, "kind": "ariane.samp.authoring-schema",
		"operations": ["authoring_schema", "model_info", "texture_preview", "resolve_plan", "apply_plan",
					   "bounds", "validate_composition", "capture_views", "group_inspect", "group_transform",
					   "group_clone", "group_delete", "material_bulk"],
		"limits": {"patch_operations": MAX_OPERATIONS, "groups": 4096, "selected_records": MAX_SELECTED_RECORDS,
				   "layout_objects": MAX_LAYOUT_ITEMS, "perimeter_openings": 128, "key_utf8_bytes": 128,
				   "group_name_utf8_bytes": 512, "diagnostics": 4096, "pair_checks": 500000,
				   "clearance_zones": 128},
		"plan": {"required": ["expected_revision"], "optional": ["existing_ids", "groups", "objects", "layouts",
									  "duplicates", "removals", "transforms", "palettes", "overrides", "bulk_materials", "deletes"],
			 "layouts": {
				 "row": {"required": ["key_prefix", "model", "count", "origin"], "step_or": ["step", "spacing", "axis"]},
				 "grid": {"required": ["key_prefix", "model", "rows", "columns", "origin", "spacing"]},
				 "radial": {"required": ["key_prefix", "model", "count", "origin", "radius"],
					"optional": ["start_angle", "sweep", "facing"]},
				 "perimeter": {"required": ["key_prefix", "model", "origin", "width", "depth"],
					"optional": ["wall_axis", "spacing", "openings", "rotation"]}},
			 "object_creation_fields": ["world", "interior", "player", "stream", "draw", "area", "priority", "group"],
			 "geometry_contract": {"placement": "Explicit positions and rotations; relative_to plus offset is the only support-like anchor.",
								   "automatic_support_or_snap": False,
								   "unsupported_fields": ["scale", "support", "snap", "boolean", "boolean_ops", "mesh", "mesh_ops", "mesh_operations", "geometry_ops"]},
			 "references": "Use a previously declared plan key or existing stable integer ID. Key references are forward-only and never allocate IDs during resolution.",
			 "material_types": ["texture", "text"],
			 "material_slots": "Only model_info-inspected slots are accepted; no slot range is assumed to exist."},
		"resolved_plan": {"required": ["kind", "valid", "expected_revision", "base_document_hash", "operations", "groups"],
						  "apply_params": {"resolved_plan": "complete resolve_plan response", "expected_revision": "optional matching revision"},
						  "computed_bounds": "new and affected existing objects have measured visual bounds or explicit unavailable status"},
		"assets": {
			"model_info": {"exactly_one_of": ["model", "id"],
						   "response_fields": ["model", "name", "txd", "render_type", "provenance", "origin_axes",
											   "visual_bounds", "collision_bounds", "material_slots", "diagnostics", "revision"]},
			"textures": {"filters": ["model", "txd", "name", "limit", "cursor"], "stale_cursor": "rejected"},
			"texture_preview": {"one_of": ["texture", "candidates"], "optional": ["compare", "output_dir"]}},
		"bounds": {"selectors": ["ids", "groups", "world", "interior"],
				   "world_interior_filters": "-1 selects all; a non-negative value also includes global records tagged -1",
				   "visual_and_collision": "reported separately; unavailable dimensions remain null"},
		"capture_views": {"required": ["output_dir"], "filters": ["world", "interior"],
						"views": ["overview", "plan", "interior"], "poses": "optional explicit pose override by view name",
						"capture": "temporary camera and SA-MP filters; pinned to document revision"},
		"mutation": {"apply_requires": ["active_session", "matching_expected_revision"],
					 "application": "one atomic samp.patch history action", "resolution": "read-only; no durable IDs allocated",
					 "responses": {"service_adapter": "bare operation result", "direct_engine_cli": "{ok,samp:...}"}}
	}


def _composition_issues(bounds_result, records, options):
	by_id = {int(row["id"]): row for row in (bounds_result.get("records") or []) if row.get("available")}
	selected = [row for row in records if int(row["id"]) in by_id]
	overlap_tolerance = _number(options.get("overlap_tolerance", 0.01), "overlap_tolerance")
	coplanar_tolerance = _number(options.get("near_coplanar_tolerance", 0.03), "near_coplanar_tolerance")
	gap_tolerance = _number(options.get("gap_tolerance", 0.12), "gap_tolerance")
	diagnostic_limit = _int(options.get("diagnostic_limit", 512), "diagnostic_limit", 2, 4096)
	max_pair_checks = _int(options.get("max_pair_checks", 100000), "max_pair_checks", 1, 500000)
	diagnostics, omitted_diagnostics = [], 0
	def report(item):
		nonlocal omitted_diagnostics
		if len(diagnostics) < diagnostic_limit - 2:
			diagnostics.append(item)
		else:
			omitted_diagnostics += 1
	for item in bounds_result.get("diagnostics", []):
		report(item)
	if min(overlap_tolerance, coplanar_tolerance, gap_tolerance) < 0:
		raise ValueError("composition tolerances must be non-negative")
	roles = options.get("roles", {})
	if not isinstance(roles, dict):
		raise ValueError("roles must map stable IDs to floor or wall")
	if any(value not in ("floor", "wall") for value in roles.values()):
		raise ValueError("roles may label records floor or wall")
	def role(row):
		return roles.get(str(row["id"]), roles.get(row["id"]))
	all_pairs = len(selected) * (len(selected) - 1) // 2
	pairs_checked = 0
	for index, left in enumerate(selected):
		a = by_id[int(left["id"])]
		for right in selected[index + 1:]:
			if pairs_checked >= max_pair_checks:
				break
			pairs_checked += 1
			b = by_id[int(right["id"])]
			overlap = [min(a["max"][axis], b["max"][axis]) - max(a["min"][axis], b["min"][axis]) for axis in range(3)]
			if options.get("check_overlaps", True) and all(value > overlap_tolerance for value in overlap):
				report(_issue("possible_bounds_overlap", "Visual AABBs overlap; inspect the rendered objects because rotation makes this heuristic conservative.", id_a=left["id"], id_b=right["id"], overlap=overlap))
			if options.get("check_near_coplanar", True):
				for axis, label in enumerate(("x", "y", "z")):
					separation = min(abs(a["max"][axis] - b["min"][axis]), abs(b["max"][axis] - a["min"][axis]))
					other_axes = [candidate for candidate in range(3) if candidate != axis]
					shared = [min(a["max"][candidate], b["max"][candidate]) - max(a["min"][candidate], b["min"][candidate]) for candidate in other_axes]
					if separation <= coplanar_tolerance and all(value > overlap_tolerance for value in shared):
						report(_issue("near_coplanar_surfaces", "AABB faces nearly share a plane over an overlapping area; review for z-fighting.", id_a=left["id"], id_b=right["id"], axis=label, separation=separation))
			if {role(left), role(right)} == {"floor", "wall"}:
				floor, fb, wall, wb = (left, a, right, b) if role(left) == "floor" else (right, b, left, a)
				horizontal = [min(fb["max"][axis], wb["max"][axis]) - max(fb["min"][axis], wb["min"][axis]) for axis in (0, 1)]
				gap = float(wb["min"][2]) - float(fb["max"][2])
				if all(value > overlap_tolerance for value in horizontal) and gap > gap_tolerance:
					report(_issue("possible_wall_floor_gap", "Explicitly labelled floor and wall bounds are vertically separated.", wall_id=wall["id"], floor_id=floor["id"], gap=gap))
				elif all(value > overlap_tolerance for value in horizontal) and gap < -gap_tolerance:
					report(_issue("wall_floor_overlap", "Explicitly labelled wall and floor bounds overlap vertically.", wall_id=wall["id"], floor_id=floor["id"], overlap=-gap))
		if pairs_checked >= max_pair_checks:
			break
	if pairs_checked < all_pairs:
		diagnostics.append(_issue("pair_scan_truncated", "Pairwise checks stopped at the configured safety limit; the remaining object pairs were not checked.",
							 "warning", pairs_checked=pairs_checked, pairs_total=all_pairs,
							 coverage=pairs_checked / all_pairs if all_pairs else 1.0))
	zones = options.get("clearance_zones", [])
	if not isinstance(zones, list) or len(zones) > 128:
		raise ValueError("clearance_zones must contain at most 128 zones")
	for zone in zones:
		if not isinstance(zone, dict) or zone.get("shape") != "rectangle":
			raise ValueError("clearance zones require explicit rectangle shapes")
		center = _vec(zone.get("center"), 2, "clearance center")
		width, depth = _number(zone.get("width"), "clearance width"), _number(zone.get("depth"), "clearance depth")
		heading = _number(zone.get("heading", 0), "clearance heading")
		if width <= 0 or depth <= 0:
			raise ValueError("clearance zone width/depth must be positive")
		c, s = math.cos(math.radians(heading)), math.sin(math.radians(heading))
		corners = [(center[0] + x * c - y * s, center[1] + x * s + y * c)
				  for x, y in ((-width / 2, -depth / 2), (width / 2, -depth / 2),
							(-width / 2, depth / 2), (width / 2, depth / 2))]
		zone_min = [min(point[axis] for point in corners) for axis in range(2)]
		zone_max = [max(point[axis] for point in corners) for axis in range(2)]
		for row in selected:
			box = by_id[int(row["id"])]
			if zone.get("z_min") is not None and box["max"][2] < _number(zone["z_min"], "clearance z_min"):
				continue
			if zone.get("z_max") is not None and box["min"][2] > _number(zone["z_max"], "clearance z_max"):
				continue
			if zone_max[0] > box["min"][0] and zone_min[0] < box["max"][0] and zone_max[1] > box["min"][1] and zone_min[1] < box["max"][1]:
				report(_issue("specified_clearance_blocked", "A visual AABB intersects the caller's keep-clear rectangle; actual route clearance remains unverified.", zone=zone.get("name", "unnamed"), id=row["id"]))
	if omitted_diagnostics:
		diagnostics.append(_issue("diagnostics_truncated", "Diagnostic output was capped; omitted findings remain unreported.",
							  "warning", omitted_count=omitted_diagnostics,
							  pair_checks={"checked": pairs_checked, "total": all_pairs}))
	return diagnostics


def validate_composition(service, **params):
	inspection = _inspect(service)
	revision = int(inspection["revision"])
	if params.get("expected_revision") is not None and _int(params["expected_revision"], "expected_revision", 0) != revision:
		raise ValueError("stale document revision for composition validation")
	objects = _select(inspection["document"], ids=params.get("ids"), groups=params.get("groups"),
					  world=params.get("world"), interior=params.get("interior"))
	result = bounds(service, ids=[int(row["id"]) for row in objects], world=params.get("world"), interior=params.get("interior"))
	diagnostics = _composition_issues(result, objects, params)
	if int(_inspect(service)["revision"]) != revision:
		raise ValueError("SA-MP document changed during composition validation")
	return {"revision": revision, "valid": not any(item.get("severity") == "error" for item in diagnostics),
			"object_count": len(objects), "diagnostics": diagnostics,
			"visual_bounds": result["visual"], "collision_bounds": result["collision"],
			"scope": "SA-MP records only; AABB heuristics are warnings, not collision or walkability proof"}


def capture_views(service, *, output_dir, ids=None, groups=None, world=None, interior=None, fov=58.0, poses=None):
	"""Frame SA-MP visual bounds with revision-pinned, temporary camera poses."""
	inspection = _inspect(service)
	revision, document = int(inspection["revision"]), inspection["document"]
	if world is None:
		world = int(document.get("preview", {}).get("world", -1))
	if interior is None:
		interior = int(document.get("preview", {}).get("interior", -1))
	world, interior = _int(world, "world", -1, 2147483647), _int(interior, "interior", -1, 2147483647)
	objects = _select(document, ids=ids, groups=groups, world=world, interior=interior)
	if not objects:
		raise ValueError("capture_views selection contains no objects for the requested filters")
	box = bounds(service, ids=[int(row["id"]) for row in objects], world=world, interior=interior)
	if not box["visual"]["available"]:
		raise ValueError("capture_views requires measured visual bounds")
	center, minimum, maximum = box["visual"]["center"], box["visual"]["min"], box["visual"]["max"]
	span = max(10.0, *(float(value) for value in box["visual"]["size"]))
	view_poses = {
		"overview": {"position": [center[0] - span * 0.9, center[1] - span * 0.9, maximum[2] + span * 1.35], "target": center},
		"plan": {"position": [center[0], center[1], maximum[2] + span * 1.5], "target": center, "up": [0.0, 1.0, 0.0]},
		"interior": {"position": [center[0] - span * 0.18, center[1] - span * 0.25, minimum[2] + min(1.7, span * 0.2)], "target": center},
	}
	if poses is not None:
		if not isinstance(poses, dict) or any(name not in view_poses or not isinstance(value, dict) for name, value in poses.items()):
			raise ValueError("poses may override overview, plan, and interior")
		for name, value in poses.items():
			view_poses[name] = {**view_poses[name], **value}
	fov = _number(fov, "fov")
	if not 10 <= fov <= 120:
		raise ValueError("fov must be between 10 and 120 degrees")
	initial_camera = service.engine("camera_context")["camera"]
	camera_revision = int(initial_camera["camera_revision"])
	output = Path(output_dir).resolve() / f"samp-review-{uuid.uuid4().hex[:12]}"
	output.mkdir(parents=True, exist_ok=False)
	captures = []
	for name, pose in view_poses.items():
		position, target = _vec(pose.get("position"), 3, f"{name}.position"), _vec(pose.get("target"), 3, f"{name}.target")
		up = _vec(pose.get("up", [0, 0, 1]), 3, f"{name}.up")
		pose_fov = _number(pose.get("fov", fov), f"{name}.fov")
		if not 10 <= pose_fov <= 120:
			raise ValueError(f"{name}.fov must be between 10 and 120 degrees")
		if math.sqrt(sum((target[i] - position[i]) ** 2 for i in range(3))) < 0.001:
			raise ValueError(f"{name} camera position and target must differ")
		result = service.client.capture_at_pose(output / f"{name}.png", position=position, target=target, up=up,
											   fov=pose_fov, label=f"samp-{name}",
											   expected_camera_revision=camera_revision,
											   samp_filters=(world, interior), expected_document_revision=revision)
		if int(result.get("document_revision", -1)) != revision:
			raise ValueError("capture engine did not confirm the pinned SA-MP document revision")
		camera_revision = int(result.get("camera_revision", camera_revision))
		if int(_inspect(service)["revision"]) != revision:
			raise ValueError("SA-MP document changed during capture; review is incomplete")
		captures.append({**result, "view": name})
	manifest = {"schema": 1, "kind": "ariane_samp_capture_views", "revision": revision,
				"filters": {"world": world, "interior": interior}, "selection": {"ids": ids, "groups": groups},
				"bounds": box, "asset_diagnostics": box["diagnostics"], "views": captures,
				"environment_changed": False, "camera_restored": all(row.get("restored") for row in captures)}
	manifest_path = output / "capture-manifest.json"
	manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
	return {**manifest, "manifest_path": str(manifest_path), "output_directory": str(output)}
