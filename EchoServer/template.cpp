// Echo server : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <conio.h>
#include <google/protobuf/util/time_util.h>
#include <unordered_map>

#include "Game.h"
#include "Final.pb.h"


#define MAX_BUFF 512
#define MAX_THREADS 32
#define DEFAULT_BUFLEN 512
HANDLE  hThreadArray[MAX_THREADS];
int numThreads = 0;

void str_echo(SOCKET s)

{
	// homework assignment - write code here to read from socket and send back string
	int n;
	char buff[MAX_BUFF];
	memset(buff, 0, sizeof(char) * MAX_BUFF);
	while ((n = recv(s, buff, MAX_BUFF, 0)) > 0)
	{
		send(s, buff, n, 0);

		/*if (n < 0 && errno == EINTR)
		{

		}
		else if (n < 0)
		{
			printf("str_echo: Read Error");
		}*/
	}	
}

// used to create users 0 - 9
void CreateAccount(Final::Account* account, int set)
{
	*account->mutable_name() = std::to_string(set);
	*account->mutable_password() = std::to_string(set);
}

// used to create user accounts the are more personalized
void CreateAccount(Final::Account* account, std::string username, std::string password)
{
	*account->mutable_name() = username;
	*account->mutable_password() = password;
}

Final::Account* FindAccount(int clientId, Final::Users* clients)
{
	for (int i = 0; i < clients->users_size(); ++i)
	{
		if (clientId == clients->mutable_users(i)->id())
		{
			return clients->mutable_users(i);
		}
	}

	return nullptr;
}

void MessageToClient(SOCKET s, char* buf, std::string output)
{
	Final::Commands message;
	std::string data;
	message.set_message(output);
	message.SerializeToString(&data);
	strncpy_s(buf, DEFAULT_BUFLEN, data.c_str(), DEFAULT_BUFLEN);

	send(s, buf, strlen(buf), 0);
}

bool Login(SOCKET sockfd, Final::Commands command, Final::Users* clients, char* buf, std::unordered_map<std::string, Final::Account*>* lobby)
{
	Final::Commands message;
	std::string data;

	for (int i = 0; i < clients->users_size(); ++i)
	{
		if (command.mutable_name()->compare(*clients->mutable_users(i)->mutable_name()) == 0)
		{
			if (clients->mutable_users(i)->loggedin())
			{
				MessageToClient(sockfd, buf, "That user is already logged in.");
				return false;
			}
			if (command.mutable_password()->compare(*clients->mutable_users(i)->mutable_password()) == 0)
			{
				clients->mutable_users(i)->set_id(sockfd);
				clients->mutable_users(i)->set_loggedin(true);

				//lobby[*command.mutable_name()] = std::move(account);
				lobby->insert({ *clients->mutable_users(i)->mutable_name(), clients->mutable_users(i) });

				MessageToClient(sockfd, buf, "You have successfully logged in.");
				return true;
			}
		}
	}
	
	MessageToClient(sockfd, buf, "invalid username or password");
	return false;
}

bool Chat(SOCKET sockfd, Final::Commands command,  char* buf, std::unordered_map<std::string, Final::Account*> lobby)
{
	Final::Commands message;
	std::string data;

	bool worked = false;

	std::unordered_map<std::string, Final::Account*>::iterator it = lobby.begin();

	while (it != lobby.end())
	{
		MessageToClient((SOCKET)it->second->id(), buf, "\n" + command.message());
		it++;
		worked = true;
	}
	

	if (!worked)
	{
		MessageToClient(sockfd, buf, "Error: failed to do lobby loop!");
	}
	return worked;

}

