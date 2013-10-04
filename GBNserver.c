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

/*    argv[0]	argv[1]		argv[2]		argv[3]	argv[4]		argv[5]
 * ./GBNserver <server_port> <error_rate> <random_seed> <output_file> <receive_log> 
 * */
int RWS =  6;
int LFRead = 0;
int LFRcvd = 0;
int LAF = 6;

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
	int upper_limit = LFRead +  MAXPCKTBUFSIZE;
	int free_slots = upper_limit - LFRcvd;
	RWS = free_slots;
	LAF = LFRcvd + RWS;

	char log_line[1024];
	char tmpstr[256];
	time_t tmptime;
	
	// buffer
	packet packet_buffer[MAXPCKTBUFSIZE];
	packet tmppacket;
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
	int window_decreasing_flag = 0;
	ACK tmpack;
	// initial shake, recv filename and filesize from client
	char recvmsg[1024];
        bzero(recvmsg,sizeof(recvmsg));
        cliLen = sizeof(cliAddr);
        nbytes = recvfrom(sd, &recvmsg, sizeof (recvmsg), 0, (struct sockaddr *) &cliAddr, &cliLen);
	printf("Client says:\n %s \n", recvmsg);
	
	// parse file name and size
	char fnamechar[256];
	char fsizechar[32]; 
	int fsize;
	char* sec_arg = strstr(recvmsg, "\t");
	strncpy(fnamechar, recvmsg, (sec_arg - recvmsg));
      	strcpy(fsizechar, sec_arg+1);
	fsize = atoi(fsizechar);
	int total = fsize / MAXDATABUFSIZE;
	if (fsize % MAXDATABUFSIZE) total += 1;
	// open file
	FILE * recvfile = fopen(argv[4], "wb");
	if (recvfile)
	{
		// send ack
		tmpack.type = ACK_TYPE;
		tmpack.seq = -1;
		tmpack.rws = RWS;
		nbytes = sendto(sd, &tmpack, sizeof(ACK), 0, (struct sockaddr*)&cliAddr, cliLen);
		
		// receiving file
		int seq = 0;
		int buf_index;
		int skip_recv = 0;
		LFRead = -1;
		LFRcvd = -1;
		LAF = RWS - 1;
		while (seq < total)
		{	
			if (!skip_recv)
			{	
				nbytes = timeout_recvfrom(sd, &(tmppacket), sizeof(packet), (struct sockaddr*)&cliAddr);
			}
			else
			{
				skip_recv = 0;
			}	
			if (nbytes)
			{// received, check if out of order
				// force decrease receive window size
				if (window_decreasing_flag) RWS--;
				// arriave in order
				if (tmppacket.seq == LFRcvd + 1)
				{// update LFRcvd, send ACK, write file,update LFRead
					// update window
					LFRcvd++;
					seq++;
					upper_limit = LFRead +  MAXPCKTBUFSIZE;
					free_slots = upper_limit - LFRcvd;
					RWS = free_slots;
					LAF = LFRcvd + RWS;
					// construct ACK
					tmpack.seq = LFRcvd;
					tmpack.freeSlots = free_slots;
					tmpack.rws = RWS;

					// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
					strcpy(log_line, "Receive ");
					sprintf(tmpstr, "%d ", seq);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRead);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRcvd);
					strcat(log_line, tmpstr);
					sprintf(log_line, "%d ", LAF);
					strcat(log_line, tmpstr);
					time(&tmptime);
					strcat(log_line, ctime(&tmptime));
					strcat(log_line, "\n");
					fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
					printf("%s", log_line);

					// send ACK
					nbytes = sendto_(sd, &tmpack, sizeof(ACK), 0, (struct sockaddr*)&cliAddr, cliLen);

					// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
					strcpy(log_line, "Send ");
					sprintf(tmpstr, "%d ", seq);
					strcat(log_line, tmpstr);
					sprintf(log_line, "%d ", free_slots);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRead);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRcvd);
					strcat(log_line, tmpstr);
					sprintf(log_line, "%d ", LAF);
					strcat(log_line, tmpstr);
					time(&tmptime);
					strcat(log_line, ctime(&tmptime));
					strcat(log_line, "\n");
					fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
					printf("%s", log_line);

					// write file
					fwrite((char*)&(tmppacket.data), sizeof(char), tmppacket.size, recvfile);
					// update LFRead
					LFRead = tmppacket.seq ; // equivalent to LFRread++;	

					// scan previous received out of order packets
					int k = LFRcvd;
					for(k = LFRcvd + 1; k < LFRcvd + 1 + MAXPCKTBUFSIZE; k++)
					{
						buf_index = k % MAXPCKTBUFSIZE;
						if (packet_buffer[buf_index].seq == LFRcvd + 1)
						{
							// update window
							LFRcvd++;
							seq++;
							upper_limit = LFRead +  MAXPCKTBUFSIZE;
							free_slots = upper_limit - LFRcvd;
							RWS = free_slots;
							// construct ACK
							tmpack.seq = LFRcvd;
							tmpack.freeSlots = free_slots;
							tmpack.rws = RWS;
							// send ACK
							nbytes = sendto_(sd, &tmpack, sizeof(ACK), 0, 
									(struct sockaddr*)&cliAddr, cliLen);

							// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
							strcpy(log_line, "Send ");
							sprintf(tmpstr, "%d ", seq);
							strcat(log_line, tmpstr);
							sprintf(log_line, "%d ", free_slots);
							strcat(log_line, tmpstr);
							sprintf(tmpstr, "%d ", LFRead);
							strcat(log_line, tmpstr);
							sprintf(tmpstr, "%d ", LFRcvd);
							strcat(log_line, tmpstr);
							sprintf(log_line, "%d ", LAF);
							strcat(log_line, tmpstr);
							time(&tmptime);
							strcat(log_line, ctime(&tmptime));
							strcat(log_line, "\n");
							fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
							printf("%s", log_line);

							// write file
							fwrite((char*)&(packet_buffer[buf_index].data), sizeof(char), packet_buffer[buf_index].size, recvfile);
							// update LFRead
							LFRead = LFRcvd;
						}	
					}
				}
				if (tmppacket.seq > LFRcvd + 1)
				{// arrive out of order, cache packet, send duplicate ACK
				 // as we are sending a duplicate ACK, which is exactly the previous ACK sent
				 // just update ack.rws would be enough
				 	tmpack.rws = RWS;	
					nbytes = sendto_(sd, &tmpack, sizeof(ACK), 0, (struct sockaddr*)&cliAddr, cliLen);

					// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
					strcpy(log_line, "Send ");
					sprintf(tmpstr, "%d ", seq);
					strcat(log_line, tmpstr);
					sprintf(log_line, "%d ", free_slots);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRead);
					strcat(log_line, tmpstr);
					sprintf(tmpstr, "%d ", LFRcvd);
					strcat(log_line, tmpstr);
					sprintf(log_line, "%d ", LAF);
					strcat(log_line, tmpstr);
					time(&tmptime);
					strcat(log_line, ctime(&tmptime));
					strcat(log_line, "\n");
					fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
					printf("%s", log_line);

					// buffer out of order packet received
					buf_index = tmppacket.seq % MAXPCKTBUFSIZE;
					packet_buffer[buf_index].seq = tmppacket.seq;
					packet_buffer[buf_index].size = tmppacket.size;
					packet_buffer[buf_index].type = tmppacket.type;
					strcpy(packet_buffer[buf_index].data, tmppacket.data);
					
				}
				if (tmppacket.seq < LFRcvd + 1)
				{// packets alreadly received and buffered, discard
					;
				}	
			}
			// decrease rws
			if (seq == 30) window_decreasing_flag = 1;
			// handle situation when RWS is 0
			if (RWS == 0) 
			{// RWS decreased to 0
				printf("RWS = 0!!!!!\n");
				if (window_decreasing_flag)
				{// reset flag and sleep 10 ms	
					window_decreasing_flag = 0;
					receiver_sleep();
					RWS = 6;
					// send ACK
					do
					{	
						tmpack.rws = RWS;
						nbytes = sendto_(sd, &tmpack, sizeof(ACK), 0, (struct sockaddr*)&cliAddr, cliLen);

						// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
						strcpy(log_line, "Send ");
						sprintf(tmpstr, "%d ", seq);
						strcat(log_line, tmpstr);
						sprintf(log_line, "%d ", free_slots);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d ", LFRead);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d ", LFRcvd);
						strcat(log_line, tmpstr);
						sprintf(log_line, "%d ", LAF);
						strcat(log_line, tmpstr);
						time(&tmptime);
						strcat(log_line, ctime(&tmptime));
						strcat(log_line, "\n");
						fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
						printf("%s", log_line);

						nbytes = timeout_recvfrom(sd, &tmppacket, sizeof(packet), (struct sockaddr*)&cliAddr);

						// s/r/rs seq [freeslot] LFRead LFRcvd LAF time
						strcpy(log_line, "Receive ");
						sprintf(tmpstr, "%d ", seq);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d ", LFRead);
						strcat(log_line, tmpstr);
						sprintf(tmpstr, "%d ", LFRcvd);
						strcat(log_line, tmpstr);
						sprintf(log_line, "%d ", LAF);
						strcat(log_line, tmpstr);
						time(&tmptime);
						strcat(log_line, ctime(&tmptime));
						strcat(log_line, "\n");
						fwrite(log_line, sizeof(char), strlen(log_line), rcvlog);
						printf("%s", log_line);

					}while(!nbytes);	
					skip_recv = 1;
				}
			}	
		}
	fclose(recvfile);	
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

