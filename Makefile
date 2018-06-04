LDFLAGS:=-losmocore -losmogsm -losmovty -ltalloc -lmnl
CFLAGS:=-Wall -g


all: osmo-ctrl-client osysmon

osmo-ctrl-client: osmo-ctrl-client.o simple_ctrl.o
	$(CC) $(LDFLAGS) -o $@ $^

osysmon: value_node.o simple_ctrl.o osysmon_ctrl.o osysmon_sysinfo.o osysmon_rtnl.o osysmon_main.o
	$(CC) $(LDFLAGS) -o $@ $^


clean:
	@rm osmo-ctrl-client *.o
