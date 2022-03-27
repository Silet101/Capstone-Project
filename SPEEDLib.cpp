#define INT8 signed __int8
#define INT16 signed __int16
#define INT32 signed __int32
#define UINT8 unsigned __int8
#define UINT16 unsigned __int16
#define UINT32 unsigned __int32

#include "TIFF_Stuff.h"
#include "HalftoningSection.h"
#include <omp.h>
#include <iostream>
#include <string.h>
//#include <stdio.h>
using namespace std;

int main(int argc, const char* argv[])
{
	TIFFHeader MyTIFFHeader;
	EDParams MyEDParams;
	//int maxThreads = omp_get_max_threads();
	int maxCores = 0;
	int pageScalingPer = 0;

	for (int i = 0; i < argc; i++) {
		if (string(argv[i]).substr(0,2) == "-v") {				// updates Verbose mode
			MyTIFFHeader.bVerbose = true;
		}
		else if (string(argv[i]).substr(0, 2) == "-x") {		// check constraints and change Horizontal Resolution
			int temp = stoi(string(argv[i]).substr(2));
			if (temp <= 50) {
				MyTIFFHeader.nHorizontalDPI = 50;
			}
			else if (temp >= 3200) {
				MyTIFFHeader.nHorizontalDPI = 3200;
			}
			else {
				MyTIFFHeader.nHorizontalDPI = temp;
			}
		}
		else if (string(argv[i]).substr(0, 2) == "-y") {		// check constraints and change Vertical Resolution
			int temp = stoi(string(argv[i]).substr(2));
			if (temp <= 50) {
				MyTIFFHeader.nVerticalDPI = 50;
			}
			else if (temp >= 3200) {
				MyTIFFHeader.nVerticalDPI = 3200;
			}
			else {
				MyTIFFHeader.nVerticalDPI = temp;
			}
		}
		else if (string(argv[i]).substr(0, 2) == "-s") {		// check constraints and change Page Scaling Percentage
			int temp = stoi(string(argv[i]).substr(2));
			if (temp <= 25) {
				pageScalingPer = 25;
			}
			else if (temp >= 400) {								// *NOT SURE WHAT UNIFORM PAGE SCALING PERCENTAGE ATTRIBUTES TO*
				pageScalingPer = 400;							// *CHANGE LATER*
			}
			else {
				pageScalingPer = temp;
			}
		}
		else if (string(argv[i]).substr(0, 2) == "-p") {		// check constraints and change Max Cores
			int temp = stoi(string(argv[i]).substr(2));
			if (temp <= 1) {
				maxCores = 1;
			}
			else if (temp >= 512) {
				maxCores = 512;
			}
			else {
				maxCores = temp;
			}
		}
		else if (string(argv[i]).substr(0, 2) == "-i") {		// updates filepath for the TIFF file
			MyTIFFHeader.strInputFile = string(argv[i]).substr(2);
		}
	}

	INT16 getImageDimensions = _GetInputImageDimensions(&MyTIFFHeader);			// opens TIFF file and gets the dimensions?

	//MyEDParams.bInputImageIsRGB = MyTIFFHeader.bInputImageIsRGB;

	INT16 rasterRow = HalftoneRasterRow(&MyEDParams, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, maxCores, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	INT16 halftoneImageFlt = HalftoneImageFlt(&MyEDParams, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, maxCores);
	INT16 generateRTLData = GenerateRTLData(MyEDParams, NULL, NULL, NULL);


	return 0;
}