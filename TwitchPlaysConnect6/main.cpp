#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <queue>
#include <vector>
#include <algorithm>
#include <string>
#include "TwitchPlaysAPI.h"
#include "FingRRR.h"
#include "myConnectAI.h"

#define CONNECT 6

#define COORDS 19
#define GRIDS 18

#define C_BLACK 1
#define C_WHITE 2
#define OPPO(X) 3 - X

#define C_INP_TWI 0
#define C_INP_AI 1
#define C_INP_MOU 2

#define C_TWI_LATENCY 20000 // ms
#define C_TWI_DECIDE 10000 // ms

using namespace std;

TwitchPlays *tClientPtr;

int P1 = C_INP_MOU, P2 = C_INP_AI;
bool output = !(P1 == C_INP_AI && P2 == C_INP_AI);

const double oriX = 99.0f, oriY = 165.0f;
const double gridSize = 800.0f / GRIDS;
const double chessSize = 40.0f;
int cX[COORDS+1], cY[COORDS+1];

short board[COORDS+1][COORDS+1];
iObject * chess[COORDS+1][COORDS+1];
short turn;
int remainGrids;
bool acceptTwitchInput = false;
clock_t latencyClk, decideClk;
const int mx[8] = {0, -1, -1, -1, 1, 1, 1, 0};
const int my[8] = {1, 1, 0, -1, 1, 0, -1, -1};
int mouseUpX, mouseUpY;

void (*funcAI)(short [COORDS+1][COORDS+1], short, int &, int &);

struct coord {
	int X, Y;
	coord(int X, int Y): X(X), Y(Y){}
	bool operator < (const coord &rhs)const
	{
		if (X != rhs.X)
			return X < rhs.X;
		else
			return Y < rhs.Y;
	}
};
vector<coord> twiInpBuf;

iScene *scene;
iObject *objBoard;

bool valid(const int &n)
{
	return n >= 1 && n <= COORDS;
}

bool valid(const int &X, const int &Y)
{
	return X >= 1 && X <= COORDS &&
		Y >= 1 && Y <= COORDS;
}

void genCoord()
{
	int i, j;
	for (i = 1; i <= COORDS; i++)
	{
		cX[i] = oriX + gridSize * (i - 1) - (chessSize / 2);
		cY[i] = oriY + gridSize * (i - 1) - (chessSize / 2);
	}
}

string getInpName(int flag)
{
	switch (flag)
	{
		case C_INP_TWI:
			return "Twitch";
			break;
		case C_INP_AI:
			return "AI";
			break;
		case C_INP_MOU:
			return "Mouse";
			break;
	}
	return "";
}

string getClrName(short color)
{
	switch (color)
	{
		case C_BLACK:
			return "Black";
			break;
		case C_WHITE:
			return "White";
			break;
	}
}

string getCoord(int X, int Y)
{
	if (X > 9)
	{
		char ret[4] = {Y + 'A' - 1, X / 10 + '0', X % 10 + '0', '\0'};
		return string(ret);
	}
	else
	{
		char ret[3] = {Y + 'A' - 1, X + '0', '\0'};
		return string(ret);
	}
}

void findClosest(int &X, int &Y, int x, int y) {
	int i, j;
	X = round((x - oriX) / gridSize) + 1;
	Y = round((y - oriY) / gridSize) + 1;
	if (!valid(X, Y))
		X = Y = -1;
}

void onUp(iObject *th, int &x, int &y) {
	// printf("*** You mouse-up'd (%d, %d)\n", x, y);
	findClosest(mouseUpX, mouseUpY, x, y);
	// printf("*** Verdict (X, Y) = (%d, %d)\n", mouseUpX, mouseUpY);
}

void initBoard()
{
	genCoord();
	memset(board, 0, sizeof(board));
	memset(chess, 0, sizeof(chess));
	latencyClk = C_TWI_LATENCY * CLOCKS_PER_SEC / 1000;
	decideClk = C_TWI_DECIDE * CLOCKS_PER_SEC / 1000;
	funcAI = NULL;
	srand(time(0));
}

void clearBoard()
{
	int i, j;
	for (i = 1; i <= COORDS; i++)
		for (j = 1; j <= COORDS; j++)
			if (chess[i][j] != NULL)
				scene->deleteObject(chess[i][j]);
}

