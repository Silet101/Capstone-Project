//#include "../dependencies/HalftoningSection.cpp"
#include <iostream>
using namespace std;

//UINT8 nInputImageBuff;


int HalftoneImageBand(int inPixWidth, int inPixHeight, int*** inImageBuffer, int outPixWidth,int outPixHeight, int*** outImageBuffer,int inChan,int outChan,int bpc) {

	//Loop through outImageBuffer, from row 0 - outPixHeight, column 0 to outPixWidth

	// locate the four closest pixels that form a square surrounding

	/*
	int height = inPixWidth;
	int width = inPixWidth;

	int scale_x = round(outPixWidth/ inPixWidth);
	int scale_y = round(outPixHeight/ inPixWidth);


	int** new_image = new int* [outPixHeight];		// Create array of zeros

	for (int i = 0; i < outPixHeight; i++) {		// Fill array with 0s
		
		new_image[i] = new int[outPixWidth];

		for (int j = 0; j < outPixWidth; j++) {
			new_image[i][j]=0;

		}

	}



	int** old_image = new int* [outPixHeight];		// Create array of zeros

	for (int i = 0; i < outPixHeight; i++) {		// Fill array with 0s

		new_image[i] = new int[outPixWidth];

		for (int j = 0; j < outPixWidth; j++) {
			new_image[i][j] = 1;

		}

	}

	*/

	for (int chan = 0; chan < 3; chan++) {



		for (int row = 0; row < outPixHeight; row++) {


			for (int col = 0; col < outPixWidth; col++) {

				int x1 = floor((row / outPixWidth) * inPixWidth);
				int y1 = floor((col / outPixHeight) * inPixHeight);

				int x2 = x1 + 1;
				int y2 = y1 + 1;


				int x_diff = x1 - x2;
				int y_diff = y1 - y2;

				int a = inImageBuffer[y2][x2][chan];
				int b = inImageBuffer[y2][x2 + 1][chan];
				int c = inImageBuffer[y2 + 1][x2][chan];
				int d = inImageBuffer[y2+1][x2+1][chan];


				int pixel = a * (1 - x_diff) * (1 - y_diff) + b * (x_diff) * \
					(1 - y_diff) + c * (1 - x_diff) * (y_diff)+d * x_diff * y_diff;



				outImageBuffer[row][col][chan] = pixel;

			}
			cout << endl;
		}
	}





	for (int i = 0; i < outPixHeight; i++) {
		for (int j = 0; j < outPixWidth; j++) {

			cout << new_image[i][j];


		}
		cout << endl;
	}

	return 0;

}

int main() {
	HalftoneImageBand(5, 5, 5, 5, 5, 5, 5, 5, 5 );

}


