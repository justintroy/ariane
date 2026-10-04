"""Compatibility facade for the SA-MP authoring helper modules."""

from __future__ import annotations

if __package__:
	from .samp_authoring_common import *
	from .samp_authoring_plan import (
		_apply_patch, _object, _patch_target, _plan_targets,
		_record_target, _validate_material, _validate_target_material,
		resolve_plan,
	)
	from .samp_authoring_apply import _expected_document, apply_plan
	from .samp_authoring_layout import _layout_items
	from .samp_authoring_review import (
		_bounded_diagnostics, _composition_issues, authoring_schema, bounds,
		validate_composition, capture_views,
	)
	from .samp_authoring_groups import (
		_transform_position, group_inspect, group_transform, group_clone,
		group_delete, material_bulk,
	)
else:
	from samp_authoring_common import *
	from samp_authoring_plan import (
		_apply_patch, _object, _patch_target, _plan_targets,
		_record_target, _validate_material, _validate_target_material,
		resolve_plan,
	)
	from samp_authoring_apply import _expected_document, apply_plan
	from samp_authoring_layout import _layout_items
	from samp_authoring_review import (
		_bounded_diagnostics, _composition_issues, authoring_schema, bounds,
		validate_composition, capture_views,
	)
	from samp_authoring_groups import (
		_transform_position, group_inspect, group_transform, group_clone,
		group_delete, material_bulk,
	)
