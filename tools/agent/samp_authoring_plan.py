"""Schema-1 SA-MP plan validation, symbolic resolution, and atomic apply."""

from __future__ import annotations

import copy
import json

if __package__:
	from .samp_authoring_common import *
	from .samp_authoring_layout import _layout_items
else:
	from samp_authoring_common import *
	from samp_authoring_layout import _layout_items

def _validate_material(material, label):
	if not isinstance(material, dict):
		raise ValueError(f"{label} must be a material object")
	if material.get("type") == "texture":
		model = _int(material.get("model"), f"{label}.model", -1, 39999)
		txd, texture = material.get("txd"), material.get("texture")
		if not isinstance(txd, str) or not txd or not isinstance(texture, str) or not texture:
			raise ValueError(f"{label} requires non-empty TXD and texture names")
		return {"type": "texture", "model": model, "txd": txd, "texture": texture,
				"color": _int(material.get("color"), f"{label}.color", 0, 4294967295)}
	if material.get("type") == "text":
		text, font, bold = material.get("text"), material.get("font"), material.get("bold", False)
		if not isinstance(text, str) or not isinstance(font, str) or not isinstance(bold, bool):
			raise ValueError(f"{label} text/font/bold fields have invalid types")
		size = _int(material.get("size", 90), f"{label}.size", 10, 140)
		font_size = _int(material.get("font_size", 24), f"{label}.font_size", 1, 255)
		align = _int(material.get("align", 1), f"{label}.align", 0, 2)
		if size % 10:
			raise ValueError(f"{label}.size must be a multiple of 10")
		return {"type": "text", "text": text, "font": font, "bold": bold,
				"size": size, "font_size": font_size, "align": align,
				"foreground": _int(material.get("foreground", 0xFFFFFFFF), f"{label}.foreground", 0, 4294967295),
				"background": _int(material.get("background", 0xFF000000), f"{label}.background", 0, 4294967295)}
	raise ValueError(f"{label}.type must be texture or text")


def _record_target(target, key_records, document):
	if isinstance(target, str):
		if target not in key_records:
			raise ValueError(f"unknown plan key reference: {target}")
		return key_records[target]
	ident = _int(target, "stable ID", 1, 2147483646)
	for row in (*document.get("objects", []), *document.get("removals", [])):
		if int(row["id"]) == ident:
			return row
	raise ValueError(f"unknown stable ID: {ident}")


def _patch_target(target, references, document):
	if isinstance(target, str):
		if target not in references:
			raise ValueError(f"unknown plan key reference: {target}")
		return {"ref": target}
	ident = _int(target, "stable ID", 1, 2147483646)
	_record_target(ident, {}, document)
	return ident


def _object(model, position, rotation, fields):
	row = {"model": _int(model, "model", 0, 39999), "position": _vec(position, 3, "position"),
		   "rotation": _vec(rotation, 3, "rotation"), "group": _group(fields.get("group", "untitled")),
		   "materials": {}}
	for field, default in DEFAULTS.items():
		value = fields.get(field, default)
		if field in ("world", "interior"):
			row[field] = _int(value, field, -1, 2147483647)
		elif field in ("player", "area", "priority"):
			row[field] = _int(value, field, -2147483648, 2147483647)
		else:
			row[field] = _number(value, field)
			if row[field] < 0:
				raise ValueError(f"{field} must be non-negative")
	return row


def _check_model(service, model, cache, diagnostics, key, *, visual_required=False):
	info = _model_info(service, model, cache)
	codes = {row.get("code") for row in info.get("diagnostics", [])}
	provenance = info.get("provenance")
	if not isinstance(provenance, dict):
		diagnostics.append(_issue("model_provenance_unavailable", "Installation provenance was not reported; model usability cannot be verified.", "error", key=key, model=model))
	elif any(provenance.get(field) is not True for field in ("defined", "installed", "renderable")):
		diagnostics.append(_issue("model_unavailable", "Model is not defined, installed, and renderable in this installation.", "error", key=key, model=model, provenance=provenance))
	if "model_missing" in codes:
		diagnostics.append(_issue("model_unavailable", "Model is not available in this installation.", "error", key=key, model=model))
	if not info.get("visual_bounds", {}).get("available"):
		severity = "error" if visual_required else "warning"
		diagnostics.append(_issue("visual_bounds_unavailable", "Visual bounds are unavailable; no dimensions were inferred.", severity, key=key, model=model))
	return info


