#pragma once


#define TRACE

#ifdef TRACE
#define print(x) { stringstream str; str << "Server: "<< x; outputMessage(str.str()); }
#else
#define print(x) { cout << "Server: "<< x << endl; }
#endif

#include <list>

struct PacketData
{
	bool isSet;
	SYSTEMTIME timeSent;
	bool isResent;
};

class Server
{
public:
	WSADATA m_wdata;
	int m_listeningPort;
	int m_routerListeningPort;
	string m_routerHostName;
	SOCKET m_listeningSocket;
	char m_recBuff[MAXPACKETSIZE];
	int m_localGeneratedNumber;
	int m_remotGeneratedNumberm;
	int m_windowSize;// sequence numbers that can be sent 0,1,2...m_windowSize-1
	int m_maxSiquenceNumber;// sequence number must be changed from 0 to m_windowSize => m_maxSiquenceNumber == m_windowSize
	
	list<MessageHeader*> m_window;
	map<int, PacketData> m_sequenceMap;

	int m_localSequence;
	int m_remoteSequence;
	int m_requestNumber;
	bool m_handshackeIsDone;
	int m_remotGeneratedNumber;
	int m_lastAccNumber;
	int m_lastNakNumber;
	int RETRY;

	int rtt;
	SYSTEMTIME handshakeSendTime;
	void CalculateRTT(const SYSTEMTIME& sentTime);


	MessageHeader* m_lastSendPacket;
	char* m_sendBuffer;
	bool m_acknowledgementState;

	SOCKADDR_IN m_localSocAddr; 
	SOCKADDR_IN m_remoteSocAddr;
	void fillSequenceMap(int windowSize);
	bool sendUDPPacket(MessageHeader* message);
	bool initializeLocalSocAdd();
	bool initializeRemoteSocAdd();
	MessageHeader* receiveUDPPacket();
	MessageHeader* runSelectMethod();
	void handShaking();
	bool getFile(string fileName);
	int generateRandomNum();

	int getLastSignificantBit(int number);
	void processIncommingMessages();
	bool fileExists(std::string fileName);
	void receiveFile(std::string fileName);
	void sendRequestedFile(std::string filePath);
	void updateSequenceNumber(int& sequence);
	void sendACK(string messageBody);
	void resendFirstPacket();
	void resendLastPacket();
	bool waitForAcknowledgement();
	MessageHeader* waitForData();
	void updateLastSendPachet(MessageHeader* lastSentPacket);
	static void writeIntoLogFile(string log);
	static void outputMessage(string message);
	string intToStr(int intValue);
	MessageHeader*  waitForHandShack();
	void sendErrorMessage(string messageBody);
	void resetState();
	void listFilesInDirectory();
	void sendFileList(string list);
	void freeLogFile(string log);

	int computeSequenceNumberRange(int windowSize);// computes the sequence number range basedd on the number of bits given
	void slideWindow(int receivedACKNumber, bool isACK);
	void waitForIntermediateAcknowledgement();
	void goBackN(int receivedNACNumber);
	MessageHeader* runIntermediateSelectMethod(int timeOut);

	void sendNAK(string messageBody);

	int previousSequenceNumber(int currentSequence)
	{
		return currentSequence == 0 ? m_maxSiquenceNumber : currentSequence - 1; 
	}
	
	bool m_processInterMediateAcks;

	Server(string router, int windowSize, int dropRate);
	Server(void){};
	~Server(void);
	bool bindToListeningPort();


	void resetStats()
	{
		sentPackets =
		sentBytes =
		recvPackets =
		recvBytes = 0;
	}

	int sentPackets;
	int sentBytes;
	int recvPackets;
	int recvBytes;

	int MsgSize(MessageHeader * msg)
	{
		return sizeof(MessageHeader) + msg->msgSize;
	}
	
};

