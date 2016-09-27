#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <queue>
// #include <vector>
#include <list>
#include <algorithm>
#include <string>
#include "pthread.h"
#include <opencv2\imgproc\imgproc.hpp>
#include <opencv2\highgui\highgui.hpp>


/* VC++ Code */
#ifdef _MSC_VER
	#define _CRT_SECURE_NO_WARNINGS
#endif


#ifndef FingRRR
#define FingRRR


#define MAX_BUFFER_SIZE 500
#define MAX_CUSTOM_DATA_SIZE 100
#define MAX_ASSETS 100
#define MAX_EVENTS 30
#define INF 1000000

#define INPUT_METHOD_ALL 0
#define INPUT_METHOD_FINGER 1
#define INPUT_METHOD_MOUSE 2
#define INPUT_METHOD_KEYBOARD 3

#define FINGER_UP 0
#define FINGER_DOWN 1
#define FINGER_HOLD 2

#define MOUSE_UP 0
#define MOUSE_DOWN 1
#define MOUSE_MOVE 2

#define INPUT_HOVER 0
#define INPUT_DOWN 1
#define INPUT_UP 2
#define INPUT_DRAG 3
#define INPUT_HOLD 4
#define INPUT_RELEASE 5
#define INPUT_NONE 10 // global event
#define INPUT_OUT 11 // global event

#define ANI_SPEEDFUNC_CUSTOM 0
#define ANI_SPEEDFUNC_CONST 1

using namespace std;
using namespace cv;


class iLine;
class iObject;
class iButton;
class iToolbar;
class iScene;

template<class T>
class aniItem;


extern VideoCapture captureDevice;
extern const double thres_err;
extern const double thres_dist;
extern const double thres_finger_down;
extern const double thres_ruleout;
extern const int thres_circle;
extern const int thres_init;
extern const int thres_hold_finger; // 300 ms
extern const int thres_hold_mouse; // 100 ms
extern int inputMethod;
extern int fingerArea; // detected finger area
extern int fingerInputArea;
extern int fingerState;
extern int fingerPosX, fingerPosY;
extern int prev_fingerPosX, prev_fingerPosY;
extern int inpPosX, inpPosY;
extern int prev_inpPosX, prev_inpPosY;
extern clock_t fingerDownStart;
extern bool drawCursorFootprintOnDrag;
extern Scalar clrCursorFootprint;
extern bool allowCont[];

extern int thres_Cr_L;
extern int thres_Cr_U;
extern int thres_Cb_L;
extern int thres_Cb_U;
extern int camWidth, camHeight;
extern int scrWidth, scrHeight;
extern Mat frameGame;

extern String windowNameThres;
extern String windowNameDetect;
extern String windowNameGame;

extern list<aniItem<double> > aniBufferD;
extern list<aniItem<int> > aniBufferI;

extern iScene *sceneActive;
extern Mat *frameActive;
extern Mat cameraFrame;
extern Mat hueImg;
extern Mat mask;

extern Mat assetsImg[];
extern Rect rect_match;
extern iObject *objActive;

double diffArea(double a1, double a2);
double diffAreaN(double a1, double a2);
bool equalArea(double a1, double a2);
bool equalLength(double l1, double l2);
bool isFingerDown(double a1);
bool ruleoutArea(double a1, double a2);
bool ruleoutLength(double l1, double l2);
inline int getMs(clock_t clocks);
inline double dist(double x1, double y1, double x2, double y2);

void drawImgOnMat(Mat &, Mat &, const int &, const int &, const unsigned char imgA = 255);
void drawImgOnMat(Mat &, Mat &, const int &, const int &, const int &, const int &, const unsigned char imgA = 255);
void fillMat(Mat &, int, int, int, int);
void fillMat(Mat &, int, int, int, int, int, int, int, int);

void init(int w, int h, bool debug = false);
static void on_trackbar_Cr_L(int thres, void*);
static void on_trackbar_Cr_U(int thres, void*);
static void on_trackbar_Cb_L(int thres, void*);
static void on_trackbar_Cb_U(int thres, void*);
void createThresholdBars();
void updateMask();
void adjustWindowThres();
void adjustWindowDetect();
void adjustWindowGame();

template<class T>
inline void fixCoord(T &x, T &y);
template <typename T>
inline void extendCoord(T &x, T &y, int &inpWidth, int &inpHeight);

Rect * boundingBoxLargest(list<Rect> &v);
bool detectRedBall(double baseArea = -1);
bool initFingerDetection();
void determineIssueFingerState();