bool Challenge(SOCKET sockfd, Final::Commands command, std::unordered_map<std::string, Final::Account*>* lobby, Final::Users* clients, char* buf, std::vector<std::pair<Final::Account*, Final::Account*>>* challengePair)
{
	Final::Commands message;
	std::string data;

	for (int i = 0; i < clients->users_size(); ++i)
	{
		// find the challenger
		if ((int)sockfd == clients->mutable_users(i)->id())
		{
			auto challenger = lobby->find(clients->mutable_users(i)->name());
			if (challenger != lobby->end())
			{
				// find challengee
				auto challengee = lobby->find(command.message());
				if (challengee != lobby->end())
				{
					if (challenger->second->name().compare(challengee->second->name()) == 0)
					{
						MessageToClient(sockfd, buf, "\nYou cannot challenge yourself.");
						return false;
					}
					// Found client to challenge
					challengee->second->set_challenged(true);

					MessageToClient((SOCKET)challengee->second->id(), buf, "\nYou have been challenged by username " + challenger->second->name() + "\n Do You Accept this challenge");
					MessageToClient(sockfd, buf, "\nYou have Challenged username " + challengee->second->name() + "\n Waiting for response");

					auto pair = std::make_pair(challengee->second, challenger->second);
					challengePair->push_back(pair);

					lobby->erase(challengee);
					lobby->erase(challenger);

					return true;
					
				}
				else
				{
					MessageToClient(sockfd, buf, "\nThat user is not available for challenge.");
					return false;
				}
			}
		}	
	}

	MessageToClient(sockfd, buf, "\nError finding user!!");
	return false;
}

void ChallengeResponse(SOCKET s, char* buf, Final::Commands command, std::vector<std::pair<Final::Account*, Final::Account*>>* challengePair, std::vector<Game*>* gameSessions, std::unordered_map<std::string, Final::Account*>* lobby, int challengeIndex)
{

	if ((int)s == challengePair[0][challengeIndex].first->id())
	{
		if (challengePair[0][challengeIndex].first->challenged())
		{
			if (command.mutable_command()->compare("yes") == 0)
			{
				MessageToClient((SOCKET)challengePair[0][challengeIndex].first->id(), buf, "\nAccepted Challenge");
				MessageToClient((SOCKET)challengePair[0][challengeIndex].second->id(), buf, "\nChallenge Accepted");

				Game* game = new Game(challengePair[0][challengeIndex].first, challengePair[0][challengeIndex].second);
				gameSessions->push_back(game);
				challengePair[0].erase(challengePair[0].begin() + challengeIndex);
			}
			else if (command.mutable_command()->compare("no") == 0)
			{
				MessageToClient((SOCKET)challengePair[0][challengeIndex].first->id(), buf, "\nDeclined Challenge");
				MessageToClient((SOCKET)challengePair[0][challengeIndex].second->id(), buf, "\nChallenge Declined");

				lobby->insert({ *challengePair[0][challengeIndex].first->mutable_name(), challengePair[0][challengeIndex].first });
				lobby->insert({ *challengePair[0][challengeIndex].second->mutable_name(), challengePair[0][challengeIndex].second });
				challengePair[0].erase(challengePair[0].begin() + challengeIndex);
			}
			else
			{
				MessageToClient((SOCKET)challengePair[0][challengeIndex].first->id(), buf, "\nEnter yes or no");
			}
		}
	}
	else
	{
		MessageToClient((SOCKET)challengePair[0][challengeIndex].second->id(), buf, "Wait for challenge to be answered.");
	}
}

bool Info(SOCKET sockfd, Final::Commands command, Final::Users* clients, char* buf)
{
	Final::Commands message;
	std::string data;
	std::string didWin;


	for (int i = 0; i < clients->users_size(); ++i)
	{
		if (command.mutable_message()->compare(*clients->mutable_users(i)->mutable_name()) == 0)
		{
			if (clients->mutable_users(i)->gamehist_size() == 0)
			{
				MessageToClient(sockfd, buf, "\n User: " + command.message() +
					" has no match data");
			}

			for (int j = 0; j < clients->mutable_users(i)->gamehist_size(); ++j)
			{
				data = clients->mutable_users(i)->mutable_gamehist(j)->oponent();
				if (clients->mutable_users(i)->mutable_gamehist(j)->victory())
				{
					didWin = "Victory";
				}
				else
				{
					didWin = "Defeat";
				}
				
				MessageToClient(sockfd, buf, "\n User: " + command.message() + 
					" played user: " + data + " and the match ended in " + didWin + 
					" for user: " + command.message());
			}
			return true;
		}
	}

	MessageToClient(sockfd, buf, "\nThat account does not exist!!");
	return false;
}

