//******************************************************************************************
//Client logic here
//******************************************************************************************




#define _CRT_SECURE_NO_WARNINGS  // removing the precompiler secure warnings that come up when using functions like scanf
#include <SDKDDKVer.h>
#include <map>
#include <queue>
#include <math.h>
#include <Windows.h>
#include <lmcons.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <iostream>
using namespace::std;
#include <fstream>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>
#include <list>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib") //liks to winsosk library
#include "Objects.h"
#include "Client.h"
#include <stdio.h>
#include <time.h>






// get current host name
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




// constructor
Client::Client(string router, int windowSize,  int dropRate)
{
	srand(time(NULL) + 15);
	freeLogFile("log.txt");
	print( "Starting on host " << getHostname());
	const char * a = getHostname().c_str();

	////////////SELF
	m_listeningPort = 5001;

	//////////////ROUTER
	m_routerListeningPort = 7001;

	m_routerHostName = router;
	m_processIntermediateAcks = true;
	m_sendBuffer = new char[maxDataSize + sizeof(MessageHeader)];
	m_acknowledgementState = false;
	bindToListeningPort();
	initializeLocalSocAdd();
	initializeRemoteSocAdd();

	//=========================
	//test
	m_windowSize = 1;
	//m_windowSize = windowSize;

	m_maxSiquenceNumber = computeSequenceNumberRange(windowSize);
	fillSequenceMap(m_maxSiquenceNumber);

	RETRY = dropRate * 5;
	if(RETRY < 100)
		RETRY = 100;

	rtt = 30;
}





Client::~Client(void)
{
}


// prepare socket data
bool Client::bindToListeningPort(){
	bool returnValue = true;

	int error = WSAStartup (0x0202, &m_wdata);   // Starts socket library
	if (error != 0)
	{
		print( "WSAStartup failed with error: " << error);
		return false; //For some reason we couldn't start Winsock
	}


	initializeLocalSocAdd();


	m_listeningSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Creates a socket

	if (m_listeningSocket == INVALID_SOCKET)
	{
		print( "Failed to listen on port: " << WSAGetLastError());
		return false; //Don't continue if socket is not created
	}

	// bides socket to a given port/address
	if (bind(m_listeningSocket, (LPSOCKADDR)&m_localSocAddr, sizeof(m_localSocAddr)) == SOCKET_ERROR)
	{
		//error accures if we try to bind to the same socket more than once
		print("Error binding socket: " << WSAGetLastError());
		return false;
	}


	return returnValue;
}


// low level send udp message function
int msgnum = 0;
MessageHeader* Client::sendUDPPacket(MessageHeader* message){
	string str;
	msgnum++;
	message->msgnumber = msgnum;

	//print("SEND: " << ToStr(message));

	int messageSize = message->msgSize + sizeof(MessageHeader);
	int result = sendto(m_listeningSocket, (char*)message, messageSize, 0, (struct sockaddr *)&m_remoteSocAddr, sizeof(m_remoteSocAddr));

	if(result == SOCKET_ERROR)
		print("Error sending: " << WSAGetLastError()); 

	// update statistics
	sentPackets++;
	sentBytes += MsgSize(message);

	return message;
}


// initialize remote socket data
bool Client::initializeRemoteSocAdd(){
	bool returnValue = true;

	m_remoteSocAddr.sin_family = AF_INET; 
	m_remoteSocAddr.sin_port  = htons(m_routerListeningPort);

	hostent * pHostEntry = gethostbyname(m_routerHostName.c_str()); // returns server IP based on server host name 
	if(pHostEntry == NULL)//if thre is no such hoste 
	{
		print("Error host not found : "<< WSAGetLastError());
		return false; 
	}
	m_remoteSocAddr.sin_addr = *(struct in_addr *) pHostEntry->h_addr;


	return returnValue;


}


// low level receive udp packet fuction
MessageHeader* Client::receiveUDPPacket(){

	MessageHeader* result = NULL;
	int size = sizeof(m_remoteSocAddr);

	int returnValue = recvfrom(m_listeningSocket, m_recBuff, MAXPACKETSIZE, 0, (SOCKADDR *) & m_remoteSocAddr, &size);
	if(returnValue == SOCKET_ERROR){
		print("Receive error: " << WSAGetLastError());		
	}
	else if(returnValue > 0) // check if really received data
	{
		result = (MessageHeader*) m_recBuff;
		//#ifdef PRINT_SEND_RCV
		//print("RCV: " << ToStr(result));
		//#endif

		// update statistics
		recvPackets++;
		recvBytes += MsgSize(result);
	}

	return result;
}


