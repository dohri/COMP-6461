#pragma once
#define _CRT_SECURE_NO_WARNINGS // this line was suggested qby the compiler (to avoid using strcpu_s )
#include <SDKDDKVer.h>
#include <queue>
#include <map>
#include <math.h >
#include <Windows.h>
#include <lmcons.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>
#include <list>
using namespace std;
#include <time.h>
#pragma comment(lib, "Ws2_32.lib") //liks to winsosk library
#include "Objects.h"
#include "Server.h"
#include <stdio.h>
#include "Server.h"


// get the current host name
string getHostname()
{
	char hostname[1024];
	DWORD size = 1024;
	GetComputerName(hostname,  &size);
	return hostname;
}


string MsgTypeToStr(MessageType t)
{
	string ret = "";
	switch(t)
	{
	case DATA: return "Data";
	case GET: return "Get";
	case PUT: return "Put";
	case ERR: return "Err";
	case LIST:return "List";
	case ACK: return "Ack";
	case USERNAME: return "Username";
	case HANDSHAKE: return "Handshake";
	case NAK: return "NAK";
	}
	stringstream str;
	str << "unknown: " << t;
	return str.str();
}


string ToStr(MessageHeader * hdr)
{
	stringstream str;
	str << " " << MsgTypeToStr(hdr->msgType) << ": seq: " << hdr->sequenceNumber << ", # " << hdr->msgnumber<< ", ack: " << hdr->ACK << ", last: " << hdr->isLastChunck<< ", size: " << hdr->msgSize;
	return str.str();
}

// server constructor
Server::Server(string router, int windowSize, int dropRate)
{
	srand(time(NULL) + 25);
	freeLogFile("log.txt");
	print( "Starting on host " << getHostname());
	////////////SELF
	m_listeningPort = 5000;

	//////////////ROUTER
	m_routerListeningPort = 7000;
	m_routerHostName = router;
	m_requestNumber = 0;
	m_processInterMediateAcks = true;

	m_handshackeIsDone = false;
	bindToListeningPort();
	initializeRemoteSocAdd();
	m_sendBuffer = new char[maxDataSize + sizeof(MessageHeader)];

	//===============================================================
	//test
	//m_windowSize = 3;
	m_windowSize = windowSize;

	m_maxSiquenceNumber = computeSequenceNumberRange(windowSize);
	fillSequenceMap(m_maxSiquenceNumber);

	rtt = 30;


	RETRY = dropRate * 5;
	if(RETRY < 100)
		RETRY = 100;
}



// destructor - free-up resources
Server::~Server(void)
{
	delete [] m_sendBuffer;
}


// delete the log file if exists
void Server::freeLogFile(string log){
	ofstream logFile;
	if(fileExists(log)){
		remove(log.c_str());
	}
}

bool 	Server::getFile(string fileName)
{
	resetStats();
	print("File download started: " << fileName << endl);

	m_acknowledgementState = false;
	std::string filePath = FILEPATH + fileName; // compute the local file path to write
	bool isLastChunck = false;
	string str;
	// receive request result message
	MessageHeader * header = NULL;
	std::ofstream file;
	// open the file in binary mode to write the incomming data
	file.open(filePath, std::ios::out | std::ios::binary);
	// check if the file is successfully opened
	if(!file.is_open())
	{
		// terminate the command in case of error
			cout<<"file can not be written: "<<filePath;
	}

	///// stats
	// enumerate the chunks
	int currentChunk = 0;
	int dataBytesReceived = 0;
	///// stats
	// receive the file data from server in a loop and write it into the file
	try{
		do{ 
			header = waitForData();
			if(header == NULL)
				return false;
			if( header->msgType == MessageType::DATA)
			{
				file.write(header->data, header->msgSize);

				currentChunk++;	
				dataBytesReceived += header->msgSize;

				print("Received data packet " << header->sequenceNumber);
				sendACK("Chank is received"); //send acknowledjment first
				updateSequenceNumber(m_remoteSequence);
				print("Send acknowledgment for packet " << header->sequenceNumber); 

				if(header->isLastChunck)
				{
					print("Last packet is received, enter last loop");
					int repeatForLastPacket = 0;
					while(repeatForLastPacket <= LAST_PACKET_RETRY)
					{
						if(runSelectMethod() != NULL)
							resendLastPacket();
						repeatForLastPacket++;
					}
					print("Finish!!!!!!!!!!!!!!!!!");
					break;
				} 

			}

		}
		while(header!= NULL && !header->isLastChunck); // finish the loop if the last chunk is received
	}
	catch(const char* ee)
	{
		if(file.is_open()){
			file.close();
		}
		remove(filePath.c_str());
		print("Exception: " << ee << endl);
		return false;
	}
	print("Download of file "<<fileName<<" is completed"<<endl);

	print("Number of DATA packets received: " << currentChunk);
	print("Number of DATA bytes received: " << dataBytesReceived);
	print("Number of packets sent: " << sentPackets);
	print("Number of bytes sent: " << sentBytes);
	print("Total number of packets received: " << recvPackets);
	print("Total number of bytes received: " << recvBytes);
	// close the file
	file.close();
	return true;	
	resetStats();
}

