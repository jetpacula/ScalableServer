all:
	$(CC) server.c -std=c11 -ljson-c  -lhiredis -pthread  -g -o server
	$(CC) client.c -std=c11 -g -O3 -Werror -Wall -Wextra -pthread -pedantic -o client -fsanitize=address

clean:
	rm -f server
	rm -f client