bool List(SOCKET sockfd, std::unordered_map<std::string, Final::Account*> lobby, char* buf)
{
	Final::Commands message;
	std::string data;

	std::unordered_map<std::string, Final::Account*>::iterator it = lobby.begin();

	data.append("\nUsers in Lobby\n");

	while (it != lobby.end())
	{
		data.append("Username: " + it->second->name() + "\n");
		
		it++;
	}

	MessageToClient(sockfd, buf, data);
	return true;
}

// TODO: Partially completed, Finish
bool Logout(SOCKET sockfd, std::unordered_map<std::string, Final::Account*>* lobby, Final::Users* clients, char* buf)
{
	Final::Commands message;
	std::string data;

	for (int i = 0; i < clients->users_size(); ++i)
	{
		if ((int)sockfd == clients->mutable_users(i)->id())
		{
			auto search = lobby->find(clients->mutable_users(i)->name());
			if (search != lobby->end())
			{
				// Found client
				clients->mutable_users(i)->set_loggedin(false);
				clients->mutable_users(i)->set_id(0);

				MessageToClient(sockfd, buf, "\nSuccessfully logged out");
				lobby->erase(search);

				return true;
			}
			else
			{
				MessageToClient(sockfd, buf, "\nError finding user!!");
				return false;
			}
		}
	}

	MessageToClient(sockfd, buf, "\nError finding user!!");
	return false;
}

void EndGameState(Final::Account* account, std::unordered_map<std::string, Final::Account*>* lobby,
	std::vector<Game*>* gameSessions, char* buf)
{
	for (int i = 0; i < gameSessions[0].size(); ++i)
	{
		if (gameSessions[0][i]->CheckAccount(account))
		{
			
			MessageToClient((SOCKET)gameSessions[0][i]->GetPlayer2()->id(), buf, "\nMoving back to lobby.");
			MessageToClient((SOCKET)gameSessions[0][i]->GetPlayer1()->id(), buf, "\nMoving back to lobby.");
			lobby->insert({ *gameSessions[0][i]->GetPlayer2()->mutable_name(), gameSessions[0][i]->GetPlayer2() });
			lobby->insert({ *gameSessions[0][i]->GetPlayer1()->mutable_name(), gameSessions[0][i]->GetPlayer1() });
			gameSessions[0].erase(gameSessions[0].begin() + i);
			return;
			
		}
	}
}
void PlayerQuit(Final::Account* account, std::unordered_map<std::string, 
	Final::Account*>* lobby, Final::Users* clients, std::vector<std::pair<Final::Account*, 
	Final::Account*>>* challengePair, std::vector<Game*>* gameSessions, char* buf)
{
	// Finds out if this client is in a challenged state
	for (int i = 0; i < challengePair[0].size(); ++i)
	{
		if (account->name().compare(challengePair[0][i].first->name()) == 0)
		{
			MessageToClient((SOCKET)challengePair[0][i].second->id(), buf, "\nOpponent Quit. Moving back to lobby.");

			lobby->insert({ *challengePair[0][i].second->mutable_name(), challengePair[0][i].second });
			challengePair[0].erase(challengePair[0].begin() + i);

			account->set_loggedin(false);
			account->set_id(0);
			return;
		}
		if (account->name().compare(challengePair[0][i].second->name()) == 0)
		{
			MessageToClient((SOCKET)challengePair[0][i].first->id(), buf, "\nOpponent Quit. Moving back to lobby.");
			lobby->insert({ *challengePair[0][i].first->mutable_name(), challengePair[0][i].first });
			
			challengePair[0].erase(challengePair[0].begin() + i);
			account->set_loggedin(false);
			account->set_id(0);
			return;
		}
	}

	int gameIndex = 0;
	// Finds out if client is in game state
	for (int i = 0; i < gameSessions[0].size(); ++i)
	{
		if (gameSessions[0][i]->CheckAccount(account))
		{
			if (account->name().compare(gameSessions[0][i]->GetPlayer1()->name()) == 0)
			{
				MessageToClient((SOCKET)gameSessions[0][i]->GetPlayer2()->id(), buf, "\nOpponent Quit. Moving back to lobby.");
				lobby->insert({ *gameSessions[0][i]->GetPlayer2()->mutable_name(), gameSessions[0][i]->GetPlayer2() });
				gameSessions[0].erase(gameSessions[0].begin() + i);
				account->set_loggedin(false);
				account->set_id(0);
				return;
			}
			else
			{
				MessageToClient((SOCKET)gameSessions[0][i]->GetPlayer1()->id(), buf, "\nOpponent Quit. Moving back to lobby.");
				lobby->insert({ *gameSessions[0][i]->GetPlayer1()->mutable_name(), gameSessions[0][i]->GetPlayer1() });
				gameSessions[0].erase(gameSessions[0].begin() + i);
				account->set_loggedin(false);
				account->set_id(0);
				return;
			}
		}
	}
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)

