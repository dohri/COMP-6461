#pragma once

#define TRACE

#ifdef TRACE
#define print(x) { stringstream str; str << "Client: "<< x; outputMessage(str.str()); }
#else
#define print(x) { cout << "Client: "<< x << endl; }
#endif

struct PacketData
{
	bool isSet;
	SYSTEMTIME timeSent;
	bool isResent;
};

class Client
{

	
public:
	
	int m_listeningPort;
	int m_routerListeningPort;
	string m_routerHostName;
	string m_hostname;
	WSADATA m_wdata;
	SOCKET m_listeningSocket;
	SOCKADDR_IN m_localSocAddr; 
	SOCKADDR_IN m_remoteSocAddr;
	char m_recBuff[MAXPACKETSIZE];
	int m_localGeneratedNumber;
	int m_remotGeneratedNumber;
	int m_localSequenceNumber;
	int m_remoteSequenceNumber;
	int m_windowSize;// sequence numbers that can be sent 0,1,2...m_windowSize-1
	int m_maxSiquenceNumber;// sequence number must be changed from 0 to m_windowSize => m_maxSiquenceNumber == m_windowSize
	int RETRY;
	char* m_sendBuffer;
	list<MessageHeader*> m_window;
	map<int,PacketData> m_sequenceMap;
	MessageHeader* m_lastSendPacket;

	ofstream m_logFile;
	bool m_acknowledgementState;// when it is falce no acknowledgment is received for a current request
	void getFilesList();
	bool bindToListeningPort();
	MessageHeader* sendUDPPacket(MessageHeader* message);
	bool initializeLocalSocAdd();
	bool initializeRemoteSocAdd();
	MessageHeader* receiveUDPPacket();
	MessageHeader*runSelectMethod();
	string getServerFiles();
	void handShaking();
	int generateRandomNum();
	int processClientMessages();
	string readCommand();
	bool getFile(string fileName);
	string getFileList();
	bool setFile(string fileName);

	void listFilesInDirectory();
	void updateSequenceNumber(int& sequence);
	void sendACK(string messageBody);
	void sendNAK(string messageBody);
	void resendLastPacket();
	bool waitForAcknowledgement();
	MessageHeader*  waitForHandshackAcknowledgement();

	MessageHeader* waitForData();
	void updateLastSendPacket(MessageHeader* lastSentPacket);

	int getLastSignificantBit(int number);
	void split(string fileList, vector<string>* fileItems);
	void parsServerFileList(string list);
	string intToStr(int intValue);
	void resetState();

	void freeLogFile(string log);
	bool fileExists(std::string fileName);

	void fillSequenceMap(int windowSize);
	void slideWindow(int receivedACKNumber, bool isACK);
	int computeSequenceNumberRange(int windowSize);
	MessageHeader* runIntermediateSelectMethod(int timeOut);
	void goBackN(int receivedNACNumber);
	void  waitForIntermediateAcknowledgement();
	void updateLastSendPachet(MessageHeader* lastSentPacket);
	void resendFirstPacket();
	bool getFileToRename(string fileName);
	void sendRequestedFile(string fileName);
	void testGet(string fileName);
	int rtt;
	SYSTEMTIME handshakeSendTime;
	void CalculateRTT(const SYSTEMTIME& sentTime);

	int previousSequenceNumber(int currentSequence)
	{
		return currentSequence == 0 ? m_maxSiquenceNumber : currentSequence - 1; 
	}

	bool m_processIntermediateAcks;

	Client(string router, int windowSize, int dropRate);
	Client(void){};
	~Client(void);


	static void writeIntoLogFile(string log);
	static void outputMessage(string message);
	
	class OperationError
	{
	public:
		string Message;
		OperationError(string message) : Message(message)
		{
		}
	};


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

