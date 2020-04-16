/*Created By Nehul Oswal*/
/* Have implemented TTL option as well. */
/*The first argument is IP address and second argument is TTL*/


/* Importing Header files */
#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h> 
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <netinet/ip_icmp.h> 
#include<ctype.h>
#include <time.h> 
#include <fcntl.h> 
#include <time.h> 
#include<errno.h>


//Ping pkt with Icmp header and message load
struct ping_pkt 
{ 
    struct icmphdr hdr; 
    char msg[64 -sizeof(struct icmphdr)]; 
}; 


//logic for ICMP checksum calculation
unsigned short checksum(void *b, int len) 
{ 
    unsigned short *buf = b; 
	unsigned int sum=0; 
	unsigned short result; 

	for ( sum = 0; len > 1; len -= 2 ) 
		sum += *buf++; 
	if ( len == 1 ) 
		sum += *(unsigned char*)buf; 
	sum = (sum >> 16) + (sum & 0xFFFF); 
	sum += (sum >> 16); 
	result = ~sum; 
	return result; 
} 


int main(int argc, char *argv[]) {

    struct timeval tv_out; 
    struct ping_pkt pckt;
    struct sockaddr_in myaddr, remaddr;
    struct timespec time_start, time_end;
    struct timeval tv;
    struct hostent* pHostInfo;
    int msg_received_count = 0;
    int transmitted = 0;
    int flag = 1;
    int resAddressSize = sizeof(remaddr);
    int ttl_val;
    long double rtt_msec=0;
    char input[sizeof(argv[1])];
    char *ip_addr;
    strcpy(input,argv[1]);

    tv_out.tv_sec = 1; 
    tv_out.tv_usec = 0; 
    
    if(argc <= 1)
    {
        printf("%s\n", "Insufficient Argument");
        return 0;
    }
    else
    {
        ttl_val = atoi(argv[2]); //ttl value from the command line
        pHostInfo = gethostbyname(input); //Resolving the hostname

        if(!pHostInfo){
            printf("Could not resolve host name\n");
            return 0;
        }
        ip_addr = inet_ntoa(*(struct in_addr *)pHostInfo->h_addr_list[0]); //Get the IP address in standard format
    }

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); //Creating Raw socket
    if(s <= 0)
    {
        perror("Socket Error");
        exit(0);
    }


    //Setting the destination adress information
    memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = inet_addr(ip_addr);
	myaddr.sin_port = htons(0);


    setsockopt(s, IPPROTO_IP, IP_TTL, &ttl_val, sizeof(ttl_val)); //Set the TTL limit on ICMP packets
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_out, sizeof(tv_out)); //Wait only for 1 sec to receive the message
    
    while(1)
    { 
        flag = 1; //To indicate whether the message has been sent successfully
        int i;

        memset(&pckt, 0, sizeof(pckt)); 
        
        //Setting ICMP packet values
        pckt.hdr.type = ICMP_ECHO; 
		pckt.hdr.un.echo.id = getpid(); 
		for (i = 0; i < sizeof(pckt.msg)-1; i++ ) 
			pckt.msg[i] = i+'0'; 
		pckt.msg[i] = 0; 

		pckt.hdr.un.echo.sequence = ++transmitted; 
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));

        sleep(1);

        //Keeping track of the  time when message is sent to calculate RTT
        clock_gettime(CLOCK_MONOTONIC, &time_start); 
        int actionSendResult = sendto(s, &pckt, sizeof(pckt), 0, (struct sockaddr*)&myaddr, sizeof(myaddr)); //Sending ICMP packet

        if(actionSendResult < 0) //Unsuccessfull Send
        {
            flag = 0;
            perror("Ping Error");
            exit(0);
        }

        char buff[sizeof(pckt)]; //Receiver Buffer

        int ressponse = recvfrom(s, buff, sizeof(buff), 0, (struct sockaddr *) &remaddr,&resAddressSize);

        struct icmphdr* recvd = (struct icmphdr *)&buff[20]; //ignoring the first 20 bytes they are occupied by IP header

        clock_gettime(CLOCK_MONOTONIC, &time_end); 
            
        double timeElapsed = ((double)(time_end.tv_nsec - time_start.tv_nsec))/1000000.0;
        rtt_msec = (time_end.tv_sec- time_start.tv_sec) * 1000.0 + timeElapsed; //Calculating RTT

        
        if(flag) 
        { 
            if(!(recvd->type ==0))  //If ECHO_REPLY not received
                printf(" Error..Packet received with ICMP type %d code %d seq %d \n", recvd->type, recvd->code, recvd->un.echo.sequence); 
            else if(transmitted != recvd->un.echo.sequence && errno == 11) //If TTL limit reached
                printf(" TTL exceeded for packet no %d \n", transmitted);
            else
            { 
                msg_received_count++; 
                printf(" transmitted_No = %d Received_No=%d packet loss = %f  rtt = %Lf ms.\n",transmitted, recvd->un.echo.sequence, ((transmitted - (float)msg_received_count )/transmitted) * 100, rtt_msec); 
            } 
        }
        else
        {
            perror("Response Error");
        }
    }
    return 0;
}