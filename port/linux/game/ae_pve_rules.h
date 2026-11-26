/*
AE_PVE_RULES.H

Which games Content Evolved's campaign score, campaign medals, service rank and Firefight stats play in: a campaign
level or a Firefight game, and only with the community services' opt-in for anything that leaves the machine.
A multiplayer game (any gametype, hosted, joined or played alone) is none of those: no record, medal or score is
written and nothing is sent.
*/

#ifndef __AE_PVE_RULES_H
#define __AE_PVE_RULES_H

/* campaign: a campaign (single player) level; firefight: a Firefight game; consented: network.community_services is
"yes". 1 when the stats may be sent or listened for */
int ae_pve_rules_stats_upload(int campaign, int firefight, int consented);

/* 1 when the game's records (medals, score, rank, stats) may be written: a campaign level or a Firefight game */
int ae_pve_rules_records(int campaign, int firefight);

/* 1 when the stats' loopback socket may listen: the opt-in is given */
int ae_pve_rules_stats_socket(int consented);

#endif /* __AE_PVE_RULES_H */