// initialize socket data
bool Client::initializeLocalSocAdd(){

	m_localSocAddr.sin_family = AF_INET;      // Address family
	m_localSocAddr.sin_port = htons (m_listeningPort);   // Assign port to this socket
	m_localSocAddr.sin_addr.s_addr = htonl (INADDR_ANY);


	return true;
}


// perform receive on timeout
MessageHeader* Client::runSelectMethod(){
	MessageHeader* message = NULL;
	fd_set readfds;
	struct timeval *tp=new timeval;

	int RetVal, fromlen, recvlen, wait_count;
	DWORD CurrentTime, count1, count2;

	count1=0; 

	wait_count=0;
	tp->tv_sec=0;
	tp->tv_usec = TIMEOUT*1000;
	try{
		FD_ZERO(&readfds);
		FD_SET(m_listeningSocket,&readfds);
		//	FD_SET(Sock2,&readfds);
		fromlen=sizeof(m_remoteSocAddr);
		//check for incoming packets.
		if((RetVal=select(1,&readfds,NULL,NULL,tp))==SOCKET_ERROR){	
			throw "Timer Out from client side";
		}
		else if(RetVal>0){
			if(FD_ISSET(m_listeningSocket, &readfds))	//incoming packet from peer host 1
			{
				message = receiveUDPPacket();
				//message = (MessageHeader*)m_recBuff;
				if(message == NULL)		
					throw " Get buffer error!";

			}
		}

	}catch(string ss){
		print("Error occurred: " << ss);
	}
	return message;

}




// do the handshaking 
// in a loop and retry if failed
// note there is no resend for handshake messages
void Client::handShaking()
{
	while(runIntermediateSelectMethod(0) != NULL)
	{
		cout << ".";
	}
	std::cout<<endl;

	bool doHandShaking = true;
	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;
	MessageHeader* serverReturnMessage = NULL;

	while(doHandShaking)
	{
		print("-------====start handshake====-------");
		m_localGeneratedNumber= generateRandomNum();
		m_localSequenceNumber = getLastSignificantBit(m_localGeneratedNumber);


		print("Client generated : "<<m_localGeneratedNumber);
		print("Client sequence number is : "<<m_localSequenceNumber);


		message->msgType = MessageType::HANDSHAKE;
		message->sequenceNumber = m_localGeneratedNumber;
		message->msgSize = 0;
		message->isLastChunck = 1;
		message->ACK = 3;
		sendUDPPacket(message);
		GetSystemTime(&handshakeSendTime);
		serverReturnMessage = runSelectMethod();

		if(serverReturnMessage != NULL && serverReturnMessage->msgType == MessageType::HANDSHAKE &&  serverReturnMessage->ACK == m_localGeneratedNumber)
		{
			CalculateRTT(handshakeSendTime);
			m_remotGeneratedNumber = serverReturnMessage->sequenceNumber;
			m_remoteSequenceNumber = getLastSignificantBit(m_remotGeneratedNumber);
			doHandShaking = false;
			// handshaking is done
		}
		else
		{
			rtt *= 2;
			if(rtt > MAXIMUM_RTT)
				rtt = MAXIMUM_RTT;
		}
	}
}


// generate random number
int Client::generateRandomNum(){
	return rand() % 256;

}


// extract sequence number
int Client::getLastSignificantBit(int number)
{
	return number % m_windowSize;
}

void Client::sendRequestedFile(std::string fileName)
{
	resetStats();
	std::string filePath = FILEPATH	+ fileName;
	int nuberOfBits = maxDataSize;// maxmum size for a chank

	int dataBytesSent = 0;
	size_t size = 0;

	ifstream file (filePath, ios::in|ios::binary|ios::ate); // opens a file for binarry reading
	if (file.is_open())
	{
		std::cout<<endl;

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
				header->sequenceNumber = m_localSequenceNumber;
				header->ACK = 3;
				if(current == 0) 
					header->ACK  = m_remoteSequenceNumber; //piggybacks sequence just received request message

				//keep last sent packet
				updateLastSendPachet(header);
				updateSequenceNumber(m_localSequenceNumber);

				numberOfSentPackets++;

				//update statistics
				current++;
				dataBytesSent += header->msgSize;

				print("Sent data packet " << header->sequenceNumber);
				//::Sleep(30);
				if(!sendUDPPacket(header)){
					file.close();
					std::cout<<endl;
					//retuns an error when connection is died
					throw "Socket error on the client side";
				}
				if(m_processIntermediateAcks && (current % 3) == 0)
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
		std::cout<<fileName<<" : File can not oppen"<<endl;
	}
	resetStats();
}


