#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <time.h>
#include "anim_internal.h"
#include "loader.h"

const int w = 1920, h = 1080;
const int fps = 60;
const int threads = 10;

typedef struct renderer_input
{
	AnimAPI *api;
	Loader *loader;
	SDL_Mutex *mutex;
	SDL_Condition *assign_cond, *signal_cond;
	Uint8 *assign_queue;
	int assign_queue_length;
	int *signal;
} renderer_input;

int SDLCALL renderer(void *user)
{
	renderer_input *ri = user;
	int curr_frame = 0;
	SDL_LockMutex(ri->mutex);
	for (;;)
	{
		while (curr_frame < ri->assign_queue_length)
		{
			if (ri->assign_queue[curr_frame] == 0) break;
			curr_frame++;
		}
		if (curr_frame >= ri->assign_queue_length)
		{
			SDL_UnlockMutex(ri->mutex);
			return 0;
		}
		ri->assign_queue[curr_frame] = 1;
		SDL_UnlockMutex(ri->mutex);
		ri->loader->anim_draw(ri->api, (float)curr_frame / fps);
		SDL_LockMutex(ri->mutex);
		*ri->signal = curr_frame;
		SDL_SignalCondition(ri->signal_cond);
		while (*ri->signal == curr_frame) SDL_WaitCondition(ri->assign_cond, ri->mutex);
	}
}

int main(int argc, char **argv)
{
	Anim anim;
	AnimAPI api;
	anim_api_init(&anim, &api);
	Loader loader;
	loader_init(&loader);
	if (argc >= 2) loader.path = argv[1];
	else loader.path = "script.dll";
	loader.copy_path = ".offline_copy_so.dll";
	if (!load_lib(&loader) || loader.anim_draw == NULL)
	{
		printf("error so\n");
		return 1;
	}
	int err = 0;

	FILE *faudio = popen("ffmpeg -y -i assets/meriron.ogg -f s16le -ar 48000 -ac 2 pipe:1", "rb");
	if (!faudio)
	{
		printf("error\n");
		return 1;
	}
	Sint16 *audio = NULL;
	const int CHUNK_SIZE = 65536;
	Sint16 *chunk = malloc(sizeof(Sint16) * CHUNK_SIZE);
	int total = 0;
	while (!feof(faudio))
	{
		int actual = fread(chunk, sizeof(Sint16), CHUNK_SIZE, faudio);
		audio = realloc(audio, sizeof(Sint16) * (total + actual));
		for (int i = 0; i < actual; i++) audio[total + i] = chunk[i];
		total += actual;
		if (actual < CHUNK_SIZE) break;
	}
	fclose(faudio);
	faudio = NULL;
	free(chunk);
	chunk = NULL;
	int frames = total * fps / 96000;
	printf("est: %d\n", frames);
	SDL_Surface *targets[threads];
	Anim anims[threads];
	AnimAPI apis[threads];
	SDL_Thread *thread[threads];
	SDL_Mutex *mutex = SDL_CreateMutex();
	SDL_Condition *assign_cond = SDL_CreateCondition();
	SDL_Condition *signal_cond = SDL_CreateCondition();
	int signals[threads];
	renderer_input ri[threads];
	Uint8 status[frames];
	for (int i = 0; i < frames; i++) status[i] = 0;
	for (int i = 0; i < threads; i++)
	{
		targets[i] = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ABGR8888);
		anim_api_init(anims + i, apis + i);
		anims[i].target = targets[i];
		signals[i] = -1;
		ri[i].api = apis + i;
		ri[i].loader = &loader;
		ri[i].mutex = mutex;
		ri[i].assign_cond = assign_cond;
		ri[i].signal_cond = signal_cond;
		ri[i].assign_queue = status;
		ri[i].assign_queue_length = frames;
		ri[i].signal = signals + i;
		thread[i] = SDL_CreateThread(renderer, "offlineT", ri + i);
	}

	FILE *fout = popen("ffmpeg -y -i assets/meriron.ogg -f rawvideo -pixel_format rgba -video_size 1920x1080 -framerate 60 -i pipe:0 -map 0:a -map 1:v -c:v libx264 -c:a aac -b:a 384k -preset veryslow -tune animation -crf 18 -pix_fmt yuv420p -movflags +faststart output.mp4", "wb");

	int rendered = 0;
	SDL_LockMutex(mutex);
	while (rendered < frames)
	{
		int thread_idx;
		for (;;)
		{
			for (thread_idx = 0; thread_idx < threads; thread_idx++)
				if (signals[thread_idx] == rendered)
					goto done_signal;
			SDL_WaitCondition(signal_cond, mutex);
		}
	done_signal:
		SDL_UnlockMutex(mutex);
		SDL_Surface *target = targets[thread_idx];
		for (int i = 0; i < h; i++)
		{
			Uint8 *row = (Uint8 *)target->pixels + target->pitch * i;
			fwrite(row, sizeof(Uint32), w, fout);
		}
		rendered++;
		SDL_LockMutex(mutex);
		signals[thread_idx] = -1;
		SDL_BroadcastCondition(assign_cond);
	}
	SDL_UnlockMutex(mutex);
	pclose(fout);
	for (int i = 0; i < threads; i++)
		SDL_WaitThread(thread[i], NULL);
	SDL_DestroyCondition(assign_cond);
	SDL_DestroyCondition(signal_cond);

	for (int i = 0; i < threads; i++)
	{
		SDL_DestroySurface(targets[i]);
		anim_api_clear(apis + i);
	}
	free(audio);
	unload_lib(&loader);
	return err;
}

