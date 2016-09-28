#include "myConnectAI.h"

using namespace std;

double riskFactor[AI_MAXCONNECT];
double riskWeight[AI_MAXCONNECT];
int mX[8] = {0, -1, -1, -1, 1, 1, 1, 0};
int mY[8] = {1, 1, 0, -1, 1, 0, -1, -1};
int mX4[4] = {1, -1, -1, 1};
int mY4[4] = {-1, -1, 1, 1};
double thresDef;
double thresAtk;
char vst[AI_COORDS+1][AI_COORDS+1];

int riskX, riskY;
double riskMax;

int bestX, bestY;

void myAI_init()
{
	int i;
	memset(vst, 0, sizeof(vst));

	riskFactor[0] = 1.0;
	riskFactor[1] = 1.0;
	for (i = 2; i <= AI_COORDS; i++)
		riskFactor[i] = riskFactor[i-1] + riskFactor[i-2];
	for ( ; i < AI_MAXCONNECT; i++)
		riskFactor[i] = riskFactor[AI_CONNECT];
	
	riskWeight[1] = 1.0;
	for (i = 2; i <= AI_COORDS; i++)
		riskWeight[i] = riskWeight[i-1] * riskFactor[i];
	for ( ; i < AI_MAXCONNECT; i++)
		riskWeight[i] = riskWeight[i-1] * riskFactor[i];
	
	// starts defending when a particular move constitutes a Connect-5 or eq. risk
	thresDef = riskWeight[AI_CONNECT - 1];

	// starts attacking when (1) no need to defend (2) attack is worthy, otherwise crabbing !
	thresAtk = (riskWeight[4] + riskWeight[5]) / 2.0f;
	
};

bool myAI_valid(int n)
{
	return n >= 1 && n <= AI_COORDS;
}

bool better(const int &X1, const int &Y1, const int &X2, const int &Y2)
{
	int midX = 1 + (AI_COORDS-1) / 2;
	int midY = 1 + (AI_COORDS-1) / 2;

	int d1 = abs(X1 - midX) + abs(Y1 - midY);
	int d2 = abs(X2 - midX) + abs(Y2 - midY);
	if (d1 > d2)
		return true;
	else if (d1 < d2)
		return false;
	else if (d1 == d2)
		return (rand() % 2);
}

double calRisk(short b[AI_COORDS+1][AI_COORDS+1], short colorOppo)
{
	int i, j, k, X, Y, cntL, cntR, cn, maxCn, lX, lY, rX, rY;
	bool isolate;
	double ret = 0;
	char v[AI_COORDS+1][AI_COORDS+1][8];

	memset(v, 0, sizeof(v));
	for (i = 1; i <= AI_COORDS; i++)
		for (j = 1; j <= AI_COORDS; j++)
			if (b[i][j] == colorOppo)
			{
				isolate = true;
				for (k = 0; k < 4; k++)
					if (!v[i][j][k])
					{
						// v[i][j][k] = v[i][j][7 - k] = 1;
						cntL = cntR = 0;
						for (X = i, Y = j; myAI_valid(X) && myAI_valid(Y) && b[X][Y] == b[i][j]; X += mX[k], Y += mY[k])
						{
							cntL++;
							v[X][Y][k] = v[X][Y][7-k] = 1;
						}
						lX = X; lY = Y;
						for (X = i, Y = j; myAI_valid(X) && myAI_valid(Y) && b[X][Y] == b[i][j]; X += mX[7-k], Y += mY[7-k])
						{
							cntR++;
							v[X][Y][k] = v[X][Y][7-k] = 1;
						}
						rX = X; rY = Y;
						cn = cntL + cntR - 1;
						maxCn = cn;
						for ( ; cn < AI_CONNECT && myAI_valid(lX) && myAI_valid(lY) && b[lX][lY] == 0; lX += mX[k], lY += mY[k])
							maxCn++;
						for ( ; cn < AI_CONNECT && myAI_valid(rX) && myAI_valid(rY) && b[rX][rY] == 0; rX += mX[7-k], rY += mY[7-k])
							maxCn++;
						
						if (cn > 1) isolate = false;
						if (cn > 1 && maxCn >= AI_CONNECT)
							ret += riskWeight[ cn ];
						// if (cn == 5)
						// 	printf("Risk @(%d, %d) = %.2f\n", i, j, ret);
					}
				if (isolate)
					for (k = 0; k < 4; k++)
						if (v[i][j][k])
						{
							isolate = false;
							break;
						}
				if (isolate)
					ret += riskWeight[ 1 ];
			}
	return ret;
}


