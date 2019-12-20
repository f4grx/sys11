OBJS=main.o log.o gdbremote.o core.o mem.o sci.o
BIN=sim

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS) -lpthread

%.o:%.c
	$(CC) -c -g -o $@ $<

.PHONY: clean
clean:
	$(RM) $(BIN) $(OBJS)

