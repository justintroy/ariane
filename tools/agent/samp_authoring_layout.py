"""Bounded SA-MP placement layout expansion using measured local bounds."""

from __future__ import annotations

import math

if __package__:
	from .samp_authoring_common import (DEFAULTS, MAX_LAYOUT_ITEMS, _euler, _group, _int, _issue, _key, _matrix, _mm, _mv, _number, _vec)
else:
	from samp_authoring_common import (DEFAULTS, MAX_LAYOUT_ITEMS, _euler, _group, _int, _issue, _key, _matrix, _mm, _mv, _number, _vec)

def _layout_items(layout, info, diagnostics):
	if not isinstance(layout, dict):
		raise ValueError("each layout must be an object")
	kind = layout.get("type")
	prefix = _key(layout.get("key_prefix"), "key_prefix")
	model = _int(layout.get("model"), "layout.model", 0, 39999)
	rotation = _vec(layout.get("rotation", [0, 0, 0]), 3, "rotation")
	group = _group(layout.get("group", "untitled"))
	common = {field: layout[field] for field in DEFAULTS if field in layout}
	common["group"] = group
	if kind == "row":
		count = _int(layout.get("count"), "row.count", 1, MAX_LAYOUT_ITEMS)
		origin = _vec(layout.get("origin"), 3, "row.origin")
		if "step" in layout:
			step = _vec(layout["step"], 3, "row.step")
		else:
			axis = _vec(layout.get("axis", [1, 0, 0]), 3, "row.axis")
			if math.sqrt(sum(value * value for value in axis)) < 1.0e-8:
				raise ValueError("row.axis must not be zero")
			spacing = _number(layout.get("spacing"), "row.spacing")
			if spacing <= 0:
				raise ValueError("row.spacing must be positive")
			step = [value * spacing for value in axis]
		world_step = _mv(_matrix(rotation), step)
		return [(f"{prefix}.{index:04d}", [origin[i] + world_step[i] * index for i in range(3)], rotation, common)
			for index in range(count)]
	if kind == "grid":
		rows = _int(layout.get("rows"), "grid.rows", 1, MAX_LAYOUT_ITEMS)
		columns = _int(layout.get("columns"), "grid.columns", 1, MAX_LAYOUT_ITEMS)
		if rows * columns > MAX_LAYOUT_ITEMS:
			raise ValueError("grid expansion exceeds 2048 objects")
		origin = _vec(layout.get("origin"), 3, "grid.origin")
		spacing = layout.get("spacing")
		if isinstance(spacing, (int, float)):
			sx = sy = _number(spacing, "grid.spacing")
		else:
			sx, sy = _vec(spacing, 2, "grid.spacing")
		if sx <= 0 or sy <= 0:
			raise ValueError("grid spacing must be positive")
		transform = _matrix(rotation)
		result = []
		for row in range(rows):
			for column in range(columns):
				local = [(column - (columns - 1) / 2) * sx,
						(row - (rows - 1) / 2) * sy, 0.0]
				shift = _mv(transform, local)
				position = [origin[i] + shift[i] for i in range(3)]
				result.append((f"{prefix}.r{row:03d}.c{column:03d}", position, rotation, common))
		return result
	if kind == "radial":
		count = _int(layout.get("count"), "radial.count", 1, MAX_LAYOUT_ITEMS)
		origin = _vec(layout.get("origin"), 3, "radial.origin")
		radius = _number(layout.get("radius"), "radial.radius")
		if radius < 0:
			raise ValueError("radial.radius must be non-negative")
		start = _number(layout.get("start_angle", 0), "radial.start_angle")
		sweep = _number(layout.get("sweep", 360), "radial.sweep")
		if not 0 < sweep <= 360:
			raise ValueError("radial.sweep must be in (0, 360]")
		facing = layout.get("facing", "none")
		if facing not in ("none", "inward", "outward", "tangent"):
			raise ValueError("radial.facing must be none, inward, outward or tangent")
		layout_matrix = _matrix(rotation)
		result = []
		for index in range(count):
			angle = start + sweep * index / count
			angle_rad = math.radians(angle)
			local = [radius * math.cos(angle_rad), radius * math.sin(angle_rad), 0.0]
			shift = _mv(layout_matrix, local)
			position = [origin[i] + shift[i] for i in range(3)]
			turn = {"inward": 90.0 + angle, "outward": -90.0 + angle,
					"tangent": angle}.get(facing, 0.0)
			item_rotation = rotation if facing == "none" else _euler(_mm(layout_matrix, _matrix([0.0, 0.0, turn])))
			result.append((f"{prefix}.{index:04d}", position, item_rotation, common))
		return result
	if kind != "perimeter":
		raise ValueError("layout.type must be row, grid, radial or perimeter")
	bounds = info.get("visual_bounds", {})
	if not bounds.get("available"):
		diagnostics.append(_issue("perimeter_requires_visual_bounds", "Perimeter layout needs measured visual bounds.", "error", key=prefix, model=model))
		return []
	local_min, local_max = _vec(bounds.get("min"), 3, "visual_bounds.min"), _vec(bounds.get("max"), 3, "visual_bounds.max")
	axis = _int(layout.get("wall_axis", 1), "perimeter.wall_axis", 0, 1)
	width, depth = _number(layout.get("width"), "perimeter.width"), _number(layout.get("depth"), "perimeter.depth")
	if width <= 0 or depth <= 0:
		raise ValueError("perimeter width and depth must be positive")
	origin = _vec(layout.get("origin"), 3, "perimeter.origin")
	openings = layout.get("openings", [])
	if not isinstance(openings, list) or len(openings) > 128:
		raise ValueError("perimeter.openings must be a list of at most 128 ranges")
	opening_ranges = {side: [] for side in ("south", "east", "north", "west")}
	for opening in openings:
		if not isinstance(opening, dict) or opening.get("side") not in opening_ranges:
			raise ValueError("each opening requires side south/east/north/west")
		start, end = _number(opening.get("start"), "opening.start"), _number(opening.get("end"), "opening.end")
		length = width if opening["side"] in ("south", "north") else depth
		if start < 0 or end <= start or end > length:
			raise ValueError("opening range must fit its side and have positive length")
		opening_ranges[opening["side"]].append((start, end))
	for side, ranges in opening_ranges.items():
		ranges.sort()
		if any(next_range[0] < previous[1] for previous, next_range in zip(ranges, ranges[1:])):
			raise ValueError(f"overlapping openings on {side}")
	spacing = _number(layout.get("spacing", 0), "perimeter.spacing")
	if spacing < 0:
		raise ValueError("perimeter.spacing must be non-negative")
	auto_spacing = spacing == 0
	axis_turn = 90.0 if axis == 0 else 0.0
	sides = [("south", width, [0, 0, 0], [1, 0], 270.0 + axis_turn),
			 ("east", depth, [width, 0, 0], [0, 1], 0.0 + axis_turn),
			 ("north", width, [width, depth, 0], [-1, 0], 90.0 + axis_turn),
			 ("west", depth, [0, depth, 0], [0, -1], 180.0 + axis_turn)]
	result = []
	layout_matrix = _matrix(rotation)
	prepared = []
	for side, length, corner, tangent, heading in sides:
		side_matrix = _matrix([0.0, 0.0, heading])
		pose_rotation = _euler(_mm(layout_matrix, side_matrix))
		matrix = _matrix(pose_rotation)
		corners = [_mv(matrix, [x, y, z]) for x in (local_min[0], local_max[0])
				   for y in (local_min[1], local_max[1]) for z in (local_min[2], local_max[2])]
		side_corners = [_mv(side_matrix, [x, y, z]) for x in (local_min[0], local_max[0])
						for y in (local_min[1], local_max[1]) for z in (local_min[2], local_max[2])]
		world_tangent = _mv(layout_matrix, [tangent[0], tangent[1], 0.0])
		projected = [sum(point[axis] * world_tangent[axis] for axis in range(3)) for point in corners]
		module_extent = max(projected) - min(projected)
		if module_extent <= 1.0e-5:
			raise ValueError("wall module has no measured extent along perimeter tangent")
		side_spacing = module_extent if auto_spacing else spacing
		if side_spacing < module_extent - 1.0e-5:
			diagnostics.append(_issue("perimeter_overlap", "Module spacing is narrower than its measured extent along this wall; overlap is possible.", key=prefix, side=side, overlap=module_extent - side_spacing))
		if side_spacing > module_extent + 1.0e-5:
			diagnostics.append(_issue("perimeter_gap", "Module spacing exceeds its measured extent along this wall; gaps are possible.", key=prefix, side=side, gap=side_spacing - module_extent))
		ratio = (length - module_extent) / side_spacing
		if not math.isfinite(ratio):
			raise ValueError("perimeter spacing would expand beyond the 2048-object limit")
		count_value = max(0.0, math.floor(ratio + 1.0e-8) + 1)
		if count_value > MAX_LAYOUT_ITEMS:
			raise ValueError("perimeter expands beyond the 2048-object limit")
		count = int(count_value)
		leftover = max(0.0, length - (module_extent + max(0, count - 1) * side_spacing))
		start_offset = leftover / 2.0
		prepared.append((side, length, corner, tangent, pose_rotation, corners, side_corners, world_tangent, module_extent, count, start_offset, side_spacing))
	total_count = sum(item[9] for item in prepared)
	if total_count > MAX_LAYOUT_ITEMS:
		raise ValueError("perimeter expands beyond the 2048-object limit")
	for side, length, corner, tangent, pose_rotation, corners, side_corners, world_tangent, module_extent, count, start_offset, side_spacing in prepared:
		for index in range(count):
			start = start_offset + index * side_spacing
			end = start + module_extent
			if any(start < opening_end - 1.0e-5 and end > opening_start + 1.0e-5
				   for opening_start, opening_end in opening_ranges[side]):
				continue
			center_distance = start + module_extent / 2
			local_center = [corner[0] + tangent[0] * center_distance,
							corner[1] + tangent[1] * center_distance, 0.0]
			minimum = [min(point[axis] for point in side_corners) for axis in range(3)]
			maximum = [max(point[axis] for point in side_corners) for axis in range(3)]
			local_position = [local_center[0] - (minimum[0] + maximum[0]) / 2,
							  local_center[1] - (minimum[1] + maximum[1]) / 2,
							  -minimum[2]]
			world_offset = _mv(layout_matrix, local_position)
			position = [origin[axis] + world_offset[axis] for axis in range(3)]
			result.append((f"{prefix}.{side}.{index:03d}", position, pose_rotation, common))
	if len(result) > MAX_LAYOUT_ITEMS:
		raise ValueError("perimeter expands to more than 2048 objects")
	return result
