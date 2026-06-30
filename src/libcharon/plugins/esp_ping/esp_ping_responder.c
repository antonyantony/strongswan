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

#include "esp_ping_responder.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <daemon.h>

/* Kernel uapi not yet in system headers; defined locally like BusyBox does
 * for the same reason (networking/ping.c). Kernel side: net/ipv4/esp_ping.c,
 * net/ipv6/esp6_ping.c, include/uapi/linux/in.h.
 */
#ifndef IPPROTO_ESP
# define IPPROTO_ESP          50
#endif
#define IP_ESP_PING_SPI       53   /* setsockopt: __be32, pin outbound SA */
#define IP_ESP_PING_LISTEN    54   /* setsockopt: __be32[], listen filter */
#define ESP_PING_RECV_SPI      1   /* cmsg: __be32 SPI of decrypting SA */
#define ESP_PING_SEND_SPI      2   /* cmsg: __be32 SPI to pin this send to */
#define ESP_ECHO_REQUEST        2  /* AGGFRAG sub-type: initiator->responder */
#define ESP_ECHO_RESPONSE       3  /* AGGFRAG sub-type: responder->initiator */
#define ESP_ECHO_FLAG_R      0x80  /* flags bit 7: return_spi follows header */

/* Cap on echoed data, matches the plan's suggested default response limit */
#define ESP_PING_MAX_DATA     512

struct esp_echo_hdr {
	uint8_t  sub_type;
	uint8_t  flags;
	uint16_t data_len;
	uint16_t id;
	uint16_t seq;
} __attribute__((packed));

typedef struct private_esp_ping_responder_t private_esp_ping_responder_t;

/**
 * Private data of an esp_ping_responder_t object.
 *
 * Dual-stack: a request for an IPv4 SA and one for an IPv6 SA look
 * identical at the esp_echo_hdr/cmsg level, but the kernel's ESP-PING
 * socket -- and thus the reply address it hands back in recvmsg()'s
 * msg_name, and the socket sendmsg() must go back out on -- is
 * family-specific (net/ipv4/esp_ping.c vs net/ipv6/esp6_ping.c). So each
 * family gets its own listen/send socket pair; whichever listen socket a
 * request arrives on tells us which family's SA it belongs to and which
 * send socket the reply has to go out on.
 */
struct private_esp_ping_responder_t {

	/**
	 * Public esp_ping_responder_t interface.
	 */
	esp_ping_responder_t public;

	/**
	 * Receive-only listener sockets, opted in via IP_ESP_PING_LISTEN.
	 * listen_fd6 is -1 if IPv6 wasn't available.
	 */
	int listen_fd4;
	int listen_fd6;

	/**
	 * Send-only sockets used exclusively for replies, -1 if monitor-only
	 * (both) or IPv6 wasn't available (send_fd6 only).
	 */
	int send_fd4;
	int send_fd6;
};

/**
 * Reply to an echo request with the same id/seq/data, pinned to reply_spi.
 */