bool Server::bindToListeningPort(){
	bool returnValue = true;

	int error = WSAStartup (0x0202, &m_wdata);   // Starts socket library
	if (error != 0)
	{
		std::cout << "wsastartup failed with error: " << error << std::endl;
		return false; //For some reason we couldn't start Winsock
	}


	initializeLocalSocAdd();
	m_listeningSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Creates a socket

	if (m_listeningSocket == INVALID_SOCKET)
	{
		print("Failed to create a socket, error: " << WSAGetLastError() << endl);
		return false; //Don't continue if socket is not created
	}

	// bides socket to a given port/address
	if (bind(m_listeningSocket, (LPSOCKADDR)&m_localSocAddr, sizeof(m_localSocAddr)) == SOCKET_ERROR)
	{
		//error accures if we try to bind to the same socket more than once
		print("Error binding socket: " << WSAGetLastError() << endl);
		return false;
	}

	return returnValue;
}


// low level send function 
int msgnum = 0;
bool Server::sendUDPPacket(MessageHeader* message)
{
	msgnum++;	
	message->msgnumber = msgnum;

	//print("SEND: " << ToStr(message));

	int messageSize = message->msgSize + sizeof(MessageHeader);
	int result = sendto(m_listeningSocket, (char*)message, messageSize, 0, (struct sockaddr *)&m_remoteSocAddr, sizeof(m_remoteSocAddr));

	// check if error occurred
	if(result == SOCKET_ERROR)
	{
		stringstream serr;
		serr << "Error sending "<< WSAGetLastError()<<endl;
		throw serr.str();
	}

	sentPackets++;
	sentBytes += MsgSize(message);

	return true;
}


// initialize socket data and get prepared for udp connection
bool Server::initializeRemoteSocAdd(){
	bool returnValue = true;

	m_remoteSocAddr.sin_family = AF_INET;
	m_remoteSocAddr.sin_port  = htons(m_routerListeningPort);

	hostent * pHostEntry = gethostbyname(m_routerHostName.c_str()); // returns server IP based on server host name 
	if(pHostEntry == NULL)//if thre is no such hoste 
	{
		std::cout << "Error host not found : "<< WSAGetLastError() << std::endl;
		return false; 
	}
	m_remoteSocAddr.sin_addr = *(struct in_addr *) pHostEntry->h_addr;


	return returnValue;


}

// initialize socket data and get prepared for udp connection
bool Server::initializeLocalSocAdd()
{

	m_localSocAddr.sin_family = AF_INET;      // Address family
	m_localSocAddr.sin_port = htons (m_listeningPort);   // Assign port to this socket
	m_localSocAddr.sin_addr.s_addr = htonl (INADDR_ANY);  

	return true;
}

// low level udp receive function
MessageHeader* Server::receiveUDPPacket(){
	MessageHeader* result = NULL;
	int size = sizeof(m_remoteSocAddr);



	int returnValue = recvfrom(m_listeningSocket, m_recBuff, MAXPACKETSIZE, 0, (SOCKADDR *) & m_remoteSocAddr, &size);
	if(returnValue == SOCKET_ERROR){
		stringstream serr;
		serr << "Receive error "<< WSAGetLastError()<<endl;
		throw serr.str();
	}
	else if (returnValue != 0) // if received data
	{
		result = (MessageHeader *)m_recBuff;
		//print("RCV: " << ToStr(result));
		recvPackets++;
		recvBytes += MsgSize(result);
	}

	return result;
}


// wait for incomming data during a timeout
MessageHeader* Server::runSelectMethod(){
	MessageHeader* result = NULL;
	fd_set readfds;
	struct timeval *tp=new timeval();

	int RetVal, fromlen, wait_count;
	DWORD count1, count2;

	count1=0; 
	count2=0;
	wait_count=0;
	tp->tv_sec=0;
	tp->tv_usec= TIMEOUT*1000;
	try{
		FD_ZERO(&readfds);
		FD_SET(m_listeningSocket,&readfds);

		fromlen=sizeof(m_remoteSocAddr);
		//check for incoming packets.
		if((RetVal=select(1,&readfds,NULL,NULL,tp))==SOCKET_ERROR){	
			throw "Timer Out from client side";
		}
		else if(RetVal>0){ // if there is actual data, then retrieve it
			if(FD_ISSET(m_listeningSocket, &readfds))	//incoming packet from peer host 1
			{
				result = receiveUDPPacket();
				if(result == NULL)		
					throw " Get buffer error!";

			}
		}

	}catch(string ss){
		cout << "Error occurred: " << ss << endl;
	}

	return result;

}


