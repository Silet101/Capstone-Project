// Copyright 2012, 2014, 2016 - 2020, 2021 Cassinian Software, LLC
// This copyright notice must be included in all uses of this code, including derivative works

#define INT8	signed __int8
#define INT16	signed __int16
#define INT32	signed __int32
#define UINT8 	unsigned __int8
#define UINT16	unsigned __int16
#define UINT32	unsigned __int32

// Error Diffusion Kernels:
//  0 = dKernela_3x2 		 3 weights
//  1 = dKernelb_3x2         4 weights
//  2 = dKernela_2x3         5 weights
//  3 = dKernela_3x3         5 weights
//  4 = dKernelb_3x3         7 weights
//  5 = dKernela_5x2         7 weights
//  6 = dKernelb_5x2         7 weights
//  7 = dKernelc_5x2         7 weights
//  8 = dKerneld_5x2         6 weights
//  9 = dKernela_5x3        12 weights
// 10 = dKernelb_5x3        12 weights
// 11 = dKernelc_5x3        10 weights
// 12 = dKerneld_5x3        10 weights
// 13 = dKernele_5x3         6 weights
// 14 = dKernela_7x4        18 weights
// 15 = dKernelb_7x4        20 weights (experimental)
// 16 = dKernelc_3x2         4 weights (experimental)

typedef struct EDParameters {
	bool bEnableParallelExecution	= true;								// Enable parallel execution
	bool bSerpentineRaster			= true;								// Enable serpentine processing of raster data
	UINT8 nEDKernelType				= 16;								// Used to select diffusion kernel (0 - 16)
	UINT8 nInputBitDepth 			= 16;								// Error diffusion bit depth, 8 or 16, >= nImageBitDepth
	UINT8 nImageBitDepth 			= 8;								// Input raster image bit depth, 8 or 16 bits/color
	bool bInputImageIsRGB 			= false;							// Input image is RGB, so must be converted to CMYK @ 16-bit
	float dHysteresis 				= 0.15F;							// Value between 0 and 1 for white noise intensity
	UINT8 nColorChannels			= 4;								// This will be 4 for now (CMYK) but could be up to 16 colors
	UINT8 nBitsPerDot				= 2;								// 1 = fixed dot size, 2 = variable dot (S, M, L)
	UINT8 nDotLevels				= 4;								// Number of unique dot sizes, including 0 (no dot)
	UINT8 nDotsPerByteBlock			= 8;								// (DotBlockBytes * BYTESIZE) / BitsPerDot
	float*** pFloatErrorBuffer[2]	= { NULL, NULL };					// 3D floating point error buffer
	UINT8* pDotLUT					= NULL;								// Pointer to the dot lookup table = (2 ^ nInputBitDepth)
	float* pFloatErrorLUT			= NULL;								// Pointer to the error lookup table associated with pDotLUT
	UINT8 nInkOrder[16]				= { 1, 2, 3, 0, 0, 0, 0, 0,			// Ink color order, C (1), M (2), Y (3), K (0)
										 0, 0, 0, 0, 0, 0, 0, 0 };
	INT16 nErrorCode 				= 0;								// Return error code (for allocation function)
	TCHAR sRetErrDescription[128];										// Return error message
} EDParams;

static const UINT8 
	nKernelHeight[] = { 2, 2, 3, 3, 3, 2, 2, 2, 						// Kernel height = height of the error buffer
						2, 3, 3, 3, 3, 3, 4, 4, 2 };

