#include "FingRRR.h"

using namespace std;
using namespace cv;

pthread_mutex_t mtx_iObj;

VideoCapture captureDevice;
const double thres_err = 0.1;
const double thres_dist = 70.0;
const double thres_finger_down = 0.2;
const double thres_ruleout = 0.7;
const int thres_circle = 1000;
const int thres_init = 100;
const int thres_hold_finger = 300; // 300 ms
const int thres_hold_mouse = 50; // 50 ms
int inputMethod = INPUT_METHOD_MOUSE;
// finger
int fingerArea; // detected finger area
int fingerInputArea;
int fingerState = FINGER_UP;
int fingerPosX = 0, fingerPosY = 0;
int prev_fingerPosX, prev_fingerPosY;
clock_t fingerDownStart = clock();
// mouse
int prev_mouseState;
int mouseState = MOUSE_UP;
int mousePosX = 0, mousePosY = 0;
int prev_mousePosX, prev_mousePosY;
bool mouseDragging = false;
clock_t mouseDownStart = clock();
// final input data
int inputState;
int prev_inputState = INPUT_NONE;
int inpPosX, inpPosY;
int prev_inpPosX, prev_inpPosY;
bool drawCursorFootprintOnDrag = false; // true;
Scalar clrCursorFootprint = CV_RGB(255, 255, 255);
bool allowCont[MAX_EVENTS];

int thres_Cr_L = 160;
int thres_Cr_U = 255;
int thres_Cb_L = 0;
int thres_Cb_U = 255;
int camWidth = 640, camHeight = 480;
int scrWidth = camWidth, scrHeight = camHeight;
Mat frameGame;

String windowNameThres = "Threshold Management";
String windowNameDetect = "Redball Detection";
String windowNameGame = "Game Window";

list<aniItem<double> > aniBufferD;
list<aniItem<int> > aniBufferI;

iScene *sceneActive;
Mat *frameActive;
Mat cameraFrame;
Mat hueImg;
Mat mask;

Mat assetsImg[MAX_ASSETS];
Rect rect_match;
iObject *objActive = NULL;

void (*userEvent)() = NULL;
pthread_t thr_animate, thr_inpFromObjs, thr_inp, thr_render, thr_userEvent;

void WE_onHover(int &x, int &y)
{
}
void WE_onDown(int &x, int &y)
{
}
void WE_onUp(int &x, int &y)
{
}
void WE_onDrag(int &x, int &y)
{
}
int phx = -1, phy = -1;
void WE_onHold(int &x, int &y, int &px, int &py)
{
	if (drawCursorFootprintOnDrag)
	{
		if (!(phx < 0 || phy < 0))
			sceneActive->createLine(phx, phy, x, y, clrCursorFootprint);
		phx = x;
		phy = y;
	}
	if (objActive != NULL)
	{
		objActive->x = inpPosX - (objActive->width >> 1);
		objActive->y = inpPosY - (objActive->height >> 1);
	}
	return ;
}
void WE_onRelease(int &x, int &y)
{
	if (drawCursorFootprintOnDrag)
	{
		phx = phy = -1;
	}
	objActive = NULL;
}
void WE_onOut(int &x, int &y)
{
}

void iLine::render(Mat &target)
{
	line(target, Point2f(x1, y1), Point2f(x2, y2), clr, thick);
	return ;
}

void iButton::render(Mat &target)
{
	if (bgImg != NULL)
		drawImgOnMat((*bgImg), target, x, y, min(width, bgImg->cols), min(height, bgImg->rows));
	else
		fillMat(target, clrR, clrG, clrB, clrA, x, y, width, height);
	return ;
}

iButton* iToolbar::createButton(int w, int h, unsigned char clrR, unsigned char clrG, unsigned char clrB, unsigned char clrA, Mat *bgImg,
	void(*onHover)(iButton *, int &, int &), void(*onDown)(iButton *, int &, int&), void(*onUp)(iButton *, int &, int&),
	void(*onDrag)(iButton *, int &, int&), void(*onHold)(iButton *, int &, int &, int &, int &),
	void(*onRelease)(iButton *, int &, int&), void(*onOut)(iButton *, int &, int&),
	int prevEvent)
{
	int cnt = lisBtns.size(), len = (height - (padding << 1));
	if (w == 0 && h == 0)
		w = h = len;
	w = min(w, width - rightX - padding);
	h = min(h, height - (padding << 1));
	iButton *ptr = new iButton(this, rightX + padding , y + padding, w, h, clrR, clrG, clrB, clrA, bgImg, onHover, onDown, onUp, onDrag, onHold, onRelease, onOut, prevEvent);
	lisBtns.push_back(ptr);
	rightX += padding + w;
	return ptr;
}

