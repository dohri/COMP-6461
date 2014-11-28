

#pragma once

//=================== Winsock Library ws2_32.lib included below=======================
#pragma comment( linker, "/defaultlib:ws2_32.lib" )
//=================== Winsock Library ws2_32.lib included below=======================

//===================== Socket header files===========================================
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <winsock.h>
#include <windows.h>
//===================== Socket header files===========================================

//=========================Header files used for I/O and other purpose================
#include <iostream>
#include <fstream>
#include <string>
#include "Thread.h"
#include <list>
//=========================Header files used for I/O purpose===========================


#include "Dirent.h" // for directory access

using namespace std;

//======================port data types=================================================
#define REQUEST_PORT 0x7070;
int port=REQUEST_PORT;
//======================port data types=================================================


//=====================Sock Version 2 being used here===================================
#define SCK_VERSION2 0x0202
//=====================Sock Version 2 being used here===================================


//======================socket data types===============================================
SOCKET s_Server;         // Server Socket obj
SOCKET c_Client;         // Client Socket obj
SOCKADDR_IN s_Bind;      // Bind information
SOCKADDR_IN s_Info;     //  Server information (SERVER NAME, IP , PORT NO....)
//======================socket data types===============================================

DIR *dir;
struct dirent *ent;

union {struct sockaddr generic;
	struct sockaddr_in ca_in;}ca;

int calen=sizeof(ca); 

int const data_buffer = 64; // Maximum data that can be sent or receive in one go in a data packet.
int const MAX = 50;          // File name cannot exceed 50 chars


//===================Buffer data types===================================================
char c_buffer[128];
char *file_buf;
int ibufferlen;
int recvd_bytes;
int sent_bytes;
//===================Buffer data types===================================================



//=============HOST DATA VARIABLES=======================================================
char localhost[11];
HOSTENT *h;
//=============HOST DATA VARIABLES=======================================================


//============TIME DEALAY  VARIABLES=====================================================
int nsa1;
int r,infds=1, outfds=0;
struct timeval timeout;
const struct timeval *tp=&timeout;
	fd_set readfds;
//============TIME DEALAY  VARIABLES=====================================================

//others
HANDLE test;
DWORD dwtest;

static string str_path;

struct FILEREQ_FRAME 
{
	unsigned char file_req;	    //GET or PUT or LIST
	char file_name[MAX];		//File Name
} filereq;

struct DATA_FRAME
{
	unsigned char header;		//ERROR,DATA,LASTPACKET....
	char data[data_buffer]; 	//Data or error message
} data_frame;

//**************************REFERENCE**************************************************************************************************************************

/*

================================HOST STRUCTURE========================================
http://msdn.microsoft.com/en-ca/library/windows/desktop/ms738552(v=vs.85).aspx
typedef struct hostent {
  char FAR      *h_name;         --> The official name of the host 
  char FAR  FAR **h_aliases;     --> A NULL-terminated array of alternate names
  short         h_addrtype;      --> The type of address being returned.
  short         h_length;        --> The length, in bytes, of each address.
  char FAR  FAR **h_addr_list;   --> A NULL-terminated list of addresses for the host. Addresses are returned in network byte order
} 
================================HOST STRUCTURE=========================================

================================Socket address structure===============================
http://msdn.microsoft.com/en-us/library/aa917469.aspx
https://www.youtube.com/watch?v=NIqYwXcUdn0:
struct sockaddr_in 
{
	short   sin_family;          --> The address family; MUST be 0x0002.  
	u_short sin_port;            --> An IP port number.
	struct  in_addr sin_addr;    --> An IP address.
	char    sin_zero[8];         --> Set to zero. This field is ignored by the server.
}; 
================================Socket address structure===============================

https://www.youtube.com/watch?v=NIqYwXcUdn0:
struct WSAData { 
   WORD wVersion; 
   WORD wHighVersion; 
   char szDescription[WSADESCRIPTION_LEN+1]; 
   char szSystemStatus[WSASYSSTATUS_LEN+1]; 
   unsigned short iMaxSockets; 
   unsigned short iMaxUdpDg; 
   char FAR * lpVendorInfo; 
};


*/