{
	// Cast the parameter to the correct data type.
	// The pointer is known to be valid because 
	// it was checked for NULL before the thread was created.
	SOCKET s = (SOCKET)lpParam;
	str_echo(s);

	closesocket(s);
	printf("thread completed.");

	return 0;
}

int _tmain(int argc, _TCHAR *argv[])
{
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	WSADATA wsaData;
	int iResult;

	int i, maxi, maxfd;
	int nready, client[FD_SETSIZE];
	size_t n;
	fd_set rset, allset;
	char buf[DEFAULT_BUFLEN];
	socklen_t clilen;
	struct sockaddr_in cliaddr;


	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0)
	{
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	// open a TCP stream
	SOCKET sockfd;
	SOCKET listenfd;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("socket error");
		exit(0);
	}

	// setup sockaddr_in
	struct sockaddr_in servaddr;
	ZeroMemory(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(13);

	// bind to socket
	iResult = bind(listenfd, (SOCKADDR *)&servaddr, sizeof(servaddr));

	if (iResult == SOCKET_ERROR)
	{
		wprintf(L"bind failed with error %u\n", WSAGetLastError());
		closesocket(listenfd);
		WSACleanup();
		return 1;
	}

	// listen for incoming connection requests
	if (listen(listenfd, SOMAXCONN) == SOCKET_ERROR)
		wprintf(L"listen function failed with error: %d\n", WSAGetLastError());


	maxfd = iResult; // [alert]: May need to change this
	maxi = -1;

	for (i = 0; i < FD_SETSIZE; ++i)
	{
		client[i] = -1;
	}
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);

	Final::Users clients;

	// Reading in the the user information
	// If file not there create the user accounts
	std::fstream input("AccountInfo.txt", std::ios::in | std::ios::binary);
	if (!input) 
	{
		std::cout << "AccountInfo.txt" << ": File not found.  Creating a new file." << std::endl;
		for (int i = 0; i < 10; ++i)
		{
			CreateAccount(clients.add_users(), i);
		}
	}
	else if (!clients.ParseFromIstream(&input)) 
	{
		std::cerr << "Failed to parse user accounts." << std::endl;
		return -1;
	}
	
	{
		// Write the new address book back to disk.
		std::fstream output("AccountInfo.txt", std::ios::out | std::ios::trunc | std::ios::binary);
		if (!clients.SerializeToOstream(&output)) {
			std::cerr << "Failed to write Account Info file." << std::endl;
			return -1;
		}
	}

	std::unordered_map<std::string, Final::Account*> lobby;
	std::vector<std::pair<Final::Account*, Final::Account*>> challengePair;
	std::vector<Game*> gameSessions;

	SOCKET AcceptSocket;
	wprintf(L"Waiting for client to connect...\n");

	for (; ;)

	{
		// structure assignment
		rset = allset;
		nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

		// new client connection
		if (FD_ISSET(listenfd, &rset))
		{
			clilen = sizeof(cliaddr);
			AcceptSocket = accept(listenfd, (SOCKADDR *)&cliaddr, &clilen);
			if (AcceptSocket == INVALID_SOCKET)
			{
				wprintf(L"accept failed with error: %ld\n", WSAGetLastError());
				closesocket(listenfd);
				WSACleanup();
				return 1;
			}
			else
				wprintf(L"Client %d connected.\n", (maxi + 2));


			for (i = 0; i < FD_SETSIZE; ++i)
			{
				if (client[i] < 0)
				{
					// save descriptor
					client[i] = AcceptSocket;
					break;
				}
			}

			if (i == FD_SETSIZE)
			{
				printf("Too many clients");
				exit(0);
			}

			// add new descriptor to set
			FD_SET(AcceptSocket, &allset);
			if (AcceptSocket > maxfd)
			{
				// for select
				maxfd = AcceptSocket;
			}
			if (i > maxi)
			{
				// max index in client[] array
				maxi = i;
			}

			if (--nready <= 0)
			{
				// no more readable descriptors
				continue;
			}
		}

		for (i = 0; i <= maxi; i++)
		{
			// Check all clients for data
			Final::Account* account = FindAccount(client[i], &clients);

			bool challenged = false;
			int challengeIndex = 0;
			bool inGame = false;

			// Finds out if this client is in a challenged state
			for (int i = 0; i < challengePair.size(); ++i)
			{
				if (account->name().compare(challengePair[i].first->name()) == 0)
				{
					challenged = true;
					challengeIndex = i;
					break;
				}
				if (account->name().compare(challengePair[i].second->name()) == 0)
				{
					challenged = true;
					challengeIndex = i;
					break;
				}
			}

			int gameIndex = 0;
			// Finds out if client is in game state
			for (int i = 0; i < gameSessions.size(); ++i)
			{
				if (gameSessions[i]->CheckAccount(account))
				{
					inGame = true;
					gameIndex = i;
					break;
				}
			}


			if ((sockfd = client[i]) < 0)
			{
				continue;
			}
			if (FD_ISSET(sockfd, &rset))
			{
				Final::Commands command;
				std::string data(buf);
				command.ParseFromString(data);
				n = recv(sockfd, buf, DEFAULT_BUFLEN, 0);
				if (n == 0 || n == -1)
				{
					closesocket(sockfd);
					FD_CLR(sockfd, &allset);
					PlayerQuit(account, &lobby, &clients, &challengePair, &gameSessions, buf);
					client[i] = -1;
					//wprintf(L"Client %d connected.\n", (i + 1));
				}
				else
				{
					Final::Commands command;
					std::string data(buf);
					command.ParseFromString(data);

					// Both values of the pair need to have value
					if (challenged)// if challenged
					{
						ChallengeResponse(sockfd, buf, command, &challengePair, &gameSessions, &lobby, challengeIndex);
					}
					else if (inGame)// if in game
					{
						if (gameSessions[gameIndex]->UpdateGame(command, account))
							EndGameState(account, &lobby, &gameSessions, buf);
						//MessageToClient(sockfd, buf, "You are in Game.");
					}
					else // lobby commands
					{
						if (command.mutable_command()->compare("login") == 0)
						{
							Login(sockfd, command, &clients, buf, &lobby);
							//lobby.insert({ *command.mutable_name(), account });
						}
						else if (command.mutable_command()->compare("chat") == 0)
						{
							Chat(sockfd, command, buf, lobby);
						}
						else if (command.mutable_command()->compare("challenge") == 0)
						{
							Challenge(sockfd, command, &lobby, &clients, buf, &challengePair);
						}
						else if (command.mutable_command()->compare("info") == 0)
						{
							Info(sockfd, command, &clients, buf);
						}
						else if (command.mutable_command()->compare("list") == 0)
						{
							List(sockfd, lobby, buf);
						}
						else if (command.mutable_command()->compare("logout") == 0)
						{
							Logout(sockfd, &lobby, &clients, buf);
							// Write the new AccountInfo back to disk.
							std::fstream output("AccountInfo.txt", std::ios::out | std::ios::trunc | std::ios::binary);
							if (!clients.SerializeToOstream(&output)) {
								std::cerr << "Failed to write address book." << std::endl;
								return -1;
							}
						}
						else
						{
							MessageToClient(sockfd, buf, "\nInvalid Command");
						}
					}
				}

				if (--nready <= 0)
					break;  // no more readable descriptors
			}
		}
	}

	{
		// Write the new AccountInfo back to disk.
		std::fstream output("AccountInfo.txt", std::ios::out | std::ios::trunc | std::ios::binary);
		if (!clients.SerializeToOstream(&output)) {
			std::cerr << "Failed to write address book." << std::endl;
			return -1;
		}
	}

	// Optional:  Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();
	// cleanup
	//closesocket(sockfd);	
	WSACleanup();

	printf("completed.");
	_getch();   // waits until key is pressed

	return 0;
}