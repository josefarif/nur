#pragma once
#define WIN32_LEAN_AND_MEAN
#include <SDL3/SDL_messagebox.h>
#include <cstdlib>

#define ERROR_POPUP(title, msg) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, nullptr); std::exit(EXIT_FAILURE);

#ifdef DEBUG
#include <SDL3/SDL_assert.h>
#include <fmt/color.h>
#include <fmt/core.h>
// bad:
#define PRINT(...) using fmt::print; using fmt::format; using fmt::emphasis::bold; using fmt::emphasis::underline; using fmt::emphasis::italic; using fmt::fg; using fmt::bg; using fmt::color::white; using fmt::color::green; using fmt::color::gray; using fmt::color::black; using fmt::color::yellow; using fmt::color::dark_golden_rod; using fmt::color::aqua; using fmt::color::blue; using fmt::color::red; print(__VA_ARGS__)
#define ERROR(msg) PRINT(fg(white) | bg(red) | bold | underline, "  ERROR  "); PRINT(fg(black) | bg(yellow), "  {}  \n", msg); SDL_TriggerBreakpoint()
#define ASSERT(condition, msg) if (!(condition)) {ERROR(fmt::format("ASSERTION FAILED: {}: {}", #condition, msg));}
#else
#define PRINT(...)
#define ERROR(...)
#define ASSERT(condition, msg) condition;
#endif
