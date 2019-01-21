/*------------------- Team 12 -------------------
|	CONTENTS									|
|	1. Libraries								|
|	2. Global Variables							|
|		a. You can change video filename here	|
|	3. Object Classes							|
|		a. Color								|
|		b. ColorBox								|
|	4. Functions Headers						|
|	5. Main Function							|
|	6. Functions								|
|		a. ROI 									|
|		b. Training Phase						|
|		c. Testing Phase						|
-------------------------------------------------*/

/** 1. Libraries **/
#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>
#include <vector>
#include <conio.h>
#include <Windows.h>

using namespace cv;
using namespace std;

/** 2. Global Variables **/
bool ldown = false; // Left Mouse Button Down (is clicking)
bool lup = false; // Left Mouse Button Up (not clicking)
Point corner1; // Use to store Point A (x1, y1)
Point corner2; // Use to store Point B (x2, y2)
Mat ROI; // Use to store ROI image frame
Rect box; //Rectangle of ROI
bool doneCropping = false;
const int NBIN = 16;
const int BINSIZE = 256 / NBIN;
const int THRES = 50;

string filename = "parking.mp4";
//string filename = "contrysideTraffic.mp4";
//string filename = "MAQ00626.MP4";
//string filename = "Home_Intrusion.mp4";
//string filename = "WavingTrees.avi";
//string filename = "highway1.avi";
//string filename = "fountain01.avi";

VideoCapture video(filename);

/** 3. Object Classes **/
class Color
{
private:
	int colorIndex;
	int quantity;
	double weightage;

public:
	Color(int colorIndex)
	{
		this->colorIndex = colorIndex;
		this->quantity = 1;
		this->weightage = 0.0;
	}

	int getColorIndex()
	{
		return colorIndex;
	}

	Vec3b getColor()
	{
		int r = colorIndex % (NBIN * NBIN) % (NBIN)* BINSIZE + BINSIZE / 2;
		int g = colorIndex % (NBIN * NBIN) / NBIN * BINSIZE + BINSIZE / 2;
		int b = colorIndex / (NBIN * NBIN) * BINSIZE + BINSIZE / 2;

		return Vec3b(b, g, r);
	}

	int getQuantity()
	{
		return quantity;
	}

	double getWeightage()
	{
		return weightage;
	}

	void increaseQuantity()
	{
		quantity++;
	}

	void setWeightage(int& total_train_frame)
	{
		weightage = quantity / (double)total_train_frame;
	}

	bool operator< (Color& c)
	{
		return c.quantity < this->quantity;
	}
};

class ColorBox
{
private:
	vector<Color> colorList;

public:
	ColorBox() {}

	vector<Color>& getColorList()
	{
		return colorList;
	}

	int findIndex(int index)
	{
		for (int i = 0; i < colorList.size(); i++)
		{
			if (colorList[i].getColorIndex() == index)
			{
				return i;
			}
		}
		return -1;
	}

	void addColor(Vec3b color)
	{
		int index = 0;

		index += (color[0] / BINSIZE) * NBIN * NBIN;
		index += color[1] / BINSIZE * NBIN;
		index += color[2] / BINSIZE;

		int found = findIndex(index);

		if (found == -1)
		{
			Color newColor(index);
			colorList.push_back(newColor);
		}
		else
		{
			colorList[found].increaseQuantity();
		}
	}

	void sortColorList()
	{
		sort(colorList.begin(), colorList.end());
	}

	Vec3b getDominantColor()
	{
		return colorList[0].getColor();
	}
};

struct ForegroundObject
{
	Point center;
	int area;
	int x;
	int y;
	int height;
	int width;

	ForegroundObject(Point center, int area, int x, int y, int h, int w)
	{
		this->center = center;
		this->area = area;
		this->x = x;
		this->y = y;
		this->height = h;
		this->width = w;
	}

	void draw(Mat& frame)
	{
		rectangle(frame, Point(x, y), Point(x + width, y + height), Scalar(0, 0, 255), 1);
		circle(frame, center, 2, Scalar(255, 0, 0), -1);
	}
};

/** 4. Function Headers **/
//-- a. ROI
static void mouse_callback(int event, int x, int y, int, void *);
//-- b. Training Phase
Mat training(double percentage, vector<ColorBox>& background);
void put_frame_to_background(Mat& training_frame, vector<ColorBox>& background);
Mat get_background_model(vector<ColorBox>& background);
//-- c. Testing Phase
void testing(vector<ColorBox>& background);
Mat get_foreground_mask(Mat& training_frame, vector<ColorBox>& background);
bool compare_color(Vec3b a, vector<Color>& colorList);
bool matchColor(Vec3b a, Vec3b b);

