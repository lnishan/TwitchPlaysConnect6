#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstring>
#include <algorithm>
#define AI_COORDS 19
#define AI_CONNECT 6
#define AI_BLACK 1
#define AI_WHITE 2
#define AI_MAXCONNECT 100
#define AI_MAXTRY 10000

using namespace std;

extern double riskFactor[AI_MAXCONNECT];
extern double riskWeight[AI_MAXCONNECT];
extern int mX[8];
extern int mY[8];
extern int mX4[4];
extern int mY4[4];
extern double thresDef;
extern double thresAtk;
extern char vst[AI_COORDS+1][AI_COORDS+1];

bool better(const int &, const int &, const int &, const int &);

extern int riskX, riskY;
extern double riskMax;
double dfs(short b[AI_COORDS+1][AI_COORDS+1], int d, const int &dLmt, const short &colorOppo);

extern int bestX, bestY;
void findBest(short b[AI_COORDS+1][AI_COORDS+1], short colorMy);

void myAI_init();

bool myAI_valid(int);
double calRisk(short b[AI_COORDS+1][AI_COORDS+1], short colorOppo);

void myAI(short b[AI_COORDS+1][AI_COORDS+1], short color, int &X, int &Y);