// DaytimeClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <string>
#include <sstream>
#include <conio.h>
#include <google/protobuf/util/time_util.h>

#include "Final.pb.h"

using namespace std;
#define DEFAULT_BUFLEN 512
#define MAX_THREADS 32
HANDLE  hThreadArray[MAX_THREADS];
int numThreads = 0;




void rec_cli(SOCKET s)
{
	char recvline[DEFAULT_BUFLEN];
	memset(recvline, 0, sizeof(char) * DEFAULT_BUFLEN);

	while (true)
	{
		if (recv(s, recvline, DEFAULT_BUFLEN, 0) == 0)
		{
			printf("str_cli: server terminated prematurely");
			exit(0);
		}

		Final::Commands response;
		std::string message = recvline;

		response.ParseFromString(message);
		std::string test = *response.mutable_message();
		printf("%s\n", test.c_str());
		//fputs(recvline, stdout);
		memset(recvline, 0, sizeof(char) * DEFAULT_BUFLEN);
		
	}
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)

{
	// Cast the parameter to the correct data type.
	// The pointer is known to be valid because 
	// it was checked for NULL before the thread was created.
	SOCKET s = (SOCKET)lpParam;
	
	rec_cli(s);
	
	closesocket(s);
	printf("thread completed.");

	return 0;

}

void str_cli(FILE *fp, SOCKET s)
{
	char sendline[DEFAULT_BUFLEN];
	
	while (fgets(sendline, DEFAULT_BUFLEN, fp) != NULL)
	{
		std::string message;

		
		std::string temp = std::string(sendline);
		temp = temp.substr(0, temp.length() - 1);

		Final::Commands command;

		size_t space = temp.find(' ');
		command.set_command(temp.substr(0, space));


		if (space != std::string::npos)
		{
			if (command.mutable_command()->compare("login") == 0)
			{

				size_t middleBegin = temp.find(' ') + 1;
				size_t middleEnd = temp.find(' ', middleBegin);
				size_t lastPart = temp.find('\n');

				command.set_name(temp.substr(middleBegin, middleEnd - middleBegin));
				command.set_password(temp.substr(middleEnd + 1, lastPart - middleEnd));


				command.SerializeToString(&message);

				strncpy_s(sendline, message.c_str(), DEFAULT_BUFLEN);

				send(s, sendline, strlen(sendline), 0);
			}
			else
			{
				command.set_message(temp.substr(temp.find(' ') + 1));

				command.SerializeToString(&message);

				strncpy_s(sendline, message.c_str(), DEFAULT_BUFLEN);
				send(s, sendline, strlen(sendline), 0);
			}
		}
		else
		{
			command.SerializeToString(&message);
			strncpy_s(sendline, message.c_str(), DEFAULT_BUFLEN);
			send(s, sendline, strlen(sendline), 0);
		}
		

	}
}

int _tmain(int argc, _TCHAR *argv[])
{
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	WSADATA wsaData;
	int iResult;

	if (argc != 2)
	{
		printf("usage: DaytimeClient <ip address>");
		exit(0);
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	SOCKET sockfd;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("socket error");
		exit(0);
	}

	struct sockaddr_in servaddr;
	ZeroMemory(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(13);
	iResult = InetPton(AF_INET, argv[1], &servaddr.sin_addr);

	if (iResult == 0)
	{
		printf("invalid string that is not dotted IPv4 address.");
		exit(0);
	}
	if (iResult <= 0)
	{
		printf("failed with error: %ld\n", WSAGetLastError());
		exit(0);
	}

	iResult = connect(sockfd, (sockaddr *)&servaddr, sizeof(servaddr));

	hThreadArray[numThreads] = CreateThread(

		NULL,                   // default security attributes

		0,                      // use default stack size  

		MyThreadFunction,       // thread function name

		(LPVOID)sockfd,   // argument to thread function 

		0,                      // use default creation flags 

		NULL);   // returns the thread identifier 

	numThreads++;
	
	str_cli(stdin, sockfd);


	

	//char recvbuf[DEFAULT_BUFLEN];
	//int recvbuflen = DEFAULT_BUFLEN;
	//memset(recvbuf, 0, sizeof(char) * DEFAULT_BUFLEN);
	//do
	//{
	//	iResult = recv(sockfd, recvbuf, recvbuflen, 0);
	//	if (iResult > 0)
	//	{
	//		// print out the returned string here
	//		std::cout << recvbuf;
	//	}
	//	else if (iResult == 0)
	//		printf("Connection closed\n");
	//	else
	//		printf("recv failed with error: %d\n", WSAGetLastError());
	//} while (iResult > 0);

	// cleanup
	closesocket(sockfd);
	WSACleanup();

	_getch(); // waits until key is pressed
	return 0;
}

