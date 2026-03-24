#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>
#include <string>
#include <sstream>

class XPM
{
    int width;
    int height;
    int bitplanes;      // last value in xpm header line
    std::vector<std::string> colors;
    std::vector<std::string> pixels;
    void constructorClear()
    {
        width = height = bitplanes = -1;
    }
public:
    XPM();
    XPM(const char **xpm);
    void Set(const char **xpm);

    ~XPM();

    XPM *CreateNew(uint32_t gap, uint32_t widthScale, uint32_t heightScale);
    // returns a correctly formed XPM
    char **GetXPM();
};


inline XPM::XPM()
{
    constructorClear();
}

inline XPM::XPM(const char **xpm)
{
    constructorClear();
    Set(xpm);
}

//
// parse the given xpm image and load up our
// member vars.
//
inline void XPM::Set(const char **xpm)
{
    constructorClear();

    int colorCount = 0;
    int i;

    pixels.clear();

    std::istringstream s(xpm[0]);
    s >> width >> height >> colorCount >> bitplanes;

    // store the colors as strings
    for(i = 1; i <= colorCount; i++) colors.push_back(xpm[i]);

    // save the pixel data as a vector of strings
    for(i = 1 + colorCount; i < 1 + colorCount + height; i++)
    {
        pixels.push_back(xpm[i]);
    }
}

inline XPM::~XPM()
{
}

inline char **XPM::GetXPM()
{
    // header plus # colors, # height in pixels + final NULL
    char **newXPM = new char *[1 + colors.size() + height + 1];

    std::ostringstream s;
    s << width << " " << height << " " << colors.size() << " " << bitplanes;

    newXPM[0] = strdup((char *) s.str().c_str());

    for(uint32_t color = 0; color < colors.size(); color++)
    {
        newXPM[1 + color] = strdup(colors[color].c_str());
    }

    for(uint32_t row = 0; row < pixels.size(); row++)
    {
        newXPM[1 + colors.size() + row] = strdup(pixels[row].c_str());
    }
    newXPM[1 + colors.size() + pixels.size()] = NULL;
    return newXPM;
}

inline XPM *XPM::CreateNew(uint32_t gap, uint32_t widthScale, uint32_t heightScale)
{
    XPM *newXPM = new XPM();

    newXPM->colors = colors;

    for(uint32_t row = 0; row < pixels.size(); row++)
    {
        for(uint32_t rowScale = 0; rowScale < heightScale; rowScale++)
        {
            std::string pixelRow;
            for(uint32_t pixel = 0; pixel < pixels[row].size(); pixel++)
            {
                for(uint32_t colScale = 0; colScale < widthScale; colScale++)
                {
                    pixelRow += pixels[row][pixel];
                }
            }
            newXPM->pixels.push_back(pixelRow);
        }
        for(uint32_t row = 0; row < gap; row++)
        {
            std::string pixelRow;
            for(uint32_t pixel = 0; pixel < pixels[0].size(); pixel++)
            {
                for(uint32_t colScale = 0; colScale < widthScale; colScale++)
                {
                    pixelRow += colors[0][0];
                }
            }
            newXPM->pixels.push_back(pixelRow);
        }
    }

    newXPM->height = newXPM->pixels.size();
    newXPM->width = newXPM->pixels[0].size();
    newXPM->bitplanes = bitplanes;
    return newXPM;
}
