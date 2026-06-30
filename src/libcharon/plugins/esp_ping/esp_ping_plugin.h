/*
 * Copyright (C) secunet Security Networks AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

/**
 * @defgroup esp_ping esp_ping
 * @ingroup cplugins
 *
 * @defgroup esp_ping_plugin esp_ping_plugin
 * @{ @ingroup esp_ping
 */

#ifndef ESP_PING_PLUGIN_H_
#define ESP_PING_PLUGIN_H_

#include <plugins/plugin.h>

typedef struct esp_ping_plugin_t esp_ping_plugin_t;

/**
 * Encrypted ESP-ping responder plugin (draft-ietf-ipsecme-encrypted-esp-ping).
 *
 * Listens for ESP_ECHO_REQUEST on any SA flagged XFRM_SA_XFLAG_ESP_PING and
 * replies with ESP_ECHO_RESPONSE. Kernel side: net/ipv4/esp_ping.c.
 */
struct esp_ping_plugin_t {

	/**
	 * Implements plugin_t interface.
	 */
	plugin_t plugin;
};

#endif /** ESP_PING_PLUGIN_H_ @}*/