def _validate_target_material(service, record, slot, material, cache, diagnostics, target, texture_cache=None):
	model = int(record["model"])
	info = _check_model(service, model, cache, diagnostics, str(target))
	slots = {int(item["slot"]) for item in info.get("material_slots", [])
			 if isinstance(item, dict) and "slot" in item}
	if not slots:
		diagnostics.append(_issue("material_slot_mapping_unavailable", "No actual material slots were reported for this model; override cannot be verified.", "error", target=target, model=model, slot=slot))
	elif slot not in slots:
		diagnostics.append(_issue("material_slot_unresolved", "Override targets no inspected surface slot on this model.", "error", target=target, model=model, slot=slot))
	for item in info.get("diagnostics", []):
		if item.get("code") in ("slot_mapping_unavailable", "material_slot_mapping_unavailable"):
			diagnostics.append(_issue("material_slot_mapping_unavailable", item.get("message", "Material slot mapping is unsupported."), "error", target=target, model=model, slot=slot))
	if material["type"] == "texture" and material["model"] != -1:
		texture_cache = texture_cache if texture_cache is not None else {}
		texture_key = (material["model"], material["txd"].casefold(), material["texture"].casefold())
		if texture_key not in texture_cache:
			query = {"txd": material["txd"], "name": material["texture"], "limit": 128}
			if material["model"] >= 0:
				query["model"] = material["model"]
			result = _engine_samp(service, "textures", **query)
			matches = [row for row in result.get("textures", [])
					   if str(row.get("txd", "")).casefold() == texture_key[1]
					   and str(row.get("texture", "")).casefold() == texture_key[2]
					   and row.get("source_valid") is not False
					   and (material["model"] < 0 or row.get("model") == material["model"])]
			if not matches:
				complete = result.get("complete") is True and not result.get("next_cursor")
				code = "texture_missing" if complete else "texture_unverified"
				message = "Texture tuple is absent from the completed local index." if code == "texture_missing" else "Texture index has not verified this source tuple; wait for indexing or correct the source."
				texture_cache[texture_key] = (code, message)
			elif not any(row.get("available") is True or row.get("usable") is True for row in matches):
				texture_cache[texture_key] = ("texture_unavailable", "Texture was indexed but its image is unavailable.")
			else:
				texture_cache[texture_key] = None
		cached_issue = texture_cache[texture_key]
		if cached_issue:
			diagnostics.append(_issue(cached_issue[0], cached_issue[1], "error", target=target, slot=slot,
									  model=material["model"], txd=material["txd"], texture=material["texture"]))
	return diagnostics


def _target_position(record, anchor, offset, space="world"):
	shift = _vec(offset, 3, "relative offset")
	if space == "local":
		shift = _mv(_matrix(anchor.get("rotation", [0, 0, 0])), shift)
	elif space != "world":
		raise ValueError("offset_space must be world or local")
	return [float(anchor["position"][i]) + shift[i] for i in range(3)]


def _pose(record, transform):
	position = _vec(record["position"], 3, "position")
	rotation = _vec(record.get("rotation", [0, 0, 0]), 3, "rotation")
	if "position" in transform:
		position = _vec(transform["position"], 3, "position")
	if "rotation" in transform:
		rotation = _vec(transform["rotation"], 3, "rotation")
	delta = _vec(transform.get("rotation_delta", [0, 0, 0]), 3, "rotation_delta")
	pivot = _vec(transform.get("pivot", position), 3, "pivot")
	translation = _vec(transform.get("translation", [0, 0, 0]), 3, "translation")
	if any(abs(item) > 0 for item in delta):
		matrix = _matrix(delta)
		position = [pivot[i] + _mv(matrix, [position[j] - pivot[j] for j in range(3)])[i] + translation[i] for i in range(3)]
		rotation = _euler(_mm(matrix, _matrix(rotation)))
	else:
		position = [position[i] + translation[i] for i in range(3)]
	return {"position": position, "rotation": rotation}