// spilt the given string into vector of strings delimiting by delimitter
void Client::split(string inputValue, vector<string>* result) {
	std::stringstream stream(inputValue);
	std::string item;
	//reads from a stream, writes into item, $ indicates end of line(separeter)
	while (std::getline(stream, item, '$')) {
		//ads item into a given vector
		result->push_back(item);
	}

}



// get the files in local directory 
void Client::listFilesInDirectory(){

	HANDLE hFile; // Handle to file
	std::string filePath = FILEPATH;
	WIN32_FIND_DATA FileInformation; // File information
	filePath = filePath + "*.*";
	// find all files
	hFile = FindFirstFile(filePath.c_str(), &FileInformation);

	int fileCount = 0;
	if(hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			// print files found
			if((FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				print(FileInformation.cFileName<< "\t" << ((FileInformation.nFileSizeHigh * (MAXDWORD+1)) + FileInformation.nFileSizeLow) / 1024 << " kb (" <<
					((FileInformation.nFileSizeHigh * (MAXDWORD+1)) + FileInformation.nFileSizeLow) << " bytes)");
				fileCount++;
			}
		}while(::FindNextFile(hFile, &FileInformation) == TRUE);
	}
	if(fileCount == 0)
		print("No files available"<<endl);
}


// process messages function
int Client::processClientMessages(){

	// print the list of available commands
	print("Available commands: ");
	print("\tget  : dowloade file");
	print("\tput  : uploads file");
	print("\trename  : rename a file");
	print("\tlist : returns available file list fron the server");
	print("\tquit : stop the process"<<endl<<endl);

	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;

	while(true){
		try{

			print("Waiting for a command..."<< std::endl);

			string command;
			command =  readCommand();
			if(command == "get") // received 'get' command
			{
				print("Process Get method."<< endl);

				print("Available files on the server ");
				// print the available server files names list
				/////////////////////////////////////////////////////////////////////////////////////////////////
				getFilesList();


				// read the file name to download
				print(endl << "Input file name");

				string fileName = readCommand();
				string checkFile = FILEPATH  + fileName  ;
				if(fileExists(checkFile ))
				{
					print("Would you like to  Rename or Cancel R/C" <<endl );
					string command = readCommand();
					
					
					 for (std::string::size_type i=0; i<command.length(); ++i)
				      command[i]=toupper(command[i]);
					if(command=="C")
					{
						break;
					}
					else if(command=="R")
					{
						print("Enter new name of file" <<endl );
						string newName = readCommand();
						//fileName = newName;
						bool retryConnectionRequest = true;
					   while(retryConnectionRequest)
					   {
							handShaking(); //proess handshack

							// set the message header to send get command to server
							message->msgType = MessageType::GET;
							message->msgSize = fileName.length() + 1; // doesn't matter just in case
							message->isLastChunck = true;
							message->sequenceNumber = m_localSequenceNumber;
							message->ACK = m_remotGeneratedNumber; // iggybackes the last acknowledgment for 3 way handshack
							// set the file name to download
							strcpy(message->data, fileName.c_str());

							print("3rd handshack is piggybacked with GET request message "<<"sdf"<<1223);
							sendUDPPacket(message);

							//updateLastSendPacket(message);
							if(getFileToRename(fileName ))
								retryConnectionRequest = false;
					   }//while
					   string newPath = "rename\\";
					   string concatOld=FILEPATH +newPath+fileName;
					   string concatNew=FILEPATH +newName;
					    char Old[100];
						char New[100];
						std::strcpy(Old, concatOld.c_str());
						std::strcpy(New, concatNew.c_str());
					   
					   int ans;
					 ans= rename(Old,New);
					  if(ans==0)
					  {
					   print("filename Converted to"   );
					   std::cout<<newName<<endl;
					  }
					  else 
						   print("Name conversion Failed" <<endl  );
					}
				}
				else
				{
				
					print(fileName << " is requseted from server "<< endl);

					bool retryConnectionRequest = true;
					while(retryConnectionRequest){
						handShaking(); //proess handshack

						// set the message header to send get command to server
						message->msgType = MessageType::GET;
						message->msgSize = fileName.length() + 1; // doesn't matter just in case
						message->isLastChunck = true;
						message->sequenceNumber = m_localSequenceNumber;
						message->ACK = m_remotGeneratedNumber; // iggybackes the last acknowledgment for 3 way handshack
						// set the file name to download
						strcpy(message->data, fileName.c_str());

						print("3rd handshack is piggybacked with GET request message "<<"sdf"<<1223);
						sendUDPPacket(message);

						//updateLastSendPacket(message);
						if(getFile(fileName))
							retryConnectionRequest = false;

					}//while
				}
				updateSequenceNumber(m_localSequenceNumber); 
			}
			else if(command == "put") // received 'set' command
			{
				  std::cout<<"Server Files";
				   string serverFiles = getServerFiles();
				//getFilesList();
					// print out the list of available files to upload
				//test
				m_windowSize=1;
				std::cout<<endl<<endl;
				print("List of available files  in Client " );
				listFilesInDirectory();
				print("");

				// get the file name to upload
				print("input file name");
				string fileName = readCommand();
				print("local file " << fileName << " upload is started");
				int check;
				check =serverFiles.find(fileName);
				if(check==std::string::npos)
				{
					// check if the fil exists
					if(fileExists(FILEPATH + fileName))
					{
						bool retryConnectionRequest = true;
						while(retryConnectionRequest)
						{
							handShaking();
							// send the set message to server
							message->msgType = MessageType::PUT; // command is set
							message->msgSize = fileName.length() + 1; // exact size to read
							message->isLastChunck = true; // doesn't matter just in case
							message->sequenceNumber = m_localSequenceNumber;
							message->ACK = m_remotGeneratedNumber; // if this value is equal to 3 => there is no acknowledgement piggybacked
							strcpy(message->data, fileName.c_str()); // prepare the message to send
							//send the message
							if(sendUDPPacket(message))
							{
								updateLastSendPacket(message);
								// upload the file
								if(setFile(fileName))
									retryConnectionRequest = false;
							}
						}

					 }
					 else 
					 {
						print("file does not exist");
					 }
				}
				else 
				{
					print("File exists in server  would you like to Rename or Cancel R/C");
					string Option= readCommand();
					//std::locale loc;
					 for (std::string::size_type i=0; i<Option.length(); ++i)
				      Option[i]=toupper(Option[i]);
					if(Option=="C")
					{
						break;
					}
					else if (Option=="R")
					{
					print("input file name to Rename and upload to server");
					string newFileName = readCommand();
					string source= ".\\files\\" + fileName;
					string destination = ".\\files\\rename\\" +fileName;
					CopyFile(source.c_str(),destination.c_str(),0);


					 string newPath = "rename\\";
					   string concatOld=FILEPATH +newPath+fileName;
					   string concatNew=FILEPATH +newPath+newFileName;
					    char Old[100];
						char New[100];
						strcpy(Old, concatOld.c_str());
						strcpy(New, concatNew.c_str());
					   
					   int ans;
					 ans= rename(Old,New);
					
					if(fileExists(concatNew))
					{
						bool retryConnectionRequest = true;
						while(retryConnectionRequest)
						{
							handShaking();
							// send the set message to server
							message->msgType = MessageType::PUT; // command is set
							message->msgSize = newFileName.length() + 1; // exact size to read
							message->isLastChunck = true; // doesn't matter just in case
							message->sequenceNumber = m_localSequenceNumber;
							message->ACK = m_remotGeneratedNumber; // if this value is equal to 3 => there is no acknowledgement piggybacked
							strcpy(message->data, newFileName.c_str()); // prepare the message to send
							//send the message
							if(sendUDPPacket(message))
							{
								updateLastSendPacket(message);
								// upload the file
								if(setFile("rename\\"+newFileName))
									retryConnectionRequest = false;
							}
						}
						remove(concatNew.c_str());
					 }
					 else 
					 {
						print("file does not exist");
					 }
					
				}
			}
				
			}
			else if(command == "rename") 
			{
				print("List of available files  in Client " );
				listFilesInDirectory();
				std::cout<<endl;

				print("Enter File Naame to rename " <<endl);
				string FileName = readCommand();
				if (fileExists(FILEPATH + FileName))
				{
					print("Enter New File Name " <<endl);
					string newFileName = readCommand();
					 
					   string concatOld=FILEPATH + FileName;
					   string concatNew=FILEPATH + newFileName;
					    char Old[100];
						char New[100];
						std::strcpy(Old, concatOld.c_str());
						std::strcpy(New, concatNew.c_str());
					   
					   int ans;
					 ans= rename(Old,New);
					  if(ans==0)
						std::cout<<"File name converted from "<<FileName<<" to "<<newFileName<<endl;
					  else
						  print("Whoops something went wrong" <<endl);

				}
				else 
					print("File Does not Exist" <<endl);



			}
				else if(command == "list")
			{
				getFilesList();
			}
			else if(command == "quit")
			{
				return 0;
			}
			else
			{
				std::cout<<endl;
				std::cout<<"Wrong command!"<<endl<<endl;
				continue;
			}
		}
		catch(const char* error)
		{
			print(error);
		}
		catch(OperationError* error)
		{
			print("Error occurred - " << error->Message);
		}


		print("Client sequence " << m_localSequenceNumber);
		print("Server sequence " << m_remoteSequenceNumber);
	}


	string str;
	std::cout<<"quit process";
	std::cin>>str;//just avoid to closing a process

	return 0;

}

