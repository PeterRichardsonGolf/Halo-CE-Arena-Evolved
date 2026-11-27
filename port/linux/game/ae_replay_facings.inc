/*
AE_REPLAY_FACINGS.INC

Included at the end of port/linux/game/network_distributed.c (it uses that file's distributed_relayed_action_read and
distributed_take). Content Evolved's network_distributed_replay_facings, as it has it: the facings (yaw and pitch, as the wire has
them) of the players that a recorded message carries, read from the recording alone for a replay's eyes view to turn
between (network_replay_playback.c). Nothing is sent, nothing of the game's state or of a message's size or version is touched.
*/

/* a recorded message's players' facings, each to found: those of a tick's relayed actions (the host's update's number their tick), or,
with own_actions, a client's own action recorded with the tick it ran (not written by this build's recorder) */
void network_distributed_replay_facings(
	byte const *data,
	long size,
	boolean own_actions,
	void (*found)(void *context, short player_index, long tick, short yaw, short pitch),
	void *context)
{
	byte const *end = data + size;
	struct distributed_message_header header;
	struct distributed_relayed_action relayed;
	long update_number;
	short index;

	if (own_actions)
	{
		long tick;

		if (size >= (long)sizeof(long) && distributed_relayed_action_read(data + sizeof(long), end, &relayed))
		{
			csmemcpy(&tick, data, sizeof(long));
			found(context, (short)relayed.player_index, tick, relayed.yaw, relayed.pitch);
		}
		return;
	}
	if (size < (long)sizeof(header))
		return;
	csmemcpy(&header, data, sizeof(header));
	/* (a tick's messages in one, as network_distributed_handle_message
	takes them: each message after its length, without the message header
	the batch has once) */
	if (header.type == _distributed_message_batch)
	{
		long offset = sizeof(header);

		while (offset + (long)sizeof(word) <= size)
		{
			byte buffer[sizeof(message_header) + DATAGRAM_MAXIMUM_SIZE + 1];
			word length;

			csmemcpy(&length, data + offset, sizeof(word));
			offset += sizeof(word);
			if (length > size - offset || length < sizeof(header) - sizeof(message_header) ||
				sizeof(message_header) + length > sizeof(buffer))
			{
				break;
			}
			csmemcpy(buffer, data, sizeof(message_header));
			csmemcpy(buffer + sizeof(message_header), data + offset, length);
			offset += length;
			if (((struct distributed_message_header const *)buffer)->type != _distributed_message_batch)
			{
				network_distributed_replay_facings(buffer, (long)(sizeof(message_header) + length), FALSE, found,
					context);
			}
		}
		return;
	}
	if (header.type != _distributed_message_relayed_actions)
		return;
	/* (as distributed_handle_actions reads them: the host's update, then
	each player's) */
	data += sizeof(header);
	if (!distributed_take(&data, end, &update_number, sizeof(update_number)))
		return;
	for (index = 0; index < header.count; index++)
	{
		word read = distributed_relayed_action_read(data, end, &relayed);

		if (!read)
			break;
		data += read;
		found(context, (short)relayed.player_index, update_number, relayed.yaw, relayed.pitch);
	}
}