void iToolbar::render(Mat &target)
{
	list<iButton *>::iterator it;
	if (bgImg != NULL)
		drawImgOnMat((*bgImg), target, x, y, min(width, bgImg->cols), min(height, bgImg->rows));
	else
		fillMat(target, clrR, clrG, clrB, clrA, x, y, width, height);
	for (it = lisBtns.begin(); it != lisBtns.end(); it++)
		(*it) -> render(target);
	return ;
}

void iObject::render(Mat &target)
{
	if (visible)
	{
		if (bgImg != NULL)
			drawImgOnMat((*bgImg), target, x, y, min(width, bgImg->cols), min(height, bgImg->rows), bgImgA);
		else
			fillMat(target, clrR, clrG, clrB, clrA, x, y, width, height);
	}
	return ;
}

iToolbar* iScene::createToolbar(int x, int y, int width, int height, int padding,
	unsigned char clrR, unsigned char clrG, unsigned char clrB, unsigned char clrA, Mat *bgImg)
{
	iToolbar *ptr = new iToolbar(x, y, width, height, padding, clrR, clrG, clrB, clrA, bgImg);
	lisTBs.push_back(ptr);
	return ptr;
}

iObject* iScene::createObject(int x, int y, int width, int height,
	unsigned char clrR, unsigned char clrG, unsigned char clrB, unsigned char clrA,
	Mat *bgImg, unsigned char bgImgA, bool visible, bool draggable,
	void(*onHover)(iObject*, int &, int &), void(*onDown)(iObject*, int &, int&), void(*onUp)(iObject*, int &, int&),
	void(*onDrag)(iObject*, int &, int&), void(*onHold)(iObject*, int &, int &, int &, int &),
	void(*onRelease)(iObject*, int &, int&), void(*onOut)(iObject*, int &, int &),
	int prevEvent)
{
	iObject *ptr = new iObject(this, x, y, width, height, clrR, clrG, clrB, clrA, bgImg, bgImgA, visible, draggable, onHover, onDown, onUp, onDrag, onHold, onRelease, onOut, prevEvent);
	lisObjs.push_back(ptr);
	return ptr;
}

void iScene::deleteObject(iObject *obj)
{
	pthread_mutex_lock(&mtx_iObj);
	list<iObject *>::iterator itObj;
	for (itObj = lisObjs.begin(); itObj != lisObjs.end(); itObj++)
		if ( (*itObj) == obj )
		{
			delete (*itObj);
			lisObjs.erase(itObj);
			break;
		}
	pthread_mutex_unlock(&mtx_iObj);
}

void iScene::clearObject()
{
	pthread_mutex_lock(&mtx_iObj);
	list<iObject *>::iterator itObj;
	for (itObj = lisObjs.begin(); itObj != lisObjs.end(); )
	{
		delete (*itObj);
		itObj = lisObjs.erase(itObj);
		break;
	}
	pthread_mutex_unlock(&mtx_iObj);
}

void iScene::createLine(int &x1, int &y1, int &x2, int &y2, Scalar &clr)
{
	lisLns.push_back(iLine(x1, y1, x2, y2, clr));
	return ;
}

void iScene::render(Mat &target)
{
	list<iToolbar *>::iterator itTB;
	list<iObject *>::iterator itObj;
	list<iLine>::iterator itLn;
	if (!(clrR == 0 && clrG == 0 && clrB == 0))
		fillMat(target, clrR, clrG, clrB, clrA, x, y, width, height);

	if (bgImg != NULL)
		drawImgOnMat((*bgImg), target, 0, 0);

	for (itTB = lisTBs.begin(); itTB != lisTBs.end(); itTB++)
		(*itTB) -> render(target);

	pthread_mutex_lock(&mtx_iObj);
	for (itObj = lisObjs.begin(); itObj != lisObjs.end(); itObj++)
		(*itObj) -> render(target);
	pthread_mutex_unlock(&mtx_iObj);

	for (itLn = lisLns.begin(); itLn != lisLns.end(); itLn++)
		(*itLn).render(target);
	return ;
}

template <typename S, typename T>
bool equal(S a, T b)
{
	return abs(double(a) - double(b)) < 1.0;
}
double diffArea(double a1, double a2)
{
	double a2sqrt = sqrt(a2);
	return abs(sqrt(a1) - a2sqrt) / a2sqrt;
}
double diffAreaN(double a1, double a2)
{
	double a2sqrt = sqrt(a2);
	return (sqrt(a1) - a2sqrt) / a2sqrt;
}
bool equalArea(double a1, double a2)
{
	double a2sqrt = sqrt(a2);
	return abs(sqrt(a1) - a2sqrt) / a2sqrt < thres_err;
}
bool equalLength(double l1, double l2)
{
	return abs(l1 - l2) < thres_err;
}
bool isFingerDown(double a1)
{
	double a2sqrt = sqrt(fingerArea);
	return (sqrt(a1) - a2sqrt) / a2sqrt >= thres_finger_down;
}
bool ruleoutArea(double a1, double a2)
{
	double a2sqrt = sqrt(a2);
	return abs(sqrt(a1) - a2sqrt) / a2sqrt > thres_ruleout;
}
bool ruleoutLength(double l1, double l2)
{
	return abs(l1 - l2) > thres_ruleout;
}
inline int getMs(clock_t clocks)
{
	return clocks * 1000 / CLOCKS_PER_SEC;
}

