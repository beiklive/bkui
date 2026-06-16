#include "sdl_platform_internal.hpp"

namespace bk::sdl
{

void PollInput(bool& running, InputState& input)
{
    input = {};

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            running = false;
            input.quitRequested = true;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_ESCAPE)
            {
                running = false;
                input.cancelPressed = true;
                input.quitRequested = true;
            }
            else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE)
            {
                input.confirmPressed = true;
            }
        }
    }
}

}
