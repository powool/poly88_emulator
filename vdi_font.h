#pragma once

#include <stdint.h>
#include <utility>
#include <vector>
#include <SDL_image.h>	   // from the separate package SDL_image

class Cvdi_font
{
	SDL_Renderer *renderer;
	SDL_Texture *textures[256];
	void setSurface(uint32_t index, const char **xpm);
	int _width, _height;
	static const int gap = 1;
	static const int widthScale = 2;
	static const int heightScale = 2;
	void setCharFromCStyleXPM(uint32_t index, const char **cStyleXPM);
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
	int width() { return _width; }
	int height() { return _height; }
	static std::pair<int, int> GetDimensions();
};
