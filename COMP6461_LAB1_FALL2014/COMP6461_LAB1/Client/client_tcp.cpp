//=================== Winsock Library ws2_32.lib included below=======================
#pragma comment( linker, "/defaultlib:ws2_32.lib" )
//=================== Winsock Library ws2_32.lib included below=======================

//===================== Socket header files===========================================
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winsock.h>
//===================== Socket header files===========================================

//=========================Header files used for I/O and other purpose================
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <windows.h>
#include <fstream>
//=========================Header files used for I/O and other purpose================

using namespace std;

//======================port data types=================================================
#define REQUEST_PORT 0x7070;
int port=REQUEST_PORT;
//======================port data types=================================================

//======================socket data types===============================================
SOCKET s;               // Socket Obj
SOCKADDR_IN sa;         // filled by bind
SOCKADDR_IN s_info;      // Server information (SERVER NAME, IP , PORT NO....)
//======================socket data types===============================================

//===================Buffer data types===================================================
int const data_buffer = 64;
int const MAX = 50;
char c_buffer[128];
char *buffer;
int ibufferlen=0;
int sent_bytes;
int recvd_bytes=0;
int msgSent;
int msgReceived = 0;
int c = 1;
//===================Buffer data types===================================================

//=============HOST DATA VARIABLES=======================================================
HOSTENT *h;
HOSTENT *r;
//=============HOST DATA VARIABLES=======================================================

char localhost[11], temp[11],y,
     remotehost[11];
//string temp = "";

//other
HANDLE test;
DWORD dwtest;

//=========Following 2 frames are used for sending the request and the following it the data==========
struct FILEREQ_FRAME
{
	unsigned char file_req;	//GET or PUT or LIST
	char fname[MAX];			//File Name
} filereq;

struct DATA_FRAME
{
	unsigned char header;		//ERROR, DATA, LASTPACKET, etc.
	char data [data_buffer];		//data or error message
} data_frame;
//======================================================================================================

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

==================================WINSOCK API DATA structure===========================

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

==================================WINSOCK API DATA structure===========================
*/

//*************************************************************************************************************************************************************