void Client::getFilesList()
{
	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;

	bool retryConnectionRequest = true;
	while(retryConnectionRequest)
	{
		handShaking();

		// request the files list from server

		// set the header
		message->msgType = MessageType::LIST;
		message->msgSize = 0; // doesn't matter just in case
		message->isLastChunck = true;
		message->sequenceNumber = m_localSequenceNumber;
		message->ACK = m_remotGeneratedNumber;
		//updateLastSendPacket(message);
		//send the command to server
		print("Connection established"<<endl);

		sendUDPPacket(message);

		m_acknowledgementState = false;
		bool t = true;
		MessageHeader* fileList;
		string list = getFileList();
		parsServerFileList(list);

		if(list.length() != 0)
		{
			updateSequenceNumber(m_localSequenceNumber);
			retryConnectionRequest = false;
		}
		else
		{
			print("--==connection failed==--");
		}
	}

	delete [] buffer;
}


string Client::getServerFiles()
{
	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;
	string list;
	bool retryConnectionRequest = true;
	while(retryConnectionRequest)
	{
		handShaking();

		// request the files list from server

		// set the header
		message->msgType = MessageType::LIST;
		message->msgSize = 0; // doesn't matter just in case
		message->isLastChunck = true;
		message->sequenceNumber = m_localSequenceNumber;
		message->ACK = m_remotGeneratedNumber;
		//updateLastSendPacket(message);
		//send the command to server
		print("Connection established"<<endl);

		sendUDPPacket(message);

		m_acknowledgementState = false;
		bool t = true;
		MessageHeader* fileList;
		 list = getFileList();
		parsServerFileList(list);

		if(list.length() != 0)
		{
			updateSequenceNumber(m_localSequenceNumber);
			retryConnectionRequest = false;
			
		}
		else
		{
			print("--==connection failed==--");
		}
	}

	delete [] buffer;
	return list;
	
	
}