// *********************************************************************************************************************************
// GenerateRTLData() is used to call HalftoneImageFlt() to halftone a band of image data; add your own code to open an image file
// Image data must be CMYK, pixel order (i.e. CMYKCMYKCMYKCMYK.., not CCCCMMMMYYYYKKKK..) either 8 or 16 bits/channel (32/64 bits/pixel)
// Image bands can be up to 255 rows tall; the halftone algorithm will scale to appropriate size for printing (up to 65,535 rows tall)
//
INT16 GenerateRTLData(													// This is a new function, for this example
	EDParams CurrentParams,												// Pointer to structure holding ED parameters
	UINT32 nRasterWidthPixels,											// RasterWidthByteBlocks * DotBlockBytes (zero padded)
	UINT16 nRasterWidthByteBlocks,										// Width of output raster row in byte blocks
	UINT16 nRasterBufferHeight) {										// Height of output raster buffer
	
	UINT8* nRTLDoubleBuffer[2][16]{};									// RTL data buffer (dot data, raster data is pixels)
	UINT32 nicInt = nRasterWidthPixels;									// RTL data must fall on a byte boundary
	
	if ((nicInt % CurrentParams.nColorChannels) != 0)
		nicInt += (CurrentParams.nColorChannels - (nicInt % CurrentParams.nColorChannels));
	
	for (int lop = 0; lop < CurrentParams.nColorChannels; lop++) {		// Allocate RTL data buffers
		for (int lp = 0; lp < 2; lp++) {								// This needs to be a double buffer
			if ((nRTLDoubleBuffer[lp][lop] = 							// Need to make sure we end on a byte boundary
				(UINT8*)calloc((size_t)nicInt * 
					(size_t)CurrentParams.nRasterBufferHeight, sizeof(UINT8))) == NULL) {

				CurrentParams.nErrorCode = (-62);						// Buffer failed to allocate, so stop
				swprintf_s(CurrentParams.sRetErrDescription,			// Report error back upstream
					sizeof(CurrentParams.sRetErrDescription),
					_T("EC(-62b) Failed to allocate PRN loop buffer!"));
				return CurrentParams.nErrorCode;
			}
		}
	}
	
	for (eblp = 0; eblp < 2; eblp++) {									// Allocate error buffers, double buffer even/odd
		if ((CurrentParams.pFloatErrorBuffer[eblp] =					// Allocate and initialize the buffers to 0
			(float ***)calloc((size_t)nKernelHeight[CurrentParams.nEDKernelType], 
				sizeof(float **))) != NULL) {							// The buffer height = error kernel height
	
			for (x = 0; x < nKernelHeight[CurrentParams.nEDKernelType]; x++) {
				if ((CurrentParams.pFloatErrorBuffer[eblp][x] =			// Each color channel is processed separate
					(float **)calloc((size_t)CurrentParams.nColorChannels, 
						sizeof(float *))) != NULL) {
	
					for (y = 0; y < CurrentParams.nColorChannels; y++) {
						if ((CurrentParams.pFloatErrorBuffer[eblp][x][y] =
							(float *)calloc((size_t)nRasterWidthPixels, 
								sizeof(float))) != NULL) {				// The buffer width = width of the output pixel buffer
							CurrentParams.nErrorCode = 0;
						}
						else {											// Buffer (width) could not be initialized
							CurrentParams.nErrorCode = (-18);			// Report error
							swprintf_s(CurrentParams.sRetErrDescription,
								_countof(CurrentParams.sRetErrDescription),
								_T("EC(-18) Failed to allocate float error buffer A!"));
							return CurrentParams.nErrorCode;
						}
					}
				}
				else {
					CurrentParams.nErrorCode = (-18);					// Buffer (channels) could not be initialized
					swprintf_s(CurrentParams.sRetErrDescription,		// Report error
						_countof(CurrentParams.sRetErrDescription),
						_T("EC(-18) Failed to allocate float error buffer B!"));
					return CurrentParams.nErrorCode;
				}
			}
		}
		else {
			CurrentParams.nErrorCode = (-18);							// Buffer (height) could not be initialized
			swprintf_s(CurrentParams.sRetErrDescription,				// Report error
				_countof(CurrentParams.sRetErrDescription),
				_T("EC(-18) Failed to allocate float error buffer C!"));
			return CurrentParams.nErrorCode;
		}
	}
	UINT32 nLUTByteSize = (UINT32)powf(2., 								// Total size of pDotLUT lookup tables in bytes
		(float)CurrentParams.nInputBitDepth);							// Either 256 (8-bit) or 65535 (16-bit) halftone
	
	if ((CurrentParams.pDotLUT = 
		(UINT8*)calloc((size_t)nLUTByteSize * 3,						// Pointer to the dot lookup table = (2 ^ nInputBitDepth)
			sizeof(UINT8))) == NULL) {									// This will be populated with values from the
																		//  dot lookup table file
		CurrentParams.nErrorCode = (-14);
		swprintf_s(CurrentParams.sRetErrDescription,
			sizeof(CurrentParams.sRetErrDescription),
			_T("EC(-14) Failed to allocate dot density LUT!"));
		return CurrentParams.nErrorCode;								// EC(-14) Failed to allocate dot density LUT
	}
	UINT8 nEDKernelSize = 												// pKernels[n] points to our kernel array
		nKernelHeight[CurrentParams.nEDKernelType] * 7;					// First element of the array is the kernel size
																		// All kernels are 7 wide
	if ((CurrentParams.pFloatErrorLUT =									// Pointer to the error lookup table associated with pDotLUT
		(float*)calloc((size_t)nLUTByteSize *							// This LUT holds the Q errors associated with each dot
			nEDKernelSize, sizeof(float))) == NULL) {					// It is the difference between printed dot density and
																		//  and the desired pixel value
		CurrentParams.nErrorCode = (-19);								// Like pDotLUT, this will be populated with values from
		swprintf_s(CurrentParams.sRetErrDescription,					//  the dot lookup table file
			sizeof(CurrentParams.sRetErrDescription),
			_T("EC(-19) Failed to allocate float error LUT!"));
		return CurrentParams.nErrorCode;
	}
	
	/* Allocate your input and output raster buffers here */
	
	/* Open your image file and copy it into your input raster
	   buffer; remember that the max input buffer height is 255
	   rows, so if the image height is more than that, pass it
	   as image bands <= 255 tall */
	
	/* The input image for this example should be CMYK; and
	   remember, the halftone algorithm scales for you */
	
	/* Call HalftoneImageFlt() here after allocating your
	   input and output raster buffers */
	   
}

