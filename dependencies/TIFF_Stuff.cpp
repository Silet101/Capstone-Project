// For this to work, you will need LibTIFF_Win32
// Use this link: https://www.dropbox.com/sh/wptrxa0yale28lv/AAApNnUci0xg1vCyYYnZaVCra?dl=0
// The following files from /source_4.1 need to be included in your project:

//	tif_aux.c
//	tif_close.c
//	tif_codec.c
//	tif_color.c
//	tif_compress.c
//	tif_dir.c
//	tif_dirinfo.c
//	tif_dirread.c
//	tif_dirwrite.c
//	tif_dumpmode.c
//	tif_error.c
//	tif_extension.c
//	tif_fax3.c
//	tif_fax3sm.c
//	tif_flush.c
//	tif_getimage.c
//	tif_jbig.c
//	tif_jpeg.c
//	tif_jpeg_12.c
//	tif_luv.c
//	tif_lzma.c
//	tif_lzw.c
//	tif_next.c
//	tif_ojpeg.c
//	tif_open.c
//	tif_packbits.c
//	tif_pixarlog.c
//	tif_predict.c
//	tif_print.c
//	tif_read.c
//	tif_stream.cxx
//	tif_strip.c
//	tif_swab.c
//	tif_thunder.c
//	tif_tile.c
//	tif_version.c
//	tif_warning.c
//	tif_webp.c
//	tif_win32.c
//	tif_write.c
//	tif_zip.c
//	tif_zstd.c

//	tiff.h
//	tiffio.h
//	tiffiop.h

#include "../../LibTiff_Win32/source_4.1/tiff.h"							// Header files for LibTIFF
#include "../../LibTiff_Win32/source_4.1/tiffio.h"

typedef struct TIFFileHeader {
	bool bVerbose 					= false;								// Enable verbose mode for console app
	bool bInputImageIsRGB 			= false;								// Input image is RGB (need to convert to CMYK if true)
	bool bInputFileIsGrayscaleK		= false;								// Grayscale, so print K only, C=M=Y=0
	bool bInputStripRead			= true;									// Read strips (true) or single raster bands
	UINT8 nImageBitDepth 			= 8;									// Input raster image bit depth, 8 or 16 bits/color
	UINT8 nInputColorChannels		= 4;									// This should be 4 for CMYK
	UINT16 nInputImagePixelWidth;											// Input image width in pixels
	UINT16 nInputImagePixelHeight;											// Input image height in pixels
	UINT16 nHorizontalDPI;													// Horizontal print DPI (dots per inch)
	UINT16 nVerticalDPI;													// Vertical print DPI
	UINT8 nInputStripSize			= 1;									// TIFF strip size = n number of rows
	UINT8 nStripReadLoopSize		= 1;									// Number of strips to read per image band
	UINT8 nInputImageBufferRows		= 255;									// Number of rows in (height of) input image band
	float dPrintedMediaWidth;												// Printed image width in inches
	float dPrintedMediaHeight;												// Printed image height in inches
	UINT32 nOutputPixelWidth;												// Printed image width in pixels
	UINT32 nOutputPixelHeight;												// Printed image height in pixels
	string strInputFile;													// Filename and path of TIFF file to be printed
	INT16 nErrorCode 				= 0;									// Return error code (for allocation function)
	TCHAR sRetErrDescription[128];											// Return error message
} TIFFHeader;

TIFFHeader MyTIFFHeader;