// handshaking function
// 2 packets of 3 way handshaking is sent/received 
void Server::handShaking()
{
	m_localGeneratedNumber = generateRandomNum();
	m_localSequence = getLastSignificantBit(m_localGeneratedNumber);
	cout<<endl;
	cout<<"server generated : "<<m_localGeneratedNumber<<endl;
	cout<<"server sequence number is : "<<m_localSequence<<endl;

	MessageHeader* message = (MessageHeader*)m_sendBuffer;
	MessageHeader* messageReceived = (MessageHeader*)m_recBuff;


	message->msgType = MessageType::HANDSHAKE;
	message->sequenceNumber =m_localGeneratedNumber;  //sends its generated sequence number
	message->isLastChunck = 1;
	message->ACK = messageReceived->sequenceNumber;// piggybackes acknowledgment for last received message from client
	message->msgSize = 0;

	sendUDPPacket(message);
	GetSystemTime(&handshakeSendTime);
	m_handshackeIsDone = true;
}


// extract the sequence number out of randomly generated number
int Server::getLastSignificantBit(int number)
{
	return number % m_windowSize;
}


// generate a random number
int Server::generateRandomNum(){
	return rand() % 256;
}


// message loop function for incomming network messages
void Server::processIncommingMessages(){

	do{
		try
		{ 
			//wait for incomming message
			MessageHeader* header = receiveUDPPacket();
			if(header != NULL)
			{
				switch(header->msgType)
				{
				case MessageType::GET:
					{
						//if incomming message contains piggybacked acknowledgment for the last 3 wey hanshack message , then accept it, otherwise ignor
						if(m_handshackeIsDone && header->ACK == m_localGeneratedNumber && header->sequenceNumber == m_remoteSequence )
						{
							CalculateRTT(handshakeSendTime);
							string fileName = header->data;
							// check if the file exists and only then send the file to client
							if(fileExists(FILEPATH + fileName)){
								sendRequestedFile(fileName);
							}
							else{
								//if file does not exist send an error message
								string wrongFile = header->data;
								string messageBody = wrongFile + " : File does not exist";
								sendErrorMessage(messageBody);
							}
							updateSequenceNumber(m_remoteSequence); //update client sequence number
							//update request number when task is done
							m_handshackeIsDone = false;
						}
						break;
					}
				

				case MessageType::PUT:
					{
						//test
						//m_windowSize=1;
						if(m_handshackeIsDone && header->ACK == m_localGeneratedNumber && header->sequenceNumber == m_remoteSequence )
						{
							// send an ACK message 
							
							sendACK("Server is ready to transfer file");
							receiveFile(header->data);
							//getFile(header->data);
							m_handshackeIsDone = false;
						}
						break;
					}

				case MessageType::HANDSHAKE:
					{
						resetState();
						m_remotGeneratedNumber = header->sequenceNumber;// number generated by client is assigned to a sequence number
						m_remoteSequence = getLastSignificantBit(m_remotGeneratedNumber);
						cout<<"client generated : "<<m_remotGeneratedNumber<<endl;
						cout<<"client sequence number "<<m_remoteSequence<<endl;
						cout<<header->sequenceNumber<<endl;
						cout<<endl;
						cout<<"handshake"<<endl;
						handShaking();
						break;
					}
				case MessageType::LIST:
					{
						if(m_handshackeIsDone && header->ACK == m_localGeneratedNumber && header->sequenceNumber == m_remoteSequence )
						{
							CalculateRTT(handshakeSendTime);
							listFilesInDirectory();	//lists files available on the server
							m_handshackeIsDone = false;
						}
						break; 

					}
				default:
					{
						if(m_lastSendPacket != NULL)
							resendLastPacket();
						else
							sendErrorMessage("communication error occurred");
						break;
					}
				} //switch
			}//if

		} //try
		catch(const char * err)
		{
			cout << err << endl;
			string ss;
			m_handshackeIsDone = false;
		}
		//cout<<"client sequence " <<m_remoteSequence<<endl;
		//cout<<"server sequence "<<m_localSequence<<endl;

	}// do while
	while(true);

}

