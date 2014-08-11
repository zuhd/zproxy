zproxy : socket_server.c main.c
	gcc -g -Wall -o $@ $^ -lpthread

clean:
	rm zproxy
