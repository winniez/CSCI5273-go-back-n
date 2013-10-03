/* GBNserver.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close()  usleep()*/
#include <string.h> /* memset() */
#include <stdlib.h>
#include <time.h>
#include "sendto_h"
#include "util.h"

#define SERVER_SLEEP 10

/*    argv[0]	argv[1]		argv[2]		argv[3]	argv[4]		argv[5]
 * ./GBNserver <server_port> <error_rate> <random_seed> <output_file> <receive_log> 
 * */

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
	int serverSleep = SERVER_SLEEP;
	int RWS =  MAXPCKTBUFSIZE;
	int LFRead = 0;
	int LFRcvd = 0;
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
	printf("error rate : %f\n",atof(argv[2]));

	/* Note: you must initialize the network library first before calling sendto_().  The arguments are the <errorrate> and <random seed> */
	init_net_lib(atof(argv[2]), atoi(argv[3]));
	printf("error rate : %f\n",atof(argv[2]));

	/* socket creation */
	if((sd=socket(AF_INET, SOCK_DGRAM, 0))<0)
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
	if(bind(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) <0)
	{
		printf("%s: cannot to bind port number %d \n",argv[0], argv[1]);
		exit(1); 
	}

	/* Open log */
	FILE * rcvlog = fopen(rcv_log_fn, "a");	// append trances to log

	/* Receive message from client */
	struct sockaddr_in cliAddr;
	int cliLen = sizeof(cliAddr);
	int nbytes;
	char recvmsg[1024];
        bzero(recvmsg,sizeof(recvmsg));
        cliLen = sizeof(cliAddr);
        nbytes = recvfrom(sd, &rcvmsg, sizeof (recvmsg), 0, (struct sockaddr *) &cliAddr, &cliLen);
	
	// parse file name and size
	char fnamechar[256];
	char fsizechar[32]; 
	char* sec_arg = strstr(recvmsg, "\n");
	strncpy(fnamechar, recvmsg, (sec_arg - recvmsg));
      	strcpy(fsizechar, sec_arg+1);

	// open file
	FILE * recvfile = fopen(fnamechar, "wb");
	if (recvfile)
	{
	}
	else
	{
		printf("Failed open file %s, please retry. \n", fnamechar);
	}	

	/*
	char recvmsg[100];
	bzero(recvmsg,sizeof(recvmsg));
	cliLen = sizeof(cliAddr);
	nbytes = recvfrom(sd, &rcvmsg, sizeof (recvmsg), 0, (struct sockaddr *) &cliAddr, &cliLen);
	printf("Client says:\n %s \n", recvmsg);
	*/
	/* Respond using sendto_ in order to simulate dropped packets */
	char response[] = "respond this";
	nbytes = sendto_(sd, response, strlen(response),0, (struct sockaddr *) &cliAddr, sizeof(cliLen));
	
	// close log file
	fclose(rcvlog);
}

