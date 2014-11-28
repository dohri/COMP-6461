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

int _tmain(int argc, _TCHAR* argv[])
{
	/*for(int i = 0; i < 20; i++)
		cout<< i << "\t" << computeSequenceNumberRange(i) << endl;*/

	
	
	int windowSize;
	std::cout<<"Input Window Size: ";
	string str;
	getline(cin, str);
	windowSize = atoi(str.c_str());

	std::cout<<"Input Router host: ";
	string router ;
	getline(cin,router);
	

	

	Server server(router, windowSize, 0);
	server.processIncommingMessages();
	return 0;
}

