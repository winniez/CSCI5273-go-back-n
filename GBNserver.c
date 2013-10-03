/* GBNserver.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close()  usleep()*/
#include <string.h> /* memset() */
#include <stdlib.h>
#include <time.h>
#include "sendto_.h"
#include "util.h"

#define SERVER_SLEEP 10

/*    argv[0]	argv[1]		argv[2]		argv[3]	argv[4]		argv[5]
 * ./GBNserver <server_port> <error_rate> <random_seed> <output_file> <receive_log> 
 * */
int serverSleep = SERVER_SLEEP;
int RWS =  6;
int LFRead = 0;
int LFRcvd = 0;


/* timeout is required when calling resetRWS()
 * ACK can be lost and lead to deadlock
 * slides Chapter2.3 page24
 */
void resetRWS()
{
	RWS = 6;
	// resend ACK
	
}	


int main(int argc, char *argv[]) {	
	int LAF = RWS;
	int upper_limit = LFRead +  MAXPCKTBUFSIZE;
	int free_slots = upper_limit - LFRcvd;
	RWS = free_slots;
	
	// buffer
	packet buffer[MAXPCKTBUFSIZE];
	
	/* check command line args. */
	if(argc<6) {
		printf("usage : %s <server_port> <error rate> <random seed> <send_file> <send_log> \n", argv[0]);
		exit(1);
	}

	/* Note: you must initialize the network library first before calling sendto_().  The arguments are the <errorrate> and <random seed> */
	init_net_lib(atof(argv[2]), atoi(argv[3]));
	printf("error rate : %f\n",atof(argv[2]));

	/* socket creation */
	int sd;
	if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("%s: cannot open socket \n",argv[0]);
		exit(1);
	}

	/* bind server port to "well-known" port whose value is known by the client */
	struct sockaddr_in servAddr;
	bzero(&servAddr,sizeof(servAddr));                    //zero the struct
	servAddr.sin_family = AF_INET;                   //address family
	servAddr.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	servAddr.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine
	if(bind(sd, (struct sockaddr *)&servAddr, sizeof(servAddr)) <0)
	{
		printf("%s: cannot to bind port number %s \n",argv[0], argv[1]);
		exit(1); 
	}

	/* Open log */
	FILE * rcvlog = fopen(argv[5], "a");	// append trances to log

	/* Receive message from client */
	struct sockaddr_in cliAddr;
	int cliLen = sizeof(cliAddr);
	int nbytes;
	
	// initial shake, recv filename and filesize from client
	char recvmsg[1024];
        bzero(recvmsg,sizeof(recvmsg));
        cliLen = sizeof(cliAddr);
        nbytes = recvfrom(sd, &recvmsg, sizeof (recvmsg), 0, (struct sockaddr *) &cliAddr, &cliLen);
	printf("Client says:\n %s \n", recvmsg);
	
	// parse file name and size
	char fnamechar[256];
	char fsizechar[32]; 
	char* sec_arg = strstr(recvmsg, "\t");
	// strncpy(fnamechar, recvmsg, (sec_arg - recvmsg));
      	strcpy(fsizechar, sec_arg+1);

	// open file
	FILE * recvfile = fopen(argv[4], "wb");
	if (recvfile)
	{
		// send ack
		ACK ack;
		ack.type = ACK_TYPE;
		ack.seq = -1;
		ack.rws = RWS;
		nbytes = sendto(sd, &ack, sizeof(ack), 0, (struct sockaddr*)&cliAddr, cliLen);
		
		// receiving file
		
	}
	else
	{
		printf("Failed open file %s, please retry. \n", fnamechar);
	}	

	// close log file
	fclose(rcvlog);
	// close socket
	close(sd);
}

