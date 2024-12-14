#include "nur.h"

#include <SDL3/SDL.h>

#include "SDL3/SDL_events.h"
#include "servers/render/render_server.h"

int main(int argc, char *argv[]) {
	PRINT(bold | fg(dark_golden_rod), "=========\n   {}\n=========\n", "nur");

	RenderServer render_server;

	bool quit = false;
	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				quit = true;
			}
		}
		render_server.draw();
	}
	return 0;
}
