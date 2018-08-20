#include"stdafx.h"
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <iostream>
#include <opencv2\highgui.hpp>
#include <opencv2\imgproc.hpp>
#include <Windows.h>
#include <iostream>
#include<tesseract\baseapi.h>
#include <leptonica\allheaders.h>
#include<zlib.h>
#include<string.h>

#define WIN_NAME "자동차 번호판 인식"
#define IMAGE_PATH "k1.jpg" // 1, 5, 6
#define CANNY_THRESHOLD_1 100
#define CANNY_THRESHOLD_2 200
#define MAX_WIDTH_HEIGHT_RATIO 3.5
#define MIN_WIDTH_HEIGHT_RATIO 0.5
#define MAX_RECT_AREA 2000
#define MIN_RECT_AREA 300
#define GRADIENT_THRESHOLD 0.1

using namespace std;
using namespace cv;

/**
*	메인 메소드
*/
int main(int argc, char *argv[]) {
	char* UTF8ToANSI(char*);
	// 이미지 로드
	Mat image = imread(IMAGE_PATH);
	Mat temp = imread("k2.jpg");
	Mat proccessedImage;
	int rows = image.rows;
	int cols = image.cols;
	imshow("Initial Image", image);
	// 그레이 스케일로 변환
	cvtColor(image, proccessedImage, CV_BGR2GRAY);

	/*byte* data = (byte*)image.data;
	byte gray = 0, r, g, b;
	int length = rows * cols*3;
	for (int i = 0; i < length; i+=3) {
		r = data[i + 0];
		g = data[i + 1];
		b = data[i + 2];
		gray = 0.299*r + 0.587*g + 0.114*b;
		if (gray < 0)
			gray = 0;
		if (gray > 255)
			gray = 255;
		data[i + 0] = gray;
		data[i + 1] = gray;
		data[i + 2] = gray;
	}*/
//	GaussianBlur(proccessedImage, proccessedImage,Size(5,5),0, 0);
	// canny 알고리즘을 이용하여 윤곽선만 남김
	Canny(proccessedImage, proccessedImage, CANNY_THRESHOLD_1, CANNY_THRESHOLD_2);

//	dilate(proccessedImage, proccessedImage, Mat(), Point(0, 0));
	// 윤곽선을 가져옴
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	findContours(proccessedImage, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point());
	vector<vector<Point>> contoursPoly(contours.size());
	vector<Rect> boundRect(contours.size());
	vector<Rect> boundRect2(contours.size());
	// 윤곽선 마다 사각형을 씌워 boundRect에 저장
	// 같은 값이 2개씩 나옴
	for (int i = 0; i < contours.size(); i++) {
		approxPolyDP(Mat(contours[i]), contoursPoly[i], 1, true);
		boundRect[i] = boundingRect(Mat(contoursPoly[i]));
	}

	// 번호판 후보군 갯수 저장 변수
	int refineryCount = 0;

	// 윤곽선 중 번호판 후보가 될 수 있는 것만 남김 (가로, 세로 비율 및 크기로 판단)
	for (int i = 0; i < contours.size(); i++) {
		double widthHeightRatio = (double)boundRect[i].height / boundRect[i].width;
		if (widthHeightRatio <= MAX_WIDTH_HEIGHT_RATIO &&
			widthHeightRatio >= MIN_WIDTH_HEIGHT_RATIO &&
			boundRect[i].area() <= MAX_RECT_AREA &&
			boundRect[i].area() >= MIN_RECT_AREA) {
			//rectangle(temp, boundRect[i].tl(), boundRect[i].br(), Scalar(255, 0, 0), 1, 8, 0);
			boundRect2[refineryCount++] = boundRect[i];
		}
	}
	// imshow("contours&rectangels", temp);

	boundRect2.resize(refineryCount);
	// 추려낸 번호판 후보군 윤곽선을 x 좌표 기준으로 정렬
	for (int i = 0; i < boundRect2.size(); i++) {
		for (int j = 0; j < boundRect2.size()-i - 1; j++) {
			if (boundRect2[j].tl().x > boundRect2[j + 1].tl().x) {
				Rect tmpRect = boundRect2[j];
				boundRect2[j] = boundRect2[j + 1];
				boundRect2[j + 1] = tmpRect;
			}
		}
	}
	// 번호판 윤곽선 객체의 시작 인덱스
	int startIdx = 0;

	// 인접 윤곽선 갯수 저장 변수
	int adjecentCount = 0;

	// 번호판 가로 길이 저장 변수
	double carLicensePlateWidth = 0;

	for (int i = 0; i < boundRect2.size(); i++) {
		int count = 0;
		double deltaX = 0.0;

		for (int j = i + 1; j < boundRect2.size(); j++) {
			// 현재 윤곽선의 x 좌표와 최단 거리 인접 윤곽선의 x 좌표 사이의 거리를 구함
			deltaX = abs(boundRect2[j].tl().x - boundRect2[i].tl().x);

			// 거리가 너무 떨어져 있는 경우 후보군에서 제외 시킴
			if (deltaX > (double)MIN_RECT_AREA) {
				break;
			}

			// 기울기를 구할 때 0으로 나누는 예외 방지
			if (deltaX == 0.0) {
				deltaX = 1.0;
			}

			// 현재 윤곽선의 y 좌표와 최단 거리 인접 윤곽선의 y 좌표 사이의 거리를 구함
			double deltaY = abs(boundRect2[j].tl().y - boundRect2[i].tl().y);

			// 기울기를 구할 때 0으로 나누는 예외 방지
			if (deltaY == 0.0) {
				deltaY = 1.0;
			}

			// 두 윤곽선의 기울기를 구함
			double gradient = deltaY / deltaX;

			// 기울기가 미세하게 차이가 있는 경우 인접 윤곽선으로 추가
			if (gradient < GRADIENT_THRESHOLD) {
				count++;	
			}
		}

		// 최대 인접 윤곽선 갯수 상태 저장
		if (count > adjecentCount) {
			startIdx = i;
			adjecentCount = count;
			carLicensePlateWidth = deltaX;
		}
	}
	// 윈도우 생성
//	namedWindow(WIN_NAME);
	
	// 윈도우에 결과 표시
	/*imshow(
		WIN_NAME,
		image(
			Rect(
				boundRect2[startIdx].tl().x - 20,
				boundRect2[startIdx].tl().y - 20,
				320,
				carLicensePlateWidth * 0.3
			)
		)
	);*/
	Mat proccessed = image(Rect(boundRect2[startIdx].tl().x - 20,
		boundRect2[startIdx].tl().y - 20,
		carLicensePlateWidth,
		carLicensePlateWidth * 0.3));
	Mat mask = getStructuringElement(MORPH_RECT, Size(3, 3),Point(1, 1));
	GaussianBlur(proccessed, proccessed, Size(3,3), 0, 0);
	cvtColor(proccessed,proccessed, CV_BGR2GRAY);
	threshold(proccessed, proccessed, 100,255, THRESH_BINARY);
	//cvErode(proccessed, proccessed, NULL, 1);
	erode(proccessed, proccessed, mask, Point(-1, -1), 1, 0,3);
	//erode(proccessed,proccessed,)
	imshow("asd", proccessed);
	tesseract::TessBaseAPI tess;
	if (tess.Init(NULL, "car2")) { 
		printf("fail to initialize");
		exit(1);
	}
	//Pix *iimage = pixRead(IMAGE_PATH);
	tess.SetImage((uchar*)proccessed.data, proccessed.size().width, proccessed.size().height, proccessed.channels(), proccessed.step1());
	tess.Recognize(0);

	char* out = UTF8ToANSI(tess.GetUTF8Text());
	printf("%s\n", out);
	tess.End();
	delete[] out;

	cout << "아무 키나 누르시면 종료합니다." << endl;

	waitKey();
	
	return 0;
}
char* UTF8ToANSI(char *pszCode)
{
	BSTR    bstrWide;
	char*   pszAnsi;
	int     nLength;
	// bstrWide 배열 생성 Lenth를 읽어 온다.
	nLength = MultiByteToWideChar(CP_UTF8, 0, pszCode, lstrlen((LPWSTR)pszCode) + 1, NULL, NULL);

	// bstrWide 메모리 설정

	bstrWide = SysAllocStringLen(NULL, nLength);

	MultiByteToWideChar(CP_UTF8, 0, pszCode, lstrlen((LPWSTR)pszCode) + 1, bstrWide, nLength);


	// char 배열 생성전 Lenth를 읽어 온다.
	nLength = WideCharToMultiByte(CP_ACP, 0, bstrWide, -1, NULL, 0, NULL, NULL);

	// pszAnsi 배열 생성
	pszAnsi = new char[nLength];

	// char 변환
	WideCharToMultiByte(CP_ACP, 0, bstrWide, -1, pszAnsi, nLength, NULL, NULL);


	// bstrWide 메모리 해제
	SysFreeString(bstrWide);

	return pszAnsi;
}

