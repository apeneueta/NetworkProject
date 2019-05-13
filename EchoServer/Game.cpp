#include "Game.h"

Game::Game(Final::Account * challenger, Final::Account * challenged)
	: m_pChallenger(challenger)
	, m_pChallenged(challenged)
	, m_challengerInit(false)
	, m_challengedInit(false)
	, m_play1Chosen(false)
	, m_play2Chosen(false)
	, m_promptForInput(true)
	, m_play1Choice(0)
	, m_play2Choice(0)
{
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0)
	{
		printf("WSAStartup failed with error: %d\n", iResult);	
	}

	MessageToClient((SOCKET)challenger->id(), buf, "\n     Please Enter the number of Units.\nInfantry, Archers, and Cavalry -Add up to 100\n\t(Example: 33 33 34)\n");
	MessageToClient((SOCKET)challenged->id(), buf, "\n     Please Enter the number of Units.\nInfantry, Archers, and Cavalry -Add up to 100\n\t(Example: 33 33 34)\n");

}

Game::~Game()
{
	delete m_pChallenged;
	delete m_pChallenger;

	WSACleanup();
}

bool Game::CheckAccount(Final::Account * account)
{
	if (account->name().compare(m_pChallenger->name()) == 0)
		return true;
	else if (account->name().compare(m_pChallenged->name()) == 0)
		return true;
	else
		return false;
}

bool Game::UpdateGame(Final::Commands command, Final::Account * account)
{
	// Initialize Players units
	{
	
	// If challenger unitialized
		if (!m_challengerInit && (account->name().compare(m_pChallenger->name()) == 0))
		{
			InitPlayer(command, account, &m_challengerInit);
			if (m_challengerInit && m_challengedInit)
			{
				MessageToClient((SOCKET)m_pChallenged->id(), buf,
					"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");
				MessageToClient((SOCKET)m_pChallenger->id(), buf,
					"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");
				return false;
			}
			return false;
		}
		// If challenged unitialized
		if (!m_challengedInit && (account->name().compare(m_pChallenged->name()) == 0))
		{
			InitPlayer(command, account, &m_challengedInit);
			if (m_challengerInit && m_challengedInit)
			{
				MessageToClient((SOCKET)m_pChallenged->id(), buf,
					"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");
				MessageToClient((SOCKET)m_pChallenger->id(), buf,
					"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");
				return false;
			}

			return false;
		}
		// If other player has not initialized units
		if (!m_challengerInit || !m_challengedInit)
		{
			MessageToClient((SOCKET)account->id(), buf, "\nYour Opponent has not chosen their units\n");
			return false;
		}
	}

	// Check for player choices
	{
		// If challenger unitialized
		if (!m_play1Chosen && (account->name().compare(m_pChallenger->name()) == 0))
		{
			InitPlayerChoice(command, account, &m_play1Chosen, &m_play1Choice);
			if (m_play1Chosen && m_play2Chosen)
			{
				MessageToClient((SOCKET)m_pChallenged->id(), buf,
					"\nBeginning Round\n");
				MessageToClient((SOCKET)m_pChallenger->id(), buf,
					"\nBeginning Round\n");
			}
			else
			{
				return false;
			}
		}
		// If challenged unitialized
		if (!m_play2Chosen && (account->name().compare(m_pChallenged->name()) == 0))
		{
			InitPlayerChoice(command, account, &m_play2Chosen, &m_play2Choice);
			if (m_play1Chosen && m_play2Chosen)
			{
				MessageToClient((SOCKET)m_pChallenged->id(), buf,
					"\nBeginning Round\n");
				MessageToClient((SOCKET)m_pChallenger->id(), buf,
					"\nBeginning Round\n");
			}
			else
			{
				return false;
			}
		}
		// If other player has not initialized units
		if (!m_play1Chosen || !m_play2Chosen)
		{
			MessageToClient((SOCKET)account->id(), buf, "\nYour Opponent has not made their move\n");
			return false;
		}
	}

	//Start game
	{
		
		//Run game
		PlayRound();

		//Check for winner
		if (CheckForWinner())
		{
			m_pChallenged->clear_player();
			m_pChallenger->clear_player();
			return true;
		}
		else
		{
			// Show players how many units they have left
			UnitsMessage(m_pChallenged);
			UnitsMessage(m_pChallenger);

			// Prompt players
			MessageToClient((SOCKET)m_pChallenged->id(), buf,
				"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");
			MessageToClient((SOCKET)m_pChallenger->id(), buf,
				"\n\tChoose the number of your action\n1: archers, 2: infantry, 3: cavalry, 4: surrender\n");

			// Reset player choices
			m_play2Chosen = false;
			m_play1Chosen = false;

		}

		return false;		
	}

	// This should not be hit
	return false;
}

