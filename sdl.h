#include <SDL.h>

class Csdl
{
    SDL_Window *window;
    SDL_Renderer *_renderer;
    static int  sdl_reference_count;
    int window_width, window_height;
	bool needsUpdate = false;
public:

    Csdl();
    ~Csdl();

    int close();
    int open();
    void set_size(int window_width, int window_height);
    void rectangle(int x, int y, int x_size, int y_size, int red, int green, int blue);
    void put_pixel(int x, int y, int red, int green, int blue);
    void print_valid_modes();
    void update();
    int get_keystroke(bool poll = true); // return -1 if none available
    int blit2screen(SDL_Texture *src, SDL_Rect &dstrect);
    SDL_Renderer *renderer() { return _renderer; }
};