static void send_reply(int send_fd, struct sockaddr *to, socklen_t tolen,
						struct esp_echo_hdr *req, void *data,
						size_t data_len, uint32_t reply_spi)
{
	char cbuf[CMSG_SPACE(sizeof(uint32_t))] = {};
	struct esp_echo_hdr eh = {
		.sub_type = ESP_ECHO_RESPONSE,
		.flags    = 0,	/* R is request-only, never set in a reply */
		.data_len = req->data_len,
		.id       = req->id,
		.seq      = req->seq,
	};
	struct iovec iov[2] = {
		{ .iov_base = &eh,   .iov_len = sizeof(eh) },
		{ .iov_base = data,  .iov_len = data_len },
	};
	struct msghdr msg = {
		.msg_name = to,
		.msg_namelen = tolen,
		.msg_iov = iov,
		.msg_iovlen = countof(iov),
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	struct cmsghdr *cmsg;

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = IPPROTO_ESP;
	cmsg->cmsg_type = ESP_PING_SEND_SPI;
	cmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
	*(uint32_t*)CMSG_DATA(cmsg) = reply_spi;

	if (sendmsg(send_fd, &msg, 0) < 0)
	{
		DBG1(DBG_NET, "esp-ping: sending reply failed: %s", strerror(errno));
	}
}

/**
 * Shared receive handler for both the IPv4 and IPv6 listen sockets.
 * @param send_fd the reply socket matching @p fd's family, -1 for monitor-only
 */
static bool receive_on(int fd, int send_fd)
{
	char cbuf[CMSG_SPACE(sizeof(uint32_t))];
	struct esp_echo_hdr eh;
	uint8_t data[ESP_PING_MAX_DATA];
	struct sockaddr_storage from;
	struct iovec iov[2] = {
		{ .iov_base = &eh,   .iov_len = sizeof(eh) },
		{ .iov_base = data,  .iov_len = sizeof(data) },
	};
	struct msghdr msg = {
		.msg_name = &from,
		.msg_namelen = sizeof(from),
		.msg_iov = iov,
		.msg_iovlen = countof(iov),
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	struct cmsghdr *cmsg;
	uint32_t incoming_spi = 0, reply_spi;
	uint8_t *payload;
	size_t payload_len;
	ssize_t len;

	len = recvmsg(fd, &msg, MSG_DONTWAIT);
	if (len < (ssize_t)sizeof(eh))
	{
		if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		{
			DBG1(DBG_NET, "esp-ping: recvmsg failed: %s", strerror(errno));
		}
		return TRUE;
	}
	if (eh.sub_type != ESP_ECHO_REQUEST)
	{	/* stray response, or garbage; nothing to do */
		return TRUE;
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		if (cmsg->cmsg_level == IPPROTO_ESP &&
			cmsg->cmsg_type == ESP_PING_RECV_SPI)
		{
			incoming_spi = *(uint32_t*)CMSG_DATA(cmsg);
		}
	}

	payload = data;
	payload_len = (size_t)len - sizeof(eh);

	if (eh.flags & ESP_ECHO_FLAG_R)
	{
		if (payload_len < sizeof(uint32_t))
		{	/* R set but return_spi missing/truncated */
			return TRUE;
		}
		reply_spi = *(uint32_t*)payload;
		payload += sizeof(uint32_t);
		payload_len -= sizeof(uint32_t);
	}
	else
	{
		reply_spi = incoming_spi;
	}

	/* TODO (draft-ietf-ipsecme-encrypted-esp-ping-03 §4.3, deferred in
	 * turk/esp-ping-plan.md "Return path validation"): validate reply_spi
	 * against the SADB before replying. Echoes unconditionally for now,
	 * fine for lab/diagnostic use, not for production. */

	{
		host_t *src = host_create_from_sockaddr((sockaddr_t*)&from);

		DBG2(DBG_NET, "esp-ping: request id 0x%04x seq %u from %H, "
			 "incoming SPI 0x%08x, reply SPI 0x%08x",
			 ntohs(eh.id), ntohs(eh.seq), src,
			 ntohl(incoming_spi), ntohl(reply_spi));
		src->destroy(src);
	}

	if (send_fd != -1)
	{
		send_reply(send_fd, (struct sockaddr*)&from, msg.msg_namelen,
				  &eh, payload, payload_len, reply_spi);
	}
	return TRUE;
}

CALLBACK(receive_echo_request4, bool,
	private_esp_ping_responder_t *this, int fd, watcher_event_t event)
{
	return receive_on(fd, this->send_fd4);
}

CALLBACK(receive_echo_request6, bool,
	private_esp_ping_responder_t *this, int fd, watcher_event_t event)
{
	return receive_on(fd, this->send_fd6);
}

/**
 * Open a socket of the given family and opt it in to the kernel's ESP-ping
 * request fan-out, filtered to @spis (or unfiltered if empty/NULL).
 */
static int open_listen_socket(int family, linked_list_t *spis)
{
	struct sockaddr_storage addr = {};
	socklen_t addrlen;
	enumerator_t *enumerator;
	uint32_t *buf = NULL;
	uintptr_t spi;
	int fd, count, i = 0;

	if (family == AF_INET6)
	{
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6*)&addr;

		addr6->sin6_family = AF_INET6;
		addrlen = sizeof(*addr6);
	}
	else
	{
		struct sockaddr_in *addr4 = (struct sockaddr_in*)&addr;

		addr4->sin_family = AF_INET;
		addrlen = sizeof(*addr4);
	}

	fd = socket(family, SOCK_DGRAM, IPPROTO_ESP);
	if (fd == -1)
	{
		DBG1(DBG_NET, "esp-ping: opening %s listen socket failed: %s",
			 family == AF_INET6 ? "IPv6" : "IPv4", strerror(errno));
		return -1;
	}

	/* Triggers esp_ping_hash() (esp_ping_prot.hash), which the generic
	 * INET bind path calls. Without this the socket never enters
	 * esp_ping_table, and esp_ping_deliver_request()'s fan-out loop
	 * finds nothing to deliver to -- requests are silently dropped even
	 * though IP_ESP_PING_LISTEN below succeeds.
	 */
	if (bind(fd, (struct sockaddr*)&addr, addrlen) == -1)
	{
		DBG1(DBG_NET, "esp-ping: %s bind failed: %s",
			 family == AF_INET6 ? "IPv6" : "IPv4", strerror(errno));
		close(fd);
		return -1;
	}

	count = spis ? spis->get_count(spis) : 0;
	if (count)
	{
		buf = malloc(count * sizeof(uint32_t));
		enumerator = spis->create_enumerator(spis);
		while (enumerator->enumerate(enumerator, &spi))
		{
			buf[i++] = (uint32_t)spi;
		}
		enumerator->destroy(enumerator);
	}

	/* IP_ESP_PING_LISTEN is a SOL_IP option; ipv6_setsockopt() forwards
	 * SOL_IP options through to ip_setsockopt() for non-SOCK_RAW sockets,
	 * so this works unchanged for the AF_INET6 socket too.
	 */
	if (setsockopt(fd, IPPROTO_IP, IP_ESP_PING_LISTEN, buf,
				   count * sizeof(uint32_t)) == -1)
	{
		DBG1(DBG_NET, "esp-ping: %s IP_ESP_PING_LISTEN failed: %s",
			 family == AF_INET6 ? "IPv6" : "IPv4", strerror(errno));
		free(buf);
		close(fd);
		return -1;
	}
	free(buf);
	return fd;
}

METHOD(esp_ping_responder_t, destroy, void,
	private_esp_ping_responder_t *this)
{
	lib->watcher->remove(lib->watcher, this->listen_fd4);
	close(this->listen_fd4);
	if (this->send_fd4 != -1)
	{
		close(this->send_fd4);
	}
	if (this->listen_fd6 != -1)
	{
		lib->watcher->remove(lib->watcher, this->listen_fd6);
		close(this->listen_fd6);
	}
	if (this->send_fd6 != -1)
	{
		close(this->send_fd6);
	}
	free(this);
}

/*
 * Described in header
 */
esp_ping_responder_t *esp_ping_responder_create(linked_list_t *spis, bool reply)
{
	private_esp_ping_responder_t *this;

	INIT(this,
		.public = {
			.destroy = _destroy,
		},
		.send_fd4 = -1,
		.send_fd6 = -1,
	);

	this->listen_fd4 = open_listen_socket(AF_INET, spis);
	if (this->listen_fd4 == -1)
	{
		free(this);
		return NULL;
	}

	/* IPv6 is best-effort: an IPv6-less host/kernel still gets working
	 * IPv4 esp-ping, same as the rest of strongSwan tolerates a missing
	 * IPv6 stack.
	 */
	this->listen_fd6 = open_listen_socket(AF_INET6, spis);

	if (reply)
	{
		/* Deliberately its own socket, never IP_ESP_PING_LISTEN, never a
		 * sticky IP_ESP_PING_SPI: only ever used to sendmsg() a reply with
		 * a per-call ESP_PING_SEND_SPI cmsg. Structurally can't receive
		 * (or be mistaken for) a request.
		 */
		this->send_fd4 = socket(AF_INET, SOCK_DGRAM, IPPROTO_ESP);
		if (this->send_fd4 == -1)
		{
			DBG1(DBG_NET, "esp-ping: opening IPv4 send socket failed: %s",
				 strerror(errno));
			close(this->listen_fd4);
			if (this->listen_fd6 != -1)
			{
				close(this->listen_fd6);
			}
			free(this);
			return NULL;
		}

		if (this->listen_fd6 != -1)
		{
			this->send_fd6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ESP);
			if (this->send_fd6 == -1)
			{
				DBG1(DBG_NET, "esp-ping: opening IPv6 send socket failed: "
					 "%s, IPv6 replies disabled", strerror(errno));
				close(this->listen_fd6);
				this->listen_fd6 = -1;
			}
		}
	}

	lib->watcher->add(lib->watcher, this->listen_fd4, WATCHER_READ,
					  receive_echo_request4, this);
	if (this->listen_fd6 != -1)
	{
		lib->watcher->add(lib->watcher, this->listen_fd6, WATCHER_READ,
						  receive_echo_request6, this);
	}

	return &this->public;
}