void drawImgOnMat(Mat &img, Mat &m, const int &x, const int &y, const unsigned char imgA)
{
	int i, j, r = min(y + img.rows - 1, m.rows - 1) - y + 1, c = min(x + img.cols - 1, m.cols - 1) - x + 1;
	double imgAf = imgA / 255.0f;
	if (img.channels() == 4)
	{
		for (i = 0; i < r; i++)
		{
			if (y + i < 0) continue;
			for (j = 0; j < c; j++)
			{
				if (x + j < 0) continue;
				Vec3b &mi = m.at<Vec3b>(y + i, x + j);
				Vec4b &imgi = img.at<Vec4b>(i, j);
				double a = (imgi[3] / 255.0f) * imgAf;
				mi[0] = imgi[0] * a + mi[0] * (1 - a);
				mi[1] = imgi[1] * a + mi[1] * (1 - a);
				mi[2] = imgi[2] * a + mi[2] * (1 - a);
			}
		}
	}
	else
	{
		for (i = 0; i < r; i++)
		{
			if (y + i < 0) continue;
			for (j = 0; j < c; j++)
			{
				if (x + j < 0) continue;
				Vec3b &mi = m.at<Vec3b>(y + i, x + j);
				Vec3b &imgi = img.at<Vec3b>(i, j);
				double &a = imgAf;
				mi[0] = imgi[0] * a + mi[0] * (1 - a);
				mi[1] = imgi[1] * a + mi[1] * (1 - a);
				mi[2] = imgi[2] * a + mi[2] * (1 - a);
			}
		}
	}
	return ;
}

void drawImgOnMat(Mat &img, Mat &m, const int &x, const int &y, const int &w, const int &h, const unsigned char imgA)
{
	int i, j, r = min(y + h - 1, m.rows - 1) - y + 1, c = min(x + w - 1, m.cols - 1) - x + 1;
	double imgAf = imgA / 255.0f;
	if (img.channels() == 4)
	{
		for (i = 0; i < r; i++)
		{
			if (y + i < 0) continue;
			for (j = 0; j < c; j++)
			{
				if (x + j < 0) continue;
				Vec3b &mi = m.at<Vec3b>(y + i, x + j);
				Vec4b &imgi = img.at<Vec4b>(i, j);
				double a = (imgi[3] / 255.0f) * imgAf;
				mi[0] = imgi[0] * a + mi[0] * (1 - a);
				mi[1] = imgi[1] * a + mi[1] * (1 - a);
				mi[2] = imgi[2] * a + mi[2] * (1 - a);
			}
		}
	}
	else
	{
		for (i = 0; i < r; i++)
		{
			if (y + i < 0) continue;
			for (j = 0; j < c; j++)
			{
				if (x + j < 0) continue;
				Vec3b &mi = m.at<Vec3b>(y + i, x + j);
				Vec3b &imgi = img.at<Vec3b>(i, j);
				double &a = imgAf;
				mi[0] = imgi[0] * a + mi[0] * (1 - a);
				mi[1] = imgi[1] * a + mi[1] * (1 - a);
				mi[2] = imgi[2] * a + mi[2] * (1 - a);
			}
		}
	}
	return ;
}

void fillMat(Mat &target, int clrR, int clrG, int clrB, int clrA)
{
	if (clrA == 0) return ;
	double a = clrA / 255.0f;
	int i, j, r = target.rows, c = target.cols;
	for (i = 0; i < r; i++)
		for (j = 0; j < c; j++)
		{
			Vec3b &mi = target.at<Vec3b>(i, j);
			mi[0] = clrB * a + mi[0] * (1 - a);
			mi[1] = clrG * a + mi[1] * (1 - a);
			mi[2] = clrR * a + mi[2] * (1 - a);
		}
	return ;
}

void fillMat(Mat &target, int clrR, int clrG, int clrB, int clrA, int x, int y, int w, int h)
{
	if (clrA == 0) return ;
	double a = clrA / 255.0f;
	int i, j, r = min(y + h, target.rows), c = min(x + w, target.cols);
	for (i = max(0, y); i < r; i++)
		for (j = max(0, x); j < c; j++)
		{
			Vec3b &mi = target.at<Vec3b>(i, j);
			mi[0] = clrB * a + mi[0] * (1 - a);
			mi[1] = clrG * a + mi[1] * (1 - a);
			mi[2] = clrR * a + mi[2] * (1 - a);
		}
	return ;
}

