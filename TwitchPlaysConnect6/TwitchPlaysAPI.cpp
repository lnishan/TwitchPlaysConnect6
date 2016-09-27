#include "TwitchPlaysAPI.h"

using namespace std;

TwitchPlays::TwitchPlays(const char *filename): callbackRaw(NULL)
{
	commandHandler.AddCommand("msg", 2, &msgCommand);
	commandHandler.AddCommand("join", 1, &joinCommand);
	commandHandler.AddCommand("part", 1, &partCommand);
	commandHandler.AddCommand("ctcp", 2, &ctcpCommand);

	FILE *fi = fopen(filename, "r");
	if (fi == NULL)
	{
		fprintf(stderr, "* Error: Cannot open config file: %s\n", filename);
		return ;
	}
	char s[200], par[200], val[200];
	int len;
	while (fgets(s, 200, fi))
	{
		len = strlen(s);
		if (len == 0) continue;
		if (s[len - 1] == '\n') s[len - 1] = '\0';
		sscanf(s, "%s = %s", par, val);
		if (strcmp(par, "host") == 0)
		{
			host = new char[len + 1];
			strcpy(host, val);
		}
		else if (strcmp(par, "port") == 0)
			port = atoi(val);
		else if (strcmp(par, "nick") == 0)
			nick = val;
		else if (strcmp(par, "user") == 0)
			user = val;
		else if (strcmp(par, "password") == 0)
			password = val;
		else if (strcmp(par, "channel") == 0)
		{
			int i;
			len = strlen(val);
			for (i = 0; i < len; i++)
				val[i] = isalpha(val[i]) ? tolower(val[i]) : val[i];
			channel = val;
		}
	}
	printf("Finished reading config file: %s\n", filename);
}

bool TwitchPlays::start()
{
	client.Debug(false);
	curTwitch = this;

	// Start the input thread
	// pthread_t thr_Inp;
	// pthread_create(&thr_Inp, NULL, inputThread, (void *)&client);

	int ret = pthread_create(&thr_listener, NULL, TwitchListener, (void *) this);

	if (ret == 0) puts("[TwitchPlaysAPI] Listener Thread Creation Successful");
	
	while (!running) ; // still loading
	return true;
}

bool TwitchPlays::stop()
{
	running = false;
	return true;
}

void TwitchPlays::hookRaw(void (*cbRaw)(string, string))
{
	callbackRaw = client.callbackRaw = cbRaw;
}

void TwitchPlays::sendMessage(string msg)
{
	client.SendIRC("PRIVMSG #" + channel + " :" + msg);
}

void * TwitchListener(void *arg)
{
	TwitchPlays *twi = (TwitchPlays *)arg;
	IRCClient &irc = twi->client;
	if (irc.InitSocket())
	{
		std::cout << "[TwitchPlaysAPI] Socket initialized. Connecting..." << std::endl;

		if (irc.Connect(twi->host, twi->port))
		{
			std::cout << "[TwitchPlaysAPI] Connected. Logging in..." << std::endl;

			if (irc.Login(twi->nick, twi->user, twi->password))
			{
				std::cout << "[TwitchPlaysAPI] Logged." << std::endl;

				twi->running = true;
				signal(SIGINT, signalHandler);

				irc.SendIRC("JOIN #" + twi->channel);

				while (irc.Connected() && twi->running)
					irc.ReceiveData();
			}

			if (irc.Connected())
				irc.Disconnect();

			;// std::cout << "Disconnected." << std::endl;
		}
	}
	return NULL;
}

void msgCommand(std::string arguments, IRCClient* client)
{
	std::string to = arguments.substr(0, arguments.find(" "));
	std::string text = arguments.substr(arguments.find(" ") + 1);

	std::cout << "To " + to + ": " + text << std::endl;
	client->SendIRC("PRIVMSG " + to + " :" + text);
};

void joinCommand(std::string channel, IRCClient* client)
{
	if (channel[0] != '#')
		channel = "#" + channel;

	client->SendIRC("JOIN " + channel);
}

void partCommand(std::string channel, IRCClient* client)
{
	if (channel[0] != '#')
		channel = "#" + channel;

	client->SendIRC("PART " + channel);
}

void ctcpCommand(std::string arguments, IRCClient* client)
{
	std::string to = arguments.substr(0, arguments.find(" "));
	std::string text = arguments.substr(arguments.find(" ") + 1);

	std::transform(text.begin(), text.end(), text.begin(), towupper);

	client->SendIRC("PRIVMSG " + to + " :\001" + text + "\001");
}

void signalHandler(int signal)
{
	curTwitch -> running = false;
}

void * inputThread(void *client)
{
	std::string command;
	while (true)
	{
		getline(std::cin, command);
		if (command == "")
			continue;

		if (command[0] == '/')
			commandHandler.ParseCommand(command, (IRCClient*)client);
		else
			((IRCClient*)client)->SendIRC(command);

		if (command == "quit")
			break;
	}

	pthread_exit(NULL);
	return NULL;
}

bool ConsoleCommandHandler::AddCommand(std::string name, int argCount, void(*handler)(std::string /*params*/, IRCClient* /*client*/))
{
	CommandEntry entry;
	entry.handler = handler;
	std::transform(name.begin(), name.end(), name.begin(), towlower);
	_commands.insert(std::pair<std::string, CommandEntry>(name, entry));
	return true;
}

void ConsoleCommandHandler::ParseCommand(std::string command, IRCClient* client)
{
	if (_commands.empty())
	{
		std::cout << "No commands available." << std::endl;
		return;
	}

	if (command[0] == '/')
		command = command.substr(1); // Remove the slash

	std::string name = command.substr(0, command.find(" "));
	std::string args = command.substr(command.find(" ") + 1);
	int argCount = std::count(args.begin(), args.end(), ' ');

	std::transform(name.begin(), name.end(), name.begin(), towlower);

	std::map<std::string, CommandEntry>::const_iterator itr = _commands.find(name);
	if (itr == _commands.end())
	{
		std::cout << "Command not found." << std::endl;
		return;
	}

	if (++argCount < itr->second.argCount)
	{
		std::cout << "Insuficient arguments." << std::endl;
		return;
	}

	(*(itr->second.handler))(args, client);
}