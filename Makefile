LDFLAGS:=-losmocore -losmogsm -ltalloc
CFLAGS:=-Wall -g

osmo-ctrl-client: osmo-ctrl-client.o simple_ctrl.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	@rm osmo-ctrl-client *.o
