#include <functional>
#include <unordered_map>
#include <SDL2/SDL.h>
#include "Input.h"

Input::Input()
{
    this->bind(SDL_KEYDOWN, [this](SDL_Event& e) { 
        if (this->key.count(e.key.keysym.sym)) 
            this->key[e.key.keysym.sym](e); 
    });
}

void Input::bind(uint32_t type, std::function<void()> callback)
{
    this->bind(type, [callback](SDL_Event &e) { (void)e; callback(); });
}

void Input::bind(uint32_t type, Input::InputCallback callback)
{
    this->map[type] = callback;
}

void Input::bindKey(uint32_t sym, std::function<void()> callback)
{
    this->bindKey(sym, [callback](SDL_Event &e) { (void)e; callback(); });
}

void Input::bindKey(uint32_t sym, InputCallback callback)
{
    this->key[sym] = callback;
}

void Input::poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (this->map.count(event.type))
            this->map[event.type](event);
}