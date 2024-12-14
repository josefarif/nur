#include <SDL3/SDL.h>

int main(int argc, char *argv[])
{	
	bool quit = false;
	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				quit = true;
			}
		}
	}

	return 0;
}
