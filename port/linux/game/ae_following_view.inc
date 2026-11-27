/*
AE_FOLLOWING_VIEW.INC

Included at the end of source/camera/following_camera.c (it uses that file's camera_track_splut). Content Evolved's
following_camera_unit_view, as it has it.
*/

/* port: the view following_camera_update gives the player of a unit in a
seat, worked out from the unit alone, as the player has no local control
(a replay's playback: port/linux/game/network_replay_playback.c). The
seat's camera track (player_control_get_unit_camera_info's), the facing
the unit's aim, and the observer's sums (observer_postcheck): focus is the
point the camera looks from, before walls, and distance how far back of it
the camera is along forward */
void following_camera_unit_view(
	long unit_index,
	real_point3d *focus,
	real_vector3d *forward,
	real *distance)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct unit_camera const *camera = &unit_definition_get(unit->definition_index)->unit.camera;
	real_euler_angles2d facing;
	real_vector3d track_offset;
	real_vector3d offset;
	real_vector2d heading;
	real depth;

	if (unit->object.parent_object_index != NONE)
	{
		struct unit_datum *vehicle = vehicle_try_and_get(unit->object.parent_object_index);

		if (vehicle)
		{
			camera = &TAG_BLOCK_GET_ELEMENT(
				&vehicle_definition_get(vehicle->definition_index)->unit.seats,
				unit->unit.parent_seat_index,
				struct unit_seat)->camera;
		}
	}
	unit_get_camera_position(unit_index, focus);
	euler_angles2d_from_vector3d(&facing, &unit->unit.aiming_vector);
	facing.pitch = PIN(facing.pitch, -_pi / 2.f, _pi / 2.f);
	vector3d_from_euler_angles2d(forward, &facing);

	camera_track_splut(camera, facing.pitch, &track_offset);
	depth = magnitude3d(&track_offset);
	offset.i = depth * cosine(facing.pitch) + track_offset.i;
	offset.j = -track_offset.j;
	offset.k = depth * sine(facing.pitch) + track_offset.k;
	*distance = MAX(depth, 0.6f);

	heading.i = forward->i;
	heading.j = forward->j;
	normalize2d(&heading);
	focus->x += offset.i * heading.i + offset.j * heading.j;
	focus->y += offset.i * heading.j - offset.j * heading.i;
	focus->z += offset.k;
	return;
}