// read input from console
string Client::readCommand()
{
	string data;
	getline(cin,data);
	std::transform(data.begin(), data.end(), data.begin(), ::tolower);

	return data;
}


void Client::updateSequenceNumber(int& sequence){
	if(sequence == m_maxSiquenceNumber){
		sequence = 0;
	}
	else{
		sequence++;
	}
}


// send acknowledgement
void Client::sendACK(string messageBody){
	std::string message  = messageBody;

	int sendSize = message.length() + sizeof(MessageHeader) + 1; //+1 for "/0" to indicate end of string
	char* buffer = new char[sendSize];

	MessageHeader * header = (MessageHeader*)buffer;

	header->msgType = MessageType::ACK;
	header->msgSize = message.length() + 1;
	header->isLastChunck = true;// this is the last reply
	header->sequenceNumber = m_remoteSequenceNumber;
	header->ACK = 3; // nothing is piggybacked
	memcpy(header->data, message.c_str() , message.length() + 1);

	updateLastSendPacket(header);

	if(!sendUDPPacket(header)){
		//retuns an error when connection is died
		throw "Socket error on the client side";
	}

}


void Client::sendNAK(string messageBody){
	print("Send NAK for packet " << m_remoteSequenceNumber);
	std::string message  = messageBody;

	int sendSize = message.length() + sizeof(MessageHeader) + 1; //+1 for "/0" to indicate end of string
	char* buffer = new char[sendSize];

	MessageHeader * header = (MessageHeader*)buffer;

	header->msgType = MessageType::NAK;
	header->msgSize = message.length() + 1;
	header->isLastChunck = true;// this is the last reply
	header->sequenceNumber = m_remoteSequenceNumber;
	header->ACK = 3; // nothing is piggybacked
	memcpy(header->data, message.c_str() , message.length() + 1);

	updateLastSendPacket(header);
	//updateSequenceNumber(m_remoteSequenceNumber);
	if(!sendUDPPacket(header)){
		//retuns an error when connection is died
		throw "Socket error on the client side";
	}

}



