#pragma once


enum MessageType { DATA = 1, GET = 2, PUT = 3, ERR = 4, LIST = 6, ACK = 7, USERNAME = 8,  HANDSHAKE = 9, NAK = 10};
#define MAX_THREADS 5
#define FILEPATH ".\\files\\"
//#define RETRY 250

#define TIMER 3
#define waitall 8
#define MAXPACKETSIZE 1024
#define maxDataSize (MAXPACKETSIZE - sizeof(MessageHeader))
#define MAXIMUM_RTT 100
#define LAST_PACKET_RETRY 50
 
#define TIMEOUT 150
struct ServerParams
{
	ServerParams(std::string hostname = "localhost", int port = 2500)
	{
		
		serverListeningPort = port;
		this->hostname = hostname;
	}
	std::string hostname;
	int serverListeningPort;

};

struct CommunicationMessage{
	std::string messageType;
	std::string messageBody;
	
};

struct MessageHeader{
	MessageType msgType;
	int sequenceNumber;
	unsigned int msgSize;
	int isLastChunck;
	int ACK; // in caseif we need to send a data and acknowledge at the same time.
						//it keeps the sequence number of the message it wants to acknowlegde	
	//for debug 
	int msgnumber;
	char data[0];


	MessageHeader * clone()
	{
		int size = sizeof(MessageHeader) + this->msgSize;
		char * buffer = new char[size];
		memcpy(buffer, this, size);		
		return (MessageHeader *)buffer;
	}
	
};

struct DataMessage{
	unsigned int size;
	byte isLast;
	unsigned char* chunck;
};