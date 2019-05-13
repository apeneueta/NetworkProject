#pragma once

#include "stdafx.h"

#include "Final.pb.h"

#define DEFAULT_BUFLEN 512

class Game
{
private:
	Final::Account* m_pChallenger;
	Final::Account* m_pChallenged;
	bool m_challengerInit;
	bool m_challengedInit;
	bool m_play1Chosen;
	bool m_play2Chosen;
	bool m_promptForInput;
	int m_play1Choice;
	int m_play2Choice;

	char buf[DEFAULT_BUFLEN];

	WSADATA wsaData;

	void MessageToClient(SOCKET s, char* buf, std::string output);
	void InitPlayer(Final::Commands command, Final::Account* account, bool* isInit);
	void InitPlayerChoice(Final::Commands command, Final::Account* account, bool* isInit, int* choice);
	void UnitsMessage(Final::Account * account);
	bool CheckForWinner();
	bool CheckUnits(Final::Account* account);
	void PlayRound();

public:
	Game(Final::Account* challenger, Final::Account* challenged);
	~Game();

	bool CheckAccount(Final::Account* account);
	bool UpdateGame(Final::Commands command, Final::Account* account);

	Final::Account* GetPlayer1() { return m_pChallenger; }
	Final::Account* GetPlayer2() { return m_pChallenged; }
};