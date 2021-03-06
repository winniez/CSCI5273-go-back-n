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
	char log_line[1024];
	char tmpstr[256];
	time_t tmptime;
	int resend_cnt[MAXPCKTBUFSIZE];
	memset(resend_cnt, 0, sizeof(resend_cnt));

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
		int last_frame_cnt = 0;
		while(LAR < total-1)
		{
			if (LFS < LAR + SWS)
			{
				buf_index = seq % MAXPCKTBUFSIZE;
				packet_buffer[buf_index].type = DATA_TYPE;
				packet_buffer[buf_index].seq = seq;
				toread = (MAXDATABUFSIZE) < (remain) ? (MAXDATABUFSIZE) : (remain);
				packet_buffer[buf_index].size = toread;
				readed = fread(packet_buffer[buf_index].data, sizeof(char), toread, sendfile);
				nbytes = sendto_(sd, &(packet_buffer[buf_index]), sizeof(packet), 0, (struct sockaddr*) &remoteServAddr, remote_len);

				// s/r/rs seq [freeslot] LAR LFS time
				strcpy(log_line, "Send\t");
			        sprintf(tmpstr, "%d\t\t", seq);
				strcat(log_line, tmpstr);
				sprintf(tmpstr, "%d\t", LAR);
				strcat(log_line, tmpstr);
				sprintf(tmpstr, "%d\t", LFS);
				strcat(log_line, tmpstr);
				time(&tmptime);
				strcat(log_line, ctime(&tmptime));
				// strcat(log_line, "\n");
				fwrite(log_line, sizeof(char), strlen(log_line), sendlog);
				printf("%s", log_line);
				LFS++; seq++; remain -= toread;
			}
			else 
			{
				nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*)&remoteServAddr);
				// printf("nbytes: %d, tmpack.seq: %d, tmpack.rws: %d.\n", nbytes, tmpack.seq, tmpack.rws);
				if (nbytes)
				{// received

					if (LAR == tmpack.seq)
					{// duplicated, resend
						SWS = tmpack.rws;
					}	
					if (LAR < tmpack.seq)
					{// update LAR, SWS
						LAR = tmpack.seq;
						SWS = tmpack.rws;
					}	
					// s/r/rs seq [freeslot] LAR LFS time
					strcpy(log_line, "Receive\t");
					sprintf(tmpstr, "%d\t", tmpack.seq);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", tmpack.freeSlots);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", LAR);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", LFS);
					strcat(log_line, tmpstr);
					time(&tmptime);
					strcat(log_line, ctime(&tmptime));
					//strcat(log_line, "\n");
					printf("%s", log_line);
					fwrite(log_line, sizeof(char), strlen(log_line), sendlog);
				}
				else 
				{// timed out, resend up to LFS
					if (LAR >= total - MAXPCKTBUFSIZE  )
					{
						resend_cnt[LAR - total + MAXPCKTBUFSIZE]++;
						if (resend_cnt[LAR - total + MAXPCKTBUFSIZE] >= 10) break;
					}
					int o = LAR + 1;
					int right_most = LFS < (LAR + SWS) ? LFS : (LAR + SWS);
					for (o = LAR + 1; o <= right_most; o++)
					{
						// printf("timed out, resend %d packet.\n", o);
						buf_index = o % MAXPCKTBUFSIZE;
						nbytes = sendto_(sd, &(packet_buffer[buf_index]), sizeof(packet),
								0, (struct sockaddr*) &remoteServAddr, remote_len);
						// resend log
						strcpy(log_line, "Resend\t");
						sprintf(tmpstr, "%d\t\t", packet_buffer[buf_index].seq);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d\t", LAR);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d\t", LFS);
						strcat(log_line, tmpstr);
						time(&tmptime);
						strcat(log_line, ctime(&tmptime));
						//strcat(log_line, "\n");
						fwrite(log_line, sizeof(char), strlen(log_line), sendlog);
						printf("%s", log_line);
					}       	
				}	
				
			}
			if (SWS == 0)
			{// solution to deadlock
			 // probe if RWS has been reset 
			 // and reset SWS
			 // retrieve ack
			 	printf("SWS = 0\n");
				strcpy(log_line, "SWS=0\n");
				fwrite(log_line, sizeof(char), strlen(log_line), sendlog);
			 	nbytes = timeout_recvfrom(sd, &tmpack, sizeof(ACK), (struct sockaddr*) &remoteServAddr);
				if (nbytes) 
				{

					// s/r/rs seq [freeslot] LAR LFS time
					strcpy(log_line, "Receive\t");
					sprintf(tmpstr, "%d\t", tmpack.seq);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", tmpack.freeSlots);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", LAR);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d\t", LFS);
					strcat(log_line, tmpstr);
					time(&tmptime);
					strcat(log_line, ctime(&tmptime));
					// strcat(log_line, "\n");
					fwrite(log_line, sizeof(char), strlen(log_line), sendlog);
					printf("%s", log_line);
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