void loadAssets()
{
	int id;
	char filename[100];
	FILE *fi = fopen("assets.txt", "r");
	while (fscanf(fi, "%d %s", &id, filename) == 2)
		assetsImg[id] = imread("assets\\" + string(filename), CV_LOAD_IMAGE_UNCHANGED);
	return ;
}

void init(int w, int h, bool debug)
{
	int i;
	pthread_mutex_init(&mtx_iObj, NULL);

	scrWidth = w;
	scrHeight = h;
	frameGame.create(scrHeight, scrWidth, CV_8UC3);
	frameActive = &frameGame;
	if (debug)
		freopen("debug.txt", "w", stdout);
	
	loadAssets();

	namedWindow(windowNameGame, WINDOW_AUTOSIZE);
	adjustWindowGame();

	switch(inputMethod)
	{
		case INPUT_METHOD_FINGER:
		{
			namedWindow(windowNameThres, WINDOW_AUTOSIZE);
			createThresholdBars();
			adjustWindowThres();

			namedWindow(windowNameDetect, WINDOW_AUTOSIZE);
			adjustWindowDetect();

			captureDevice.open(0);
			if (initFingerDetection())
			{
				puts("Initialized Successfully");
				printf("Finger area detected: %d\n", fingerArea);
				puts("Moving on ...");
			}
			else
			{
				puts("Critical Error! (Too much noise?)");
				waitKey(10000);
				return ;
			}
			break;
		}
		case INPUT_METHOD_MOUSE:
		{
			setMouseCallback(windowNameGame, &setMouseData, NULL);
		}
	}

	// set whether INPUT_X is allowed to be executed consectively
	for (i = 0; i < MAX_EVENTS; i++)
		allowCont[i] = false; // disallowed by default
	allowCont[INPUT_HOVER] =
	allowCont[INPUT_HOLD] = 
	allowCont[INPUT_NONE] = true;
}

static void on_trackbar_Cr_L(int thres, void*)
{
	thres_Cr_L = thres;
	if (thres_Cr_L > thres_Cr_U)
		setTrackbarPos("Cr LBound", windowNameThres, thres_Cr_U - 1);

	return ;
}
static void on_trackbar_Cr_U(int thres, void*)
{
	thres_Cr_U = thres;
	if (thres_Cr_U<thres_Cr_L)
		setTrackbarPos("Cr UBound", windowNameThres, thres_Cr_L + 1);

	return ;
}
static void on_trackbar_Cb_L(int thres, void*)
{
	thres_Cb_L = thres;
	if (thres_Cb_L > thres_Cb_U)
		setTrackbarPos("Cb LBound", windowNameThres, thres_Cb_U - 1);

	return ;
}
static void on_trackbar_Cb_U(int thres, void*)
{
	thres_Cb_U = thres;
	if (thres_Cb_U < thres_Cr_L)
		setTrackbarPos("Cb UBound", windowNameThres, thres_Cb_L + 1);

	return ;
}

void createThresholdBars()
{
	createTrackbar("Cr LBound", windowNameThres, &thres_Cr_L, 255, on_trackbar_Cr_L);
	createTrackbar("Cr UBound", windowNameThres, &thres_Cr_U, 255, on_trackbar_Cr_U);

	createTrackbar("Cb LBound", windowNameThres, &thres_Cb_L, 255, on_trackbar_Cb_L);
	createTrackbar("Cb UBound", windowNameThres, &thres_Cb_U, 255, on_trackbar_Cb_U);

	return ;
}

void updateMask()
{
	for (int i = 0; i < hueImg.rows; i++)
	for (int j = 0; j<hueImg.cols; j++)
	if (hueImg.at<Vec3b>(i, j)[2] > thres_Cb_U || hueImg.at<Vec3b>(i, j)[2] < thres_Cb_L || hueImg.at<Vec3b>(i, j)[1] > thres_Cr_U || hueImg.at<Vec3b>(i, j)[1] < thres_Cr_L)
		mask.at<uchar>(i, j) = 1;

	return ;
}

template <typename T>
inline void fixCoord(T &x, T &y)
{
	int midX = camWidth >> 1, midY = camHeight >> 1;
	double lFront = sqrt(fingerArea), lRear = sqrt(fingerInputArea);
	x = midX + double(x - midX) * (lFront / lRear);
	y = midY + double(y - midY) * (lFront / lRear);

	// mirror
	x = camWidth - x;
	// y = same

	if (x < 0) x = 0;
	if (y < 0) y = 0;

	return;
}