// *********************************************************************************************************************************
// HalftoneImageFlt() is used to spawn multiple instances of HalftoneRasterRow(), each executing in a separate concurrtent thread
// Threads are configured with thread-local variables and execute one per virtual core, up to 8 (i.e. 8 cores or 4 HT cores)
// You pass your raster image band to this function, and it then splits the image band into color channels and odd/even raster rows
// It synchronizes the concurrent instances of HalftoneRasterRow() and returns a scaled and assembled halftoned image band
// 
INT16 HalftoneImageFlt(
	EDParams* CurrentParams,											// Pointer to structure holding ED parameters
	void* pInputRasterBuffer,											// Pointer to a buffer containing raster data
	UINT8* pOutputRasterBuffer,											// Pointer to the output raster (halftone) buffer
	UINT16 nInputImagePixelWidth,										// Pixel wiodth of the input image buffer, max 65535 columns
	UINT8 nInputImageBufferRows,										// Number of rows in the input image buffer, max 255 per band
	UINT32 nRasterWidthPixels,											// RasterWidthByteBlocks * DotBlockBytes (zero padded)
	UINT16 nRasterBufferHeight,											// Height of output raster buffer	
	UINT8* pRTLDataBuffer[16],											// RTL data buffer (dot data, raster data is pixels)
	float* dTIFFDotLevelPct,											// Used to calculate dot level for simulated (TIFF) image
	UINT32 nDotVol[16][16],												// Used to report ink drop volume by dot size and color
	UINT16 nNumberOfRasterRows,											// The number of raster rows to be processed
	int nThreads) {														// Number of cores we have available, max = 8

	UINT8 nWrkrThrd;													// 2 = Interlacing raster rows, 1 = not interlacing
	UINT32 nThreadDotVol[2][16][16]{};									// Used to tabulate ink usage, see nDotVol[][] (below)
	float dRandRange, nscl, nrs, nrf, dMaxPixVal;						// Variables for noise generator and default max pixel value

	if (CurrentParams->nInputBitDepth == 8 && 							// Selected error diffusion bit depth
		CurrentParams->nImageBitDepth == 16) {							// Raster image bit depth
																		// nInputBitDepth must be >= nImageBitDepth
		CurrentParams->nErrorCode = (-71);								// So report an error back upstream
		swprintf_s(CurrentParams->sRetErrDescription,
			sizeof(CurrentParams->sRetErrDescription),
			_T("EC(-71) Cannot use 8-bit diffusion on 16-bit image!"));
		return CurrentParams->nErrorCode;
	}
	else if (CurrentParams->nInputBitDepth == 16 || 					// Error diffusion bit depth is 16
			 CurrentParams->bInputImageIsRGB) {							// If the input image was RGB, will be 16 bit regardless
		nscl = 257.F;													// Make sure the RAND range is 16 bit
		dMaxPixVal = 65535.F;											// Set max pixel value to 65535
	}
	else {
		nscl = 1;														// Make the RAND range is 8 bit
		dMaxPixVal = 255.F;												// Set max pixel value to 255
	}																	// Noise is adaptive, little bit helps
	if (CurrentParams->dHysteresis < 0. ||								// Zero = no white noise
		CurrentParams->dHysteresis > 1.F)								// Cannot be more than 1
		CurrentParams->dHysteresis = 0.15F;								// Default value for white noise intensity

	dRandRange = 4.F + (CurrentParams->dHysteresis * 24.F);				// The range for RAND = noise range, 4 - 28
	dRandRange *= nscl;													// If 16-bit, range is 1028 - 7196
	nrs = dRandRange + 1.F;												// Max adaptive noise threshold
	nrf = dRandRange / 2.F;												// Minimum adaptive noise threshold

	if (CurrentParams->bEnableParallelExecution && nThreads >= 8) {		// Parallel execution is enabled, with 8 or more CPUs
		nWrkrThrd = 2;													// Interlace raster rows, odd/even processed on different cores

#pragma omp parallel for												// OpenMP patallel for ...
		for (int cclp = 0; 												// Using 8 threads: C, M, Y, K Even + C, M, Y, K Odd
			cclp < (int)(CurrentParams->nColorChannels * 
				nWrkrThrd); cclp++) {
			
			INT8 nStep, tlop, nColorChannel;							// Step forward or back, for serpentine raster
			UINT32 nColMax;												// Total number of columns
			bool bSerpentine = false;									// We define serpentine structure here, for interlacing

			if (CurrentParams->bSerpentineRaster) {						// Alternate pass direction
				if ((cclp % 2) == 0) {									// 0, 2, 4, 6
					nStep = 1;											// Forward pass
					nColMax = 0;										// Start at column 0 (left-to-right)
					tlop = 0;											// Start row loop at zero
					nColorChannel = cclp / 2;							// Color channel sequence = 0, 1, 2, 3
				}
				else {													// 1, 3, 5, 7
					nStep = -1;											// Reverse pass
					nColMax = nRasterWidthPixels - 1;					// Start at last column (right-to-left)
					tlop = 1;											// Start at row 1 for odd rows
					nColorChannel = (cclp - 1) / 2;						// Color channel sequence = 0, 1, 2, 3
				}
			}
			else {														// Same pass direction, not doing serpentine (for testing)
				nStep = 1;												// Forward pass
				nColMax = 0;											// Always start at column 0

				if ((cclp % 2) == 0) {									// 0, 2, 4, 6
					tlop = 0;											// Start at row 0 for even rows
					nColorChannel = cclp / 2;							// Color channel sequence = 0, 1, 2, 3
				}
				else {													// 1, 3, 5, 7
					tlop = 1;											// Start at row 1 for odd rows
					nColorChannel = (cclp - 1) / 2;						// Color channel sequence = 0, 1, 2, 3
				}
			} // Now, call HalftoneRasterRow() to start the error diffusion process...

			HalftoneRasterRow(CurrentParams, pInputRasterBuffer, pOutputRasterBuffer, tlop, nStep,
				pRTLDataBuffer, nNumberOfRasterRows, nColorChannel, nColMax, nscl, nThreads, dMaxPixVal, 
				nrs, nrf, nWrkrThrd, dTIFFDotLevelPct, nThreadDotVol[tlop][nColorChannel], bSerpentine, 
				nKernelHeight[CurrentParams->nEDKernelType], nRasterWidthPixels, (float)nRasterBufferHeight,
				(float)nInputImageBufferRows, (float)CurrentParams->nColorChannels, (float)nInputImagePixelWidth);
		}
	}
	else if (CurrentParams->bEnableParallelExecution && 
				nThreads >= 4) {										// Parallel execution is enabled, with 4 or more CPUs
		nWrkrThrd = 1;													// No interlaced passes, process rows sequentially

#pragma omp parallel for												// OpenMP patallel for ...
		for (int cclp = 0; 												// Only using 4 threads, C, M, Y, K
			cclp < (int)(CurrentParams->nColorChannels); cclp++) {
			
			INT8 nStep = 1, tlop = 0;									// Step forward, from row 0
			UINT32 nColMax = 0;											// Start on column 0

			HalftoneRasterRow(CurrentParams, pInputRasterBuffer, pOutputRasterBuffer, tlop, nStep,
				pRTLDataBuffer, nNumberOfRasterRows, (UINT8)cclp, nColMax, nscl, nThreads, dMaxPixVal, nrs, 
				nrf, nWrkrThrd, dTIFFDotLevelPct, nThreadDotVol[0][cclp], CurrentParams->bSerpentineRaster, 
				nKernelHeight[CurrentParams->nEDKernelType], nRasterWidthPixels, (float)nRasterBufferHeight, 
				(float)nInputImageBufferRows, (float)CurrentParams->nColorChannels, (float)nInputImagePixelWidth);
		}
	}
	else if (CurrentParams->bEnableParallelExecution && 
				nThreads >= 2) {										// Parallel execution is enabled, with 2 or more CPUs
		nWrkrThrd = 2;													// Interlaced passes, even on CPU 1, odd on CPU 2

#pragma omp parallel for												// OpenMP patallel for ...
		for (int tlop = 0; tlop < (int)nWrkrThrd; tlop++) {				// Only using 2 threads, so channels process sequentially
			INT8 nStep, cclp;											// Step forward or back, for serpentine raster
			UINT32 nColMax;												// Total number of columns
			bool bSerpentine = false;									// We define serpentine structure here, for interlacing

			if (CurrentParams->bSerpentineRaster) {						// Alternate pass direction
				if (tlop == 0) {										// Even passes
					nStep = 1;											// Forward pass, left-to-right
					nColMax = 0;										// Start at column 0
				}
				else {													// Odd passes
					nStep = -1;											// Reverse pass, right-to-left
					nColMax = nRasterWidthPixels - 1;					// Start at last column
				}
			}
			else {														// Same pass direction, not doing serpentine (for testing)
				nStep = 1;												// Forward pass for both even and odd
				nColMax = 0;											// Always start at column 0
			} // Now, call HalftoneRasterRow...

			for (cclp = 0; cclp < CurrentParams->nColorChannels; cclp++) {
				HalftoneRasterRow(CurrentParams, pInputRasterBuffer, pOutputRasterBuffer, (UINT8)tlop, 
					nStep, pRTLDataBuffer, nNumberOfRasterRows, cclp, nColMax, nscl, nThreads, dMaxPixVal, 
					nrs, nrf, nWrkrThrd, dTIFFDotLevelPct, nThreadDotVol[tlop][cclp], bSerpentine,
					nKernelHeight[CurrentParams->nEDKernelType], nRasterWidthPixels, (float)nRasterBufferHeight, 
					(float)nInputImageBufferRows, (float)CurrentParams->nColorChannels, (float)nInputImagePixelWidth);
			}
		}
	}
	else {
		nWrkrThrd = 1;													// 1 thread, no interlaced passes
		INT8 nStep = 1, tlop = 0;										// Step forward, from row 0
		UINT32 nColMax = 0;												// Start on column 0

		for (UINT8 cclp = 0; cclp < CurrentParams->nColorChannels; cclp++) {
			HalftoneRasterRow(CurrentParams, pInputRasterBuffer, pOutputRasterBuffer, (UINT8)tlop,
				nStep, pRTLDataBuffer, nNumberOfRasterRows, cclp, nColMax, nscl, nThreads, dMaxPixVal, nrs, 
				nrf, nWrkrThrd, dTIFFDotLevelPct, nThreadDotVol[0][cclp], CurrentParams->bSerpentineRaster,
				nKernelHeight[CurrentParams->nEDKernelType], nRasterWidthPixels, (float)nRasterBufferHeight, 
				(float)nInputImageBufferRows, (float)CurrentParams->nColorChannels, (float)nInputImagePixelWidth);
		}
	}
	UINT8 clp, dlp;														// Color channel loop, dot level loop

	for (clp = 0; clp < CurrentParams->nColorChannels; clp++) {			// Loop thru color channels
		for (dlp = 0; dlp < CurrentParams->nDotLevels; dlp++)			// Loop through dot levels
			nDotVol[CurrentParams->nInkOrder[clp]][dlp] +=				// Sum each dot size for each color channel
				nThreadDotVol[0][CurrentParams->nInkOrder[clp]][dlp];

		if (nWrkrThrd == 2) {											// If interlacing was employed
			for (dlp = 0; dlp < CurrentParams->nDotLevels; dlp++)		// Cycle through dot levels for odd raster rows
				nDotVol[CurrentParams->nInkOrder[clp]][dlp] +=			// Sum each odd dot size for each color channel
					nThreadDotVol[1][CurrentParams->nInkOrder[clp]][dlp];
		}
	}
	return 0;															// We were done, 0 = success
}

