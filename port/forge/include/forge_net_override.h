/*
 * OpenCE : Faber - Dynamic Network Version Override Header
 * Included via -include on network_client_manager.c, network_server_message_handler.c, and p2p_lobby.c.
 * Routes HALO_PORT_NETWORK_VERSION through forge_get_network_version():
 *   - Standard Multiplayer menu -> OpenCE's own HALO_PORT_NETWORK_VERSION (standard OpenCE servers/clients)
 *   - Forge Multiplayer menu    -> FORGE_NETWORK_VERSION (Forge-only servers/clients)
 */

#ifndef FORGE_NET_OVERRIDE_H
#define FORGE_NET_OVERRIDE_H

#include "halo_port_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

extern long forge_get_network_version(void);

#ifdef __cplusplus
}
#endif

#undef HALO_PORT_NETWORK_VERSION
#define HALO_PORT_NETWORK_VERSION (forge_get_network_version())

#endif /* FORGE_NET_OVERRIDE_H */