// download the file from client
void Server:: receiveFile(std::string fileName)
{
	resetStats(); // reset statistics

	int dataBytesReceived = 0;

	// calculate the file path, where to store the icomming data
	std::string filePath = FILEPATH + fileName;
	bool isLastChunck = false;//last chank or not
	size_t SizeOfFileChunck;
	std::ofstream file;
	string str;
	int currentCunck = 0;
	//open a file for binary write
	file.open(filePath, std::ios::out | std::ios::binary);
	try{
		do{
			//cout << "enter waitForData" <<endl;
			MessageHeader* header = waitForData(); //accept a chanck of the file and reads the header
			//cout << "exit waitForData" <<endl;
			SizeOfFileChunck = header->msgSize;//shows how many bites should be written
			isLastChunck = header->isLastChunck;//is it the last chanck or not
			//if file is oppen without errors
			if(file.is_open()){
				//write received chanck
				file.write(header->data, SizeOfFileChunck);
				str = "Chank" + intToStr(currentCunck) + "is transmited";
				// send acknowledgement
				print("Received data packet " << header->sequenceNumber);
				string str = "received ack " + m_remoteSequence;
				sendACK(str );
				print("Send acknowledgment for packet " << header->sequenceNumber); 

				// update statistics
				currentCunck++;
				dataBytesReceived += header->msgSize;
			}

			if(header->isLastChunck)
			{
				// when the last chunk is received
				// wait in a loop and resend the last acknowledgement
				// this could happen if the client did not received the last acknowledgement
				int repeatForLastPacket = 0;
				while(repeatForLastPacket <= LAST_PACKET_RETRY)
				{
					if(runSelectMethod() != NULL)
						resendLastPacket();
					repeatForLastPacket++;
				}
				isLastChunck = true;
			}

		}while(!isLastChunck); //while it is not the last chunck
	}catch(const char* ee){
		if(file.is_open()){
			file.close();
			remove(filePath.c_str());
			throw ee;
		}
	}
	// close the file
	file.close();

	print("Download of file "<<fileName<<" is completed"<<endl);

	// print statistics
	print("Number of DATA packets received: " << currentCunck);
	print("Number of DATA bytes received: " << dataBytesReceived);
	print("Number of packets sent: " << sentPackets);
	print("Number of bytes sent: " << sentBytes);
	print("Total number of packets received: " << recvPackets);
	print("Total number of bytes received: " << recvBytes);
}


// check if the current file exists
bool Server::fileExists(string fileName)
{
	string filePath = fileName;
	bool returnValue = true;
	//This will get the file attributes bitlist of the file
	DWORD fileAtt = GetFileAttributesA(filePath.c_str());

	//If an error occurred it will equal to INVALID_FILE_ATTRIBUTES
	if(fileAtt == INVALID_FILE_ATTRIBUTES)
		returnValue = false;
	return returnValue;

}


// upload the requested file to client side
void Server::sendRequestedFile(std::string fileName)
{
	resetStats();
	std::string filePath = FILEPATH	+ fileName;
	int nuberOfBits = maxDataSize;// maxmum size for a chank

	int dataBytesSent = 0;
	size_t size = 0;

	ifstream file (filePath, ios::in|ios::binary|ios::ate); // opens a file for binarry reading
	if (file.is_open())
	{
		cout<<endl;

		//if file is oppen
		file.seekg(0,ios::end);	//moves coursor at the end of a file
		size = file.tellg();//gets the size of the file
		file.seekg(0,ios::beg);//moves cursor t the beginning of the file, to start reading it
		size_t currentSize = size;//
		int current = 0;//the number of current sended packet
		int isLast = 0; //this variable shows whether the sending packet is the last one or not, if it is equal 0 => is not the last

		MessageHeader * header = (MessageHeader*)m_sendBuffer; //maps message header into a buffer
		int numberOfSentPackets  = 0;

		while(currentSize != 0 || !m_window.empty()) 
		{
			numberOfSentPackets = m_window.size();
			while(currentSize != 0 && numberOfSentPackets < m_windowSize){

				//while there is something to read in the file
				if(currentSize > nuberOfBits)
				{
					file.read(header->data, nuberOfBits ); //read file by the size of nuberOfBits
					currentSize = currentSize - nuberOfBits;//reduce by the bits already read
				}
				else
				{
					//if what is left is smaller than we wxpect
					//indicate that that is the last chank
					nuberOfBits = currentSize;
					file.read(header->data, nuberOfBits );
					currentSize = 0; 
					isLast = 1;//indicates last chanck
				}

				int sendSize = nuberOfBits + sizeof(MessageHeader); //parameter that shows the size of the message we have to send

				//builds message header	
				header->msgType = MessageType::DATA;
				header->msgSize = nuberOfBits;
				header->isLastChunck = isLast;
				header->sequenceNumber = m_localSequence;
				header->ACK = 3;
				if(current == 0) 
					header->ACK  = m_remoteSequence; //piggybacks sequence just received request message

				//keep last sent packet
				updateLastSendPachet(header);
				updateSequenceNumber(m_localSequence);

				numberOfSentPackets++;

				//update statistics
				current++;
				dataBytesSent += header->msgSize;

				print("Sent data packet " << header->sequenceNumber);
				//::Sleep(30);
				if(!sendUDPPacket(header)){
					file.close();
					cout<<endl;
					//retuns an error when connection is died
					throw "Socket error on the client side";
				}
				if(m_processInterMediateAcks && (current % 3) == 0)
					waitForIntermediateAcknowledgement();
			}//while second


			bool confirmation =  waitForAcknowledgement();
			//updateSequenceNumber(m_localSequence);
			if(!confirmation)
			{
				print("File processing is failed"<<endl);
				break;
			}



		} //While first
		file.close();

		print("Upload of file " << fileName << " is completed "<<endl);

		// print statistics
		print("Number of DATA packets sent: " << current);
		print("Number of DATA bytes sent: " << dataBytesSent);
		print("Number of packets sent: " << sentPackets);
		print("Number of bytes sent: " << sentBytes);
		print("Total number of packets received: " << recvPackets);
		print("Total number of bytes received: " << recvBytes);


		/*ofstream logFile;
		logFile.open("result.txt", ios::out | ios::app);
		if(logFile.is_open())
		{
		logFile<<sentPackets<<endl;
		logFile.close();
		}*/

	}
	else{
		sendErrorMessage(fileName + " : File can not oppen");
	}
	resetStats();
}

