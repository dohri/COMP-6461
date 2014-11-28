//**********************************************************************************************************************************
//    Entry point for client code
//**********************************************************************************************************************************


#define _CRT_SECURE_NO_WARNINGS 
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
#include <fstream>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>
#include <list>
using namespace std;
#pragma comment(lib, "Ws2_32.lib") //liks to winsosk library
#include "protocol.h"

#include "Client.h"
#include <stdio.h>
#include <time.h>


int _tmain(int argc, _TCHAR* argv[])
{
	
	int windowSize;
	cout<<"Input Window Size: ";
	string str;
	getline(cin, str);
	windowSize = atoi(str.c_str());

	cout<<"Input Router host: ";
	string router;
	getline(cin,router);


	Client client(router, windowSize,0);
	client.processClientMessages();

	return 0;
}

