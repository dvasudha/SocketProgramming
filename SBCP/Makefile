default: server client

server.o: server.c
	gcc -g -c server.c -o server.o
server: server.o
	gcc server.o -o server
client.o: client.c
	gcc -g -c client.c -o client.o
client: client.o
	gcc client.o -o client

clean:
	rm -rf server client *.o