// updates the sequence number
void Server::updateSequenceNumber(int& sequence){
	if(sequence == m_maxSiquenceNumber){
		sequence = 0;
	}
	else{
		sequence++;
	}

}


// send the acknowledgment
void Server::sendACK(string messageBody){
	std::string message  = messageBody;

	int sendSize = message.length() + sizeof(MessageHeader) + 1; //+1 for "/0" to indicate end of string
	char* buffer = new char[sendSize];

	MessageHeader * header = (MessageHeader*)buffer;

	header->msgType = MessageType::ACK;
	header->msgSize = message.length() + 1;
	header->isLastChunck = true;// this is the last reply
	header->sequenceNumber = m_remoteSequence;
	header->ACK = 3;//nothing is piggybacked
	memcpy(header->data, message.c_str() , message.length() + 1);
	m_lastSendPacket = header;//update last sent packet
	updateSequenceNumber(m_remoteSequence);//update sender/client sequence number
	if(!sendUDPPacket(header)){
		cout<<endl;
		//retuns an error when connection is died
		throw "Socket error on the client side";
	}
}


// resend last sent packet
void Server::resendFirstPacket(){
	if(!m_window.empty())
	{
		m_sequenceMap[m_window.front()->sequenceNumber].isResent = true;
		sendUDPPacket(m_window.front());
	}
}

void Server::resendLastPacket(){
	if(m_lastSendPacket != NULL)
		sendUDPPacket(m_lastSendPacket);
}



// waits for acknowledgement packet
bool Server::waitForAcknowledgement()
{
	MessageHeader* ACKmessage = NULL;
	int count = 0;
	int lastNak = -1;

	int rcvcount = 0;
	int rcvackcount = 0;
	int rcvnakcount = 0;
	while(count <= RETRY)
	{
		lastNak = -1;
		count++;
		rcvcount = 0;

		while(!m_window.empty()  &&
			(ACKmessage = runSelectMethod()) != NULL)
		{
			rcvcount++;
			if(ACKmessage->msgType == MessageType::ACK)
			{
				print("Received ACK for packet " << ACKmessage->sequenceNumber);
				rcvackcount++;
				if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
					slideWindow(ACKmessage->sequenceNumber, true);
			}
			else if(ACKmessage->msgType == MessageType::NAK)
			{
				print("Received NAK for packet " << ACKmessage->sequenceNumber);
				lastNak = ACKmessage->sequenceNumber;
				rcvnakcount++;
			}
			else if(ACKmessage->msgType == MessageType::HANDSHAKE)
				throw "fail send failed: HANDSHAKE requested";

			if(rcvcount >= m_window.size())
				break;
		}

		if(lastNak != -1)
		{
			if(m_sequenceMap[previousSequenceNumber(lastNak)].isSet)
				slideWindow(lastNak, false);

			if(m_sequenceMap[lastNak].isSet)
				goBackN(lastNak);
		}

		if(m_window.empty() || (lastNak != -1 && lastNak != m_window.front()->sequenceNumber))
		{
			break;
		}

		if(rcvcount == 0)
		{
			list<MessageHeader*>::iterator lit = m_window.begin();
			for(; lit != m_window.end(); lit++)
			{
				m_sequenceMap[(*lit)->sequenceNumber].isResent = true;
				print("Resend data packet " << (*lit)->sequenceNumber);
				sendUDPPacket(*lit);
			}
		}
	}

	return ((rcvnakcount + rcvackcount) > 0) || m_window.empty();
}

