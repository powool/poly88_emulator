#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>
#include <string>
#include <sstream>

class XPM
{
    int m_width;
    int m_height;
    int m_bitplanes;      // last value in xpm header line
    std::vector<std::string> m_colors;
    std::vector<std::string> pixels;
    void constructorClear()
    {
        m_width = m_height = m_bitplanes = -1;
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
    s >> m_width >> m_height >> colorCount >> m_bitplanes;

    // store the colors as strings
    for(i = 1; i <= colorCount; i++) m_colors.push_back(xpm[i]);

    // save the pixel data as a vector of strings
    for(i = 1 + colorCount; i < 1 + colorCount + m_height; i++)
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
    char **newXPM = new char *[1 + m_colors.size() + m_height + 1];

    std::ostringstream s;
    s << m_width << " " << m_height << " " << m_colors.size() << " " << m_bitplanes;

    newXPM[0] = strdup((char *) s.str().c_str());

    for(uint32_t color = 0; color < m_colors.size(); color++)
    {
        newXPM[1 + color] = strdup(m_colors[color].c_str());
    }

    for(uint32_t row = 0; row < pixels.size(); row++)
    {
        newXPM[1 + m_colors.size() + row] = strdup(pixels[row].c_str());
    }
    newXPM[1 + m_colors.size() + pixels.size()] = NULL;
    return newXPM;
}

inline XPM *XPM::CreateNew(uint32_t gap, uint32_t widthScale, uint32_t heightScale)
{
    XPM *newXPM = new XPM();

    newXPM->m_colors = m_colors;

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
                    pixelRow += m_colors[0][0];
                }
            }
            newXPM->pixels.push_back(pixelRow);
        }
    }

    newXPM->m_height = newXPM->pixels.size();
    newXPM->m_width = newXPM->pixels[0].size();
    newXPM->m_bitplanes = m_bitplanes;
    return newXPM;
}