//*************************************************************************************************************************************************************

//===================================================================MAIN THREAD=============================================================================
// Reference : http://msdn.microsoft.com/en-ca/library/windows/desktop/ms682516(v=vs.85).aspx




unsigned int _stdcall transferring(void *data) 
{
	string str1= "";
	SOCKET *c_Client = (SOCKET*) data;
	SOCKET cli = *c_Client;


	cout << "\n New thread created" << endl;
   // SOCKET current_client = (SOCKET)lpParam;  // set our socket to the current client socket in connection  request
  
/*	// buffer to hold our received data
	char buf[100];
	// buffer to hold our sent data
	char sendData[100];
	// for error checking 
	int res; */
  
	while(true) 
	{
		char clientHostName[128] = {0}; //Variable to hold Client host name

		
		if((recvd_bytes = recv(cli,c_buffer,128,0)) == SOCKET_ERROR)
			throw "Receive error in server program\n";
		else
		{
			for(int i = 0; i < strlen(c_buffer); i++)
				clientHostName[i] = c_buffer[i]; 
			cout << "User " << clientHostName << " connected ";
		}

		recvd_bytes = recv(cli, (char *)&filereq, sizeof(filereq), 0); //Get or Put OR LIST request from client (0 for GET and 1 for PUT ELSE LIST)

		if(recvd_bytes == SOCKET_ERROR)
			throw "Receive error in server program\n";
		else
		{
			cout << filereq.file_name << " to be ";
			if(filereq.file_req == '0')
			{	
				cout << "sent" << endl;
			}
			else if(filereq.file_req == '1')
			{
				cout<<"Received" << endl;
			}
			else
			{
				cout << "list sent." << endl;
			}
		}

		//===========FILE REQUEST = GET , SEND FILE=========================================
		if(filereq.file_req == '0')
		{
			cout << "Sending file to " << clientHostName << endl;
 
			ifstream fileout;

			//BINARY FILE HANDLING
			fileout.open(filereq.file_name, ios_base::in | ios_base::binary );  //open for input in binary mode to transfer BINARY FILES

			//File Check
			if (!fileout.is_open())  //If file does not exit
			{
				data_frame.header = '9'; //char 9 indicates an error
					
				//Error message on client side
				send(cli, (char *)&data_frame, sizeof(data_frame), 0);

				//Error message on server side
				cout <<"ERROR: File Does Not Exist!" << endl;
				cout << "File " << filereq.file_name << " cannot be sent to client " << clientHostName << "!" << endl;
			}
			else //If file exists
			{
				//Get file size
				fileout.seekg (0, ios::end);
				int filesize = fileout.tellg();
				fileout.seekg (0, ios::beg);

				//To keep track of bits read and transferred 
				int bits_trans = 0;

				//To keep track of bits left to be read and transferred 
				int bits_left_trans = filesize;

				//A flag of type bool named last packet to identify the last packet that is to be transmitted and before which the buffer is to be flushed
				bool lastpacket = false;

				//Loop to maintain frequency of packets sent until whole file is transferred 
				while(lastpacket == false)
				{
					//If the file pointer is still at beg of file or is in the middle and not reached the last packet to be transferred 
					if(bits_left_trans > data_buffer)
					{
						
						data_frame.header = '1'; //char 1 means this data sent is still in inbetween the file i.e after beg and before last packet or EOF

						fileout.read(data_frame.data, data_buffer); //Read data into the data frame until the allocated size (128) is reached

						//=================== Update bits transferred and left to be transferred ================================
						bits_left_trans = bits_left_trans - data_buffer;
						bits_trans = bits_trans + data_buffer;
                        //=================== Update bits transferred and left to be transferred ================================
						
						//Send entire data frame to Client
						send(cli, (char *)&data_frame, sizeof(data_frame), 0); //Send entire data frame to Client before next frame is transferred

						
						lastpacket = false; // Flag is still false as it is not the last packet to be transferred
					}
					else //If the bits left to be transferred is greater than the buffer size/ allocated size then this is the last frame of data to be sent
						if(bits_left_trans <= data_buffer)
						{
							//Anything other than 1 will be last packet or data frame to be sent i.e data with EOF
							data_frame.header = (unsigned char)bits_left_trans; // range from 0-255

							// we need to flush the data frame before it should be last packet is sent as we are not using the entire allocated buffer size
							memset(data_frame.data, 0, sizeof(data_frame.data));

							//Read last chunk of data into frame with size as LAST PACKET size
							fileout.read(data_frame.data, bits_left_trans);

							//Update bits left and bits transferred ideally bits transferred will be size of file and bits left vice-versa
							bits_left_trans = bits_left_trans - bits_left_trans;
							bits_trans = bits_trans + bits_left_trans;

							
							send(cli, (char *)&data_frame, sizeof(data_frame), 0); //Send last packet to Client

							
							lastpacket = true;  //Flag is updated to true as this is last packet to be sent and exit loop.
						}
				}
			}
			fileout.close(); //CLOSE FILE

			//If there was no error in sending the file then following message is displayed on server side 
			if(data_frame.header != '9')
			{
				cout << "File " << filereq.file_name << " sent to client " << clientHostName << "!" << endl;
			}
		}
		//==================================================
		/*if(filereq.file_req == '2')
		{
			if ((dir = opendir ("C:\\Users\\DhruvOhri\\Documents\\Visual Studio 2012\\Projects\\Server\\Debug\\")) != NULL)
			{
				/* print all the files and directories within directory 
			    while ((ent = readdir (dir)) != NULL) 
				{
					printf ("%s\n", ent->d_name);
					str1 = str1 + ent->d_name;
					str1 = str1 + "\n";
				}
				closedir (dir);
			}
			else
			{
				/* could not open directory 
				perror ("could not open directory");
			}
			strcpy(data_frame.data,str1.c_str());
			send(cli, (char *)&data_frame, sizeof(data_frame), 0);
		} */

		//=====================================================
		else if(filereq.file_req == '1')// FILE REQUEST = 1 i.e PUT --> store file on server
		{
			cout << "Receiving file from " << clientHostName << ", waiting..." << endl;

			//Since file is to be written onto the server thus creating a ofstream object "filein"
			ofstream filein;

			//Handling in binary mode even if the file exists it will create a new file and overwrite it.
			filein.open(filereq.file_name, ios::out | ios::binary );

			//=======================SAME LOGIC FOR LAST PACKET AS ABOVE===========================
			bool lastpacket = false;

			//Packets are received continuously until whole file is transmitted from client to server 
			while(lastpacket == false)
			{
				//Get data frame from client side 
				recvd_bytes = recv(cli, (char *)&data_frame, sizeof(data_frame), 0);

				//EXCEPTION HANDLED "if there is any error while receiving packets"
				if(recvd_bytes == SOCKET_ERROR)
					throw "\nFile could not be received";
				else //no error
				{
					switch (data_frame.header)
					{
						case '1': //Data frame received is either at start of file or inbetween the file
							//Writing the received frame to the file
							filein.write (data_frame.data, sizeof(data_frame.data));
							lastpacket = false;
							break;
						default: //EOF LAST CHUNK OF DATA FRAME received from CLIENT
							filein.write (data_frame.data, (int)data_frame.header);
							lastpacket = true;
							break;
					}
				}
			}

			//CLOSE FILE
			filein.close();

			//message saying file has been received
			cout << "File " << filereq.file_name << " received from client " << clientHostName << "!" << endl;
		}
		else
		{
			if ((dir = opendir ("C:\\Users\\DhruvOhri\\Documents\\Visual Studio 2012\\Projects\\Server\\Debug")) != NULL) 
				{
					/* print all the files and directories within directory */
					while ((ent = readdir (dir)) != NULL)
						{
							str1 = str1 + ent->d_name + "\n";
							//printf ("%s\n", ent->d_name);
					    }
					strcpy(data_frame.data,str1.c_str());
					send(cli, (char *)&data_frame, sizeof(data_frame), 0);
					closedir (dir);
			}else 
			{
				/* could not open directory */
				perror ("");
				strcpy(data_frame.data,"could not open directory");
				send(cli, (char *)&data_frame, sizeof(data_frame), 0);
				return EXIT_FAILURE;
			}
		}
		//cout<<" CLIENT CLOSED:";
		// Close the current socket thus ending the current thread for the current client.
		closesocket(cli);
	    ExitThread(0);
		
		//Successful exit / Termination of the thread
		
	}
	
}