//========================================================================
// wait for incomming data packet


MessageHeader* Server::waitForData(){
	MessageHeader* ACKmessage = NULL;
	bool restartHandshack = false;

	int count = 0;
	while(count < RETRY)
	{
		//print("Waiting for data sequence : " << m_lastSendPacket->sequenceNumber);
		bool resend = false;
		ACKmessage = runSelectMethod();
		if(ACKmessage != NULL)
		{
			if(ACKmessage->msgType == MessageType::DATA &&  ACKmessage->sequenceNumber == m_remoteSequence)
			{
				// data is received can return from function
				break;
			}
			else if(ACKmessage->msgType == MessageType::DATA &&  ACKmessage->sequenceNumber != m_remoteSequence)
			{
				// data is received can return from function
				sendNAK("");
			}
			else if(ACKmessage->msgType == HANDSHAKE)
			{
				throw "Connection reset";
			}
		}
		else
		{
			sendNAK("");
		}
		count++;
	}

	if(count >= RETRY)
		throw "Connection Error :";
	return ACKmessage;

}


//========================================================================





//=====================================================================

void Server::updateLastSendPachet(MessageHeader* lastSentPacket)
{
	if(m_sequenceMap.find(lastSentPacket->sequenceNumber) == m_sequenceMap.end())
		throw "Error: sequence not found in map";

	m_sequenceMap[lastSentPacket->sequenceNumber].isSet = true;
	GetSystemTime(&m_sequenceMap[lastSentPacket->sequenceNumber].timeSent);
	m_sequenceMap[lastSentPacket->sequenceNumber].isResent = false; 
	m_window.push_back(lastSentPacket->clone());


	//-------------------update the packet--------------------
	if(m_lastSendPacket != NULL)
		delete m_lastSendPacket;

	m_lastSendPacket = lastSentPacket->clone();
	//-------------------update
}

void Server::writeIntoLogFile(string log){
	ofstream logFile;
	logFile.open("log.txt", ios::out | ios::app);
	if(logFile.is_open())
	{
		logFile<<log<<endl;
		logFile.close();
	}

}

void Server::outputMessage(string message){

	stringstream sss;
	
	sss<< message;
	writeIntoLogFile(sss.str());
	cout<<sss.str()<<endl;}

string Server::intToStr(int intValue){

	stringstream strValue;
	strValue<<intValue;
	return strValue.str();
}

// wait for handshake
MessageHeader*  Server::waitForHandShack(){
	string str;
	MessageHeader* ACKmessage = NULL;
	int count = 0;
	while(count <= RETRY)
	{
		print("Waits for handshake "<<m_lastSendPacket->sequenceNumber);
		ACKmessage = runSelectMethod();
		if(ACKmessage != NULL && ACKmessage->msgType == MessageType::HANDSHAKE &&  ACKmessage->sequenceNumber == m_localGeneratedNumber){
			updateSequenceNumber(m_localSequence);
			updateSequenceNumber(m_remoteSequence);
			break;
		}
		else
		{
			resendLastPacket();
			count++;
		}
		if(count == RETRY)
			throw "Connection Error : Client does not receive ack";
	}

	return ACKmessage;
}


// notify about error to the client
void Server::sendErrorMessage(string messageBody){

	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;
	MessageHeader* messageReceived = (MessageHeader*)m_recBuff;
	message->msgType = MessageType::ERR;
	message->sequenceNumber = messageReceived->sequenceNumber; //acknowledges sequenvce number it received from a client
	strcpy(message->data, messageBody.c_str());//attaches its own sequence number 
	message->msgSize = messageBody.length() + 1;
	message->ACK = 3;
	sendUDPPacket(message);
	updateLastSendPachet(message); 

}


// reset local state
void Server::resetState()
{
	while(!m_window.empty())
		m_window.pop_front();

	map<int, PacketData>::iterator mit = m_sequenceMap.begin(); 
	for( ; mit != m_sequenceMap.end(); mit++)
		mit->second.isSet = false;
}





