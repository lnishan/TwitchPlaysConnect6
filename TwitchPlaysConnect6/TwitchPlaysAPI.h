#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
#include <algorithm>
#include <signal.h>
#include "pthread.h"
#include "IRCClient.h"

using namespace std;

#ifndef _TWITCH_PLAYS
#define _TWITCH_PLAYS

class TwitchPlays;
class ConsoleCommandHandler;

class TwitchPlays
{
public:
	pthread_t thr_listener;

	IRCClient client;
	char *host;
	int port;
	string nick, user;
	string password;
	string channel;
	
	volatile bool running;

	void (*callbackRaw)(string, string);

	TwitchPlays(const char *filename = "TwitchPlays.cfg");
	bool start();
	bool stop();
	void hookRaw(void (*cbRaw)(string, string));
	void sendMessage(string msg);
};

class ConsoleCommandHandler
{
public:
    bool AddCommand(std::string name, int argCount, void (*handler)(std::string /*params*/, IRCClient* /*client*/));
    void ParseCommand(std::string command, IRCClient* client);

private:
    struct CommandEntry
    {
        int argCount;
        void (*handler)(std::string /*arguments*/, IRCClient* /*client*/);
    };

    std::map<std::string, CommandEntry> _commands;
};

void msgCommand(std::string arguments, IRCClient* client);
void joinCommand(std::string channel, IRCClient* client);
void partCommand(std::string channel, IRCClient* client);
void ctcpCommand(std::string arguments, IRCClient* client);
void signalHandler(int signal);
void * inputThread(void *client);

static ConsoleCommandHandler commandHandler;
static TwitchPlays *curTwitch;
void * TwitchListener(void *);

#endif