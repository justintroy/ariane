"""Revision-pinned atomic SA-MP plan application and uncertain-result recovery."""

from __future__ import annotations

import copy
import json

if __package__:
	from .samp_authoring_common import *
else:
	from samp_authoring_common import *

def _expected_document(document, operations, groups):
	"""Simulate the documented record edits for lost-response recovery only."""
	result = copy.deepcopy(document)
	for group in groups:
		if group not in result["groups"]:
			result["groups"].append(group)
	key_ids = {}
	for original in operations:
		operation = copy.deepcopy(original)
		action = operation.pop("op")
		key = operation.pop("key", None)
		if isinstance(operation.get("id"), dict):
			key_ref = operation["id"].get("ref")
			if key_ref not in key_ids:
				raise ValueError(f"patch reference is not defined before use: {key_ref}")
			operation["id"] = key_ids[key_ref]
		created_id = int(result["next_id"])
		if action == "place":
			row = {"id": created_id, "model": 0, "position": [0.0, 0.0, 0.0],
				   "rotation": [0.0, 0.0, 0.0], "world": -1, "interior": -1,
				   "player": -1, "stream": 300.0, "draw": 0.0, "area": -1,
				   "priority": 0, "group": "untitled", "materials": {}}
			row.update(operation["object"])
			row["id"] = created_id
			if row["group"] not in result["groups"]:
				result["groups"].append(row["group"])
			result["objects"].append(row)
			result["next_id"] += 1
		elif action == "removal":
			row = copy.deepcopy(operation["removal"])
			row.setdefault("group", "untitled")
			row["id"] = created_id
			if row["group"] not in result["groups"]:
				result["groups"].append(row["group"])
			result["removals"].append(row)
			result["next_id"] += 1
		elif action == "duplicate":
			kind = operation.get("kind", "objects")
			source = next(row for row in result[kind] if int(row["id"]) == int(operation["id"]))
			row = copy.deepcopy(source)
			row["id"] = created_id
			result[kind].append(row)
			result["next_id"] += 1
		elif action in ("update", "material", "delete"):
			kind = operation.get("kind", "objects")
			index = next(i for i, row in enumerate(result[kind]) if int(row["id"]) == int(operation["id"]))
			row = result[kind][index]
			if action == "update":
				row.update(operation["changes"])
			elif action == "delete":
				result[kind].pop(index)
			else:
				materials = row.setdefault("materials", {})
				if operation["material"] is None:
					materials.pop(str(operation["slot"]), None)
				else:
					materials[str(operation["slot"])] = operation["material"]
		else:
			raise ValueError(f"unsupported operation in resolved plan: {action}")
		if key is not None:
			key_ids[key] = created_id
	return result, key_ids


def apply_plan(service, resolved_plan, *, expected_revision=None):
	"""Apply one patch and recover only from inspection-proven retry states."""
	if not isinstance(resolved_plan, dict) or resolved_plan.get("schema") != PLAN_SCHEMA or resolved_plan.get("kind") != RESOLVED_KIND:
		raise ValueError("apply_plan requires a schema 1 resolved plan")
	if not resolved_plan.get("valid"):
		raise ValueError("resolved plan contains errors; repair diagnostics before applying")
	revision = _int(resolved_plan.get("expected_revision"), "expected_revision", 0, 2147483647)
	if expected_revision is not None and _int(expected_revision, "expected_revision", 0) != revision:
		raise ValueError("expected_revision does not match the resolved plan")
	operations, groups = resolved_plan.get("operations"), resolved_plan.get("groups")
	if not isinstance(operations, list) or len(operations) > MAX_OPERATIONS or not isinstance(groups, list) or len(groups) > 4096:
		raise ValueError("resolved plan exceeds patch limits")
	if service.engine("session_status").get("active") is not True:
		raise RuntimeError("begin a scratch session before applying an SA-MP plan")
	before = _inspect(service)
	if int(before["revision"]) != revision or _hash(before["document"]) != resolved_plan.get("base_document_hash"):
		raise ValueError("stale resolved plan; inspect and resolve at the current revision")
	expected_document, references = _expected_document(before["document"], operations, groups)
	request = {"op": "patch", "operations": operations, "groups": groups, "expected_revision": revision}
	def submit():
		return service.engine("samp", [json.dumps(request, separators=(",", ":"))])["samp"]
	try:
		result = submit()
		return {"revision": result["revision"], "references": result.get("references", {}),
				"recovered": False, "retried": False, "applied": len(operations), "document": result.get("document")}
	except Exception as error:
		# Structured engine rejection means the request reached validation; do not replay it.
		if getattr(error, "response", {}).get("ok") is False:
			raise
		try:
			observed = _inspect(service, include_history=True)
		except Exception:
			raise error
		observed_revision = int(observed["revision"])
		if observed_revision == revision and _hash(observed["document"]) == resolved_plan.get("base_document_hash"):
			try:
				result = submit()
				return {"revision": result["revision"], "references": result.get("references", {}),
						"recovered": False, "retried": True, "applied": len(operations), "document": result.get("document")}
			except Exception as retry_error:
				if getattr(retry_error, "response", {}).get("ok") is False:
					raise
				error = retry_error
				try:
					observed = _inspect(service, include_history=True)
				except Exception:
					raise error
				observed_revision = int(observed["revision"])
		if observed_revision == revision + 1 and observed.get("document") == expected_document:
			history = observed.get("history", [])
			if history and int(history[-1].get("revision", -1)) == observed_revision and history[-1].get("label") == "SA-MP patch":
				return {"revision": observed_revision, "references": references, "recovered": True,
						"retried": False, "applied": len(operations), "document": observed["document"],
						"recovery": "verified exact document and one SA-MP patch history entry"}
		raise RuntimeError(f"plan result is uncertain at revision {observed_revision}; inspect document before retrying: {error}") from error
