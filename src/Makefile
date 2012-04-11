CC = gcc
CFLAGS = -Wall -g
LDFLAGS += -L$(STAGING_DIR)/root-brcm47xx/usr/lib -lcurl -lpthread -lmicrohttpd -lstdc++
OBJS = main.o

all: ahans

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

ahans: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f ahans *.o
