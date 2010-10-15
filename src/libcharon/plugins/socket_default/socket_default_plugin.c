/*
 * Copyright (C) 2010 Tobias Brunner
 * Hochschule fuer Technik Rapperswil
 * Copyright (C) 2010 Martin Willi
 * Copyright (C) 2010 revosec AG
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

#include "socket_default_plugin.h"

#include "socket_default_socket.h"

#include <daemon.h>

typedef struct private_socket_default_plugin_t private_socket_default_plugin_t;

/**
 * Private data of socket plugin
 */
struct private_socket_default_plugin_t {

	/**
	 * Implements plugin interface
	 */
	socket_default_plugin_t public;

};

METHOD(plugin_t, destroy, void,
	private_socket_default_plugin_t *this)
{
	charon->socket->remove_socket(charon->socket,
						(socket_constructor_t)socket_default_socket_create);
	free(this);
}

/*
 * see header file
 */
plugin_t *socket_default_plugin_create()
{
	private_socket_default_plugin_t *this;

	INIT(this,
		.public = {
			.plugin = {
				.destroy = _destroy,
			},
		},
	);

	charon->socket->add_socket(charon->socket,
						(socket_constructor_t)socket_default_socket_create);

	return &this->public.plugin;
}