double dfs(short b[AI_COORDS+1][AI_COORDS+1], int d, const int &dLmt, const short &colorOppo)
{
	if (d > dLmt) return calRisk(b, colorOppo);
	int i, j, k, nX, nY;
	double ret = -1, tmp;
	char v[AI_COORDS+1][AI_COORDS+1];
	memset(v, 0, sizeof(v));

	for (i = 1; i <= AI_COORDS; i++)
		for (j = 1; j <= AI_COORDS; j++)
			if (b[i][j] == colorOppo)
			{
				for (k = 0; k < 8; k++)
				{
					nX = i + mX[k];
					nY = j + mY[k];
					if (myAI_valid(nX) && myAI_valid(nY) && !v[nX][nY] && b[nX][nY] == 0)
					{
						v[nX][nY] = 1;
						b[nX][nY] = colorOppo;
						tmp = dfs(b, d+1, dLmt, colorOppo);
						if (tmp >= ret)
						{
							ret = tmp;
							if (d == 1 && tmp > riskMax)
							{
								riskX = nX;
								riskY = nY;
								riskMax = ret;
							}
						}
						b[nX][nY] = 0;
					}
				}
			}
	return ret;
}

void findBest(short b[AI_COORDS+1][AI_COORDS+1], short colorMy)
{
	int i, j, k, iX, iY, nX, nY, cntOppo;
	int dist, maxDist = -(1LL << 30);
	short colorOppo = 3 - colorMy;
	bestX = bestY = -1;
	int midX = 1 + (AI_COORDS-1) / 2;
	int midY = 1 + (AI_COORDS-1) / 2;
	memset(vst, 0, sizeof(vst));

	for (i = 1; i <= AI_COORDS; i++)
		for (j = 1; j <= AI_COORDS; j++)
			if (b[i][j] == colorMy)
			{
				for (k = 0; k < 4; k++)
				{
					nX = i + mX4[k];
					nY = j + mY4[k];
					if (myAI_valid(nX) && myAI_valid(nY) && b[nX][nY] == 0 && !vst[nX][nY])
					{
						vst[nX][nY] = 1;
						dist = 0;
						cntOppo = 0;
						for (iX = 1; iX <= AI_COORDS; iX++)
							for (iY = 1; iY <= AI_COORDS; iY++)
								if (b[iX][iY] == colorOppo)
								{
									dist += abs(nX - iX) + abs(nY - iY);
									++cntOppo;
								}
						dist -= ( abs(nX - midX) + abs(nY - midY) ) * cntOppo;
						if (dist > maxDist)
						{
							maxDist = dist;
							bestX = nX;
							bestY = nY;
						}
						else if (dist == maxDist)
						{
							if (better(nX, nY, bestX, bestY))
							{
								bestX = nX;
								bestY = nY;
							}
						}
					}
				}
			}
}

void myAI(short b[AI_COORDS+1][AI_COORDS+1], short color, int &X, int &Y)
{
	srand(time(0));
	int midX = 1 + (AI_COORDS-1) / 2;
	int midY = 1 + (AI_COORDS-1) / 2;

	// calculate my risk
	riskMax = -1;
	dfs(b, 1, 2, color);
	double riskMe = riskMax;
	int riskMeX = riskX, riskMeY = riskY;

	riskMax = -1;
	dfs(b, 1, 2, 3 - color);
	double riskOppo = riskMax;

	if (riskMe > riskOppo)
	{
		X = riskMeX;
		Y = riskMeY;
		return;
	}
	else if (riskMax >= thresDef) // Defend !
	{
		X = riskX;
		Y = riskY;
		return ;
	}
	else
	{
		if (riskMe >= thresAtk) // Attack !
		{
			X = riskX;
			Y = riskY;
			return ;
		}
		else
		{
			findBest(b, color);
			X = bestX;
			Y = bestY;
			// printf("~~~ BestX = %d, BestY = %d\n", bestX, bestY);
		}
	}

	int i, j, cnt = 0;
	int add = 0, add2 = 0, ub = AI_COORDS - midX, ub2 = AI_COORDS - midY;
	while (!myAI_valid(X) || !myAI_valid(Y) || b[X][Y] != 0)
	{
		if (cnt >= AI_MAXTRY) break;
		X = midX + ((rand() % 2) * 2 - 1) * add;
		Y = midY + ((rand() % 2) * 2 - 1) * add2;
		add = (add + 1) % (ub+1);
		add2 = (add2 + 1) % (ub2+1);
		++cnt;
	}
	if (!myAI_valid(X) || !myAI_valid(Y))
	{
		for (i = 1; i <= AI_COORDS; i++)
			for (j = 1; j <= AI_COORDS; j++)
				if (b[i][j] == 0)
				{
					X = i;
					Y = j;
					break;
				}
	}
	// printf("~~~ Risk Max = %.2f, RiskX = %d, RiskY = %d\n", riskMax, riskX, riskY); 
	// printf("Risk %.2f\n", calRisk(b, 3 - color));
}