Lab4 = server client

Obj = server.o client.o message.o table.o
all: ${Lab4}
server: server.o message.o table.o
	gcc server.o message.o table.o -pthread -g -o server
client: client.o message.o table.o
	gcc client.o message.o table.o -pthread -lncurses -g -o client
server.o: server.c
	gcc server.c -c -g -o server.o
client.o: client.c
	gcc client.c -c -g -o client.o
message.o: message.c
	gcc message.c -c -g -o message.o
table.o: hash_table.c
	gcc hash_table.c -c -g -o table.o
clean:  
	rm -f ${Lab4} ${Obj}