void Game::MessageToClient(SOCKET s, char* buf, std::string output)
{
	Final::Commands message;
	std::string data;
	message.set_message(output);
	message.SerializeToString(&data);
	strncpy_s(buf, DEFAULT_BUFLEN, data.c_str(), DEFAULT_BUFLEN);

	send(s, buf, strlen(buf), 0);
	memset(buf, 0, sizeof(char) * DEFAULT_BUFLEN);
}

void Game::InitPlayer(Final::Commands command, Final::Account * account, bool* isInit)
{
	int archers = atoi(command.command().c_str());

	std::string message = command.message();
	//message = message.substr(0, message.length() - 1);

	size_t space = message.find(' ');
	size_t middleBegin = message.find(' ') + 1;
	//size_t middleEnd = message.find(' ', middleBegin);

	int infantry = atoi(message.substr(0,space).c_str());
	int cavalry = atoi(message.substr(middleBegin).c_str());

	int total = archers + infantry + cavalry;

	if (total <= 100)
	{
		Final::Player* player = account->mutable_player();

		player->add_units(archers);
		player->add_units(infantry);
		player->add_units(cavalry);

		*isInit = true;

		/*std::string line1 = "\nYou have these units\n";

		std::string line2 = "Archers: " + std::to_string(account->player().units(0)) +
			", Infantry: " + std::to_string(account->player().units(1)) +
			", Cavalry: " + std::to_string(account->player().units(2));

		std::string line3 = "\n";
		MessageToClient((SOCKET)account->id(), buf, line1 + line2 + line3);*/

		UnitsMessage(account);
	}
	else
	{
		MessageToClient((SOCKET)account->id(), buf, "\nThe number of units needs to be under 100\n");
	}
}

void Game::InitPlayerChoice(Final::Commands command, Final::Account * account, bool * isInit, int * choice)
{
	*choice = atoi(command.command().c_str());

	if (*choice >= 1 && *choice <= 4)
	{
		if (*choice == 4)
		{
			*isInit = true;
			return;
		}
		if (account->player().units(*choice - 1) <= 0)
		{
			MessageToClient((SOCKET)account->id(), buf, "\nYou have no more units of this type.\n");
			return;
		}

		*isInit = true;
	}
	else
	{
		MessageToClient((SOCKET)account->id(), buf, "\nInvalid Choice\n");
	}
}

void Game::UnitsMessage(Final::Account * account)
{
	std::string line1 = "\nYou have these units\n";

	std::string line2 = "Archers: " + std::to_string(account->player().units(0)) +
		", Infantry: " + std::to_string(account->player().units(1)) +
		", Cavalry: " + std::to_string(account->player().units(2));

	std::string line3 = "\n";
	MessageToClient((SOCKET)account->id(), buf, line1 + line2 + line3);
}

bool Game::CheckForWinner()
{
	bool p1NoUnits = CheckUnits(m_pChallenger);
	bool p2NoUnits = CheckUnits(m_pChallenged);

	// Both surrender Nobody wins Nobody loses game over
	if (m_play1Choice == 4 && m_play2Choice == 4)
	{
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou both surrendered. Nobody Wins.\n");
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou both surrendered. Nobody Wins.\n");
		return true;
	}

	// Handling draw Nobody wins Nobody loses game over
	if (p1NoUnits && p2NoUnits)
	{
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou both have no more units. Nobody Wins.\n");
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou both have no more units. Nobody Wins.\n");
		return true;
	}

	// Handling if someone surrenders
	if (m_play1Choice == 4)
	{
		//Challenged wins
		m_pChallenged->set_wins(m_pChallenged->wins() + 1);

		m_pChallenged->add_gamehist();
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_oponent(m_pChallenger->name());
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_victory(true);

		//Challenger loses
		m_pChallenger->set_loses(m_pChallenger->loses() + 1);
		m_pChallenger->add_gamehist();
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_oponent(m_pChallenged->name());
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_victory(false);


		//Victory Message
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou Win. Game Over.\n");
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou Lose. Game OVer.\n");
		return true;
	}
	if (m_play2Choice == 4)
	{
		//Challenger wins
		m_pChallenger->set_wins(m_pChallenger->wins() + 1);
					
		m_pChallenger->add_gamehist();
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_oponent(m_pChallenged->name());
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_victory(true);

		//Challenged loses
		m_pChallenged->set_loses(m_pChallenged->loses() + 1);
		m_pChallenged->add_gamehist();
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_oponent(m_pChallenger->name());
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_victory(false);
		
		//Victory Message
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou Win. Game Over.\n");
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou Lose. Game OVer.\n");
		return true;
	}

	if (p1NoUnits)
	{
		//Challenged wins
		m_pChallenged->set_wins(m_pChallenged->wins() + 1);

		m_pChallenged->add_gamehist();
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_oponent(m_pChallenger->name());
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_victory(true);

		//Challenger loses
		m_pChallenger->set_loses(m_pChallenger->loses() + 1);
		m_pChallenger->add_gamehist();
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_oponent(m_pChallenged->name());
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_victory(false);
		
		//Victory Message
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou Win. Game Over.\n");
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou Lose. Game OVer.\n");
		return true;
	}
	if (p2NoUnits)
	{
		//Challenger wins
		m_pChallenger->set_wins(m_pChallenger->wins() + 1);

		m_pChallenger->add_gamehist();
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_oponent(m_pChallenged->name());
		m_pChallenger->mutable_gamehist(m_pChallenger->gamehist_size() - 1)->set_victory(true);

		//Challenged loses
		m_pChallenged->set_loses(m_pChallenged->loses() + 1);
		m_pChallenged->add_gamehist();
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_oponent(m_pChallenger->name());
		m_pChallenged->mutable_gamehist(m_pChallenged->gamehist_size() - 1)->set_victory(false);
		
		//Victory Message
		MessageToClient((SOCKET)m_pChallenger->id(), buf,
			"\nYou Win. Game Over.\n");
		MessageToClient((SOCKET)m_pChallenged->id(), buf,
			"\nYou Lose. Game OVer.\n");
		return true;
	}

	return false;
}