void Client::resendLastPacket()
{
	if(m_lastSendPacket != NULL)
		sendUDPPacket(m_lastSendPacket);
}

bool Client::waitForAcknowledgement()
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
				rcvackcount++;
				if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
					slideWindow(ACKmessage->sequenceNumber, true);
			}
			else if(ACKmessage->msgType == MessageType::NAK)
			{

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
		
		if(m_window.empty() || lastNak != -1 && lastNak != m_window.front()->sequenceNumber)
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

bool Client::getFile(string fileName)
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
		throw new OperationError("file can not be written: "+ filePath);
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
				updateSequenceNumber(m_remoteSequenceNumber);
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


bool Client::getFileToRename(string fileName)
{
	resetStats();
	print("File download started: " << fileName << endl);
	string copyTo = "\\rename\\";
	m_acknowledgementState = false;
	std::string filePath = FILEPATH +copyTo +fileName; // compute the local file path to write
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
		throw new OperationError("file can not be written: "+ filePath);
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
				updateSequenceNumber(m_remoteSequenceNumber);
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












MessageHeader* Client::waitForData(){
	
	string str;
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
			if(ACKmessage->msgType  == MessageType::ACK && m_acknowledgementState == false)
			{
				m_acknowledgementState = true;
				break;
			}
			else if(ACKmessage->msgType == MessageType::ERR) //&& ACKmessage->sequenceNumber == m_remoteSequenceNumber)
			{
				throw new OperationError(ACKmessage->data);
			}
			else if(ACKmessage->msgType == MessageType::DATA)
			{
				if((!m_acknowledgementState && ACKmessage->sequenceNumber == m_remoteSequenceNumber && ACKmessage->ACK == m_localSequenceNumber)
					||(m_acknowledgementState && ACKmessage->sequenceNumber == m_remoteSequenceNumber))
				{
					if(!m_acknowledgementState)
						m_acknowledgementState = true;
					break;
				}
				else
				{
					if(!m_acknowledgementState){
						restartHandshack = true;
						break;
					}
					if(m_acknowledgementState){
						sendNAK("");
						ACKmessage = NULL;
					}
				}

			}

		}
		else if(m_acknowledgementState)
		{
			sendNAK("");
		}
		else
		{
			restartHandshack = true;
			break;
		}

		count++;
	}

	if(restartHandshack)
		return NULL;
	return ACKmessage;
}

void Client::updateLastSendPacket(MessageHeader* lastSentPacket){
	if(m_lastSendPacket != NULL)
		delete m_lastSendPacket;
	m_lastSendPacket = lastSentPacket->clone();
}



void Client::outputMessage(string message){
	
	stringstream sss;
	
	sss<< message;
	writeIntoLogFile(sss.str());
	std::cout<<sss.str()<<endl;
}

string Client::intToStr(int intValue){

	stringstream strValue;
	strValue<<intValue;
	return strValue.str();
}



void Client::writeIntoLogFile(string log){
	ofstream logFile ;
	logFile.open("log.txt", ios::out | ios::app);
	if(logFile.is_open())
	{
		logFile<<log<<endl;
		logFile.close();
	}

}