template <typename T>
inline void extendCoord(T &x, T &y, int &inpWidth, int &inpHeight)
{
	x = double(x) * scrWidth / inpWidth;
	y = double(y) * scrHeight / inpHeight;
}

inline double dist(double x1, double y1, double x2, double y2)
{
	double xd = x1 - x2, yd = y1 - y2;
	return sqrt(xd * xd + yd * yd);
}

void adjustWindowThres()
{
	moveWindow(windowNameThres, 200, 0);
	resizeWindow(windowNameThres, 300, 200);
}

void adjustWindowDetect()
{
	moveWindow(windowNameDetect, 200 + 300 + 5, 0);
}

void adjustWindowGame()
{
	moveWindow(windowNameGame, 200 + 300 + 5, 0);
	resizeWindow(windowNameGame, scrWidth, scrHeight);
}

Rect * boundingBoxLargest(list<Rect> &v)
{
	list<Rect>::iterator it;
	Rect *ret = NULL;
	int maxArea = 0;
	for (it = v.begin(); it != v.end(); it++) {
		if ((*it).area() > maxArea)
		{
			maxArea = (*it).area();
			ret = &(*it);
		}
	}
	return ret;
}

bool detectRedBall(double baseArea)
{
	int i, area;
	Rect *ptrRectMatch;
	Mat grayImg;
	bool ret = false;
	// double minDiff, tmpDiff, minDist, tmpDist;
	prev_fingerPosX = fingerPosX;
	prev_fingerPosY = fingerPosY;

	// Rect rect_match;
	vector<vector<cv::Point>> contours;
	vector<cv::Vec4i> hierarchy;
	list<cv::Rect> boundingBox;

	cvtColor(cameraFrame, hueImg, CV_BGR2YCrCb);

	mask = Mat::zeros(hueImg.rows, hueImg.cols, CV_8U);
	updateMask();
	cameraFrame.setTo(Vec3b(0, 0, 0), mask);

	cvtColor(cameraFrame, grayImg, CV_BGR2GRAY);
	findContours(grayImg, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cv::Point());

	for (i = 0; i<contours.size(); i++)
		boundingBox.push_back(boundingRect(contours[i]));

	if (boundingBox.size() > 0)
	{
		ptrRectMatch = boundingBoxLargest(boundingBox);

		area = ptrRectMatch -> area();
		if (baseArea < 0.0 || (baseArea >= 0.0 && !ruleoutArea(area, baseArea)))
		{
			rect_match = *ptrRectMatch;
			fingerPosX = rect_match.x + (rect_match.width >> 1);
			fingerPosY = rect_match.y + (rect_match.height >> 1);
			fingerInputArea = area;

			fixCoord(fingerPosX, fingerPosY);
			extendCoord(fingerPosX, fingerPosY, camWidth, camHeight);
			
			rectangle(cameraFrame, rect_match, cv::Scalar(0, 0, 255), 1, 8, 0);
			ret = true;
		}
		else
		{
			fingerPosX = prev_fingerPosX;
			fingerPosY = prev_fingerPosY;
		}
	}
	else
	{
		fingerPosX = prev_fingerPosX;
		fingerPosY = prev_fingerPosY;
	}
	return ret;
}

double initBuffer[MAX_BUFFER_SIZE];
int initBufferCount[MAX_BUFFER_SIZE];
bool initFingerDetection()
{
	int i, cnt = 0, area;
	fingerPosX = camWidth >> 1; fingerPosY = camHeight >> 1;
	inpPosX = camWidth >> 1; inpPosY = camHeight >> 1;
	puts("Please place your finger in front of the screen.");
	puts("Waiting 3 seconds before continuing");
	waitKey(3000);
	puts("Detecting your finger ...");
	puts("Please try holding the finger still in the mean time.");
	while (1)
	{
		captureDevice >> cameraFrame;

		detectRedBall();
		area = rect_match.area();
		for (i = 0; i < cnt; i++)
		if (equalArea(area, initBuffer[i]))
		{
			initBuffer[i] = double(initBuffer[i] * initBufferCount[i] + area) / (initBufferCount[i] + 1);
			initBufferCount[i]++;
			if (initBufferCount[i] >= thres_init)
			{
				fingerArea = initBuffer[i];
				return true;
			}
			break;
		}

		if (i == cnt) // no match
		{
			if (cnt < MAX_BUFFER_SIZE)
			{
				initBuffer[cnt] = area;
				initBufferCount[cnt++] = 1;
			}
			else
				return false;
		}

		imshow(windowNameDetect, cameraFrame);
		waitKey(1);
	}
	return false;
}

