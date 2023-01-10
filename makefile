CFLAGS = -g -Wall -Wextra -std=gnu11

BIN = upush_client upush_server

all: $(BIN)

upush_server: upush_server.c
	gcc $(CFLAGS) upush_server.c send_packet.c -o upush_server

upush_client: upush_client.c
	gcc $(CFLAGS) upush_client.c send_packet.c -o upush_client

clean:
	rm -f $(BIN)