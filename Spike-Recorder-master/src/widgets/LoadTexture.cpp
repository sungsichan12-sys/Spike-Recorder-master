#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <string>
#if defined(_WIN32)
    #include <stdio.h>
    #include <cstdlib>
#endif // defined
namespace BackyardBrains {

namespace Widgets {

namespace {
bool isPowerOfTwo(int value)
{
	return value > 0 && (value & (value - 1)) == 0;
}

int nextPowerOfTwo(int value)
{
	unsigned int result = 1;
	while(result < static_cast<unsigned int>(value) &&
	      result <= static_cast<unsigned int>(std::numeric_limits<int>::max()) / 2)
		result <<= 1;
	return result >= static_cast<unsigned int>(value) ? static_cast<int>(result) : 0;
}

bool supportsNonPowerOfTwoTextures()
{
	const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
	if(version)
	{
		while(*version && !std::isdigit(static_cast<unsigned char>(*version)))
			++version;
		int major = 0;
		int minor = 0;
		if(std::sscanf(version, "%d.%d", &major, &minor) == 2 && major >= 2)
			return true;
	}

	const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
	if(!extensions)
		return false;
	const char *extension = "GL_ARB_texture_non_power_of_two";
	const size_t extensionLength = std::strlen(extension);
	const char *found = std::strstr(extensions, extension);
	while(found)
	{
		const bool startsToken = found == extensions || found[-1] == ' ';
		const bool endsToken = found[extensionLength] == '\0' || found[extensionLength] == ' ';
		if(startsToken && endsToken)
			return true;
		found = std::strstr(found + extensionLength, extension);
	}
	return false;
}

bool isDarkThemeIcon(const char *filename)
{
	if(!filename)
		return false;
	const char *baseName = std::strrchr(filename, '/');
	const char *windowsBaseName = std::strrchr(filename, '\\');
	if(!baseName || (windowsBaseName && windowsBaseName > baseName))
		baseName = windowsBaseName;
	baseName = baseName ? baseName + 1 : filename;
	static const char *icons[] = {
		"analysis.bmp", "analysiscrossed.bmp", "analysishigh.bmp",
		"backward.bmp", "backwardhigh.bmp", "config.bmp", "configcrossed.bmp",
		"confighigh.bmp", "ekg.bmp", "ekghigh.bmp", "fft.bmp", "fftcrossed.bmp",
		"ffthigh.bmp", "file.bmp", "filehigh.bmp", "forward.bmp",
		"forwardhigh.bmp", "pause.bmp", "pausehigh.bmp", "play.bmp", "playhigh.bmp",
		"rec.bmp", "rechigh.bmp", "save.bmp", "savehigh.bmp", "speaker.bmp",
		"speakeroff.bmp", "thresh.bmp", "threshcrossed.bmp", "threshhigh.bmp"
	};
	for(size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i)
		if(std::strcmp(baseName, icons[i]) == 0)
			return true;
	return false;
}

void applyDarkThemeToIcon(SDL_Surface *surface)
{
	if(!surface || (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0))
		return;
	const bool locked = SDL_MUSTLOCK(surface) != 0;
	for(int y = 0; y < surface->h; ++y)
	{
		Uint8 *row = static_cast<Uint8 *>(surface->pixels) + y * surface->pitch;
		for(int x = 0; x < surface->w; ++x)
		{
			Uint8 *bytes = row + x * surface->format->BytesPerPixel;
			Uint32 pixel = 0;
			std::memcpy(&pixel, bytes, surface->format->BytesPerPixel);
			Uint8 r, g, b, a;
			SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
			const int brightness = (static_cast<int>(r) + g + b) / 3;
			const int chroma = std::max(static_cast<int>(r),
				std::max(static_cast<int>(g), static_cast<int>(b))) -
				std::min(static_cast<int>(r),
				std::min(static_cast<int>(g), static_cast<int>(b)));
			if(chroma <= 22)
			{
				// The legacy buttons were painted on white squares. Turn the white
				// canvas transparent and recolor gray artwork to the shared teal accent.
				const int coverage = std::max(0, std::min(255, (238 - brightness) * 5));
				r = 87;
				g = 211;
				b = 195;
				a = static_cast<Uint8>((static_cast<unsigned int>(a) * coverage) / 255);
			}
			pixel = SDL_MapRGBA(surface->format, r, g, b, a);
			std::memcpy(bytes, &pixel, surface->format->BytesPerPixel);
		}
	}
	if(locked)
		SDL_UnlockSurface(surface);
}
}

GLuint LoadTexture(const char *filename)
{
	GLuint texture = 0;

	//SDL_Surface * const surface = IMG_Load(filename);
	SDL_Surface * const loadedSurface = SDL_LoadBMP(filename);

	if (loadedSurface)
	{
		SDL_Surface *convertedSurface = NULL;
		SDL_Surface *surface = loadedSurface;
		const bool fontAtlas = std::strstr(filename, "ui-font.bmp") != NULL;
		if(fontAtlas || isDarkThemeIcon(filename))
		{
			convertedSurface = SDL_ConvertSurfaceFormat(loadedSurface,
				SDL_PIXELFORMAT_ABGR8888, 0);
			if(convertedSurface)
			{
				surface = convertedSurface;
				if(fontAtlas) {
					// White glyphs on black become coverage alpha, preventing black
					// boxes around antialiased text on any panel background.
					if(SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
					for(int y=0; y<surface->h; ++y) {
						Uint32 *row = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(surface->pixels) + y*surface->pitch);
						for(int x=0; x<surface->w; ++x) {
							Uint8 r,g,b,a; SDL_GetRGBA(row[x],surface->format,&r,&g,&b,&a);
							row[x] = SDL_MapRGBA(surface->format,255,255,255,r);
						}
					}
					if(SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
				} else applyDarkThemeToIcon(surface);
			}
		}
		const bool needsPadding = !isPowerOfTwo(surface->w) || !isPowerOfTwo(surface->h);
		if(needsPadding && !supportsNonPowerOfTwoTextures())
		{
			const int paddedWidth = nextPowerOfTwo(surface->w);
			const int paddedHeight = nextPowerOfTwo(surface->h);
			if(paddedWidth == 0 || paddedHeight == 0)
			{
				fprintf(stderr, "Could not resize texture '%s' to a power-of-two size.\n", filename);
				if(convertedSurface)
					SDL_FreeSurface(convertedSurface);
				SDL_FreeSurface(loadedSurface);
				return 0;
			}

			SDL_Surface *paddedSurface = SDL_CreateRGBSurface(0, paddedWidth, paddedHeight,
				surface->format->BitsPerPixel, surface->format->Rmask,
				surface->format->Gmask, surface->format->Bmask, surface->format->Amask);
			if(!paddedSurface || SDL_BlitScaled(surface, NULL, paddedSurface, NULL) != 0)
			{
				fprintf(stderr, "Could not resize texture '%s': %s\n", filename, SDL_GetError());
				if(paddedSurface)
					SDL_FreeSurface(paddedSurface);
				if(convertedSurface)
					SDL_FreeSurface(convertedSurface);
				SDL_FreeSurface(loadedSurface);
				return 0;
			}
			surface = paddedSurface;
		}


		GLint bytespp = surface->format->BytesPerPixel;

		GLenum texture_format;
		if(bytespp == 4) {
			if(surface->format->Rmask == 0x000000ff)
				texture_format = GL_RGBA;
			else
				texture_format = GL_BGRA;
		}
		else if(bytespp == 3) {
			if (surface->format->Rmask == 0x000000ff)
				texture_format = GL_RGB;
			else
				texture_format = GL_BGR;
		} else {
			fprintf(stderr, "Skipping texture '%s': unsupported %d-bit bitmap.\n",
				filename, surface->format->BitsPerPixel);
			if(convertedSurface)
				SDL_FreeSurface(convertedSurface);
			SDL_FreeSurface(loadedSurface);
			return 0;
		}


		glGenTextures(1, &texture);

		glBindTexture(GL_TEXTURE_2D, texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


		glTexImage2D(GL_TEXTURE_2D, 0, bytespp, surface->w, surface->h, 0, texture_format, GL_UNSIGNED_BYTE, surface->pixels);

		if(surface != loadedSurface && surface != convertedSurface)
			SDL_FreeSurface(surface);
		if(convertedSurface)
			SDL_FreeSurface(convertedSurface);
		SDL_FreeSurface(loadedSurface);
	}
	else
	{
		fprintf(stderr, "SDL could not load \"%s\": %s\n", filename, SDL_GetError());
		return 0;
	}



	return texture;
}

} // namespace Widgets

} // namespace BackyardBrains
