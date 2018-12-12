SUBDIRS = doc

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = \
	$(all_includes) \
	-I$(top_srcdir) \
	$(NULL)

AM_CFLAGS = \
	-Wall \
	$(LIBOSMOCORE_CFLAGS) \
	$(LIBOSMOGSM_CFLAGS) \
	$(LIBOSMOVTY_CFLAGS) \
	$(LIBMNL_CFLAGS) \
	$(NULL)

AM_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	$(NULL)

bin_PROGRAMS = \
	osmo-sysmon \
	osmo-ctrl-client \
	$(NULL)

osmo_sysmon_LDADD = \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOGSM_LIBS) \
	$(LIBOSMOVTY_LIBS) \
	$(LIBMNL_LIBS) \
	$(NULL)

osmo_sysmon_SOURCES = \
	value_node.c \
	simple_ctrl.c \
	osysmon_ctrl.c \
	osysmon_sysinfo.c \
	osysmon_rtnl.c \
	osysmon_file.c \
	osysmon_main.c \
	$(NULL)

osmo_ctrl_client_LDADD = \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOGSM_LIBS) \
	$(NULL)

osmo_ctrl_client_SOURCES = \
	simple_ctrl.c \
	osmo-ctrl-client.c \
	$(NULL)

noinst_HEADERS = \
	osysmon.h \
	simple_ctrl.h \
	value_node.h \
	$(NULL)
