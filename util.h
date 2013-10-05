#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

#define MAXDATABUFSIZE 1024
#define MAXPCKTBUFSIZE 9
#define MAXATTEMPTS 10
#define ACK_TYPE 0
#define DATA_TYPE 1
#define SEND 2
#define RECV 3
#define RESEND 4
// TIMEOUT 50 ms
#define TIMEOUT 50 
#define SERVER_SLEEP 10

typedef struct {
        int type;
	int seq;
        int size;
        char data[MAXDATABUFSIZE];
} packet;

typedef struct {
	int type;
        int seq;
        int rws;
	int freeSlots;
} ACK;


void timestamp(char* timestr)
{
	time_t ltime; /* calendar time */
        ltime = time(NULL); /* get current cal time */
	timestr = ctime(&ltime);
	// timestr = asctime(localtime(&ltime));
}

int difftime_ms(struct timespec* end, struct timespec* start)
{
	int diff = (end->tv_sec - start->tv_sec) * 10^(3);
	diff = diff + (end->tv_nsec - start->tv_nsec) * 10^(-6);
	return diff;
}	

void receiver_sleep()
{
	useconds_t time_interval = SERVER_SLEEP;
	usleep(time_interval);
}	

/*
 * timeout_recvfrom
 * return 1 for successfully received
 * return 0 for timeout
 */
int timeout_recvfrom (int sock, void *buf, int length, struct sockaddr *connection)
{
	// printf("in timeout_recvfrom\n");
	fd_set socks;
	// add socket into fd_set
	FD_ZERO(&socks);
	FD_SET(sock, &socks);
	struct timeval t;
	t.tv_usec = TIMEOUT;
	int connection_len = sizeof(connection);

	if (select(sock + 1, &socks, NULL, NULL, &t) &&
		recvfrom(sock, buf, length, 0, (struct sockaddr *)connection, &connection_len)>=0)
	{        
		// received msg
		// printf("leaving timeout_recvfrom, return 1\n");
		return 1;
	}	
	else
	{	
		// timed out
		// printf("leaving timeout_recvfrom, return 0\n");
		return 0;
	}	
}