void place(int X, int Y, short color)
{
	int &x = cX[X], &y = cY[Y];
	if (board[X][Y] == 0)
	{
		board[X][Y] = color;
		chess[X][Y] = scene->createObject(x, y, chessSize, chessSize, 0, 0, 0, 0, &assetsImg[color], 255, true, false);
	}
}

void getRandomCoord(int &X, int &Y)
{
	int rX, rY;
	do
	{
		// ( 0 ~ COORDS-1 ) + 1
		rX = (rand() % COORDS) + 1;
		rY = (rand() % COORDS) + 1;
	} while (board[rX][rY] != 0);
	X = rX;
	Y = rY;
}

void hookAI( void (*AI)(short [COORDS+1][COORDS+1], short, int &, int &) )
{
	funcAI = AI;
}

void getInput(int flag, int &X, int &Y)
{
	switch (flag)
	{
		case C_INP_TWI:
		{
			twiInpBuf.clear();
			
			clock_t curT = clock();
			clock_t &sigStT = curT, sigEdT = sigStT + decideClk;
			
			puts("[Screen] Green -- to be shown after C_TWI_LATENCY ms"); // Green Signal
			clock_t inpStT = curT + latencyClk, inpEdT = inpStT + decideClk;
			int cnt = 0;

			bool pSigEd = false, pInpSt = false, pInpEd = false;

			while (cnt < 3)
			{
				curT = clock();
				if (curT >= sigEdT)
				{
					if (!pSigEd)
					{
						
						puts("[Screen] Red -- to be shown after C_TWI_LATENCY ms"); // Red Signal
						pSigEd = true;
						++cnt;
					}
				}
				if (curT >= inpStT)
				{
					acceptTwitchInput = true;
					if (!pInpSt)
					{
						puts("[Internal] Input Start");
						char *msg = new char[100];
						sprintf(msg, "Your turn. You have %.2f seconds.", C_TWI_DECIDE / 1000.0f);
						if (output) tClientPtr -> sendMessage( string(msg) );
						pInpSt = true;
						++cnt;
					}
				}
				if (curT >= inpEdT)
				{
					acceptTwitchInput = false;
					if (!pInpEd)
					{
						puts("[Internal] Input End");
						if (output) tClientPtr -> sendMessage("End of your turn. Please stop typing.");
						pInpEd = true;
						++cnt;
					}
				}
				waitKey(10);
			}

			if (twiInpBuf.size() > 0)
			{
				int i, j, sz = twiInpBuf.size(), tCnt, maxCnt = 0;
				sort(twiInpBuf.begin(), twiInpBuf.end());
				for (i = 0; i < sz; i++)
				{
					coord &inp = twiInpBuf[i];
					tCnt = 1;
					for (j = i+1; j < sz; j++)
					{
						coord &inpNxt = twiInpBuf[j];
						if (inp.X == inpNxt.X && inp.Y == inpNxt.Y)
							tCnt++;
						else
							break;
					}
					if (tCnt > maxCnt)
					{
						maxCnt = tCnt;
						X = inp.X;
						Y = inp.Y;
					}
					i = j-1;
				}
			}
			else
				getRandomCoord(X, Y);
			break;
		}
		case C_INP_AI:
		{
			if (funcAI != NULL)
				(*funcAI)(board, turn, X, Y);
			else
				getRandomCoord(X, Y);
			break;
		}
		case C_INP_MOU:
		{
			mouseUpX = mouseUpY = -1;
			while (mouseUpX == -1 /* && mouseUpY == -1 */) 
				waitKey(10);
			X = mouseUpX;
			Y = mouseUpY;
			break;
		}
	}
	if ( !(valid(X) && valid(Y) && board[X][Y] == 0) ) getRandomCoord(X, Y);
}

int detState(int X, int Y)
{
	if (remainGrids == 0) return -1; // draw
	int i, iX, iY, cntL, cntR;
	for (i = 0; i < 4; i++)
	{
		cntL = cntR = 0;
		for (iX = X, iY = Y; valid(iX) && valid(iY) && board[iX][iY] == board[X][Y]; iX += mx[i], iY += my[i])
			cntL++;
		for (iX = X, iY = Y; valid(iX) && valid(iY) && board[iX][iY] == board[X][Y]; iX += mx[7 - i], iY += my[7 - i])
			cntR++;
		if (cntL + cntR - 1 >= CONNECT)
		{
			return board[X][Y]; // has won !
		}
	}
	return 0; // not yet
}

