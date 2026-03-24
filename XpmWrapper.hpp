#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>
#include <string>
#include <sstream>

/**
 * XPM helper class to aid parsing and scaling
 *
 * See https://en.wikipedia.org/wiki/X_PixMap
 */
class XpmWrapper
{
    int width;
    int height;
	int colorCount;
    int bitPlanes;      // last value in xpm header line
	bool deallocateXpmData;
	char **xpmData;
    void Set(const char **xpm);
	void setFontValues() {
		std::istringstream s(xpmData[0]);
		s >> width >> height >> colorCount >> bitPlanes;
	}

	int HeaderRowIndex() {return 0;}
	int ColorRowIndex() {return 1;}
	int PixelRowIndex(int row) {return ColorRowIndex() + colorCount + row;}

	// horizontally scale the given row
	char *ScalePixelRow(int scale, int row) {
		const char *originalRowPixels = xpmData[PixelRowIndex(row)];

		char *newRowPixels = (char *) malloc(width * scale + 1);

		int newColumn = 0;
		for(int column = 0; column < width; column++) {
			for(int colScale = 0; colScale < scale; colScale++) {
				newRowPixels[newColumn++] = originalRowPixels[column];
			}
		}
		newRowPixels[newColumn++] = '\0';

		return newRowPixels;
	}

public:
    XpmWrapper(XpmWrapper &xpm, int gap, int widthScale, int heightScale) : deallocateXpmData(true)
	{
		char **oldXpmData = xpm.xpmData;
		width = xpm.width * widthScale;
		height = xpm.height * (heightScale + gap);
		colorCount = xpm.colorCount;
		bitPlanes = xpm.bitPlanes;

		// header row + one row per color, plus pixel height, plus trailing NULL pointer
		xpmData = (char **) malloc(sizeof(void *) * (1 + colorCount + height + 1));

		// create new header with newly scaled values
		char *newHeader = (char *) malloc(128);
		sprintf(newHeader, "%d %d %d %d", width, height, colorCount, bitPlanes);

		xpmData[0] = newHeader;

		// clone the colors, they don't change
		for (auto color = 0; color < colorCount; color++) {
			xpmData[1 + color] = strdup(oldXpmData[1 + color]);
		}

		int newPixelRowIndex = 1 + colorCount;
		// for each original row...
		for(int originalRowIndex = 0; originalRowIndex < xpm.height; originalRowIndex++)
		{
			auto newRowPixels = xpm.ScalePixelRow(widthScale, originalRowIndex);

			// clone the first horizontally scaled row
			xpmData[newPixelRowIndex++] = newRowPixels;

			// when scaling, sucessive scaled rows are equal to the first, but
			// need to be allocated individually, to allow clean deallocation.
			for(int rowScale = 1; rowScale < heightScale; rowScale++)
			{
				xpmData[newPixelRowIndex++] = strdup(newRowPixels);
			}

			// skip creating a gap row if none are called for
			if(gap) {
				auto newRowPixels = (char *) malloc(xpm.width * widthScale + 1);
				int newColumn;
				for(newColumn = 0; newColumn < width; newColumn++) {
					newRowPixels[newColumn] = xpmData[1][0]; // background 'character'
				}
				newRowPixels[newColumn] = '\0';

				xpmData[newPixelRowIndex++] = newRowPixels;
				for(int gapRow = 1; gapRow < gap; gapRow++) {
					xpmData[newPixelRowIndex++] = strdup(newRowPixels);
				}
			}
		}
		xpmData[newPixelRowIndex++] = nullptr;
	}

    XpmWrapper(const char **xpm) : deallocateXpmData(false) {
		xpmData = const_cast<char **>(xpm);
		setFontValues();
	}

    ~XpmWrapper() {
		if (deallocateXpmData) {
			for(int i=0; xpmData[i] ; i++) free(xpmData[i]);
			free(xpmData);
		}
	}

    // returns the C style XPM
    const char **GetXpmData() {
		return const_cast<const char **>(xpmData);
	}
};
