Readme about Go-Back-N, Sliding Windows and Window-Based Flow Control

Author:
Xinying Zeng, Yisha Wu

Description:
We have 2 struct: packet and ACK. packet is used to store the type of packet (send, receive, resend), sequence number, size and data. ACK is used to store send type, sequence number, RWS and free slots of the server.

The process of transmitting files by our application is:
1. The client read the filename and file size and transmit them to the server. The server send ACK #-1 back.
2. The client split file into packets, assign seq number and size to each packet. Client then transmits packets till LFS hits window size. On the server side, when packet arrives in order, server updates window size (free slots etc.), send ACK and write file, server also scan buffer looking for previously arrived out of order packets, writes data into file by sequence; when packet arrives out of order, server sends duplicate ACK and store packet in buffer.
3. If the client receives the ACK within timeout, slide the sending window right. Otherwise, client resends the window.
4. For window-based-control, afte server receives 30 packets the server starts to shut down its receive window 1 packet at a time until RWS = 0. Then the server must wait 10 ms before it can reset its window size to 6. After RWS reset to 6, server keep sending ACK with RWS=6 until receive packet from client. On the client side, SWS is synced with RWS on ACK arrives, when SWS hits 0, clients keep polling socket to get ACK with reset RWS. 

To compile the source code, input "make all".
We attached 2 test files: foo1(22133 bytes) and test.csv (248136 bytes).
To run the application, input "./GBNserver <port number> <error rate> <seed> <recv filename> <recv log name>" in the server side. 
e.g. ./GBNserver 5005 0.2 3430 recv server.log
And in the client side, input "./GBNclient <server IP> <port number> <error rate> <seed> <send filename> <send log name>".
e.g. ./GBNclient 127.0.1.1 5005 0.3 1236 test.csv client.log

For our application, we can reliably transfer a data file between a client and server even with error rate, slide windows at both the client and server, with cumulative acknowledgements, retransmit files when timeout, discard of out of range packets, buffer out of order packets. And we implemented the window-based flow control by adjusting SWS according to the received RWS.

To solve the case of losting the ACK in the last SWS, the server is closed so we set the client to retransmit the last packets just like normal timeout. The different is that we set them only transmit only 10 times at most. Then we close any open files, client log and client application. 

The 1024 bytes is the size of packet data, the header is the other member of the packet struct.

The ACK only contains the type, sequence number, RWS and number of free slots.