//this method is done based on the internet 
//lists all the files that are in server's file directory
void Server::listFilesInDirectory(){

	HANDLE hFile; // Handle to file
	string filePath = FILEPATH;
	WIN32_FIND_DATA FileInformation; // File information
	filePath = filePath + "*.*";
	//find the first file in the given path
	hFile = FindFirstFile(filePath.c_str(), &FileInformation);

	stringstream str;

	if(hFile != INVALID_HANDLE_VALUE)
	{
		do
		{//if file exists add into a file list
			if((FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				str << "$" << FileInformation.cFileName << "\t" << ((FileInformation.nFileSizeHigh * (MAXDWORD+1)) + FileInformation.nFileSizeLow) / 1024 << " kb (" <<
					((FileInformation.nFileSizeHigh * (MAXDWORD+1)) + FileInformation.nFileSizeLow) << " bytes)";
				//cout<< FileInformation.cFileName<< std::endl;
			}
			//while there is a file find the next file
		}while(::FindNextFile(hFile, &FileInformation) == TRUE);

		string files = str.str();
		if(files.length() == 0)
			sendErrorMessage("No files availabled for download");
		else
			sendFileList(files);
	}
}


// send files list to client, available for download
void Server::sendFileList(string list){
	size_t currentSize = list.length();//
	int current = 0;//the number of current sended packet
	int isLast = 0; //this variable shows whether the sending packet is the last one or not, if it is equal 0 => is not the last
	MessageHeader * header = (MessageHeader*)m_sendBuffer; //maps message header into a buffer
	bool isFirstChanck = true;
	int nuberOfBits = maxDataSize - 1;
	int start = 0;
	int numberOfSentPackets  = 0;
	while(currentSize != 0 || !m_window.empty()) 
	{
		numberOfSentPackets = m_window.size();//(m_window.size() + 1); / 2;

		while(currentSize != 0 && numberOfSentPackets < m_windowSize){

			//while there is something to read in the file
			if(currentSize > nuberOfBits)
			{
				strcpy(header->data, list.substr(0, nuberOfBits).c_str());
				list = list.substr(nuberOfBits);
				//file.read(header->data, nuberOfBits ); //read file by the size of nuberOfBits
				currentSize = currentSize - nuberOfBits;//reduce by the bits already read
			}
			else
			{
				//if what is left is smaller than we wxpect
				//indicate that that is the last chank
				nuberOfBits = currentSize;
				strcpy(header->data, list.c_str());

				//file.read(header->data, nuberOfBits );
				currentSize = 0; 
				isLast = 1;//indicates last chanck
			}

			int sendSize = nuberOfBits + sizeof(MessageHeader) +1; //parameter that shows the size of the message we have to send

			//builds message header	
			header->msgType = MessageType::DATA;
			header->msgSize = nuberOfBits +1;
			header->isLastChunck = isLast;
			header->sequenceNumber = m_localSequence;
			if(isFirstChanck){
				header->ACK  = m_remoteSequence;
				updateSequenceNumber(m_remoteSequence);
				isFirstChanck = false;
			}
			else{
				updateSequenceNumber(m_remoteSequence);
				header->ACK = 3;
			}


			//piggybacks sequence just received request message

			//keep last sent packet
			updateLastSendPachet(header);
			updateSequenceNumber(m_localSequence);
			sendUDPPacket(header);
			numberOfSentPackets++;

		}
		bool confirmation = waitForAcknowledgement();

		if(confirmation || m_window.empty()){
			cout<<"chank : " <<	current<<" is send" <<endl;
		}
		else{
			cout<< "file processing is failed"<<endl;
			updateLastSendPachet(NULL);
			break;
		}

		current++;
	}

	print("File send is finished ");

}



int Server::computeSequenceNumberRange(int windowSize){
	/*int mGBN = log(windowSize + 1) / log(2) + 1;
	return pow(2,mGBN) - 1;*/

	double logvalue = log(windowSize + 1) / log(2);
	int mGBN = logvalue;
	if((double)mGBN != logvalue)
		mGBN++;

	//cout<< logvalue<< "|" <<mGBN << "\t--- ";
	return pow(2,mGBN) - 1;
}


void Server::CalculateRTT(const SYSTEMTIME& sentTime)
{
	FILETIME ftSent;
	SystemTimeToFileTime(&sentTime, &ftSent);
	//SystemTimeToFileTime(&m_sequenceMap[m_window.front()->sequenceNumber].timeSent, &ftSent);

	SYSTEMTIME tm;
	GetSystemTime(&tm);
	FILETIME ftNow;
	SystemTimeToFileTime(&tm, &ftNow);

	int sample = (*((ULONGLONG*)&ftNow) - *((ULONGLONG*)&ftSent))/10000;
	//(*((ULONGLONG*)&ftNow) - *((ULONGLONG*)&ftSent))/10000;

	rtt = (1 - 0.125) * rtt + 0.125 * sample;
	if(rtt > MAXIMUM_RTT)
		rtt = MAXIMUM_RTT;

	//print("RTT: " << rtt);
}


void Server::slideWindow(int receivedACKNumber, bool isACK){
	while(!m_window.empty() && m_window.front()->sequenceNumber != receivedACKNumber){
		m_sequenceMap[m_window.front()->sequenceNumber].isSet = false;
		delete m_window.front();
		m_window.pop_front();
	}

	if(isACK && !m_window.empty() && m_window.front()->sequenceNumber == receivedACKNumber){
		// calculate rtt
		if(!m_sequenceMap[m_window.front()->sequenceNumber].isResent)
		{
			CalculateRTT(m_sequenceMap[m_window.front()->sequenceNumber].timeSent);
		}

		m_sequenceMap[m_window.front()->sequenceNumber].isSet = false;
		delete m_window.front();
		m_window.pop_front();
	}
}

void Server::fillSequenceMap(int windowSize){
	for(int i = 0; i<= windowSize; i++){
		m_sequenceMap[i].isSet = false;
	}

}



void  Server::waitForIntermediateAcknowledgement()
{
	MessageHeader* ACKmessage = NULL;
	int count = 0;

	ACKmessage = runIntermediateSelectMethod(0);
	if(ACKmessage != NULL)
	{
		if( ACKmessage->msgType == MessageType::ACK)  
		{
			print("Received ACK for packet " << ACKmessage->sequenceNumber);
			if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
				slideWindow(ACKmessage->sequenceNumber, true);

		} 
		else if(ACKmessage->msgType == MessageType::NAK)
		{
			print("Received NAK for packet " << ACKmessage->sequenceNumber);
			if(m_sequenceMap[previousSequenceNumber(ACKmessage->sequenceNumber)].isSet)
				slideWindow(ACKmessage->sequenceNumber, false);

			if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
				goBackN(ACKmessage->sequenceNumber);
		}
	}
}
void Server::goBackN(int receivedNACNumber){

	if(!m_sequenceMap[receivedNACNumber].isSet){
		//	previousSequenceNumber(receivedNACNumber)
		slideWindow(receivedNACNumber,false);
	}
	else{
		while(!m_window.empty() && m_window.front()->sequenceNumber != receivedNACNumber){
			m_sequenceMap[m_window.front()->sequenceNumber].isSet = false;
			delete m_window.front();
			m_window.pop_front();
		}


		if(!m_window.empty() && m_window.front()->sequenceNumber == receivedNACNumber){
			MessageHeader* keepfront;
			list<MessageHeader*>::iterator lit = m_window.begin();
			for(; lit != m_window.end(); lit++)
			{
				sendUDPPacket(*lit);
				print("Resend data packet " << (*lit)->sequenceNumber);
				m_sequenceMap[(*lit)->sequenceNumber].isResent = true;
			}
		}
	}
}

MessageHeader* Server::runIntermediateSelectMethod(int timeOut){
	MessageHeader* result = NULL;
	fd_set readfds;
	struct timeval *tp=new timeval();

	int RetVal, fromlen, wait_count;
	DWORD count1, count2;

	count1=0; 
	count2=0;
	wait_count=0;
	tp->tv_sec=0;//TIMEOUT;
	tp->tv_usec= timeOut;//rtt*1000;//TIMEOUT*1000;
	try{
		FD_ZERO(&readfds);
		FD_SET(m_listeningSocket,&readfds);

		fromlen=sizeof(m_remoteSocAddr);
		//check for incoming packets.
		if((RetVal=select(1,&readfds,NULL,NULL,tp))==SOCKET_ERROR){	
			throw "Timer Out from client side";
		}
		else if(RetVal>0){ // if there is actual data, then retrieve it
			if(FD_ISSET(m_listeningSocket, &readfds))	//incoming packet from peer host 1
			{
				result = receiveUDPPacket();
				if(result == NULL)		
					throw " Get buffer error!";

			}
		}

	}catch(string ss){
		cout << "Error occurred: " << ss << endl;
	}

	return result;

}

void Server::sendNAK(string messageBody){
	print("Send NAK for packet " << m_remoteSequence);
	std::string message  = messageBody;
	int sendSize = message.length() + sizeof(MessageHeader) + 1; //+1 for "/0" to indicate end of string
	char* buffer = new char[sendSize];

	MessageHeader * header = (MessageHeader*)buffer;

	header->msgType = MessageType::NAK;
	header->msgSize = message.length() + 1;
	header->isLastChunck = true;// this is the last reply
	header->sequenceNumber = m_remoteSequence;
	header->ACK = 3; // nothing is piggybacked
	memcpy(header->data, message.c_str() , message.length() + 1);

	//updateLastSendPacket(header);
	//updateSequenceNumber(m_remoteSequenceNumber);
	if(!sendUDPPacket(header)){
		//retuns an error when connection is died
		throw "Socket error on the client side";
	}

}

