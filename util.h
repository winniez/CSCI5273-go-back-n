#include <time.h>
#include <string.h>

#define MAXDATABUFSIZE 1024
#define MAXPCKTBUFSIZE 10
#define MAXATTEMPTS 10
#define ACK_TYPE 0
#define DATA_TYPE 1
#define SEND 2
#define RECV 3
#define RESEND 4

typedef struct {
        int type;
	int seq;
        int size;
        char data[MAXBUFSIZE];
} packet;

typedef struct {
	int type;
        int seq;
        int rws;
} ACK;


char* timestamp()
{
	time_t ltime; /* calendar time */
        ltime = time(NULL); /* get current cal time */
	char timestr[32];
	timestr = ctime(&ltime);
	//timestr = asctime(localtime(&ltime));
	return timestr;
}

char* log_entry(int type, int seq, int freeSlot)
{
	char entry[512];
	if (type == SEND)
	{
		strcpy(entry, "<Send> ");
	}
	if (type == RECV)
	{
		strcpy(entry, "<Receive> ");
	}
	if (type == RESEND)
	{
		strcpy(entry, "<Resend> ");
	}	

	return entry;
}	


void waitFor (unsigned int secs) {
	retTime = time(0) + secs;     // Get finishing time.
	while (time(0) < retTime);    // Loop until it arrives.
}
