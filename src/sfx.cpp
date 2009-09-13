/*
    Copyright (C) 2005-2007 Tom Beaumont

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef DISABLE_SOUND
#include <list>
#include <vector>
#include <SDL/SDL_mixer.h>
#include "sfx.h"
#include "system-directory.h"

static const char* const music_names[HHOP_MUSIC_MAX] =
{
	"music-ending",
	"music-game",
//	"music-map",
//	"music-title",
//	"music-win"
};
static const char* const sound_names[HHOP_SOUND_MAX] =
{
	"sound-builder",
	"sound-collapse",
	"sound-death",
	"sound-disintegrate",
	"sound-explode-big",
	"sound-explode-small",
	"sound-floater-enter",
	"sound-floater-move",
	"sound-found-antiice",
	"sound-found-jump",
	"sound-ice",
	"sound-laser",
	"sound-lift-up",
	"sound-lift-down",
	"sound-spinner",
	"sound-step",
	"sound-trampoline",
	"sound-ui-fade",
	"sound-ui-menu",
	"sound-used-antiice",
	"sound-used-jump",
	"sound-win"
};

/* We store delayed sound effects in a queue and play them back when the
   time is right. It's sort of ugly but makes creating samples easier. */
class SoundQueue
{
public:
	SoundQueue(int ty, double ti) : type(ty), time(ti) { }
public:
	int type;
	double time;
};

class SoundEngine
{
public:
	SoundEngine(const char* path) : music_curr(-1), music_next(-1)
	{
		int i;
		int j;
		char* pth;
		const char* name;
		lisysDir* dir;

		// Open data directory.
		dir = lisys_dir_open (path);
		if (dir == NULL)
			return;
		lisys_dir_set_filter (dir, LISYS_DIR_FILTER_FILES);
		if (!lisys_dir_scan (dir))
		{
			lisys_dir_free (dir);
			return;
		}

		// Scan for sound and music.
		for (i = 0 ; i < lisys_dir_get_count (dir) ; i++)
		{
			name = lisys_dir_get_name (dir, i);
			pth = lisys_dir_get_path (dir, i);
			for (j = 0 ; j < HHOP_MUSIC_MAX ; j++)
			{
				if (strstr (name, music_names[j]) == name)
				{
					Mix_Music* music = Mix_LoadMUS(pth);
					if (music)
						music_chunks[j].push_back(music);
					else
						fprintf(stderr, "Cannot load music `%s': %s\n", name, Mix_GetError());
				}
			}
			for (j = 0 ; j < HHOP_SOUND_MAX ; j++)
			{
				if (strstr (name, sound_names[j]) == name)
				{
					Mix_Chunk* sound = Mix_LoadWAV(pth);
					if (sound)
						sound_chunks[j].push_back(sound);
					else
						fprintf(stderr, "Cannot load effect `%s': %s\n", name, Mix_GetError());
				}
			}
			free (pth);
		}

		lisys_dir_free (dir);
	}
	~SoundEngine()
	{
		int j;

		for (j = 0 ; j < HHOP_MUSIC_MAX ; j++)
		{
			std::vector<Mix_Music*>::iterator i;
			for (i = music_chunks[j].begin() ; i != music_chunks[j].end() ; i++)
				Mix_FreeMusic (*i);
		}
		for (j = 0 ; j < HHOP_SOUND_MAX ; j++)
		{
			std::vector<Mix_Chunk*>::iterator i;
			for (i = sound_chunks[j].begin() ; i != sound_chunks[j].end() ; i++)
				Mix_FreeChunk (*i);
		}
	}
	void PlayMusic(int type)
	{
		int size = music_chunks[type].size();
		if (size)
		{
			int music = (int)(round((float) rand() / RAND_MAX) * (size - 1));
			Mix_FadeInMusic(music_chunks[type][music], -1, HHOP_FADE_MUSIC_IN);
		}
	}
	void PlaySound(int type)
	{
		int size = sound_chunks[type].size();
		if (size)
		{
			int sound = (int)(round((float) rand() / RAND_MAX) * (size - 1));
			Mix_PlayChannel(-1, sound_chunks[type][sound], 0);
		}
	}
	void QueueMusic(int type)
	{
		if (music_curr != type || music_next != type)
		{
			music_curr = -2;
			music_next = type;
			Mix_FadeOutMusic(HHOP_FADE_MUSIC_OUT);
		}
	}
	void QueueSound(int type, double time)
	{
		sound_queue.push_back(SoundQueue(type, time));
	}
	void UndoQueue()
	{
		sound_queue.clear();
	}
	void Update(double time)
	{
		while (true)
		{
			std::list<SoundQueue>::iterator i;

			// Find the first effect that needs playing.
			for (i = sound_queue.begin() ; i != sound_queue.end() ; i++)
			{
				if (time >= i->time)
				{
					PlaySound(i->type);
					break;
				}
			}

			// Erase the effect or stop if not found.
			if (i != sound_queue.end())
				sound_queue.erase(i);
			else
				break;
		}
		if (Mix_FadingMusic() == MIX_NO_FADING)
		{
			if (music_curr != music_next)
			{
				PlayMusic(music_next);
				music_curr = music_next;
			}
		}
	}
public:
	int music_curr;
	int music_next;
	std::vector<Mix_Music*> music_chunks[HHOP_MUSIC_MAX];
	std::vector<Mix_Chunk*> sound_chunks[HHOP_SOUND_MAX];
	std::list<SoundQueue> sound_queue;
};

static SoundEngine* sound_engine;
#endif

void InitSound(const char* path)
{
#ifndef DISABLE_SOUND
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1)
	{
		fprintf(stderr, "Initializing audio failed: %s\n", Mix_GetError());
		exit(1);
	}
	Mix_AllocateChannels(HHOP_EFFECT_CHANNELS);

	sound_engine = new SoundEngine(path);
#endif
}

void FreeSound()
{
#ifndef DISABLE_SOUND
	delete sound_engine;
	Mix_CloseAudio();
#endif
}

void PlayMusic(int type)
{
#ifndef DISABLE_SOUND
	sound_engine->QueueMusic(type);
#endif
}

void PlaySound(int type)
{
#ifndef DISABLE_SOUND
	sound_engine->PlaySound(type);
#endif
}

void QueueSound(int type, double time)
{
#ifndef DISABLE_SOUND
	sound_engine->QueueSound(type, time);
#endif
}

void UndoSound()
{
#ifndef DISABLE_SOUND
	sound_engine->UndoQueue();
#endif
}

void UpdateSound(double time)
{
#ifndef DISABLE_SOUND
	sound_engine->Update(time);
#endif
}