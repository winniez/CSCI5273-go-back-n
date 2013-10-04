C = gcc
all: 
	$(CC) sendto_.h util.h GBNserver.c -o GBNserver -lrt
	$(CC) sendto_.h util.h GBNclient.c -o GBNclient -lrt
server: 
	$(CC) sendto_.h util.h GBNserver.c -o GBNserver -lrt
client:
	$(CC) sendto_.h util.h GBNclient.c -o GBNclient -lrt
clean:
	$(RM) server clien

