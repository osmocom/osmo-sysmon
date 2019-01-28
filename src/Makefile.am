LDADD = libintern.la $(LIBOSMOCORE_LIBS) $(LIBOSMOGSM_LIBS)

AM_CPPFLAGS = \
	$(all_includes) \
	-I$(top_srcdir) \
	$(NULL)

AM_CFLAGS = \
	-Wall \
	$(LIBOSMOCORE_CFLAGS) \
	$(LIBOSMOGSM_CFLAGS) \
	$(NULL)

AM_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	$(NULL)

bin_PROGRAMS = \
	osmo-sysmon \
	osmo-ctrl-client \
	$(NULL)

noinst_LTLIBRARIES = libintern.la
libintern_la_SOURCES = simple_ctrl.c
libintern_la_LIBADD = $(LIBOSMOCORE_LIBS) $(LIBOSMOGSM_LIBS)

osmo_sysmon_CFLAGS = $(LIBMNL_CFLAGS) $(LIBOSMOVTY_CFLAGS) $(LIBOPING_CFLAGS) $(AM_CFLAGS)

osmo_sysmon_LDADD = $(LDADD) \
	$(LIBOSMOVTY_LIBS) \
	$(LIBMNL_LIBS) \
	$(LIBOPING_LIBS) \
	$(NULL)

osmo_sysmon_SOURCES = \
	value_node.c \
	osysmon_ctrl.c \
	osysmon_sysinfo.c \
	osysmon_rtnl.c \
	osysmon_file.c \
	osysmon_ping.c \
	osysmon_main.c \
	$(NULL)

noinst_HEADERS = \
	osysmon.h \
	simple_ctrl.h \
	value_node.h \
	$(NULL)
