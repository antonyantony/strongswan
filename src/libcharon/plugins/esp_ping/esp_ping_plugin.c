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

#include "esp_ping_plugin.h"
#include "esp_ping_responder.h"

#include <daemon.h>

typedef struct private_esp_ping_plugin_t private_esp_ping_plugin_t;

/**
 * Private data of esp_ping plugin
 */
struct private_esp_ping_plugin_t {

	/**
	 * Implements plugin_t interface.
	 */
	esp_ping_plugin_t public;

	/**
	 * Request listener and (unless monitor-only) responder.
	 */
	esp_ping_responder_t *responder;
};

METHOD(plugin_t, get_name, char*,
	private_esp_ping_plugin_t *this)
{
	return "esp-ping";
}

/**
 * Parse plugins.esp-ping.spis (comma-separated hex/dec SPI list) from
 * strongswan.conf into a list usable by esp_ping_responder_create().
 *
 * No IKEv2 ENCRYPTED_PING_SUPPORTED negotiation tie-in yet (deferred, see
 * turk/esp-ping-plan.md Step 13) -- an empty/unset setting listens to all
 * ESP-ping SAs, which is sufficient for testing.
 */
static linked_list_t *load_spis()
{
	linked_list_t *spis;
	enumerator_t *enumerator;
	char *token, *value;

	spis = linked_list_create();
	value = lib->settings->get_str(lib->settings,
					"%s.plugins.esp-ping.spis", "", lib->ns);

	enumerator = enumerator_create_token(value, ",", " ");
	while (enumerator->enumerate(enumerator, &token))
	{
		uintptr_t spi = strtoul(token, NULL, 0);

		if (spi)
		{
			spis->insert_last(spis, (void*)spi);
		}
	}
	enumerator->destroy(enumerator);

	return spis;
}

/**
 * Register plugin features
 */
static bool register_esp_ping(private_esp_ping_plugin_t *this,
							  plugin_feature_t *feature, bool reg, void *data)
{
	if (reg)
	{
		linked_list_t *spis;
		bool reply;

		reply = lib->settings->get_bool(lib->settings,
						"%s.plugins.esp-ping.respond", TRUE, lib->ns);
		spis = load_spis();

		this->responder = esp_ping_responder_create(spis, reply);
		spis->destroy(spis);
		if (!this->responder)
		{
			DBG1(DBG_NET, "esp-ping: failed to start (see preceding error)");
			return FALSE;
		}
		DBG1(DBG_NET, "esp-ping: listening for encrypted ESP-ping requests "
			 "(respond=%s)", reply ? "yes" : "no");
	}
	else
	{
		this->responder->destroy(this->responder);
	}
	return TRUE;
}

METHOD(plugin_t, get_features, int,
	private_esp_ping_plugin_t *this, plugin_feature_t *features[])
{
	static plugin_feature_t f[] = {
		PLUGIN_CALLBACK((plugin_feature_callback_t)register_esp_ping, NULL),
			PLUGIN_PROVIDE(CUSTOM, "esp-ping"),
	};
	*features = f;
	return countof(f);
}

METHOD(plugin_t, destroy, void,
	private_esp_ping_plugin_t *this)
{
	free(this);
}

/*
 * Described in header
 */
PLUGIN_DEFINE(esp_ping)
{
	private_esp_ping_plugin_t *this;

	INIT(this,
		.public = {
			.plugin = {
				.get_name = _get_name,
				.get_features = _get_features,
				.reload = (void*)return_false,
				.destroy = _destroy,
			},
		},
	);

	return &this->public.plugin;
}
