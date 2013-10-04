/* GBNclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
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

int SWS = 9;
int LAR = 0;
int LFS = 0;

int main(int argc, char *argv[]) {
	int packet_size = sizeof(packet);
	int nbytes;
	int remote_len;
	struct timespec cur_time;
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
		SWS = tmpack.rws;
		// resend when timed out
		while (!nbytes)
		{
			nbytes = sendto(sd, msg, sizeof(msg), 0, (struct sockaddr*)&remoteServAddr, remote_len);
			// get ack
			nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*)&remoteServAddr);
			// update SWS
			SWS = tmpack.rws;
		}
		
		int seq = 0;
		int buf_index = seq % MAXDATABUFSIZE;
		int remain = fsize;
		int total = fsize / MAXDATABUFSIZE;
		if (fsize % MAXDATABUFSIZE) total += 1;
		int readed, toread;
		LAR = -1; 
		LFS = -1;
		while(remain >0)
		{
			if (LFS <= LAR + SWS)
			{
				buf_index = seq % MAXDATABUFSIZE;
				packet_buffer[buf_index].type = DATA_TYPE;
				packet_buffer[buf_index].seq = seq;
				toread = (MAXDATABUFSIZE) < (remain) ? (MAXDATABUFSIZE) : (remain);
				packet_buffer[buf_index].size = toread;
				readed = fread(packet_buffer[buf_index].data, sizeof(char), toread, sendfile);
				nbytes = sendto_(sd, &(packet_buffer[buf_index]), sizeof(packet), 0, (struct sockaddr*) &remoteServAddr, remote_len);

				// s/r/rs seq [freeslot] LAR LFS time
				char send_line[500] = "Send ";
				char tmp_send_line[500];
				sprintf(tmp_send_line, "%d", seq);
				strcat(send_line, tmp_send_line);
				strcat(send_line, " ");
				sprintf(tmp_send_line, "%d", LAR);
				strcat(send_line, tmp_send_line);
				strcat(send_line, " ");
				sprintf(tmp_send_line, "%d", LFS);
				strcat(send_line, tmp_send_line);
				strcat(send_line, " ");
				time_t ti;
				time(&ti);
				strcat(send_line, ctime(&ti));
				strcat(send_line, "\n");
				fwrite(send_line, strlen(send_line) * sizeof(char), 1, sendlog);

				// set timer
				departs[buf_index].seq = seq;
				clock_gettime(CLOCK_MONOTONIC, &(departs[buf_index].send_stamp));
				//time(&(departs[buf_index].send_timestamp));
				LFS++;
				remain -= toread;
				seq++;
				buf_index = seq % MAXDATABUFSIZE;
			}
			else 
			{
				nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*)&remoteServAddr);
				if (nbytes)
				{// received

					// s/r/rs seq [freeslot] LAR LFS time
					char recv_line[500] = "Receive ";
					char tmp_recv_line[500];
					sprintf(tmp_recv_line, "%d", seq);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", tmpack.freeSlots);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", LAR);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", LFS);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					time_t ti;
					time(&ti);
					strcat(recv_line, ctime(&ti));
					strcat(recv_line, "\n");
					fwrite(recv_line, strlen(recv_line) * sizeof(char), 1, sendlog);


					if (LAR == tmpack.seq) 
					{//duplicate, resend 	
						SWS = tmpack.rws;
					}
					if (LAR > tmpack.seq)
					{// update LAR
						LAR = tmpack.seq;
						SWS = tmpack.rws;
					}	
				}
				// check if there's timeout
				int k;
				clock_gettime(CLOCK_MONOTONIC, &cur_time);
				// time(&cur_time); 
				for (k=0; k<MAXPCKTBUFSIZE; k++)
				{
					if (difftime_ms(&cur_time, &(departs[k].send_stamp)) > 50)
					{// resend
						int o = 0;
						int right_most = (LFS) < (LAR + SWS)? (LFS) : (LAR + SWS);
						for (o = LAR + 1; o <= right_most; o++)
						{
							buf_index = o % MAXDATABUFSIZE;
							nbytes = sendto_(sd, &(packet_buffer[buf_index]), sizeof(packet), 
									0, (struct sockaddr*) &remoteServAddr, remote_len);

							// s/r/rs seq [freeslot] LAR LFS time
							char resend_line[500] = "Resend ";
							char tmp_resend_line[500];
							sprintf(tmp_resend_line, "%d", seq);
							strcat(resend_line, tmp_resend_line);
							strcat(resend_line, " ");
							sprintf(tmp_resend_line, "%d", LAR);
							strcat(resend_line, tmp_resend_line);
							strcat(resend_line, " ");
							sprintf(tmp_resend_line, "%d", LFS);
							strcat(resend_line, tmp_resend_line);
							strcat(resend_line, " ");
							time_t ti;
							time(&ti);
							strcat(resend_line, ctime(&ti));
							strcat(resend_line, "\n");
							fwrite(resend_line, strlen(resend_line) * sizeof(char), 1, sendlog);

							// set timer
							clock_gettime(CLOCK_MONOTONIC, &departs[buf_index].send_stamp);
						}	
						break;	
					}	
				}	

			}
			if (SWS == 0)
			{// solution to deadlock
			 // probe if RWS has been reset 
			 // and reset SWS
			 // retrieve ack
			 	nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*) &remoteServAddr);
				if (nbytes) 
				{

					// s/r/rs seq [freeslot] LAR LFS time
					char recv_line[500] = "Receive ";
					char tmp_recv_line[500];
					sprintf(tmp_recv_line, "%d", seq);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", tmpack.freeSlots);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", LAR);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					sprintf(tmp_recv_line, "%d", LFS);
					strcat(recv_line, tmp_recv_line);
					strcat(recv_line, " ");
					time_t ti;
					time(&ti);
					strcat(recv_line, ctime(&ti));
					strcat(recv_line, "\n");
					fwrite(recv_line, strlen(recv_line) * sizeof(char), 1, sendlog);

					SWS = (tmpack.rws) > 0 ? (tmpack.rws) : 0;
				}	
			}	
		}	
		fclose(sendfile);	
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