void Client::freeLogFile(string log){
	ofstream logFile;
	if(fileExists(log)){
		remove(log.c_str());
	}
}


bool Client::fileExists(string fileName)
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

bool Client::setFile(string fileName)
{
	resetStats();
	bool result = false;
	// compute local file name
	std::string filePath = FILEPATH	+ fileName;
	int nuberOfBits = maxDataSize;// maxmum size for a chank

	size_t size = 0;
	char* byteStream = 0;
	int dataBytesSent = 0;

	// open the file
	print("Preparing file to send to server : " <<filePath <<endl);
	ifstream file (filePath, ios::in|ios::binary|ios::ate);
	// if the file is opened
	if (file.is_open())
	{
		//wait for a confirmation from a server side

		MessageHeader * header = waitForHandshackAcknowledgement();
		if(header == NULL)
			return false;

		print(endl << "File transfer has started "<< endl);

		// comput the file size
		file.seekg(0,ios::end);
		size = file.tellg();
		file.seekg(0,ios::beg);
		size_t currentSize = size;
		int current = 0;
		int isLast = 0; //this variable shows whether the sending packet is the last one or not, if it is equal 0 => is not the last
		//MessageHeader * header = (MessageHeader*)m_sendBuffer; //maps message header into a buffer
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
				header->sequenceNumber = m_localSequenceNumber;
				header->ACK = 3;

				//keep last sent packet
				updateLastSendPachet(header);
				updateSequenceNumber(m_localSequenceNumber);

				numberOfSentPackets++;

				print("Sent data packet " << header->sequenceNumber);
				//::Sleep(30);
				if(!sendUDPPacket(header)){
					file.close();
					std::cout<<endl;
					//retuns an error when connection is died
					throw "Socket error on the client side";
				}
				
				if(m_processIntermediateAcks && (current % 3) == 0)
					waitForIntermediateAcknowledgement();
			}//while second


			bool confirmation =  waitForAcknowledgement();
			
			if(confirmation){
				print("Received ACK for packet " << header->sequenceNumber);
			}
			else{
				print("File processing is failed"<<endl);
				break;
			}


			//update statistics
			current++;
			dataBytesSent += header->msgSize;

		} //While first
		file.close();
		result = true;

		print("Number of DATA packets sent: " << current);
		print("Number of DATA bytes sent: " << dataBytesSent);
		print("Number of packets sent: " << sentPackets);
		print("Number of bytes sent: " << sentBytes);
		print("Total number of packets received: " << recvPackets);
		print("Total number of bytes received: " << recvBytes);

		resetStats();

	}
	else
	{
		throw new OperationError("file doesn't exist");
	}

	return result;
}


void Client::resetState(){
	m_localSequenceNumber = m_localGeneratedNumber; //getLastSignificantBit(m_localGeneratedNumber);
	m_remoteSequenceNumber = m_remotGeneratedNumber; //getLastSignificantBit(m_remotGeneratedNumber);
}



void Client::parsServerFileList(string list)
{
	// wait for a message containing file name list

	// parse and split the data into vector
	vector<string> fileItems;
	split(list, &fileItems);

	vector<string>::iterator it;
	// print the file names list
	for(it = fileItems.begin(); it != fileItems.end(); ++it){
		print(*it);

	}
}

string Client::getFileList(){
	int currentChunk = 0;
	MessageHeader* header = NULL;
	string list;
	bool t = true;
	// receive the file data from server in a loop and write it into the file
	try{
		do{ 
			header =  waitForData();
			if(header == NULL)
				return "";
			if( header != NULL && header->msgType == MessageType::DATA)
			{
				//parsServerFileList(fileList);
				//updateSequenceNumber(m_remoteSequenceNumber);
				t = false;
				list = list + header->data;
				//std::cout<<currentChunk << " chank is downloaded " <<endl;
				currentChunk++;	
				//m_lastSendPacket = header;
				sendACK("chank is received"); //send acknowledjment first

				//print("data: " << list<<endl<<"------------------------"<<endl);

				updateSequenceNumber(m_remoteSequenceNumber);
				if(header->isLastChunck)
				{
					print("Enter retry send last packet");
					int repeatForLastPacket = 0;
					while(repeatForLastPacket <= LAST_PACKET_RETRY)
					{
						if(runSelectMethod() != NULL)
							resendLastPacket();
						repeatForLastPacket++;
					}
				}
			}
		}
		while(header!= NULL && !header->isLastChunck); // finish the loop if the last chunk is received
	}catch(const char* ee){
		throw ee;
	}

	return list;
}


