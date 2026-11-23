/*
AE_ARM_SHIELD_LOG.H

What the first-person arm shield (viewmodel_shield.c, display.viewmodel_shield) reads, in the game's log once a second
for each local player while their shield is over 1 or charging (ae_shield_effects_rules.h): a report that the overshield
was not seen on the arms can be read from the log alone.
*/

#ifndef __AE_ARM_SHIELD_LOG_H
#define __AE_ARM_SHIELD_LOG_H

void ae_arm_shield_log(
	long unit_index,
	short local_player_index);

#endif /* __AE_ARM_SHIELD_LOG_H */