int black, white, draw;
void startGame()
{
	srand(time(0));
	memset(board, 0, sizeof(board));
	clearBoard();
	turn = C_BLACK;
	remainGrids = COORDS * COORDS;
	acceptTwitchInput = false;
	int inpMethod, inpX1, inpY1, inpX2, inpY2;
	int ret;
	string msg;

	msg = "Welcome to another round of Connect6 !";
	puts(msg.c_str());
	if (output) tClientPtr -> sendMessage(msg);

	inpMethod = (turn == C_BLACK) ? P1 : P2;
	getInput( inpMethod /* Player 1 = Twitch by default */, inpX1, inpY1);
	place(inpX1, inpY1, turn);
	msg = " - " + getClrName(turn) + " (" + getInpName(P1) + ") has chosen " + getCoord(inpX1, inpY1);
	cout << msg << endl;
	if (output) tClientPtr -> sendMessage(msg);

	turn = OPPO(turn); // 3 - turn;
	--remainGrids;

	while (remainGrids)
	{
		inpMethod = (turn == C_BLACK) ? P1 : P2;

		getInput( inpMethod /* Player 1 = Twitch by default, Player 2 = AI by default */, inpX1, inpY1);
		place(inpX1, inpY1, turn);
		getInput( inpMethod /* Player 1 = Twitch by default, Player 2 = AI by default */, inpX2, inpY2);
		place(inpX2, inpY2, turn);
		
		msg = " - " + getClrName(turn) + " (" + getInpName(inpMethod) + ") has chosen " + getCoord(inpX1, inpY1) + " and " + getCoord(inpX2, inpY2);
		cout << msg << endl;
		if (output) tClientPtr -> sendMessage(msg);

		turn = OPPO(turn); // 3 - turn;
		remainGrids -= 2;
		ret = detState(inpX1, inpY1);
		ret = max(ret, detState(inpX2, inpY2));
		if (ret != 0) break;

		waitKey(10);
	}
	if (remainGrids == 0) ret = -1;

	// string msg
	switch (ret)
	{
		case C_BLACK:
			msg = "Black (" + getInpName(P1) + ") has won !";
			++black;
			break;
		case C_WHITE:
			msg = "White (" + getInpName(P2) + ") has won !";
			++white;
			break;
		case -1:
			msg = "Draw !";
			++draw;
			break;
	}
	puts(msg.c_str());
	if (output) tClientPtr -> sendMessage(msg);

	if (P1 == C_INP_TWI || P2 == C_INP_TWI)
		waitKey(C_TWI_LATENCY);
}


void event()
{
	black = white = draw = 0;
	while (1)
	{
		startGame();

		string msg = "Game has ended. Restarting in 10 seconds.";
		puts(msg.c_str());
		if (output) tClientPtr -> sendMessage(msg);

		waitKey(10000);
	}
	FILE *fo = fopen("result.txt", "w");
	fprintf(fo, "(Black, White, Draw) = (%d, %d, %d)\n", black, white, draw);
}

// short color = 1;
void handleRaw(string caller, string text)
{
	cout << caller << ": " << text << endl;
	if (!acceptTwitchInput) return ;
	if (text.length() >= 2)
	{
		const char *txt = text.c_str();
		int X = atoi(&txt[1]);
		int Y = toupper(txt[0]) - 'A' + 1;
		if (valid(X) && valid(Y))
			twiInpBuf.push_back( coord(X, Y) );
	}
}

int main()
{
	tClientPtr = new TwitchPlays("TwitchPlays.cfg");
	TwitchPlays &tClient = (*tClientPtr);

	inputMethod = INPUT_METHOD_MOUSE;
	init(1000, 1000, false);

	// P1 = C_INP_TWI;
	// P2 = C_INP_AI;
	initBoard();
	myAI_init();


	scene = new iScene(0, 0, 1000, 1000, 0, 0, 0, 0, &assetsImg[6]);
	objBoard = scene->createObject(63, 127, 872, 872, 0, 0, 0, 0, &assetsImg[0], 255, true, false);
	objBoard->onUp = onUp;
	sceneActive = scene;



	hookAI(myAI);

	tClient.hookRaw(handleRaw);
	if (P1 == C_INP_TWI || P2 == C_INP_TWI) tClient.start();

	hookUserEvent(event);
	start();

	return 0;
}