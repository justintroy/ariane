"""Atomic source-group and bulk-material authoring operations."""

from __future__ import annotations

if __package__:
	from .samp_authoring_common import (_euler, _group, _inspect, _int, _issue, _matrix, _mm, _mv, _number, _select, _vec)
	from .samp_authoring_plan import (_apply_patch, _validate_material, _validate_target_material)
	from .samp_authoring_review import bounds
else:
	from samp_authoring_common import (_euler, _group, _inspect, _int, _issue, _matrix, _mm, _mv, _number, _select, _vec)
	from samp_authoring_plan import (_apply_patch, _validate_material, _validate_target_material)
	from samp_authoring_review import bounds

def group_inspect(service, group, *, world=None, interior=None):
	group = _group(group)
	inspection = _inspect(service)
	document, revision = inspection["document"], int(inspection["revision"])
	objects = [row for row in document.get("objects", []) if row.get("group") == group]
	removals = [row for row in document.get("removals", []) if row.get("group") == group]
	measured = bounds(service, groups=[group], world=world, interior=interior) if objects else None
	if int(_inspect(service)["revision"]) != revision:
		raise ValueError("SA-MP document changed during group inspection")
	return {"group": group, "revision": revision, "object_count": len(objects),
			"removal_count": len(removals), "object_ids": [row["id"] for row in objects],
			"removal_ids": [row["id"] for row in removals], "bounds": measured,
			"group_retained_when_empty": group in document.get("groups", [])}


def _transform_position(position, pivot, rotation_delta, translation):
	rotated = _mv(_matrix(rotation_delta), [position[i] - pivot[i] for i in range(3)])
	return [pivot[i] + rotated[i] + translation[i] for i in range(3)]


def group_transform(service, group, *, expected_revision, pivot, rotation_delta=(0, 0, 0), translation=(0, 0, 0)):
	group = _group(group)
	inspection = _inspect(service)
	revision = _int(expected_revision, "expected_revision", 0)
	if int(inspection["revision"]) != revision:
		raise ValueError("stale document revision")
	pivot = _vec(pivot, 3, "pivot")
	delta, translation = _vec(rotation_delta, 3, "rotation_delta"), _vec(translation, 3, "translation")
	rotation_matrix = _matrix(delta)
	objects = [row for row in inspection["document"].get("objects", []) if row.get("group") == group]
	removals = [row for row in inspection["document"].get("removals", []) if row.get("group") == group]
	if len(objects) + len(removals) > 4096:
		raise ValueError("group transform exceeds the 4096-operation limit")
	operations = []
	for row in objects:
		position = _vec(row["position"], 3, "position")
		rotation = _vec(row.get("rotation", [0, 0, 0]), 3, "rotation")
		operations.append({"op": "update", "id": int(row["id"]), "changes": {
			"position": _transform_position(position, pivot, delta, translation),
			"rotation": _euler(_mm(rotation_matrix, _matrix(rotation)))}})
	for row in removals:
		operations.append({"op": "update", "id": int(row["id"]), "kind": "removals",
						   "changes": {"position": _transform_position(_vec(row["position"], 3, "position"), pivot, delta, translation)}})
	return _apply_patch(service, operations, revision)


def group_clone(service, group, new_group, key_prefix, *, expected_revision,
				pivot=(0, 0, 0), rotation_delta=(0, 0, 0), translation=(0, 0, 0)):
	group, new_group = _group(group), _group(new_group, "new_group")
	key_prefix = _key(key_prefix, "key_prefix")
	inspection = _inspect(service)
	revision = _int(expected_revision, "expected_revision", 0)
	if int(inspection["revision"]) != revision:
		raise ValueError("stale document revision")
	records = [("objects", row) for row in inspection["document"].get("objects", []) if row.get("group") == group]
	records += [("removals", row) for row in inspection["document"].get("removals", []) if row.get("group") == group]
	if not records or len(records) > 2048:
		raise ValueError("group clone requires 1-2048 records")
	pivot, delta, translation = _vec(pivot, 3, "pivot"), _vec(rotation_delta, 3, "rotation_delta"), _vec(translation, 3, "translation")
	rotation_matrix = _matrix(delta)
	operations, keys = [], []
	for index, (kind, row) in enumerate(records):
		key = _key(f"{key_prefix}.{index:04d}", "clone key")
		operations.append({"op": "duplicate", "key": key, "id": int(row["id"]), "kind": kind})
		changes = {"group": new_group,
				   "position": _transform_position(_vec(row["position"], 3, "position"), pivot, delta, translation)}
		if kind == "objects":
			changes["rotation"] = _euler(_mm(rotation_matrix, _matrix(row.get("rotation", [0, 0, 0]))))
		operations.append({"op": "update", "id": {"ref": key}, "kind": kind, "changes": changes})
		keys.append(key)
	result = _apply_patch(service, operations, revision, groups=[new_group])
	return {**result, "group": new_group, "clone_keys": keys}


def group_delete(service, group, *, expected_revision):
	group = _group(group)
	inspection = _inspect(service)
	revision = _int(expected_revision, "expected_revision", 0)
	if int(inspection["revision"]) != revision:
		raise ValueError("stale document revision")
	objects = [row for row in inspection["document"].get("objects", []) if row.get("group") == group]
	removals = [row for row in inspection["document"].get("removals", []) if row.get("group") == group]
	if len(objects) + len(removals) > 4096:
		raise ValueError("group delete exceeds the 4096-operation limit")
	operations = [{"op": "delete", "id": int(row["id"])} for row in objects]
	operations += [{"op": "delete", "id": int(row["id"]), "kind": "removals"} for row in removals]
	if not operations:
		return {"revision": revision, "group": group, "deleted": 0,
				"group_retained": group in inspection["document"].get("groups", [])}
	result = _apply_patch(service, operations, revision)
	return {**result, "group": group, "deleted": len(operations),
			"group_retained": group in inspection["document"].get("groups", [])}


def material_bulk(service, *, expected_revision, slot, material, ids=None, groups=None):
	if ids is None and groups is None:
		raise ValueError("material_bulk requires ids or groups")
	inspection = _inspect(service)
	revision = _int(expected_revision, "expected_revision", 0)
	if int(inspection["revision"]) != revision:
		raise ValueError("stale document revision")
	objects = _select(inspection["document"], ids=ids, groups=groups)
	if not objects:
		raise ValueError("material_bulk selection is empty")
	setting = _validate_material(material, "material")
	slot = _int(slot, "material slot", 0, 15)
	cache, texture_cache, diagnostics = {}, {}, []
	for row in objects:
		_validate_target_material(service, row, slot, setting, cache, diagnostics, int(row["id"]), texture_cache)
	errors = [item for item in diagnostics if item.get("severity") == "error"]
	if errors:
		codes = ", ".join(sorted({item["code"] for item in errors}))
		raise ValueError(f"material_bulk preflight failed: {codes}")
	operations = [{"op": "material", "id": int(row["id"]), "slot": slot, "material": setting} for row in objects]
	result = _apply_patch(service, operations, revision)
	return {**result, "selected_ids": [int(row["id"]) for row in objects], "slot": slot}
