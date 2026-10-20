#include "widgets/Application.h"
#include "Game.h"
#include "Log.h"
#include <SDL.h>
#include <cstring>
#include <string>
#include <cerrno>
#include <ctime>
#include <exception>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

int main(int argc, char *argv[]) {
	char *basePath = SDL_GetBasePath();
	if(!basePath)
		BackyardBrains::Log::fatal("could not determine executable directory: %s", SDL_GetError());
	std::string path(basePath);
	SDL_free(basePath);

#ifdef __APPLE__
	path += "../Resources";
#endif

	if(chdir(path.c_str()) != 0)
		BackyardBrains::Log::fatal("could not change directory to '%s': %s",
			path.c_str(), strerror(errno));

	time_t t;
	time(&t);
	BackyardBrains::Log::msg("BYB SpikeRecorder started on %s", ctime(&t));

	try {
	BackyardBrains::Game game;

	game.run();
	} catch(std::exception &e) {
		BackyardBrains::Log::fatal("Exception occured: %s\n", e.what());
	}
	BackyardBrains::Log::msg("BYB SpikeRecorder exited normally.");
	return 0;
}
