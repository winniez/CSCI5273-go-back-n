/* GBNclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>   /* memset() */
#include <sys/time.h> /* select() */
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "sendto_.h"
#include "util.h"

/*
 * 	argv[0]		argv[1]		argv[2]  argv[3]	argv[4]		argv[5]		argv[6]
 * ./GBNclient <server_ip_address> <server_port> <error_rate> <random_seed> <send_file> <send_log>
 */

int main(int argc, char *argv[]) {
	int SWS = 9;
	int LAR = 0;
	int LFS = 0;
	int packet_size = sizeof(packet);
	int nbytes;
	int remote_len;
	// buffer
	packet packet_buffer[MAXPCKTBUFSIZE];	
	// ACK ack_buffer[MAXPCKTBUFSIZE];
	sendInfo departs[MAXPCKTBUFSIZE];

	/* check command line args. */
	if(argc<7)
	{
		printf("usage : %s <server_ip> <server_port> <error rate> <random seed> <send_file> <send_log> \n", argv[0]);
		exit(1);
	}
	

	/* Note: you must initialize the network library first before calling sendto_().  The arguments are the <errorrate> and <random seed> */
	init_net_lib(atof(argv[3]), atoi(argv[4]));
	printf("error rate : %f\n",atof(argv[3]));

	/* socket creation */
	int sd;
	if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("%s: cannot create socket \n",argv[0]);
		exit(1);
	}

	/* get server IP address (input must be IP address, not DNS name) */
	struct sockaddr_in remoteServAddr;
	bzero(&remoteServAddr, sizeof(remoteServAddr));               //zero the struct
	remoteServAddr.sin_family = AF_INET;                 //address family
	remoteServAddr.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remoteServAddr.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address
	printf("%s: sending data to '%s:%s' \n", argv[0], argv[1], argv[2]);
	remote_len = sizeof(remoteServAddr);

	/* Open log */
	FILE * sendlog = fopen(argv[6], "w");
	/* Open file to send */
	FILE * sendfile = fopen(argv[5], "rb");
	if (sendfile)
	{	
		// determine file size
		int fsize;
		char fsizechar[32];
		fseek(sendfile, 0, SEEK_END); // seek to end of file
		fsize = ftell(sendfile); // get current file pointer
		fseek(sendfile, 0, SEEK_SET); // seek back to beginning of file
		sprintf(fsizechar, "%d", fsize);
		printf("file size %s \n", fsizechar);

		// send file size & hand shake
		char msg[1024];
		ACK tmpack;
		strcpy(msg, argv[5]);
		strcat(msg, "\t");
		strcat(msg, fsizechar);
		nbytes = sendto(sd, msg, sizeof(msg), 0, (struct sockaddr*)&remoteServAddr, remote_len);
		// get ack
		nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*)&remoteServAddr);  	
		// resend when timed out
		while (!nbytes)
		{
			nbytes = sendto(sd, msg, sizeof(msg), 0, (struct sockaddr*)&remoteServAddr, remote_len);
			// get ack
			nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*)&remoteServAddr);
		}
		
		int seq = 0;
		int remain = fsize;
		while(remain >=0)
		{
				
			// update indexing
			remain -= MAXDATABUFSIZE;
			seq++;
	
		}	
	
	}
	else
	{
		printf("Failed open file %s, please retry. \n", argv[5]);
	}	
	
	// close socket	
	close(sd);
	// close log file
	fclose(sendlog);
}