HANDLE myhandle[2];
int myhandleCounter = 0;
int threadID = 0;

int main(void)
{
	DWORD thread;
	WSADATA wsadata;

	try
	{        		 
		if (WSAStartup(SCK_VERSION2,&wsadata)!=0)
		{  
			cout<<"Error in starting WSAStartup()\n";
		}
		else
		{
			file_buf="WSAStartup was suuccessful\n";   
			WriteFile(test,file_buf,sizeof(file_buf),&dwtest,NULL); 

			/* display the wsadata structure */
			cout<< endl
				<< "wsadata.wVersion "       << wsadata.wVersion       << endl
				<< "wsadata.wHighVersion "   << wsadata.wHighVersion   << endl
				<< "wsadata.szDescription "  << wsadata.szDescription  << endl
				<< "wsadata.szSystemStatus " << wsadata.szSystemStatus << endl
				<< "wsadata.iMaxSockets "    << wsadata.iMaxSockets    << endl
				<< "wsadata.iMaxUdpDg "      << wsadata.iMaxUdpDg      << endl;
		}

		//Display info of local host

		gethostname(localhost,10);
		cout << "hostname: " <<localhost << endl;

		if((h=gethostbyname(localhost)) == NULL) 
		{
			cout << "gethostbyname() cannot get local host info?"
				<< WSAGetLastError() << endl; 
			exit(1);
		}

		//Create the server socket
		if((s_Server = socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET) 
			throw "can't initialize socket";
		// For UDP protocol replace SOCK_STREAM with SOCK_DGRAM 


		//Fill-in Server Port and Address info.
		s_Bind.sin_family = AF_INET;
		s_Bind.sin_port = htons(port);
		s_Bind.sin_addr.s_addr = htonl(INADDR_ANY);


		//Bind the server port
		if (bind(s_Server,(LPSOCKADDR)&s_Bind,sizeof(s_Bind)) == SOCKET_ERROR)
			throw "can't bind the socket";
		cout << "Bind was successful" << endl;

		//Successfull bind, now listen for client requests.
		if(listen(s_Server,SOMAXCONN) == SOCKET_ERROR)
			throw "couldn't  set up listen on socket";
		else cout << "Listen was successful" << endl;

		while(c_Client = accept(s_Server,0,0))
		{
			if(c_Client == INVALID_SOCKET)
			{
				cout<<"Couldn't accept connection\n";
				continue;
			}
			_beginthreadex(0,0,transferring,(void*)&c_Client,0,0);
		}

	} //try loop 

	//Display needed error message.

	catch(char* str) { cerr<<str<<WSAGetLastError()<<endl;}

	//close client socket
	closesocket(c_Client);		

	//close server socket
	closesocket(s_Server); 

	/* When done uninstall winsock.dll (WSACleanup()) and exit */
	WSACleanup();
	return 0;
	system("pause");
}



