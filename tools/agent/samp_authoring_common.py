"""Shared validation, rotation, inspection, and SA-MP selection helpers."""

from __future__ import annotations

import hashlib
import json
import math
import re

PLAN_SCHEMA = 1
RESOLVED_KIND = "ariane.samp.resolved-plan"
MAX_OPERATIONS = 4096
MAX_SELECTED_RECORDS = 4096
MAX_LAYOUT_ITEMS = 2048
KEY_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9._:/-]{0,127}\Z")
DEFAULTS = {"world": -1, "interior": -1, "player": -1, "stream": 300.0,
			"draw": 0.0, "area": -1, "priority": 0}
__all__ = ["PLAN_SCHEMA", "RESOLVED_KIND", "MAX_OPERATIONS", "MAX_SELECTED_RECORDS",
		   "MAX_LAYOUT_ITEMS", "DEFAULTS", "_number", "_vec", "_int", "_key",
		   "_group", "_matrix", "_mv", "_mm", "_euler", "_transformed_bounds",
		   "_hash", "_engine_samp", "_inspect", "_issue", "_select", "_model_info"]


def _number(value, name):
	if isinstance(value, bool) or not isinstance(value, (int, float)):
		raise ValueError(f"{name} must be a JSON number")
	try:
		result = float(value)
	except (TypeError, ValueError, OverflowError) as error:
		raise ValueError(f"{name} must be a finite number") from error
	if not math.isfinite(result):
		raise ValueError(f"{name} must be a finite number")
	return result


def _vec(value, length, name):
	if not isinstance(value, (list, tuple)) or len(value) != length:
		raise ValueError(f"{name} must contain {length} numbers")
	return [_number(component, name) for component in value]


def _int(value, name, minimum=None, maximum=None):
	if isinstance(value, bool) or not isinstance(value, (int, float)):
		raise ValueError(f"{name} must be an integer")
	try:
		result = int(value)
		if float(value) != result:
			raise ValueError(f"{name} must be an integer")
	except (TypeError, ValueError, OverflowError) as error:
		raise ValueError(f"{name} must be an integer") from error
	if (minimum is not None and result < minimum) or (maximum is not None and result > maximum):
		raise ValueError(f"{name} is outside its supported range")
	return result


def _key(value, name="key"):
	if not isinstance(value, str) or not KEY_RE.fullmatch(value):
		raise ValueError(f"{name} must be 1-128 ASCII identifier characters")
	return value


def _group(value, name="group"):
	if not isinstance(value, str) or not value or len(value.encode("utf-8")) > 512 or "\x00" in value:
		raise ValueError(f"{name} must be a non-empty string of at most 512 UTF-8 bytes")
	return value


def _matrix(rotation):
	"""SA-MP render order Rz * Rx * Ry, matching samp_rotation.cpp."""
	rx, ry, rz = (math.radians(item) for item in _vec(rotation, 3, "rotation"))
	cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
	return [[cz * cy - sz * sx * sy, -sz * cx, cz * sy + sz * sx * cy],
			[sz * cy + cz * sx * sy, cz * cx, sz * sy - cz * sx * cy],
			[-cx * sy, sx, cx * cy]]


def _mv(matrix, vector):
	return [sum(matrix[row][column] * vector[column] for column in range(3)) for row in range(3)]


def _mm(a, b):
	return [[sum(a[row][k] * b[k][column] for k in range(3)) for column in range(3)] for row in range(3)]


def _euler(matrix):
	"""Inverse SA-MP Rz*Rx*Ry extraction, including its gimbal fallback."""
	x = math.asin(max(-1.0, min(1.0, matrix[2][1])))
	if abs(math.cos(x)) > 1.0e-6:
		y = math.atan2(-matrix[2][0], matrix[2][2])
		z = math.atan2(-matrix[0][1], matrix[1][1])
	else:
		y = 0.0
		z = math.atan2(matrix[1][0], matrix[0][0])
	return [math.degrees(x), math.degrees(y), math.degrees(z)]


def _transformed_bounds(bounds, position, rotation):
	if not isinstance(bounds, dict) or not bounds.get("available"):
		return {"available": False, "source": bounds.get("source") if isinstance(bounds, dict) else None}
	minimum, maximum = _vec(bounds.get("min"), 3, "bounds.min"), _vec(bounds.get("max"), 3, "bounds.max")
	if any(minimum[i] > maximum[i] for i in range(3)):
		raise ValueError("model bounds have min greater than max")
	base, transform = _vec(position, 3, "position"), _matrix(rotation)
	points = [_mv(transform, [x, y, z]) for x in (minimum[0], maximum[0])
			  for y in (minimum[1], maximum[1]) for z in (minimum[2], maximum[2])]
	world_min = [min(point[i] for point in points) + base[i] for i in range(3)]
	world_max = [max(point[i] for point in points) + base[i] for i in range(3)]
	return {"available": True, "min": world_min, "max": world_max,
			"size": [world_max[i] - world_min[i] for i in range(3)],
			"source": bounds.get("source"), "space": "world_aabb_from_model_local_bounds"}


def _hash(document):
	canonical = json.dumps(document, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
	return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _engine_samp(service, operation, **params):
	return service._samp_engine(operation, **params)


def _inspect(service, include_history=False):
	return _engine_samp(service, "inspect", include_history=include_history)


def _issue(code, message, severity="warning", **fields):
	return {"code": code, "message": message, "severity": severity, **fields}


def _select(document, *, ids=None, groups=None, world=None, interior=None):
	if ids is not None and (not isinstance(ids, list) or len(ids) > MAX_SELECTED_RECORDS):
		raise ValueError("ids must be a list of at most 4096 stable IDs")
	if groups is not None and (not isinstance(groups, list) or len(groups) > 4096):
		raise ValueError("groups must be a list of at most 4096 source groups")
	selected_ids = None if ids is None else {_int(item, "stable ID", 1, 2147483646) for item in ids}
	if groups is not None and any(not isinstance(group, str) or not group or len(group.encode("utf-8")) > 512 for group in groups):
		raise ValueError("group names must be non-empty strings")
	selected_groups = None if groups is None else set(groups)
	if world is not None:
		world = _int(world, "world", -1, 2147483647)
	if interior is not None:
		interior = _int(interior, "interior", -1, 2147483647)
	objects = []
	for row in document.get("objects", []):
		if selected_ids is not None and row.get("id") not in selected_ids:
			continue
		if selected_groups is not None and row.get("group") not in selected_groups:
			continue
		if world is not None and world != -1 and row.get("world", -1) not in (-1, world):
			continue
		if interior is not None and interior != -1 and row.get("interior", -1) not in (-1, interior):
			continue
		objects.append(row)
	if selected_ids is not None:
		missing = selected_ids - {row.get("id") for row in objects}
		if missing:
			raise ValueError(f"unknown or filtered stable IDs: {sorted(missing)}")
	if len(objects) > MAX_SELECTED_RECORDS:
		raise ValueError("selection exceeds 4096 SA-MP objects")
	return objects


def _model_info(service, model, cache):
	if model not in cache:
		cache[model] = _engine_samp(service, "model_info", model=model)
	return cache[model]
