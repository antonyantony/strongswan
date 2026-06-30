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
 * @defgroup esp_ping_responder esp_ping_responder
 * @{ @ingroup esp_ping
 */

#ifndef ESP_PING_RESPONDER_H_
#define ESP_PING_RESPONDER_H_

#include <collections/linked_list.h>

typedef struct esp_ping_responder_t esp_ping_responder_t;

/**
 * Encrypted ESP-ping request listener, and (unless monitor-only) responder.
 *
 * Owns two sockets, deliberately kept separate so they can't interact:
 * a receive-only listener socket (opted in to the kernel's request
 * fan-out via IP_ESP_PING_LISTEN) and, unless created monitor-only, a
 * send-only socket used exclusively to reply (IP_ESP_PING_SEND_SPI cmsg
 * per reply, never a sticky IP_ESP_PING_SPI).
 */
struct esp_ping_responder_t {

	/**
	 * Destroy a esp_ping_responder_t.
	 */
	void (*destroy)(esp_ping_responder_t *this);
};

/**
 * Create an esp_ping_responder instance.
 *
 * @param spis		SPIs to listen for (uintptr_t entries), or an empty/NULL
 *					list to listen for all ESP-ping SAs
 * @param reply		TRUE to open a send-only socket and reply to requests,
 *					FALSE for a passive monitor that only observes them
 * @return			responder instance, NULL if either socket failed to open
 */
esp_ping_responder_t *esp_ping_responder_create(linked_list_t *spis, bool reply);

#endif /** ESP_PING_RESPONDER_H_ @}*/