template <typename S, typename T>
bool aniEqual(S, T);
template <typename T>
double posFuncConst(T* curVal, clock_t elapSec, clock_t totSec, int argc, double *args);
void createAnimation(int &target, double elapSec, double arg0, unsigned int aniFuncType, void (*posFunc)(int *, int *, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *) = NULL, void (*callback)() = NULL);
void createAnimation(double &target, double elapSec, double arg0, unsigned int aniFuncType, void (*posFunc)(double *, double *, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *) = NULL, void (*callback)() = NULL);

void loadAssets();

template<class T>
void execEvent(T *target, int &signal, int &x, int &y, int &px, int &py);
void inputHandler(int signal, int &x, int &y, int &px, int &py);
void animate();
void inpFromObjs();
bool inpFinger();
static void setMouseData(int event, int x, int y, int flags, void *userdata);
bool inpMouse();
bool inp(int flag);
void scanGlobalEvents();
void renderCursor();

extern void (*userEvent)();
void hookUserEvent(void (*)());

void * P_animate(void *arg);
void * P_inpFromObjs(void *arg);
void * P_inpKeyboard(void *arg);
void * P_inp(void *arg);
void * P_render(void *arg);
void * P_userEvent(void *arg);
extern pthread_t thr_animate, thr_inpFromObjs, thr_inp, thr_render;
void start();

extern pthread_mutex_t mtx_iObj;

void WE_onHover(int &, int &);
void WE_onDown(int &, int &);
void WE_onUp(int &, int &);
void WE_onDrag(int &, int &);
void WE_onHold(int &, int &, int &, int &);
void WE_onRelease(int &, int &);
void WE_onOut(int &, int &);

class iLine
{
public:
	int x1, y1, x2, y2;
	Scalar clr;
	int thick;
	iLine(int x1, int y1, int x2, int y2, Scalar clr, int thick=3): x1(x1), y1(y1), x2(x2), y2(y2), clr(clr), thick(thick){}
	void render(Mat &);
};
class iButton
{
public:
	iToolbar *srcIToolbar;
	int x, y; // top-left coordinates
	int width, height;
	unsigned char clrR, clrG, clrB, clrA;
	Mat *bgImg;
	bool draggable;
	int prevEvent;
	int customData[MAX_CUSTOM_DATA_SIZE];
	
	void (*onHover)(iButton *, int &, int &);
	void (*onDown)(iButton *, int &, int &);
	void (*onUp)(iButton *, int &, int &);
	void (*onDrag)(iButton *, int &, int &);
	void (*onHold)(iButton *, int &, int &, int &, int &);
	void (*onRelease)(iButton *, int &, int &);
	void (*onOut)(iButton *, int &, int &);

	iButton( iToolbar *src, int x, int y, int width, int height,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0, Mat *bgImg = NULL,
		void (*onHover)(iButton *, int &, int &)=NULL, void (*onDown)(iButton *, int &, int&)=NULL, void (*onUp)(iButton *, int &, int&)=NULL,
		void (*onDrag)(iButton *, int &, int&)=NULL, void (*onHold)(iButton *, int &, int &, int &, int &)=NULL,
		void (*onRelease)(iButton *, int &, int&)=NULL, void (*onOut)(iButton *, int &, int&)=NULL,
		bool draggable = false, int prevEvent = INPUT_NONE):
		srcIToolbar(src), x(x), y(y), width(width), height(height), clrR(clrR), clrG(clrG), clrB(clrB), clrA(clrA), bgImg(bgImg),
		onHover(onHover), onDown(onDown), onUp(onUp), onDrag(onDrag), onHold(onHold), onRelease(onRelease), onOut(onOut),
		draggable(draggable), prevEvent(prevEvent){}
	
	void render(Mat &);
};

class iToolbar
{
public:
	int x, y;
	int width, height;
	int padding;
	unsigned char clrR, clrG, clrB, clrA;
	Mat *bgImg;
	list<iButton *> lisBtns;
	int rightX;

	iToolbar(int x, int y, int width, int height, int padding = 2,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0, Mat *bgImg = NULL) :
		x(x), y(y), width(width), height(height), padding(padding), clrR(clrR), clrG(clrG), clrB(clrB), clrA(clrA), bgImg(bgImg), rightX(0) {}