def _limit(operations):
	if len(operations) > MAX_OPERATIONS:
		raise ValueError("plan expands to more than 4096 patch operations")


def _append_operation(operations, operation):
	if len(operations) >= MAX_OPERATIONS:
		raise ValueError("plan expands beyond the 4096-operation limit")
	operations.append(operation)


_UNSUPPORTED_GEOMETRY_FIELDS = {
	"scale", "support", "snap", "boolean", "boolean_ops", "mesh", "mesh_ops",
	"mesh_operations", "geometry_ops",
}


def _reject_unsupported_geometry(plan):
	for key in plan:
		if isinstance(key, str) and key.casefold() in _UNSUPPORTED_GEOMETRY_FIELDS:
			raise ValueError(f"plan.{key} is unsupported; use explicit positions/rotations and relative_to offsets")
	records = []
	objects = plan.get("objects", [])
	if isinstance(objects, dict):
		records.extend((f"objects[{name!r}]", row) for name, row in objects.items())
	elif isinstance(objects, list):
		records.extend((f"objects[{index}]", row) for index, row in enumerate(objects))
	for section in ("layouts", "duplicates", "transforms"):
		items = plan.get(section, [])
		if isinstance(items, list):
			records.extend((f"{section}[{index}]", row) for index, row in enumerate(items))
	for path, record in records:
		if not isinstance(record, dict):
			continue
		for key in record:
			if isinstance(key, str) and key.casefold() in _UNSUPPORTED_GEOMETRY_FIELDS:
				raise ValueError(f"{path}.{key} is unsupported; use explicit positions/rotations and relative_to offsets")



