/*
AE_GLUE_PARTY_ONLINE.H

DEV MODE's `party ...` commands: the online party service's link driven from the console (ae_party_client.h), so a party, an
invite or a QUICK PLAY can be staged with no screen. Log prefix "ae party:".
*/

#ifndef __AE_GLUE_PARTY_ONLINE_H
#define __AE_GLUE_PARTY_ONLINE_H

struct ae_dev_cmd;

/* `party code` | `party online <status|start|stop|name|invite|answer|leave|kick|promote|privacy|link|newlink|seats|follow|quickplay|cancel>`:
text is the one-line answer; 1 when the command applied */
int ae_party_online_dev(struct ae_dev_cmd const *cmd, char *text, int size);

#endif
