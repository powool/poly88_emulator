#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "sdl.h"
#include <iostream>

Csdl::Csdl()
{
    window = NULL;
    _renderer = NULL;
	window_width = 100;
	window_height = 100;
}

Csdl::~Csdl()
{
    this->close();
}

int Csdl::close()
{
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(window);
    window = NULL;
    _renderer = NULL;
    if(--sdl_reference_count == 0) SDL_Quit();
    return 0;
}

void Csdl::set_size(int window_width, int window_height)
{
    SDL_SetWindowSize(window, window_width, window_height);
}

int Csdl::open()
{
    if(sdl_reference_count++ == 0 && SDL_Init(SDL_INIT_VIDEO)<0)
    {
        fprintf(stderr, "failed to open SDL: %s\n", SDL_GetError());
        return 1;
    }

//    print_valid_modes();
    window = SDL_CreateWindow("Poly-88", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
    if(window==NULL)
    {
        fprintf(stderr, "Unable to set %dx%d video: %s\n", window_width, window_height, SDL_GetError());
        return 1;
    }

    _renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(_renderer==NULL)
    {
        fprintf(stderr, "Unable to set %dx%d video: %s\n", window_width, window_height, SDL_GetError());
        return 1;
    }


    this->window_width = window_width;
    this->window_height = window_height;
//    SDL_EnableUNICODE(1);
//    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	// this sleep gets the initial rendering right - I have no idea why:
	sleep(1);
    return 0;
}

void Csdl::rectangle(
    int x,
    int y,
    int x_size,
    int y_size,
    int red,
    int green,
    int blue)
{
	needsUpdate = true;
	SDL_SetRenderDrawColor(_renderer, red, green, blue, SDL_ALPHA_OPAQUE);

    SDL_Rect foo;
    foo.x = x;
    foo.y = y;
    foo.w = x_size;
    foo.h = y_size;

    SDL_RenderFillRect(_renderer, &foo);
}

void Csdl::put_pixel(int x, int y, int red, int green, int blue)
{
	needsUpdate = true;
	SDL_SetRenderDrawColor(_renderer, red, green, blue, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawPoint(_renderer, x, y);
}

int Csdl::blit2screen(SDL_Texture *src, SDL_Rect &dstrect)
{
	needsUpdate = true;
    return SDL_RenderCopy(_renderer, src, NULL, &dstrect);
}


void Csdl::update()
{
	if(needsUpdate)
	{
		SDL_RenderPresent(_renderer);
		needsUpdate = false;
	}
}

void Csdl::print_valid_modes()
{
#if 0
    SDL_Rect **modes;
    int i;
    modes = SDL_ListModes(NULL,SDL_FULLSCREEN|SDL_HWSURFACE);

    if(modes==(SDL_Rect **) 0)
    {
        printf("No modes available!\n");
        return;
    }
    if(modes==(SDL_Rect**)-1)
    {
        printf("All resolutions available.\n");
    }
    else for(i=0; modes[i]; i++)
        {
            printf("\t%d by %d pixels.\n", modes[i]->w, modes[i]->h);
        }
#endif
}

int Csdl::sdl_reference_count = 0;

int Csdl::get_keystroke(bool poll)
{
    SDL_Event event;

    if(poll)
    {
        while(1)
        {
            if(SDL_PollEvent(&event) == 0) return -1;
            if(event.type==SDL_KEYDOWN) break;
        }
    }
    else
    {
        while(1)
        {
            SDL_WaitEvent(&event);
            if(event.type==SDL_KEYDOWN) break;
        }
    }

    return event.key.keysym.sym;
}


#if 0
int main(int argc, char **argv)
{

    Csdl screen;

    screen.print_valid_modes();

    screen.put_pixel(50,50,255,255,255);

    for(int i=0; i<360; i++)
    {
        screen.put_pixel(i, (int)(sin(i/M_PI/2) * 100 + 200), 255,0,255);
    }

    sleep(5);
}
#endif
