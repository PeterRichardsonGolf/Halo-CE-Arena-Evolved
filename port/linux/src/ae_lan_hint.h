/*
AE_LAN_HINT.H

The LAN list's firewall hint (item 73): on Linux, which asks nothing when a game listens (Windows does), a firewall
that is on silently hides LAN games. When Join Game > LAN has found nothing for AE_LAN_HINT_DELAY_MS and a firewall is
on, the list's two lines under it say so (ae_glue_lan.c, menu_functions.c's browser_update). Plain C: no SDL, no game
headers (tools/test_ae_units.py builds it with port/linux/tests/ae_lan_hint_test.c).
*/

#ifndef __AE_LAN_HINT_H
#define __AE_LAN_HINT_H

/* how long the list looks before the hint (it is shown only while nothing is found) */
#define AE_LAN_HINT_DELAY_MS 10000UL

/* the hint's two lines (the list's lines hold 63 characters each); the ports are README's "Playing over LAN": the
game's UDP 5150-5151 and 5160, and map sharing's TCP 5153 (p2p.h P2P_MAP_SHARE_PORT) */
#define AE_LAN_HINT_LINE_1 "No games found. On Linux a firewall can hide LAN games:"
#define AE_LAN_HINT_LINE_2 "see README > Playing over LAN (UDP 5150-5151, 5160; TCP 5153)"

/* whether the hint is due: the list has been looking for elapsed_ms, has found no game (empty), and a firewall is
on (firewall) */
int ae_lan_hint_due(unsigned long elapsed_ms, int empty, int firewall);

/* whether a firewall is on, read from the files of the system under root ("" for this one; a test's fake tree),
never as root and never asking anything: ufw's /etc/ufw/ufw.conf says ENABLED=yes (readable by everyone), or
firewalld runs (its pid file, /run/firewalld.pid or /var/run/firewalld.pid, names a live process, or a process is
named firewalld in /proc). Its name ("ufw", "firewalld") goes in what. Other firewalls (nftables or iptables rules
set by hand) cannot be read without root: not found */
int ae_lan_firewall_in(char const *root, char *what, int size);

/* this system's: ae_lan_firewall_in("") on Linux desktops (or the root $AE_TEST_FIREWALL_ROOT names, a game test's
fake tree); 0 elsewhere (Windows and macOS ask, or need nothing; the
dedicated server, Android and the web have no LAN list) */
int ae_lan_firewall(char *what, int size);

#endif