//=========================================================================MAIN Program============================================================================
int main(void)
{
	WSADATA wsadata;

	try
	{
		if (WSAStartup(0x0202,&wsadata)!=0) // WSA STARTUP starting the WINSOCK API
		{
			cout<<"Error in starting WSAStartup()" << endl;
		}
		else
		{
			buffer="WSAStartup was successful\n";
			WriteFile(test,buffer,sizeof(buffer),&dwtest,NULL);

			
			cout<< endl
				<< "wsadata.wVersion "       << wsadata.wVersion       << endl
				<< "wsadata.wHighVersion "   << wsadata.wHighVersion   << endl
				<< "wsadata.szDescription "  << wsadata.szDescription  << endl
				<< "wsadata.szSystemStatus " << wsadata.szSystemStatus << endl
				<< "wsadata.iMaxSockets "    << wsadata.iMaxSockets    << endl
				<< "wsadata.iMaxUdpDg "      << wsadata.iMaxUdpDg      << endl << endl;
		}

		gethostname(localhost,10); // DISPLAY LOACAL HOST NAME THAT WOULD BE SAME FOR BOTH CLIENT AND SERVER SINCE RUNNING ON SAME MACHINE
		cout << "ftp_tcp starting at host: [" << localhost << "]" << endl;

		if((h=gethostbyname(localhost)) == NULL)
			throw "gethostbyname failed\n";

		while(!strcmp(temp, "quit") == 0) // Loop will run user does not enter QUIT
		{

			//Ask for name of remote server only once, on next request no need to mention the hostname again
			if(c == 1)
			{
			cout << "please enter your remote server name: " << flush ;
			cin >> remotehost ;
			strcpy(temp,remotehost);
			c++;
			}
			else
			{
				strcpy(remotehost,"Dhruv"); // COPY server host name here to remote host (Client), I have hard coded here because local host is DHRUV one can define to variable as well
			}
			
			if (strcmp(temp, "quit") == 0)
				exit(0);

			//Display remote host name
			cout << "Remote host name is: \"" << remotehost << "\"" << endl;

			if((r=gethostbyname(remotehost)) == NULL)
				throw "remote gethostbyname failed\n";

			//Create the socket, we are using connection oriented so TCP protocol
			if((s = socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET)
				throw "Socket failed\n";

			//Specify server address for client to connect to server.
			memset(&s_info,0,sizeof(s_info));
			memcpy(&s_info.sin_addr,r->h_addr,r->h_length);
			s_info.sin_family = r->h_addrtype;
			s_info.sin_port = htons(port);

			//Display the host machine internet address
			cout << "Connecting to remote host: ";
			cout << inet_ntoa(s_info.sin_addr) << endl;

			//Connect Client to the server
			if (connect(s,(LPSOCKADDR)&s_info,sizeof(s_info)) == SOCKET_ERROR)
				throw "connect failed\n";

			//sending client's hostname to server
			sprintf(c_buffer, localhost);
			msgSent = 0;
			ibufferlen = strlen(c_buffer);
			msgSent = send(s,c_buffer,ibufferlen,0);

			//variable to hold file_req of transfer i.e. get or put or list
			char file_req[5];


			//Flush the file request for every other client request
			memset(file_req, '\0', sizeof(file_req));

			//make sure input from user is get, put or list or else keep asking
			while(!strcmp(file_req, "get") == 0 && !strcmp(file_req, "put") == 0 && !strcmp(file_req,  "list") == 0)
			{
				cout << "File request GET / PUT / LIST: ";
				//get file_req of transfer from user
				cin >> file_req;
			}

			//File request get is char 0 and put is char 1 and anything other than 0 or 1 is list
			
			// Hard coded temp to host name that is server host name and so that user does not has to input again.
			// We can also define it from remote host name variable as well.
			if(strcmp(file_req, "get") == 0)
			{
				filereq.file_req = '0';
				strcpy(temp,"Dhruv");  
			}
			if(strcmp(file_req, "put") == 0)
			{
				filereq.file_req = '1';
				strcpy(temp,"dhruv");
			}
			if(strcmp(file_req, "list") == 0)
			{
				filereq.file_req = '2';
				strcpy(temp,"dhruv");
			}
			else
				strcpy(temp,"quit");


			//If client request is get or put then enter file name else it is list then no need to ask file name
			if(strcmp(file_req,"list")!= 0)
			{
				cout << "Enter file name: ";
				cin >> filereq.fname;
			}

			//Logic for PUT or sending file 
			if(filereq.file_req == '1')
			{
				ifstream checkfile;
				checkfile.open(filereq.fname, ios_base::in | ios_base::binary ); // Handling binary files here thus binary mode / checking file status

				// Loop will continously until user enters correct file name
				while(!checkfile.is_open()) 
				{
					checkfile.close();

					//Flushing file name on every request
					memset(filereq.fname, 0, sizeof(filereq.fname));
					cout << "Enter file name: ";
					cin >> filereq.fname;

					// Check file status again 
					checkfile.open(filereq.fname, ios_base::in | ios_base::binary );
				}
				checkfile.close();
			}

			send(s, (char *)&filereq, sizeof(filereq), 0); // Send file request to server 

			//Logic for Get
			if(filereq.file_req == '0')
			{
				cout << "Receiving file from " << remotehost << endl;

				//Open file in write mode thus ofstream object
				ofstream filein;

				//Open for output in binary mode (overwrite file if it already exists)
				filein.open(filereq.fname, ios::out | ios::binary );

				//A flag of type bool named last packet to identify the last packet to be transmitted  but before we have to flush the buffer
				bool lastpacket = false;

				//Loop to maintain frequency of packets sent until whole file is transferred 
				while(lastpacket == false)
				{
					//Get data and keep on writing to file
					recvd_bytes = recv(s, (char *)&data_frame, sizeof(data_frame), 0);

					//check for error else receive packets
					if(recvd_bytes == SOCKET_ERROR)
						throw "Receive error in server program\n";
					else //no error
					{
					// IF HEADER IS 9 then there is some error , HEADER = 1 means has not been completely transmitted else this is last packet   
						switch (data_frame.header)
						{
							case '9': // HEADER  = 9 
								filein.close();
								remove(filereq.fname); // DELETE FILE
								cout <<"ERROR: File Does Not Exist!" << endl;
								cout << "File " << filereq.fname << " cannot be retrieved from the server ";
								cout << remotehost << "!" << endl << endl;
								lastpacket = true;
								break;
							case '1': //HEADER = 1
					            filein.write (data_frame.data, sizeof(data_frame.data));
								lastpacket = false;
								break;
							default: //HEADER != 1 or 9 for last packet
							    filein.write (data_frame.data, (int)data_frame.header);
								lastpacket = true;
								break;
						}
					}
				}


				//Check if there was any error or not and show the file name
				if(data_frame.header != '9')
				{
					filein.close();
					cout << "File " << filereq.fname << " received from host " << remotehost << "!" << endl << endl;
				}
			}
			else if(filereq.file_req == '1') //Handling PUT
			{
				cout << "Sending file to " << remotehost << endl;
				ifstream fileout;

				//handling binary file
				fileout.open(filereq.fname, ios_base::in | ios_base::binary );
			    if (!fileout.is_open()) //Checking if file even exists on client or not
				{
					//if file does not exist: error message
					cout <<"ERROR: File Does Not Exist!" << endl << endl;
				}
				else 
				{
					//get the file size
					fileout.seekg (0, ios::end);  // Set pointer at start end of file
					int filesize = fileout.tellg(); // Since pointer is end of file tellg will get size
					fileout.seekg (0, ios::beg); // again set pointer at beginning of file again since file has to be transmitted from start

					//To keep track of bits read and transferred 
					int bits_trans = 0;

					//To keep track of bits left to be read and transferred 
					int bits_left_trans = filesize;

					//A flag of type bool named last packet to identify the last packet that is to be transmitted and before which the buffer is to be flushed
					bool lastpacket = false;

					//Keep sending data until whole file is transmitted
					while(lastpacket == false)
					{
						//if bits left to be transferred is greater than the buffer size that is amount of bits transferred in one go which is 64 bytes then last packet is not received 
						if(bits_left_trans > data_buffer)
						{
							//HEADER = 1 
							data_frame.header = '1';

							
							fileout.read(data_frame.data, data_buffer);

							//=================== Update bits transferred and left to be transferred ================================
							bits_left_trans = bits_left_trans - data_buffer;
							bits_trans = bits_trans + data_buffer;
							//=================== Update bits transferred and left to be transferred ================================
							
							send(s, (char *)&data_frame, sizeof(data_frame), 0); // send bits to the server in size of 64 bits per packet

							
							lastpacket = false;
						}
						else //If bits left is less than buffer size then it is the last chunk of data to be transmitted before whole file is sent
							if(bits_left_trans <= data_buffer)
							{
								//HEADER other than 1 or 9
								data_frame.header = (unsigned char)bits_left_trans;

								//Flush the buffer before last chunk of data is sent so that damaged or corrupted packet is not sent
								memset(data_frame.data, 0, sizeof(data_frame.data));

								// Send only the last chunk of data in bits or bits left to be transmitted
								fileout.read(data_frame.data, bits_left_trans);

								//=================== Update bits transferred and left to be transferred ================================
								bits_left_trans = bits_left_trans - bits_left_trans;
								bits_trans = bits_trans + bits_left_trans;
								//=================== Update bits transferred and left to be transferred ================================
								
							
								send(s, (char *)&data_frame, sizeof(data_frame), 0);
								lastpacket = true;
							}
					}
				}
				fileout.close();
				cout << "File " << filereq.fname << " sent to host " << remotehost << "!" << endl << endl;
			}
			else
			{
				recvd_bytes = recv(s, (char *)&data_frame, sizeof(data_frame), 0);
				cout<<data_frame.data;
			}

			cout<<"\n Do you want to send another file (type y for yes):";
			cin>>y;
			if(y == 'y')
				strcpy(temp,"dhruv"); // Again hard coded the since client and server are run on same machine
			else
				strcpy(temp,"quit");

		} //END OF WHILE
	} // END OF TRY

	//Display any needed error response
	catch (char *str) { cerr<<str<<":"<<dec<<WSAGetLastError()<<endl;}
	
	closesocket(s);

	// Always call WINSCOK clean method in the end
	WSACleanup();
	return 0;
}