/** 5. Main Functions **/
int main(void)
{
	system("Color F0"); // Set white background, black font

	//-- 1. Check whether video exist or not
	// If not, then exit the program
	if (!video.isOpened())
	{
		cout << "Can't find video file named - " << filename << "." << endl;
		system("PAUSE");
		return -1;
	}

	int total_frame = (int)video.get(CV_CAP_PROP_FRAME_COUNT);
	int video_height = (int)video.get(CV_CAP_PROP_FRAME_HEIGHT);
	int video_width = (int)video.get(CV_CAP_PROP_FRAME_WIDTH);

	//-- 2. Show the properties of the Video
	cout << "-----// Video Information //-----" << endl;
	cout << "Total Frame: " << total_frame << endl;
	cout << "Video Height: " << video_height << "px" << endl;
	cout << "Video Width: " << video_width << "px" << endl << endl;

	cout << "Press and hold mouse left button to select." << endl;
	cout << "\tor" << endl;
	cout << "Press 'q' to exit" << endl << endl;

	video.read(ROI);
	imshow("Select ROI", ROI);
	setMouseCallback("Select ROI", mouse_callback);
	while (char(waitKey(1)) != 'q' && !doneCropping) {/*Critical Section*/ }

	//-- 3. Training Phase
	vector<ColorBox> background(box.area(), ColorBox());
	Mat background_model = training(0.3, background); // train only 30% of the frame

	//-- 4. Testing Phase
	video.open(filename);
	testing(background);

	cout << "\nDone" << endl;
	system("PAUSE");
	return 0;
}

/** 6. Functions **/
//-- a. ROI
void mouse_callback(int event, int x, int y, int, void *)
{
	if (event == EVENT_LBUTTONDOWN)
	{
		ldown = true;
		corner1.x = x;
		corner1.y = y;
		cout << "Corner 1 at " << corner1 << endl;
	}

	if (event == EVENT_LBUTTONUP)
	{
		if (abs(x - corner1.x) > 10 && abs(y - corner1.y) > 10)
		{
			lup = true;

			corner2.x = x;
			corner2.y = y;

			if (y > ROI.size().height) corner2.y = ROI.size().height;
			if (x > ROI.size().width) corner2.x = ROI.size().width;

			if (y < 0) corner2.y = 0;
			if (x < 0) corner2.x = 0;

			cout << "Corner 2 at " << corner2 << endl;
		}
		else
		{
			cout << "Please select a bigger region" << endl;
			ldown = false;
		}
	}

	if (ldown == true && lup == false)
	{
		Point pt;
		pt.x = x;
		pt.y = y;

		Mat locale_img = ROI.clone();

		if (y > ROI.size().height) corner2.y = ROI.size().height;
		if (x > ROI.size().width) corner2.x = ROI.size().width;

		if (y < 0) pt.y = 0;
		if (x < 0) pt.x = 0;

		rectangle(locale_img, corner1, pt, Scalar(0, 0, 255), 2);

		imshow("Select ROI", locale_img);
	}

	if (ldown == true && lup == true)
	{
		ldown = false;
		lup = false;

		box.width = abs(corner1.x - corner2.x);
		box.height = abs(corner1.y - corner2.y);

		box.x = min(corner1.x, corner2.x);
		box.y = min(corner1.y, corner2.y);

		doneCropping = true;
	}
}

//-- b. Training Phase
Mat training(double percentage, vector<ColorBox>& background)
{
	if (percentage <= 0.0 || percentage > 1.0)
	{
		percentage = 0.3;
	}

	Mat training_frame;
	int total_frame = (int)video.get(CV_CAP_PROP_FRAME_COUNT);
	int total_train_frame = int(total_frame * percentage);

	cout << "\n-----// Training Phase //-----" << endl;
	cout << "Train " << percentage * 100 << "% of total " << total_frame << " frames." << endl;
	cout << "Learning Frame 1 to Frame " << total_train_frame - 1 << "." << endl;

	for (int i = 1; i <= total_train_frame - 1; i++)
	{
		video.read(training_frame);
		training_frame = training_frame(box);
		imshow("Learning Progress", training_frame);
		waitKey(1);

		put_frame_to_background(training_frame, background);

		cout << "\r" << (i * 100 / total_train_frame) + 1 << "%";
	}
	Mat background_model = get_background_model(background);
	imshow("Background Trained", background_model);
	waitKey(1);

	for (int i = 0; i < background.size(); i++)
	{
		for (int j = 0; j < background[i].getColorList().size(); j++)
		{
			background[i].getColorList().at(j).setWeightage(total_train_frame);

		}
	}

	cout << endl;
	Sleep(1000); // Sleep for 1 second
	destroyWindow("Select ROI");
	destroyWindow("Learning Progress");
	return background_model;
}

