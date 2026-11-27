/*
SOCIAL_CREATOR.H

What the account made: Forge reports a map saved under a new name here (social_creator_add). This build keeps no
account service, so the count goes nowhere (ae_forge_glue.c).
*/

#ifndef SOCIAL_CREATOR_H
#define SOCIAL_CREATOR_H

enum
{
	_social_creator_forge_seconds,
	_social_creator_range_seconds,
	_social_creator_forge_maps,
	_social_creator_tag_profiles,
	_social_creator_campaign_profiles,
	_social_creator_multiplayer_profiles,
	NUMBER_OF_SOCIAL_CREATOR_FIELDS
};

/* something made: `count` of a field (a Forge map saved under a new name) */
void social_creator_add(short field, long count);

#endif