bool Game::CheckUnits(Final::Account * account)
{
	for (int i = 0; i < 3; ++i)
	{
		if (account->player().units(i) > 0)
			return false;
	}
	return true;
}

void Game::PlayRound()
{
	std::string unitType;
	int damage = 10;
	int hiDamage = 12;
	int loDamage = 7;

	// Player 2 is the m_challenged
	switch (m_play2Choice)
	{
		case 1:
		{
			unitType = "Archers";
			break;
		}
		case 2:
		{
			unitType = "Infantry";
			break;
		}
		case 3:
		{
			unitType = "Cavalry";
			break;
		}
		case 4:
		{
			unitType = "Surrender";
			break;
		}
	}
	MessageToClient((SOCKET)m_pChallenged->id(), buf,
		"\n\tUser " + m_pChallenged->name() + " Chose: " + unitType);
	MessageToClient((SOCKET)m_pChallenger->id(), buf,
		"\n\tUser " + m_pChallenged->name() + " Chose: " + unitType);

	// Player 1 is the m_challenger
	switch (m_play1Choice)
	{
		case 1:
		{
			unitType = "Archers";
			break;
		}
		case 2:
		{
			unitType = "Infantry";
			break;
		}
		case 3:
		{
			unitType = "Cavalry";
			break;
		}
		case 4:
		{
			unitType = "Surrender";
			break;
		}
	}

	MessageToClient((SOCKET)m_pChallenged->id(), buf,
		"\n\tUser " + m_pChallenger->name() + "Chose: " + unitType);
	MessageToClient((SOCKET)m_pChallenger->id(), buf,
		"\n\tUser " + m_pChallenger->name() + "Chose: " + unitType);

	// Someone surrendered
	if (m_play1Choice == 4 || m_play2Choice == 4)
		return;

	// Players chose the same unitstypes
	if (m_play1Choice == m_play2Choice)
	{
		//same choice
		int newValue = m_pChallenged->player().units(m_play1Choice - 1) - damage;
		if (newValue < 0)
			newValue = 0;

		m_pChallenged->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);

		newValue = m_pChallenger->player().units(m_play1Choice - 1) - damage;
		m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);

		return;
	}

	if (m_play1Choice == 1)
	{
		if (m_play2Choice == 2)
		{
			//Archery better than infantry
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);
	
			newValue = m_pChallenger->player().units(m_play1Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		if (m_play2Choice == 3)
		{
			//Archery worse than cavalry
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);

			newValue = m_pChallenger->player().units(m_play1Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		return;
	}

	if (m_play1Choice == 2)
	{
		if (m_play2Choice == 3)
		{
			//Infantry better than Cavalry
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);

			newValue = m_pChallenger->player().units(m_play1Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		if (m_play2Choice == 1)
		{
			//Infantry worse than Archery
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);

			newValue = m_pChallenger->player().units(m_play1Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		return;
	}

	if (m_play1Choice == 3)
	{
		if (m_play2Choice == 1)
		{
			//cavalry better than archery
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);

			newValue = m_pChallenger->player().units(m_play1Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		if (m_play2Choice == 2)
		{
			//cavalry worse than infantry
			int newValue = m_pChallenged->player().units(m_play2Choice - 1) - loDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenged->mutable_player()->mutable_units()->Set(m_play2Choice - 1, newValue);

			newValue = m_pChallenger->player().units(m_play1Choice - 1) - hiDamage;
			if (newValue < 0)
				newValue = 0;
			m_pChallenger->mutable_player()->mutable_units()->Set(m_play1Choice - 1, newValue);
		}
		return;
	}

}
