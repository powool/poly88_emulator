#pragma once

#include <stdint.h>
#include <vector>
#include "SDL_image.h"      // from the separate package SDL_image

class Cvdi_font
{
    SDL_Renderer *m_renderer;
    SDL_Texture * textures[256];
    void setSurface(uint32_t index, const char **xpm);
    int m_width, m_height;
public:
    Cvdi_font(SDL_Renderer *renderer);
    SDL_Texture **get_font_textures()
    {
        return textures;
    };
    SDL_Texture * operator [](uint32_t i)
    {
        return textures[i & 0xff];
    };
    int width() { return m_width; }
    int height() { return m_height; }
};