INT16 _GetInputImageDimensions(TIFFHeader* MyTIFFHeader) {
	const char* sInFile = MyTIFFHeader->strInputFile.c_str();				// This is the name of the TIFF file we want to print
	TIFF* inputTIFF;														// This is the pointer to the TIFF buffer
	float dImageWidth, dImageHeight;										// Image dimensions

	if (MyTIFFHeader->bVerbose) {											// If verbose mode is false, don't let LibTIFF write
		TIFFSetWarningHandler(NULL);										//  errors to stdout (turn off errors and warnings)
		TIFFSetWarningHandlerExt(NULL);
		TIFFSetErrorHandler(NULL);
		TIFFSetErrorHandlerExt(NULL);
	}

	if ((inputTIFF = TIFFOpen(sInFile, "r")) == NULL) {						// Attempt to open the TIFF file from disk
		MyTIFFHeader->nErrorCode = (-72);									// If NULL is returned, something went wrong
		swprintf_s(MyTIFFHeader->sRetErrDescription,
			_countof(MyTIFFHeader->sRetErrDescription),
			_T("EC(-72) Failed to read input image file, %hs!"), sInFile);
		return MyTIFFHeader->nErrorCode;
	}
	UINT16 _inImageRows;													// Local variables to hold the TIFF header info
	UINT16 _inImageCols;
	UINT16 _imageChn;
	UINT16 _bitsPerSample;
	UINT16 _photometric;
	UINT16 _resUnit;
	float _xRes;
	float _yRes;
	UINT16 _RowsPerStrip;

	TIFFGetField(inputTIFF, TIFFTAG_IMAGELENGTH, &_inImageRows);			// Get image length (pixel rows)
	TIFFGetField(inputTIFF, TIFFTAG_IMAGEWIDTH, &_inImageCols);				// Get image width (pixels columns)
	TIFFGetField(inputTIFF, TIFFTAG_SAMPLESPERPIXEL, &_imageChn);			// Get number of channels per pixel
	TIFFGetField(inputTIFF, TIFFTAG_BITSPERSAMPLE, &_bitsPerSample);		// Get the size of the channels
	TIFFGetField(inputTIFF, TIFFTAG_PHOTOMETRIC, &_photometric);			// Get the photometric model
	TIFFGetField(inputTIFF, TIFFTAG_RESOLUTIONUNIT, &_resUnit);				// Get resolution unit (inches or centimeters)
	TIFFGetField(inputTIFF, TIFFTAG_XRESOLUTION, &_xRes);					// Get resolution in X direction
	TIFFGetField(inputTIFF, TIFFTAG_YRESOLUTION, &_yRes);					// Get resolution in Y direction
	TIFFGetField(inputTIFF, TIFFTAG_ROWSPERSTRIP, &_RowsPerStrip);			// Get rows per strip, default = 1

	MyTIFFHeader->nInputImagePixelWidth = _inImageCols;						// Input image width & height in pixels
	MyTIFFHeader->nInputImagePixelHeight = _inImageRows;

	dImageWidth = (float)_inImageCols / _xRes;								// Physical width & height
	dImageHeight = (float)_inImageRows / _yRes;

	MyTIFFHeader->dPrintedMediaWidth = dImageWidth;							// Printed image physical width & height
	MyTIFFHeader->dPrintedMediaHeight = dImageHeight;

	if (_imageChn == 3)														// Is the image RGB or CMYK.?
		MyTIFFHeader->bInputImageIsRGB = true;
	else
		MyTIFFHeader->bInputImageIsRGB = false;

	if (_bitsPerSample == 8 || _bitsPerSample == 16)						// Only accept 8 or 16-bit images
		MyTIFFHeader->nImageBitDepth = _bitsPerSample;
	else {
		MyTIFFHeader->nErrorCode = (-73);									// Something else, so report an error
		swprintf_s(MyTIFFHeader->sRetErrDescription,
			_countof(MyTIFFHeader->sRetErrDescription),
			_T("EC(-73) Invalid image pixel bit depth: %d!"), _bitsPerSample);
		return MyTIFFHeader->nErrorCode;
	}	

	if (_imageChn == 1)														// Image must be grayscale, so will print only K
		MyTIFFHeader->bInputFileIsGrayscaleK = true;
	else
		MyTIFFHeader->bInputFileIsGrayscaleK = false;

	if (_imageChn == 1 || _imageChn == 3 || _imageChn == 4)					// Image is neither gray, RGB, or CMYK, so report an error
		MyTIFFHeader->nInputColorChannels = (UINT8)_imageChn;
	else {
		MyTIFFHeader->nErrorCode = (-74);
		swprintf_s(MyTIFFHeader->sRetErrDescription,
			_countof(MyTIFFHeader->sRetErrDescription),
			_T("EC(-74) Invalid number of input color channels: %d!"), _imageChn);
		return MyTIFFHeader->nErrorCode;
	}

	MyTIFFHeader->nOutputPixelWidth = (UINT32)ceilf(dImageWidth *			// Output image width & height in pixels
		(float)MyJobHeader->nHorizontalDPI);
	MyTIFFHeader->nOutputPixelHeight = (UINT32)ceilf(dImageHeight *
		(float)MyJobHeader->nVerticalDPI);

	if (_RowsPerStrip < 256) {
		MyTIFFHeader->bInputStripRead = true;								// Read strips (true) or single raster bands
		MyTIFFHeader->nStripReadLoopSize =									// Number of strips to read per image band
			(UINT8)floorf(255.F / (float)_RowsPerStrip);
		MyTIFFHeader->nInputImageBufferRows =								// Number of rows in (height of) input image band 
			MyTIFFHeader->nStripReadLoopSize * _RowsPerStrip;
		MyTIFFHeader->nInputStripSize = (UINT8)_RowsPerStrip;				// TIFF strip size = n number of rows
	}
	else {
		MyTIFFHeader->bInputStripRead = false;								// Rear one row at a time, not multiple rows (strip)
		MyTIFFHeader->nStripReadLoopSize = 255;
		MyTIFFHeader->nInputImageBufferRows = 255;
		MyTIFFHeader->nInputStripSize = 1;
	}
	
	// The image is open now, and you have your image size and band size
	// Figure out how many bands you need, based on image band height vs. image height
	// ** Now you can start halftoning the image **


	
	TIFFClose(inputTIFF);													// Close the TIFF image file one you are done
	return 0;																// No errors, so return zero
}
