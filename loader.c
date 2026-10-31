#include "loader.h"
#include <sys/stat.h>

void loader_init(Loader *loader)
{
	memset(loader, 0, sizeof *loader);
}

bool load_lib(Loader *loader)
{
	struct stat st;
	stat(loader->path, &st);
	if (loader->last_modified >= st.st_mtime) return true;
	SDL_IOStream *ios = SDL_IOFromFile(loader->path, "rb");
	if (ios == NULL)
	{
		SDL_LogError(2, "%s", SDL_GetError());
		return false;
	}
	SDL_UnloadObject(loader->so);
	loader->anim_draw = NULL;
	bool s = SDL_CopyFile(loader->path, loader->copy_path);
	if (!s) SDL_LogError(2, "%s", SDL_GetError());
	SDL_CloseIO(ios);
	if (!s) return false;

	SDL_SharedObject *so = SDL_LoadObject(loader->copy_path);
	if (so == NULL)
	{
		SDL_LogError(2, "%s", SDL_GetError());
		return false;
	}
	loader->last_modified = st.st_mtime;
	SDL_FunctionPointer anim_draw = SDL_LoadFunction(so, "anim_draw");
	if (anim_draw == NULL)
	{
		SDL_LogError(2, "%s", SDL_GetError());
		SDL_UnloadObject(so);
		return false;
	}
	loader->so = so;
	loader->anim_draw = (void (*)(AnimAPI *, float))anim_draw;
	return true;
}

void unload_lib(Loader *loader)
{
	SDL_UnloadObject(loader->so);
	SDL_RemovePath(loader->copy_path);
	loader_init(loader);
}