def resolve_plan(service, plan):
	"""Expand a bounded plan read-only and return exact patch operations."""
	if not isinstance(plan, dict) or plan.get("schema", PLAN_SCHEMA) != PLAN_SCHEMA:
		raise ValueError("plan must be a schema 1 object")
	_reject_unsupported_geometry(plan)
	revision = _int(plan.get("expected_revision"), "expected_revision", 0, 2147483647)
	inspection = _inspect(service)
	if int(inspection["revision"]) != revision:
		raise ValueError(f"stale document revision: expected {revision}, got {inspection['revision']}")
	base_document = inspection["document"]
	document = copy.deepcopy(base_document)
	base_document_hash = _hash(base_document)
	diagnostics, operations, key_records, references, cache = [], [], {}, {}, {}
	groups = plan.get("groups", [])
	if not isinstance(groups, list) or len(groups) > 4096:
		raise ValueError("groups must contain at most 4096 names")
	group_names = {_group(name) for name in groups}
	objects = plan.get("objects", [])
	if isinstance(objects, dict):
		if len(objects) > MAX_OPERATIONS:
			raise ValueError("objects exceeds the 4096-operation limit")
		objects = [{"key": name, **fields} for name, fields in objects.items()]
	if not isinstance(objects, list):
		raise ValueError("objects must be an array or key-to-object mapping")
	if len(objects) > MAX_OPERATIONS:
		raise ValueError("objects exceeds the 4096-operation limit")
	for spec in objects:
		if not isinstance(spec, dict):
			raise ValueError("each object must be an object")
		key = _key(spec.get("key"), "object.key")
		model = _int(spec.get("model"), "model", 0, 39999)
		group = _group(spec.get("group", "untitled"))
		if "position" in spec:
			position = _vec(spec["position"], 3, "position")
		elif "relative_to" in spec:
			anchor = _record_target(spec["relative_to"], key_records, document)
			position = _target_position({}, anchor, spec.get("offset", [0, 0, 0]), spec.get("offset_space", "world"))
		else:
			raise ValueError(f"object {key} requires position or relative_to/offset")
		rotation = _vec(spec.get("rotation", [0, 0, 0]), 3, "rotation")
		record = _object(model, position, rotation, {**spec, "group": group})
		_check_model(service, model, cache, diagnostics, key)
		if key in references:
			raise ValueError(f"duplicate plan key: {key}")
		references[key] = {"kind": "objects", "model": model, "group": group}
		key_records[key] = record
		_append_operation(operations, {"op": "place", "key": key, "object": record})
		group_names.add(group)
	layouts = plan.get("layouts", [])
	if not isinstance(layouts, list) or len(layouts) > MAX_OPERATIONS:
		raise ValueError("layouts must be a list of at most 4096 layouts")
	for layout in layouts:
		if not isinstance(layout, dict):
			raise ValueError("each layout must be an object")
		model = _int(layout.get("model"), "layout.model", 0, 39999)
		prefix = _key(layout.get("key_prefix"), "key_prefix")
		group = _group(layout.get("group", "untitled"))
		info = _check_model(service, model, cache, diagnostics, prefix, visual_required=layout.get("type") == "perimeter")
		group_names.add(group)
		for key, position, rotation, fields in _layout_items(layout, info, diagnostics):
			if key in references:
				raise ValueError(f"duplicate plan key: {key}")
			record = _object(model, position, rotation, fields)
			key_records[key] = record
			references[key] = {"kind": "objects", "model": model, "group": group}
			_append_operation(operations, {"op": "place", "key": key, "object": record})
		if len(key_records) > MAX_SELECTED_RECORDS:
			raise ValueError("plan creates more than 4096 objects")
	_limit(operations)
	duplicates = plan.get("duplicates", [])
	if not isinstance(duplicates, list) or len(duplicates) > MAX_OPERATIONS:
		raise ValueError("duplicates must be a list of at most 4096 entries")
	for duplicate in duplicates:
		if not isinstance(duplicate, dict):
			raise ValueError("each duplicate must be an object")
		key, source = _key(duplicate.get("key"), "duplicate.key"), duplicate.get("source")
		if key in references:
			raise ValueError(f"duplicate plan key: {key}")
		row = _record_target(source, key_records, document)
		kind = "removals" if "radius" in row and "rotation" not in row else "objects"
		_append_operation(operations, {"op": "duplicate", "key": key, "id": _patch_target(source, references, document), "kind": kind})
		new = copy.deepcopy(row)
		new.pop("id", None)
		changes = {}
		if "group" in duplicate:
			changes["group"] = _group(duplicate["group"])
			group_names.add(changes["group"])
		if "position" in duplicate:
			changes["position"] = _vec(duplicate["position"], 3, "duplicate.position")
		if "rotation" in duplicate:
			changes["rotation"] = _vec(duplicate["rotation"], 3, "duplicate.rotation")
		if "relative_to" in duplicate:
			anchor = _record_target(duplicate["relative_to"], key_records, document)
			changes["position"] = _target_position(new, anchor, duplicate.get("offset", [0, 0, 0]), duplicate.get("offset_space", "world"))
		if changes:
			_append_operation(operations, {"op": "update", "id": {"ref": key}, "kind": kind, "changes": changes})
			new.update(changes)
		key_records[key] = new
		references[key] = {"kind": kind, "model": new.get("model"), "group": new.get("group")}
		if new.get("group"):
			group_names.add(_group(new["group"]))
	_limit(operations)
	removals = plan.get("removals", [])
	if not isinstance(removals, list) or len(removals) > MAX_OPERATIONS:
		raise ValueError("removals must be a list of at most 4096 entries")
	for removal in removals:
		if not isinstance(removal, dict):
			raise ValueError("each removal must be an object")
		key = _key(removal.get("key"), "removal.key")
		if key in references:
			raise ValueError(f"duplicate plan key: {key}")
		model = _int(removal.get("model"), "removal.model", -1, 39999)
		position = _vec(removal.get("position"), 3, "removal.position")
		radius = _number(removal.get("radius"), "removal.radius")
		if radius < 0:
			raise ValueError("removal.radius must be non-negative")
		group = _group(removal.get("group", "untitled"), "removal.group")
		row = {"model": model, "position": position, "radius": radius, "group": group}
		_append_operation(operations, {"op": "removal", "key": key, "removal": row})
		key_records[key] = row
		references[key] = {"kind": "removals", "model": model, "group": group}
		group_names.add(group)
	transforms = plan.get("transforms", [])
	if not isinstance(transforms, list) or len(transforms) > MAX_OPERATIONS:
		raise ValueError("transforms must be a list of at most 4096 entries")
	for transform in transforms:
		if not isinstance(transform, dict):
			raise ValueError("each transform must be an object")
		target = transform.get("target")
		row = _record_target(target, key_records, document)
		changes = _pose(row, transform)
		if "rotation" not in row:
			changes = {"position": changes["position"]}
		for field in ("world", "interior"):
			if field in transform:
				changes[field] = _int(transform[field], field, -1, 2147483647)
		for field in ("player", "area", "priority"):
			if field in transform:
				changes[field] = _int(transform[field], field, -2147483648, 2147483647)
		for field in ("stream", "draw"):
			if field in transform:
				changes[field] = _number(transform[field], field)
				if changes[field] < 0:
					raise ValueError(f"{field} must be non-negative")
		if "group" in transform:
			changes["group"] = _group(transform["group"])
			group_names.add(changes["group"])
		_append_operation(operations, {"op": "update", "id": _patch_target(target, references, document), "changes": changes})
		row.update(changes)
	deletions = plan.get("deletes", [])
	if not isinstance(deletions, list) or len(deletions) > MAX_OPERATIONS:
		raise ValueError("deletes must be a list of at most 4096 targets")
	pending_deletions = []
	for deletion in deletions:
		if isinstance(deletion, dict):
			target, kind = deletion.get("target"), deletion.get("kind", "objects")
		else:
			target, kind = deletion, "objects"
		if kind not in ("objects", "removals"):
			raise ValueError("delete kind must be objects or removals")
		row = _record_target(target, key_records, document)
		actual_kind = "objects" if "rotation" in row else "removals"
		if kind != actual_kind:
			raise ValueError(f"delete kind does not match target {target}")
		if len(operations) + len(pending_deletions) >= MAX_OPERATIONS:
			raise ValueError("deletions exceed the 4096-operation limit")
		pending_deletions.append({"op": "delete", "id": _patch_target(target, references, document), "kind": kind})
	palettes = plan.get("palettes", {})
	if not isinstance(palettes, dict):
		raise ValueError("palettes must map names to material settings")
	validated_palettes = {name: _validate_material(value, f"palette {name}") for name, value in palettes.items()}
	texture_cache = {}
	overrides = plan.get("overrides", [])
	if not isinstance(overrides, list) or len(overrides) > MAX_OPERATIONS:
		raise ValueError("overrides must be a list of at most 4096 entries")
	for override in overrides:
		if not isinstance(override, dict):
			raise ValueError("each override must be an object")
		target = override.get("target")
		row = _record_target(target, key_records, document)
		if "rotation" not in row:
			raise ValueError("material overrides can target objects only")
		slot = _int(override.get("slot"), "material slot", 0, 15)
		if "palette" in override:
			try:
				material = validated_palettes[override["palette"]]
			except KeyError as error:
				raise ValueError(f"unknown material palette: {override['palette']}") from error
		elif "material" in override:
			material = _validate_material(override["material"], "override.material")
		else:
			raise ValueError("override requires palette or material")
		_validate_target_material(service, row, slot, material, cache, diagnostics, target, texture_cache)
		_append_operation(operations, {"op": "material", "id": _patch_target(target, references, document), "slot": slot, "material": material})
		row.setdefault("materials", {})[str(slot)] = material
	bulk_materials = plan.get("bulk_materials", [])
	if not isinstance(bulk_materials, list) or len(bulk_materials) > MAX_OPERATIONS:
		raise ValueError("bulk_materials must be a list of at most 4096 entries")
	for bulk in bulk_materials:
		if not isinstance(bulk, dict):
			raise ValueError("bulk_materials entries must be objects")
		selected = _plan_targets(bulk, key_records, document)
		if len(operations) + len(selected) > MAX_OPERATIONS:
			raise ValueError("bulk material expansion exceeds the 4096-operation limit")
		material = _validate_material(bulk.get("material"), "bulk material")
		slot = _int(bulk.get("slot"), "material slot", 0, 15)
		for target, row in selected:
			if "rotation" not in row:
				raise ValueError("bulk material can target objects only")
			_validate_target_material(service, row, slot, material, cache, diagnostics, target, texture_cache)
			_append_operation(operations, {"op": "material", "id": _patch_target(target, references, document), "slot": slot, "material": material})
			row.setdefault("materials", {})[str(slot)] = material
	if len(operations) + len(pending_deletions) > MAX_OPERATIONS:
		raise ValueError("deletions exceed the 4096-operation limit")
	operations.extend(pending_deletions)
	_limit(operations)
	if len(group_names) > 4096:
		raise ValueError("plan contains more than 4096 groups")
	existing_ids = plan.get("existing_ids", [])
	if not isinstance(existing_ids, list) or len(existing_ids) > MAX_SELECTED_RECORDS:
		raise ValueError("existing_ids must be a list of at most 4096 IDs")
	existing = {}
	for ident in existing_ids:
		row = _record_target(ident, key_records, document)
		existing[str(_int(ident, "existing ID", 1, 2147483646))] = {"kind": "objects" if "rotation" in row else "removals", "group": row.get("group"), "model": row.get("model")}
	computed_bounds = []
	deleted_keys = {operation["id"]["ref"] for operation in pending_deletions
					if isinstance(operation.get("id"), dict) and isinstance(operation["id"].get("ref"), str)}
	for key, row in key_records.items():
		if "rotation" not in row or key in deleted_keys:
			continue
		info = _model_info(service, int(row["model"]), cache)
		computed_bounds.append({"key": key, "model": row["model"], "group": row.get("group"),
							   **_transformed_bounds(info.get("visual_bounds", {}), row["position"], row["rotation"])})
	deleted_ids = {int(operation["id"]) for operation in pending_deletions
				   if isinstance(operation.get("id"), int) and operation.get("kind", "objects") == "objects"}
	affected_ids = {int(ident) for ident in existing}
	affected_ids.update(int(operation["id"]) for operation in operations
						if operation.get("op") in ("update", "material")
						and isinstance(operation.get("id"), int)
						and operation.get("kind", "objects") == "objects")
	object_by_id = {int(row["id"]): row for row in document.get("objects", [])}
	for ident in sorted(affected_ids - deleted_ids):
		row = object_by_id.get(ident)
		if row is None:
			continue
		info = _model_info(service, int(row["model"]), cache)
		measured = _transformed_bounds(info.get("visual_bounds", {}), row["position"], row["rotation"])
		if not measured.get("available"):
			diagnostics.append(_issue("visual_bounds_unavailable", "Affected existing object has no measured visual bounds; no dimensions were inferred.", "warning", id=ident, model=row["model"]))
		computed_bounds.append({"id": ident, "model": row["model"], "group": row.get("group"), **measured})
	if int(_inspect(service)["revision"]) != revision:
		raise ValueError("SA-MP document changed while resolving plan")
	return {"schema": PLAN_SCHEMA, "kind": RESOLVED_KIND,
			"valid": not any(item.get("severity") == "error" for item in diagnostics),
			"expected_revision": revision, "base_document_hash": base_document_hash,
			"operations": operations, "groups": sorted(group_names),
			"references": references, "existing_records": existing,
			"computed_bounds": computed_bounds, "diagnostics": diagnostics,
			"operation_count": len(operations), "group_count": len(group_names)}


def _plan_targets(spec, key_records, document):
	targets = spec.get("targets")
	if targets is not None:
		if not isinstance(targets, list) or len(targets) > MAX_SELECTED_RECORDS:
			raise ValueError("targets must contain at most 4096 IDs/keys")
		return [(target, _record_target(target, key_records, document)) for target in targets]
	group = _group(spec.get("group"), "group")
	selected = [(key, row) for key, row in key_records.items() if row.get("group") == group]
	selected += [(int(row["id"]), row) for row in document.get("objects", []) if row.get("group") == group]
	if len(selected) > MAX_SELECTED_RECORDS:
		raise ValueError("bulk group selection exceeds 4096 objects")
	return selected



def _apply_patch(service, operations, revision, groups=None):
	if not operations or len(operations) > MAX_OPERATIONS:
		raise ValueError("mutation must compile to 1-4096 operations")
	status = service.engine("session_status")
	if status.get("active") is not True:
		raise RuntimeError("begin a scratch session before mutating SA-MP records")
	return _engine_samp(service, "patch", operations=operations, groups=groups or [], expected_revision=revision)
