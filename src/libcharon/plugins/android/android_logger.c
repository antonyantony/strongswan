/*
 * Copyright (C) 2010-2012 Tobias Brunner
 * Hochschule fuer Technik Rapperswil
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

#include <string.h>
#include <android/log.h>

#include "android_logger.h"

#include <library.h>
#include <daemon.h>
#include <threading/mutex.h>

typedef struct private_android_logger_t private_android_logger_t;

/**
 * Private data of an android_logger_t object
 */
struct private_android_logger_t {

	/**
	 * Public interface
	 */
	android_logger_t public;

	/**
	 * logging level
	 */
	int level;

	/**
	 * Mutex to ensure multi-line log messages are not torn apart
	 */
	mutex_t *mutex;
};


METHOD(logger_t, log_, void,
	private_android_logger_t *this, debug_t group, level_t level,
	int thread, ike_sa_t* ike_sa, char *message)
{
	int prio = level > 1 ? ANDROID_LOG_DEBUG : ANDROID_LOG_INFO;
	char sgroup[16];
	char *current = message, *next;
	snprintf(sgroup, sizeof(sgroup), "%N", debug_names, group);
	this->mutex->lock(this->mutex);
	while (current)
	{	/* log each line separately */
		next = strchr(current, '\n');
		if (next)
		{
			*(next++) = '\0';
		}
		__android_log_print(prio, "charon", "%.2d[%s] %s\n",
							thread, sgroup, current);
		current = next;
	}
	this->mutex->unlock(this->mutex);
}

METHOD(logger_t, get_level, level_t,
	private_android_logger_t *this, debug_t group)
{
	return this->level;
}

METHOD(android_logger_t, destroy, void,
	private_android_logger_t *this)
{
	this->mutex->destroy(this->mutex);
	free(this);
}

/**
 * Described in header.
 */
android_logger_t *android_logger_create()
{
	private_android_logger_t *this;

	INIT(this,
		.public = {
			.logger = {
				.log = _log_,
				.get_level = _get_level,
			},
			.destroy = _destroy,
		},
		.mutex = mutex_create(MUTEX_TYPE_DEFAULT),
		.level = lib->settings->get_int(lib->settings,
								"%s.plugins.android.loglevel", 1, charon->name),
	);

	return &this->public;
}

