#include "nur.h"

#include <SDL3/SDL_messagebox.h>

int main(int argc, char *argv[]) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "title", "error", nullptr);
    return 0;
}