void put_frame_to_background(Mat& training_frame, vector<ColorBox>& background)
{
	int cols = box.width;
	int rows = box.height;

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			background[i*cols + j].addColor(training_frame.at<Vec3b>(i, j));
			background[i*cols + j].sortColorList();
		}
	}
}

Mat get_background_model(vector<ColorBox>& background)
{
	int cols = box.width;
	int rows = box.height;

	Mat background_model(rows, cols, CV_8UC3);
	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			background_model.at<Vec3b>(i, j) = background[i*cols + j].getDominantColor();
		}
	}
	return background_model;
}

//-- c. Testing Phase
void testing(vector<ColorBox>& background)
{
	moveWindow("Background Trained", 100, 100);

	Mat testing_frame;
	Mat foreground_mask, display;
	int frame_counter = 1;
	int total_frame = (int)video.get(CV_CAP_PROP_FRAME_COUNT);

	cout << "\n-----// Testing Phase //-----" << endl;
	cout << "Testing " << total_frame << "frames." << endl;

	for (int i = 1; i <= total_frame - 1; i++)
	{
		video.read(testing_frame);
		testing_frame = testing_frame(box);
		imshow("Original Video", testing_frame);
		moveWindow("Original Video", 300, 100);

		foreground_mask = get_foreground_mask(testing_frame, background);
		bitwise_not(foreground_mask, display);
		imshow("Foreground Mask", display);
		moveWindow("Foreground Mask", 100, 300);

		Mat structElement = getStructuringElement(MORPH_ELLIPSE, Size(2, 2));
		morphologyEx(foreground_mask, foreground_mask, MORPH_OPEN, structElement);
		structElement = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
		morphologyEx(foreground_mask, foreground_mask, MORPH_CLOSE, structElement);
		//morphologyEx(foreground_mask, foreground_mask, MORPH_CLOSE, structElement);
		bitwise_not(foreground_mask, display);
		imshow("Morphology Transformations", display);
		moveWindow("Morphology Transformations", 300, 300);

		Mat labels, stats, centroids;
		int nLabels = connectedComponentsWithStats(foreground_mask, labels, stats, centroids);
		vector<ForegroundObject> f_objects;
		for (int i = 0; i < nLabels; i++)
		{
			if (stats.at<int>(i, CC_STAT_AREA) < 80)
			{
				continue;
			}
			int x = stats.at<int>(i, CC_STAT_LEFT);
			int y = stats.at<int>(i, CC_STAT_TOP);
			int height = stats.at<int>(i, CC_STAT_HEIGHT);
			int width = stats.at<int>(i, CC_STAT_WIDTH);

			ForegroundObject current_object(Point(centroids.at<double>(i, 0), centroids.at<double>(i, 1)), stats.at<int>(i, CC_STAT_AREA), x, y, height, width);
			f_objects.push_back(current_object);
		}

		for (int i = 0; i < f_objects.size(); i++)
		{
			f_objects[i].draw(testing_frame);
		}

		imshow("Testing Result", testing_frame);
		moveWindow("Testing Result", 500, 300);

		cout << "\r" << (i * 100 / total_frame) + 1 << "%";
		waitKey(1);
	}

	cout << endl;
}

Mat get_foreground_mask(Mat& testing_frame, vector<ColorBox>& background)
{
	int cols = box.width;
	int rows = box.height;

	Mat foreground_mask(rows, cols, CV_8UC1);
	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			foreground_mask.at<uchar>(i, j) = 255 *
				!compare_color(testing_frame.at<Vec3b>(i, j), background[i*cols + j].getColorList());
		}
	}
	return foreground_mask;
}

bool compare_color(Vec3b a, vector<Color>& colorList)
{
	double accumulate = 0;
	for (int i = 0; accumulate < 0.75; i++) {

		if (matchColor(a, colorList[i].getColor()))
		{
			return true;
		}
		accumulate += colorList[i].getWeightage();
	}
	return false;
}

bool matchColor(Vec3b a, Vec3b b)
{
	for (int i = 0; i < 3; i++)
	{
		if (abs(a.val[i] - b.val[i]) > THRES)
		{
			return false;
		}
	}
	return true;
}