template<class T>
void execEvent(T *target, int &signal, int &x, int &y, int &px, int &py)
{
	// printf("Signal %d at (%d, %d)\n", signal, x, y);
	target->prevEvent = signal;
	switch (signal)
	{
	case INPUT_HOVER:
		if (target->onHover != NULL)
			target->onHover(target, x, y);
		break;
	case INPUT_DOWN:
		if (target->onDown != NULL)
			target->onDown(target, x, y);
		break;
	case INPUT_UP:
		if (target->onUp != NULL)
			target->onUp(target, x, y);
		break;
	case INPUT_DRAG:
		if (target->onDrag != NULL && target->draggable)
			target->onDrag(target, x, y);
		break;
	case INPUT_HOLD:
		if (target->onHold != NULL)
			target->onHold(target, x, y, px, py);
		break;
	case INPUT_RELEASE:
		if (target->onRelease != NULL)
			target->onRelease(target, x, y);
		break;
	case INPUT_OUT:
		if (target->onOut != NULL)
			target->onOut(target, x, y);
		break;
	}
	return ;
}

void inputHandler(int signal, int &x, int &y, int &px, int &py)
{
	inputState = signal;
	if (inputState == prev_inputState && !allowCont[signal]) return ;
	prev_inputState = inputState;
	list<iToolbar *>::iterator itTB;
	list<iButton *>::iterator itBtn;
	list<iObject *>::iterator itObj;
	for (itTB = sceneActive->lisTBs.begin(); itTB != sceneActive->lisTBs.end(); itTB++)
	{
		iToolbar* &tb = (*itTB);
		for (itBtn = tb->lisBtns.begin(); itBtn != tb->lisBtns.end(); itBtn++)
		{
			iButton* &btn = (*itBtn);
			if (btn->x <= x && x < btn->x + btn->width && btn->y <= y && y < btn->y + btn->height)
				execEvent(btn, signal, x, y, px, py);
		}
	}
	for (itObj = sceneActive->lisObjs.begin(); itObj != sceneActive->lisObjs.end(); itObj++)
	{
		iObject* &obj = (*itObj);
		if (obj->x <= x && x < obj->x + obj->width && obj->y <= y && y < obj->y + obj->height)
			execEvent(obj, signal, x, y, px, py);
	}
	// World Events (Unbound by 
	switch (signal)
	{
	case INPUT_HOVER:
		WE_onHover(x, y);
		break;
	case INPUT_DOWN:
		WE_onDown(x, y);
		break;
	case INPUT_UP:
		WE_onUp(x, y);
		break;
	case INPUT_DRAG:
		WE_onDrag(x, y);
		break;
	case INPUT_HOLD:
		WE_onHold(x, y, px, py);
		break;
	case INPUT_RELEASE:
		WE_onRelease(x, y);
		break;
	case INPUT_OUT:
		WE_onOut(x, y);
		break;
	}

	
	return ;
}

template<typename T>
void posFuncConst(T* stVal, T* curVal, const clock_t &st, const clock_t &tot, const clock_t &lT, const clock_t &cT, const int &argc, const double * args)
{
	(*curVal) = (*stVal) + args[0] * (cT - st);
}

// single argument animation . ex: constant speed
void createAnimation(int &target, double elapSec, double arg0, unsigned int aniFuncType, void (*posFunc)(int*, int*, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *), void (*callback)()) // arg0 possibly speed
{
	clock_t curTime = clock();
	double *args;
	switch (aniFuncType)
	{
	case ANI_SPEEDFUNC_CONST:
	{
		args = new double[1];
		args[0] = arg0 / CLOCKS_PER_SEC; // convert to pixels/clock
		// printf("%f\n", args[0]);
		aniItem<int> res(&target, curTime, curTime + double(elapSec) * CLOCKS_PER_SEC, &posFuncConst<int>, 1, args, callback);
		aniBufferI.push_back(res);
		break;
	}
	case ANI_SPEEDFUNC_CUSTOM:
	{
		args = new double[1];
		args[0] = arg0;
		// printf("%f\n", args[0]);
		aniItem<int> res(&target, curTime, curTime + double(elapSec) * CLOCKS_PER_SEC, &posFuncConst<int>, 1, args, callback);
		aniBufferI.push_back(res);
		break;
	}
	}
}
void createAnimation(double &target, double elapSec, double arg0, unsigned int aniFuncType, void (*posFunc)(double*, double*, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *), void (*callback)()) // arg0 possibly speed
{
	clock_t curTime = clock();
	double *args;
	switch (aniFuncType)
	{
	case ANI_SPEEDFUNC_CONST:
	{
		args = new double[1];
		args[0] = arg0 / CLOCKS_PER_SEC;
		aniItem<double> res(&target, curTime, curTime + double(elapSec) * CLOCKS_PER_SEC, &posFuncConst<double>, 1, args, callback);
		aniBufferD.push_back(res);
		break;
	}
	case ANI_SPEEDFUNC_CUSTOM:
	{
		args = new double[1];
		args[0] = arg0 / CLOCKS_PER_SEC;
		aniItem<double> res(&target, curTime, curTime + double(elapSec) * CLOCKS_PER_SEC, &posFuncConst<double>, 1, args, callback);
		aniBufferD.push_back(res);
		break;
	}
	}
}