MessageHeader*  Client::waitForHandshackAcknowledgement(){
	string str;
	MessageHeader* ACKmessage = NULL;
	int count = 0;
	while(count <= RETRY)
	{
		ACKmessage = runSelectMethod();
		if(ACKmessage != NULL && ACKmessage->msgType == MessageType::ACK &&  ACKmessage->sequenceNumber == m_localSequenceNumber){
			updateSequenceNumber(m_localSequenceNumber);
			// acknowledgement received
			break;
		}

		count++;
		if(count >= RETRY)
			ACKmessage = NULL;
	}

	return ACKmessage;
}


int Client::computeSequenceNumberRange(int windowSize){
	/*int mGBN = log(windowSize + 1) / log(2) + 1;
	return pow(2,mGBN) - 1;*/
	double logvalue = log(windowSize + 1) / log(2);
	int mGBN = logvalue;
	if((double)mGBN != logvalue)
		mGBN++;

	//std::cout<< logvalue<< "|" <<mGBN << "\t--- ";
	return pow(2,mGBN) - 1;
}


void Client::slideWindow(int receivedACKNumber, bool isACK){
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

void Client::fillSequenceMap(int windowSize){
	for(int i = 0; i<= windowSize; i++){
		m_sequenceMap[i].isSet = false;
	}

}






void Client::CalculateRTT(const SYSTEMTIME& sentTime)
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


MessageHeader* Client::runIntermediateSelectMethod(int timeOut){
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


void  Client::waitForIntermediateAcknowledgement(){

	MessageHeader* ACKmessage = NULL;
	int count = 0;

	ACKmessage = runIntermediateSelectMethod(1);
	if(ACKmessage != NULL){
		if( ACKmessage->msgType == MessageType::ACK)  
		{
			if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
				slideWindow(ACKmessage->sequenceNumber, true);

		}
		if(ACKmessage->msgType == MessageType::NAK){
			if(m_sequenceMap[ACKmessage->sequenceNumber].isSet)
			{
				slideWindow(ACKmessage->sequenceNumber, false);
				resendFirstPacket();

			}

		}
	}
}


void Client::goBackN(int receivedNACNumber){

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
				print("Resend data packet " << (*lit)->sequenceNumber);
				sendUDPPacket(*lit);
				m_sequenceMap[(*lit)->sequenceNumber].isResent = true;
			}
		}
	}
}



void Client::updateLastSendPachet(MessageHeader* lastSentPacket)
{
	if(m_sequenceMap.find(lastSentPacket->sequenceNumber) == m_sequenceMap.end())
		throw "Error: sequence not found in map";

	m_sequenceMap[lastSentPacket->sequenceNumber].isSet = true;
	GetSystemTime(&m_sequenceMap[lastSentPacket->sequenceNumber].timeSent);
	m_sequenceMap[lastSentPacket->sequenceNumber].isResent = false; 
	m_window.push_back(lastSentPacket->clone());
}



void Client::resendFirstPacket(){
	if(!m_window.empty())
	{
		m_sequenceMap[m_window.front()->sequenceNumber].isResent = true;
		sendUDPPacket(m_window.front());
	}
}



void Client::testGet(string fileName)
{
	print(fileName << " is requeted from server "<< endl);

	char* buffer = new char[maxDataSize];
	MessageHeader* message = (MessageHeader*)buffer;

	bool retryConnectionRequest = true;
	while(retryConnectionRequest){
		handShaking(); //proess handshack

		// set the message header to send get command to server
		message->msgType = MessageType::GET;
		message->msgSize = fileName.length() + 1; // doesn't matter just in case
		message->isLastChunck = true;
		message->sequenceNumber = m_localSequenceNumber;
		message->ACK = m_remotGeneratedNumber; // iggybackes the last acknowledgment for 3 way handshack
		// set the file name to download
		strcpy(message->data, fileName.c_str());

		print("3rd handshack is piggybacked with GET request message "<<"sdf"<<1223);
		sendUDPPacket(message);

		//updateLastSendPacket(message);
		if(getFile(fileName))
			retryConnectionRequest = false;

	}//while
	updateSequenceNumber(m_localSequenceNumber); 

}
