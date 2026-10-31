#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <time.h>
#include "polygon.h"
#include "anim_internal.h"
#include "loader.h"

int main(int argc, char **argv)
{
	Anim anim;
	AnimAPI api;
	anim_api_init(&anim, &api);
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to initialize SDL subsystems\n");
		return 1;
	}
	int w = -1, h = -1;
	SDL_Window *window;
	SDL_Renderer *renderer;
	if (!SDL_CreateWindowAndRenderer("animate", 768, 768, SDL_WINDOW_RESIZABLE, &window, &renderer))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to create SDL window\n");
		return 1;
	}
	SDL_Texture *texture = NULL;
	int err = 0;
	bool running = true;
	Uint64 last_tick = SDL_GetTicks();
	float time = 0.0f;
	float last_load = -1.0f / 0.0f;
	float avg_ft = 0.0f, avg_ft_w = 0.0f;
	float anim_time = 0.0f, anim_speed = 1.0f;
	bool pause = false;

	Loader loader;
	loader_init(&loader);
	if (argc >= 2) loader.path = argv[1];
	else loader.path = "script.dll";
	loader.copy_path = ".anim_copy_so.dll";


	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key)
				{
				case SDLK_SPACE:
					pause = !pause;
					break;
				case SDLK_PLUS:
				case SDLK_EQUALS:
					anim_speed *= 2.0f;
					break;
				case SDLK_MINUS:
					anim_speed *= 0.5f;
					break;
				case SDLK_LEFT:
					anim_time -= 5.0f;
					break;
				case SDLK_RIGHT:
					anim_time += 5.0f;
					break;
				case SDLK_J:
					anim_time -= 10.0f;
					break;
				case SDLK_L:
					anim_time += 10.0f;
					break;
				case SDLK_0:
					anim_time = 0.0f;
					break;
				case SDLK_C:
					anim_clear_cache(&anim);
					break;
				default: break;
				}
				break;
			default: break;
			}
		}
		int nw = -1, nh = -1;
		if (!SDL_GetWindowSize(window, &nw, &nh))
		{
			err = 1;
			goto done;
		}
		if (w != nw || h != nh)
		{
			w = nw, h = nh;
			SDL_DestroyTexture(texture);
			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		}
		int mdt = SDL_GetTicks() - last_tick;
		while (mdt < 16)
		{
			SDL_Delay(16 - mdt);
			mdt = SDL_GetTicks() - last_tick;
		}
		last_tick += mdt;
		time += 0.001f * mdt;

		if (!pause) anim_time += 0.001f * anim_speed * mdt;
		char title[64];
		avg_ft = avg_ft * avg_ft_w + mdt * (1.0f - avg_ft_w);
		avg_ft_w = (224.0f * avg_ft_w + 31.0f) * (1.0f / 256.0f);
		SDL_snprintf(title, sizeof title, "animate | %.3fms | %.3fs | %.3fx", avg_ft, anim_time, anim_speed);
		SDL_SetWindowTitle(window, title);

		if (time - last_load >= 1.0f)
		{
			load_lib(&loader);
			last_load = time;
		}

		SDL_Surface *surface;
		SDL_LockTextureToSurface(texture, NULL, &surface);
		anim.target = surface;
		if (loader.anim_draw) loader.anim_draw(&api, anim_time);
		SDL_UnlockTexture(texture);
		surface = NULL;

		SDL_SetRenderDrawColor(renderer, 0xFF, 0xE8, 0xFB, 0xFF);
		SDL_RenderClear(renderer);
		SDL_RenderTexture(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	}
done:
	unload_lib(&loader);
	anim_api_clear(&api);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return err;
}