	iButton* createButton(int w = 0, int h = 0, unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0, Mat *bgImg = NULL,
		void (*onHover)(iButton *, int &, int &)=NULL, void (*onDown)(iButton *, int &, int&)=NULL, void (*onUp)(iButton *, int &, int&)=NULL,
		void (*onDrag)(iButton *, int &, int&)=NULL, void (*onHold)(iButton *, int &, int &, int &, int &)=NULL,
		void (*onRelease)(iButton *, int &, int&) = NULL, void (*onOut)(iButton *, int &, int&) = NULL,
		int prevEvent = INPUT_NONE);

	void render(Mat &);
};

class iObject
{
public:
	iScene *srcScene;
	int x, y; // top-left coordinates
	int width, height;
	unsigned char clrR, clrG, clrB, clrA;
	Mat *bgImg; unsigned char bgImgA;
	bool visible, draggable;
	int prevEvent;
	int customData[MAX_CUSTOM_DATA_SIZE];

	void (*onHover)(iObject *, int &, int &);
	void (*onDown)(iObject *, int &, int &);
	void (*onUp)(iObject *, int &, int &);
	void (*onDrag)(iObject *, int &, int &);
	void (*onHold)(iObject *, int &, int &, int &, int &);
	void (*onRelease)(iObject *, int &, int &);
	void (*onOut)(iObject *, int &, int &);

	iObject(iScene *srcScene, int x, int y, int width, int height,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0,
		Mat *bgImg = NULL, unsigned char bgImgA = 255, bool visible = true, bool draggable = false,
		void (*onHover)(iObject *, int &, int &)=NULL, void (*onDown)(iObject *, int &, int&)=NULL, void (*onUp)(iObject *, int &, int&)=NULL,
		void (*onDrag)(iObject *, int &, int&)=NULL, void (*onHold)(iObject *, int &, int &, int &, int &)=NULL,
		void (*onRelease)(iObject *, int &, int&)=NULL, void (*onOut)(iObject *, int &, int&)=NULL,
		int prevEvent = INPUT_NONE):
		srcScene(srcScene), x(x), y(y), width(width), height(height), clrR(clrR), clrG(clrG), clrB(clrB), clrA(clrA), 
		bgImg(bgImg), bgImgA(bgImgA), visible(visible), draggable(draggable),
		onHover(onHover), onDown(onDown), onUp(onUp), onDrag(onDrag), onHold(onHold), onRelease(onRelease), onOut(onOut),
		prevEvent(prevEvent){}

	void render(Mat &);
};

class iScene
{
public:
	int x, y;
	int width, height;
	unsigned char clrR, clrG, clrB, clrA;
	Mat *bgImg;
	list<iToolbar *> lisTBs;
	list<iObject *> lisObjs;
	list<iLine> lisLns;

	iScene(int x, int y, int width, int height,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0, Mat *bgImg = NULL) :
		x(x), y(y), width(width), height(height), clrR(clrR), clrG(clrG), clrB(clrB), clrA(clrA), bgImg(bgImg) {}

	iToolbar* createToolbar(int x, int y, int width, int height, int padding = 2,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0, Mat *bgImg = NULL);

	iObject* createObject(int x, int y, int width, int height,
		unsigned char clrR = 255, unsigned char clrG = 255, unsigned char clrB = 255, unsigned char clrA = 0,
		Mat *bgImg = NULL, unsigned char bgImgA = 255, bool visible = true, bool draggable = false,
		void (*onHover)(iObject*, int &, int &)=NULL, void (*onDown)(iObject*, int &, int&)=NULL, void (*onUp)(iObject*, int &, int&)=NULL,
		void (*onDrag)(iObject*, int &, int&)=NULL, void (*onHold)(iObject*, int &, int &, int &, int &)=NULL,
		void (*onRelease)(iObject*, int &, int&) = NULL, void (*onOut)(iObject*, int &, int &)=NULL,
		int prevEvent = INPUT_NONE);
	void deleteObject(iObject *);
	void clearObject();

	void createLine(int &, int &, int &, int &, Scalar &);

	void render(Mat &);
};

template<typename T>
class aniItem
{
public:
	T *target;
	T stVal;
	clock_t st, ed, tot, lT;
	void (*posFunc)(T *, T *, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *);
	int argc; double *args;
	void (*callback)();
	
	aniItem(T *target, clock_t st, clock_t ed, 
		void (*posFunc)(T *, T *, const clock_t &, const clock_t &, const clock_t &, const clock_t &, const int &, const double *) = NULL, 
		int argc = 1, double* args = NULL, void (*callback)() = NULL) : target(target), st(st), ed(ed), 
		posFunc(posFunc), argc(argc), args(args), callback(callback){
		tot = ed - st + 1;
		stVal = *target;
		lT = st;
	}
};


#endif