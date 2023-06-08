#pragma once

#include <functional>
#include <unordered_map>
#include <SDL2/SDL.h>

struct Input
{
    using InputCallback = std::function<void(SDL_Event&)>;
    std::unordered_map<uint32_t, InputCallback> map;
    std::unordered_map<uint32_t, InputCallback> key;

    Input();
    void bind(uint32_t type, std::function<void()> callback);
    void bind(uint32_t type, InputCallback callback);
    void bindKey(uint32_t sym, std::function<void()> callback);
    void bindKey(uint32_t sym, InputCallback callback);
    void poll();
};