void animate()
{
	list<aniItem<double> >::iterator itD;
	list<aniItem<int> >::iterator itI;
	clock_t cT = clock();
	for (itD = aniBufferD.begin(); itD != aniBufferD.end(); )
	{
		aniItem<double> &a = (*itD);
		if (a.ed > cT) // expired
		{
			(*a.callback)();
			delete a.args;
			itD = aniBufferD.erase(itD);
		}
		else
		{ 
			itD->posFunc(&a.stVal, a.target, a.st, a.tot, a.lT, cT, a.argc, a.args);
			a.lT = cT;
			itD++;
		}
	}
	for (itI = aniBufferI.begin(); itI != aniBufferI.end(); )
	{
		aniItem<int> &a = (*itI);
		if (cT > a.ed) // expired
		{
			(*a.callback)();
			delete a.args;
			itI = aniBufferI.erase(itI);
		}
		else
		{
			itI->posFunc(&a.stVal, a.target, a.st, a.tot, a.lT, cT, a.argc, a.args);
			a.lT = cT;
			itI++;
		}
	}
	return ;
}

void inpFromObjs()
{

}

// sets fingerState
void determineIssueFingerState()
{
	int prevState = fingerState;
	if (isFingerDown(fingerInputArea))
	{
		if (prevState == FINGER_UP) // up
		{
			fingerState = FINGER_DOWN;
			fingerDownStart = clock();
			inputHandler(INPUT_DOWN, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
		}
		else if (prevState == FINGER_DOWN) // down
		{
			if (getMs(clock() - fingerDownStart) >= thres_hold_finger)
			{
				fingerState = FINGER_HOLD;
				inputHandler(INPUT_DRAG, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
			}
			/* else
				fingerState = FINGER_DOWN; */
		}
		else if (prevState == FINGER_HOLD)
			inputHandler(INPUT_HOLD, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
	}
	else
	{
		fingerState = FINGER_UP;
		if (prevState == FINGER_UP)
			inputHandler(INPUT_HOVER, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
		else if (prevState == FINGER_DOWN)
			inputHandler(INPUT_UP, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
		else if (prevState == FINGER_HOLD)
			inputHandler(INPUT_RELEASE, fingerPosX, fingerPosY, prev_fingerPosX, prev_fingerPosY);
	}
	return ;
}

bool inpFinger()
{
	captureDevice >> cameraFrame;
	if (detectRedBall(fingerArea))
	{
		determineIssueFingerState();

		system("cls");
		printf("The position of the red ball: (%d, %d)\n", fingerPosX, fingerPosY);
		printf("Finger Area: %d\n", fingerInputArea);
		printf("Finger area detected: %d\n", fingerArea);
		printf("Finger State: %d\n", fingerState);
		printf("Ruleout: %d\n", ruleoutArea(fingerInputArea, fingerArea));

		return true;
	}
	else
	{

		system("cls");
		puts("Finger Not Detected!");

		return false;
	}
}

static void setMouseData(int event, int x, int y, int flags, void *userdata)
{
	// printf("%d at (%d, %d)\n", event, x, y);
	prev_mousePosX = mousePosX;
	prev_mousePosY = mousePosY;
	mousePosX = x;
	mousePosY = y;
	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		mouseState = MOUSE_DOWN;
		mouseDownStart = clock();
		break;
	case EVENT_LBUTTONUP:
		mouseState = MOUSE_UP;
		break;
	case EVENT_MOUSEMOVE:
		break;
	}
	// printf("%d %d\n", prev_mouseState, mouseState);
}

bool inpMouse()
{
	if (mouseState == MOUSE_DOWN)
	{
		// printf("%lld %lld\n", mouseDownStart, clock());
		if (prev_mouseState == MOUSE_UP)
			inputHandler(INPUT_DOWN, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
		else 
		{
			if (mouseDragging)
				inputHandler(INPUT_HOLD, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
			else
			{
				if (getMs(clock() - mouseDownStart) >= thres_hold_mouse)
				{
					mouseDragging = true;
					inputHandler(INPUT_DRAG, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
					// printf("Started Dragging at (%d, %d)\n", mousePosX, mousePosY);
				}
			}
		}
	}
	else
	{
		if (mouseDragging)
		{
			inputHandler(INPUT_RELEASE, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
			mouseDragging = false;
		}
		else if (prev_mouseState == MOUSE_DOWN)
			inputHandler(INPUT_UP, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
		inputHandler(INPUT_HOVER, mousePosX, mousePosY, prev_mousePosX, prev_mousePosY);
	}
	prev_mouseState = mouseState;
	return true;
}

bool inp(int flag)
{
	prev_inpPosX = inpPosX; prev_inpPosY = inpPosY;
	bool ret = false;
	switch (flag)
	{
	case INPUT_METHOD_ALL:
		/**/
		break;
	case INPUT_METHOD_FINGER:
		ret = inpFinger();
		inpPosX = fingerPosX;
		inpPosY = fingerPosY;
		break;
	case INPUT_METHOD_MOUSE:
		inpMouse();
		inpPosX = mousePosX;
		inpPosY = mousePosY;
		break;
	case INPUT_METHOD_KEYBOARD:
		break;
	}
	return ret;
}

void scanGlobalEvents()
{
	list<iToolbar *>::iterator itTB;
	list<iButton *>::iterator itBtn;
	list<iObject *>::iterator itObj;
	int &x = inpPosX, &y = inpPosY, &px = prev_inpPosX, &py = prev_inpPosY, signal;
	for (itTB = sceneActive->lisTBs.begin(); itTB != sceneActive->lisTBs.end(); itTB++)
	{
		iToolbar* &tb = (*itTB);
		for (itBtn = tb->lisBtns.begin(); itBtn != tb->lisBtns.end(); itBtn++)
		{
			iButton* &btn = (*itBtn);
			if (btn->prevEvent == INPUT_HOVER && !(btn->x <= x && x <= btn->x + btn->width && btn->y <= y && y <= btn->y + btn->height))
			{
				signal = INPUT_OUT;
				execEvent(btn, signal, x, y, px, py);
				WE_onOut(x, y);
			}
		}
	}
	for (itObj = sceneActive->lisObjs.begin(); itObj != sceneActive->lisObjs.end(); itObj++)
	{
		iObject* &obj = (*itObj);
		if (obj->prevEvent == INPUT_HOVER && !(obj->x <= x && x <= obj->x + obj->width && obj->y <= y && y <= obj->y + obj->height))
		{
			signal = INPUT_OUT;
			execEvent(obj, signal, x, y, px, py);
			WE_onOut(x, y);
		}
	}
	return ;
}

void renderCursor(Mat &frame)
{
	int cursorSize;
	switch (inputState)
	{
	case INPUT_DOWN:
		cursorSize = 7;
		break;
	case INPUT_DRAG:
	case INPUT_HOLD:
		cursorSize = 9;
		break;
	default:
		cursorSize = 5;
	}
	circle(frame, Point2f(inpPosX, inpPosY), cursorSize, CV_RGB(255, 0, 0), CV_FILLED);
	return ;
}



void * P_animate(void *arg)
{
	while (1)
	{
		animate();
		waitKey(1);
	}
	puts("P_animate terminated");
	return NULL;
}

void * P_inpFromObjs(void *arg)
{
	while (1)
	{
		inpFromObjs(); // collision detection
		waitKey(1);
	}
	puts("P_inpFromObjs terminated");
	return NULL;
}

void * P_inp(void *arg)
{
	while(1)
	{
		inp(inputMethod);
		scanGlobalEvents();
		waitKey(1);
	}
	puts("P_inp terminated");
	return NULL;
}

void * P_render(void *arg)
{
	while (1)
	{
		(*frameActive) = Scalar(0, 0, 0);
		sceneActive->render(*frameActive);
		renderCursor(*frameActive);
		imshow(windowNameGame, *frameActive);
		waitKey(1);
	}
	puts("P_Render terminated");
	return NULL;
}

void * P_userEvent(void *arg)
{
	(*userEvent)();
	return NULL;
}

void start()
{
	pthread_create(&thr_animate, NULL, P_animate, NULL);
	pthread_create(&thr_inpFromObjs, NULL, P_inpFromObjs, NULL);
	pthread_create(&thr_inp, NULL, P_inp, NULL);
	pthread_create(&thr_render, NULL, P_render, NULL);
	if (userEvent != NULL) pthread_create(&thr_userEvent, NULL, P_userEvent, NULL);

	int key;
	while (1)
	{
		key = waitKey(0);
		printf("%d\n", key);
		waitKey(1);
	}

	pthread_join(thr_animate, NULL);
	pthread_join(thr_inpFromObjs, NULL);
	pthread_join(thr_inp, NULL);
	pthread_join(thr_render, NULL);
	if (userEvent != NULL) pthread_join(thr_userEvent, NULL);
}

void hookUserEvent(void (*event)())
{
	userEvent = event;
}