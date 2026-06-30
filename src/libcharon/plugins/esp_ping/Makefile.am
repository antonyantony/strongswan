AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	-I$(top_srcdir)/src/libcharon

AM_CFLAGS = \
	$(PLUGIN_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-esp-ping.la
else
plugin_LTLIBRARIES = libstrongswan-esp-ping.la
endif

libstrongswan_esp_ping_la_SOURCES = \
	esp_ping_responder.h esp_ping_responder.c \
	esp_ping_plugin.h esp_ping_plugin.c

libstrongswan_esp_ping_la_LDFLAGS = -module -avoid-version