// *********************************************************************************************************************************
// HalftoneRasterRow() is called by HalftoneImageFlt() multiple times to scale and halftone a raster image for printing
// This function is effectively a grayscale halftone, as it only handles a single color channel
//
INT16 HalftoneRasterRow(
	EDParams* CurrentParams,											// Pointer to structure holding ED parameters
	void* pInputRasterBuffer,											// Pointer to a buffer containing raster data
	UINT8* pOutputRasterBuffer,											// Pointer to the output raster (halftone) buffer
	UINT8 tlop,															// To select the row to start on, 0 for even, 1 for odd
	INT8 nStepFN,														// Step forward (even rows) or reverse (odd rows)
	UINT8* pRTLData[16],												// RTL data buffer (dot data, raster data is pixels)
	UINT16 nCurrentRasterRow,											// The number of raster rows to be processed
	UINT8 nColorChannel,												// The color channel being processed (C, M, Y, or K)
	UINT32 nColMaxFN,													// Total number of columns
	float nscl,															// Define the RAND range, 8-bit or-16 bit
	int nThreads,														// Number of cores we have available
	float dMaxPixVal,													// Maximum pixel value, 8-bit or 16-bit
	float nrs,															// Max adaptive noise threshold
	float nrf,															// Minimum adaptive noise threshold
	UINT8 nWrkrThrd,													// 2 = Interlacing raster rows, 1 = not interlacing
	float* dTIFFDotLevelPct,											// Used to calculate dot level for simulated (TIFF) image
	UINT32 nDotVol[16],													// Used to report ink drop volume by dot size and color
	bool bDoSerpentineRaster,											// Apply serpentine raster
	UINT8 nKernelRows,													// Height of the ED kernel = 2, 3, or 4 rows
	UINT32 dBufferWidth,												// The width of the output raster buffer
	float dNewHeight,													// The new (scaled) height of the output raster band
	float dOriginalHeight,												// The height of the input image raster band
	float dColorChannels,												// The total number of color channels
	float dInputImageWidth) {											// The width of the input image raster band

	INT8 nStep = nStepFN;												// nStep is used locally, initialized with nStepFN
	UINT8 nDotOut, lpa, lpb, lpc;										// Halftoned dot, local loop counters
	UINT16 nR1C1, nR1C2, nR2C1, nR2C2, nPixelValue, cy;					// Nearest neighbors for bilinear interpolation
	UINT32 nRTLIndex, nPixelIndex, nDotLUTIndex;						// Local index variables
	UINT32 nRTLWidth, nIndexWidth, nColMaxValT, nColMax = nColMaxFN;	// Input and output raster row width
	INT32 nTempPixVal, nDotLUTCount, nDotCount;							// Local variables for storing dot counts
	float dOrgPixVal, dQErr, dQAvg, dAvgPixValue = 0;					// Local variables used for bilinear interpolation
	float dOutputWidth = (float)dBufferWidth * dColorChannels;			// Output buffer width in bytes
	float dInputWidth = dInputImageWidth * dColorChannels;				// Input buffer width in bytes

	omp_set_num_threads(nThreads);										// Number of threads to use when creating parallel region

	float dY, dX, dX1, dX2, dY1, dY2;									// These are local intermediate variables used
	float dPix, dFP, dC1, dC2, dR1, dX2X, dY2Y, dX2X1, dY2Y1;			//  for bilinear interpolation section
	float dX1Y1, dX2Y1, dX1Y2, dX2Y2, dX2X2X1, dY2Y2Y1, dR2 = 0.;
	UINT32 nQ11, nQ12, nQ21, nQ22;
    
	UINT8 nPreviewTIFFColorChannelOrder[16] = { 0, 1, 2, 3,
												0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
	for (cy = (UINT16)tlop; 
		cy < nCurrentRasterRow; cy += (UINT16)nWrkrThrd) {				// Count through rows, from top to bottom
		
		if (bDoSerpentineRaster) {										// If the rows are not interlaced, and serpentine is selected
			if ((cy % 2) == 0) {										//  alternate here: even left-to-right, odd right-to-left
				nStep = 1;												// Scan forward
				nColMax = 0;											// Start at column 0
			}
			else {														// Odd rows
				nStep = -1;												// Scan reverse
				nColMax = dBufferWidth - 1;								// Start at the last column
			}
		}
		else {															// Not doing serpentine, or using interlaced rows
			if (nWrkrThrd == 1) {										// Not using interlaced rows either
				nStep = 1;												// So all rows scan forward
				nColMax = 0;											// All rows start at first column
			}
		}																// When rows are interlaced, HalftoneImageFlt() handles this
		nColMaxValT = nColMax;											// Local variable to store backup of nColMax
		srand((unsigned)time(NULL));									// Seed RAND() with time stamp
		nRTLWidth = dBufferWidth * cy;									// Store RTL data width
		nIndexWidth = (UINT32)dOutputWidth * cy;						// Store the scaled index width
    
		while (nColMax >= 0 && nColMax < dBufferWidth) {				// Scan between column 0 and last column, forward or reverse
			nRTLIndex = nRTLWidth + nColMax;							// The RTL data width is not the same as the input raster width
			nPixelIndex = 												// We need to start with the output pixel index
				nIndexWidth + (nColMax * (UINT8)dColorChannels);		// Then figure out which pixels in the input buffer surround
			dPix = 														//  the pixel we need to halftone
				(float)(nPixelIndex + nColorChannel) / dOutputWidth;	// We will actually halftone the interpolated pixel
    
			dFP = modff((modff(dPix, &dY) * dOutputWidth), &dX);		// We need to figure out the X, Y coordinates of the corners
			dC1 = fmaxf(floorf((dX / dOutputWidth) * dInputWidth), 0.);	// Which will be 'real' pixels in the input raster buffer
			dC2 = 
				fminf(ceilf((dX / dOutputWidth) * dInputWidth), dInputWidth - 1.F);
			dR1 = fmaxf(floorf((dY / dNewHeight) * dOriginalHeight), 0.);
			dR2 = 
				fminf(ceilf((dY / dNewHeight) * dOriginalHeight), dOriginalHeight - 1.F);
    
			nQ11 = (UINT32)fmaxf((floorf(((dR1 * dInputWidth) + dC1) /	// Now we need to know what those corner points are
				dColorChannels) * dColorChannels) + (float)nColorChannel, 0.);
			nQ12 = (UINT32)fmaxf((floorf(((dR1 * dInputWidth) + dC2) /
				dColorChannels) * dColorChannels) + (float)nColorChannel, 0.);
			nQ21 = (UINT32)fmaxf((floorf(((dR2 * dInputWidth) + dC1) /
				dColorChannels) * dColorChannels) + (float)nColorChannel, 0.);
			nQ22 = (UINT32)fmaxf((floorf(((dR2 * dInputWidth) + dC2) /
				dColorChannels) * dColorChannels) + (float)nColorChannel, 0.);
    
			dX /= dOutputWidth;											// Translate our actual position in the input raster buffer
			dY /= dNewHeight;											// We need to know both our location height and width
																		//  which will most likely be between real rows and columns
			dX1 = dC1 / dInputWidth;									// Now zero in to our location inside the 4 nearest neighbors
			dX2 = dC2 / dInputWidth;
			dY1 = dR1 / dOriginalHeight;
			dY2 = dR2 / dOriginalHeight;
    
			if (CurrentParams->nImageBitDepth == 8 && 					// Input raster buffer is 8-bit, so pInputRasterBuffer is UINT8
				!CurrentParams->bInputImageIsRGB) {						// RGB input images are always converted to 16-bit CMYK!
				
				nR1C1 = (UINT16)((UINT8*)pInputRasterBuffer)[nQ11] * (UINT16)nscl;
				nR1C2 = (UINT16)((UINT8*)pInputRasterBuffer)[nQ12] * (UINT16)nscl;
				nR2C1 = (UINT16)((UINT8*)pInputRasterBuffer)[nQ21] * (UINT16)nscl;
				nR2C2 = (UINT16)((UINT8*)pInputRasterBuffer)[nQ22] * (UINT16)nscl;
			}
			else {														// Input raster buffer is 16-bit, so pInputRasterBuffer is UINT16
				nR1C1 = ((UINT16*)pInputRasterBuffer)[nQ11];			// We will always force the scaled raster to 16-bit
				nR1C2 = ((UINT16*)pInputRasterBuffer)[nQ12];			//  for improved gradient reproduction and optimum dynamic range
				nR2C1 = ((UINT16*)pInputRasterBuffer)[nQ21];
				nR2C2 = ((UINT16*)pInputRasterBuffer)[nQ22];
			}
			dX2X = fabsf(dX2 - dX);										// Figure out the location of the interpolated pixel
			dY2Y = fabsf(dY2 - dY);										// It will be somewhere between the 4 corners
			dX2X1 = fmaxf(fabsf(dX2 - dX1), 0.0001F);					// Just make sure it's not a zero value
			dY2Y1 = fmaxf(fabsf(dY2 - dY1), 0.0001F);					//  in case of an edge condition
    
			dX2X2X1 = (dX2X / dX2X1);									// Triangulate to find location relative to corners
			dY2Y2Y1 = (dY2Y / dY2Y1);									// Next four lines will weight the corners based on proximity
    
			dX1Y1 = clamp((float)nR1C1 - (dX2X2X1 * ((float)nR1C1 - (float)nR1C2)), 0.F, dMaxPixVal);
			dX2Y1 = clamp((float)nR2C1 - (dX2X2X1 * ((float)nR2C1 - (float)nR2C2)), 0.F, dMaxPixVal);
			dX1Y2 = clamp((float)nR1C1 - (dY2Y2Y1 * ((float)nR1C1 - (float)nR2C1)), 0.F, dMaxPixVal);
			dX2Y2 = clamp((float)nR1C2 - (dY2Y2Y1 * ((float)nR1C2 - (float)nR2C2)), 0.F, dMaxPixVal);
    
			nPixelValue = 0;											// Initialize the pixel value, variable gets reused
			dOrgPixVal = 												// This is the interpolated pixel value, clamped 0. - 65535.
				clamp((dX1Y1 + dX2Y1 + dX1Y2 + dX2Y2) / 4.F, 0.F, dMaxPixVal);
			
			dQErr = 													// Accumulated Q error to be applied to the current pixel
				CurrentParams->pFloatErrorBuffer[tlop][0][nColorChannel][nColMax];
			dAvgPixValue += dOrgPixVal;									// Rolling pixel average, accumulate next pixel
			dAvgPixValue /= 2.F;										// Calculate rolling average
			dQAvg = 
				1.F - (abs(dAvgPixValue - dOrgPixVal) / dMaxPixVal);	// Calculate variance in rolling pixel average
																		// Low variance = low frequency image data
			if (dOrgPixVal > 0.) {										// Error diffusion artifacts are most obvious in low freq data
				if (CurrentParams->dHysteresis == 0.)					// If hysteresis = 0, we're adding no white noise
					nPixelValue = (UINT16)clamp((INT32)roundf(			// Get the pixel value and add the p[0] Q error to it
						dOrgPixVal + dQErr), 0, (INT32)dMaxPixVal);		// This is the accumulate erro from previous pixels
				else {													// Okay, add soem white noise
					nTempPixVal = (INT32)roundf(dOrgPixVal + dQErr);	// Combine the pixel value with the p[0] Q error
					nPixelValue = (UINT16)clamp(nTempPixVal + (INT32)(	// Add white noise to the pixel+error sum
						(float)((rand() % (INT32)nrs) - (INT32)			// Low variance = more noise to prevent graininess
						nrf) * dQAvg), 0, (INT32)dMaxPixVal);			// High frequency image data does not show artifacts
				}														// Noise in high freq data degrades quality
			}															// If dOrgPixVal == 0 there is no color, so don't add error
			nDotOut = CurrentParams->pDotLUT[nPixelValue * 3];			// nPixelValue is the index to the dot LUT, nDotOut is the dot value
			nDotLUTIndex = nPixelValue * CurrentParams->nEDKernelSize;	// The index into the start of the error lookup table
			lpc = 0;													// The error offset in the error lookup table
    
			for (lpa = 0; lpa < nKernelRows; lpa++) {					// The number of rows in the error kernel
				nDotLUTCount = (INT32)nColMax - (nStep * 3);			// We need to find the right location in the error buffer
    
				for (lpb = 0; lpb < 7; lpb++) {							// All of our error kernels are 7 wide (2x7, 3x7, or 4x7)
					if (nDotLUTCount >= 0 && 							// Don't go outside the actual image buffer
						nDotLUTCount < (INT32)dBufferWidth)				// Don't care about errors that extend beyond the image width
																		// Be sure to accumulate excesss errors
						CurrentParams->pFloatErrorBuffer[tlop][lpa][nColorChannel][nDotLUTCount] +=
							CurrentParams->pFloatErrorLUT[nDotLUTIndex + lpc];
    
					nDotLUTCount += nStep;								// Used for counting dots, for reporting ink usage
					lpc++;												// Step to the next column in the error buffer
				}
			}
			
			// pOutputRasterBuffer is used to generate a TIFF image preview, so dots are converted to 8-bit values
			pOutputRasterBuffer[nPixelIndex + nPreviewTIFFColorChannelOrder[nColorChannel]] =
				(UINT8)fminf(floorf(dTIFFDotLevelPct[nDotOut] * 255.F), 255.F);
			
			// pRTLData is used to generate printer data, so dots are kept as either 1 or 2-bit values
			pRTLData[CurrentParams->nInkOrder[nColorChannel]][nRTLIndex] = nDotOut;
			nDotVol[nDotOut]++;											// This is just for counting specific dots (S, M, L)
    
			for (lpa = 1; lpa < nKernelRows; lpa++) {					// We need to move the errors from lower rows up as we go
				CurrentParams->pFloatErrorBuffer[tlop][lpa - 1][nColorChannel][nColMax] =
					CurrentParams->pFloatErrorBuffer[tlop][lpa][nColorChannel][nColMax];
			}
			CurrentParams->pFloatErrorBuffer[tlop][nKernelRows - 1][nColorChannel][nColMax] = 0.;
			nColMax += nStep;											// And then the bottom row of the error buffer is zeroed
		}																// Okay, we're done with the row
		nColMax = nColMaxValT;											// Reset the column back to left edge or right edge
	}																	// Done with all the rows now
    return 0;															// Return to reassemble the halftoned band
}
