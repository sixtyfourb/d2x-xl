#include <stdio.h>

#include "descent.h"
#include "error.h"
#include "hmpfile.h"
#include "audio.h"
#include "songs.h"
#include "config.h"
#include "midi.h"
#include "rwops.h"
#include "strutil.h"
#include "findfile.h"

CMidi midi;

//------------------------------------------------------------------------------

void CMidi::Init (void)
{
m_nVolume = 255;
m_nPaused = 0;
m_music = NULL;
m_hmp = NULL;
}

//------------------------------------------------------------------------------

void CMidi::Shutdown (void)
{
if (gameStates.sound.audio.bNoMusic)
	return;

#if (defined (_WIN32) || USE_SDL_MIXER)
if (m_hmp) {
	hmp_close (m_hmp);
	m_hmp = NULL;
	songManager.SetPlaying (0);
	}
#endif
Fadeout ();
}

//------------------------------------------------------------------------------

void CMidi::Fadeout (void)
{
if (gameStates.sound.audio.bNoMusic)
	return;

#if USE_SDL_MIXER
if (!audio.Available ()) 
	return;
if (gameOpts->sound.bUseSDLMixer) {
	if (gameOpts->sound.bFadeMusic) {
		Mix_FadeOutMusic (300);
		SDL_Delay (330);
#if 0
		while (Mix_PlayingMusic ())
			SDL_Delay (1);		
#endif
		}
	int32_t nVolume = m_nVolume;
	SetVolume (0);
	m_nVolume = nVolume;
	Mix_HaltMusic ();
	Mix_FreeMusic (m_music);
	m_music = NULL;
	}
#endif
}

//------------------------------------------------------------------------------

int32_t CMidi::SetVolume (int32_t nVolume)
{
if (gameStates.sound.audio.bNoMusic)
	return 0;

#if (defined (_WIN32) || USE_SDL_MIXER)
	int32_t nLastVolume = m_nVolume;

if (nVolume < 0)
	m_nVolume = 0;
else if (nVolume > 127)
	m_nVolume = 127;
else
	m_nVolume = nVolume;

#	if USE_SDL_MIXER
if (gameOpts->sound.bUseSDLMixer)
	Mix_VolumeMusic (m_nVolume);
#	endif
#	if defined (_WIN32)
#		if USE_SDL_MIXER
else 
#		endif
if (m_hmp) {
	// scale up from 0-127 to 0-0xffff
	nVolume = 65535 * m_nVolume / 128;
	midiOutSetVolume ((HMIDIOUT) m_hmp->hmidi, nVolume | (nVolume << 16));
	}
if ((songManager.Playing () < 0) || !nVolume)
	FixVolume (m_nVolume);
#	endif
return nLastVolume;
#else
return 0;
#endif
}

//------------------------------------------------------------------------------

void CMidi::FixVolume (int32_t nVolume)
{
#ifdef _WIN32
if (gameStates.sound.bMidiFix && (songManager.Playing () <= 0)) {
	HMIDIOUT hMIDI;
	midiOutOpen (&hMIDI, -1, NULL, NULL, CALLBACK_NULL);
	nVolume = 65535; // * nVolume / 128;
	midiOutSetVolume (hMIDI, nVolume | (nVolume << 16));
	midiOutClose (hMIDI);
	}
#endif
}

//------------------------------------------------------------------------------
/* Point SDL_mixer at a SoundFont it can actually open.
 *
 * The songs in the HOG files are MIDI, so something has to synthesize them, and
 * on anything but Windows that something is SDL_mixer. Its FluidSynth backend
 * needs a SoundFont and knows of exactly one: whichever path was compiled into
 * the library. That path names a package of the distribution SDL_mixer itself
 * was packaged for, which is not the distribution this runs on, so it is not
 * there. The TiMidity fallback then wants /etc/timidity.cfg and a patch set,
 * which are not there either, and every song fails to load with "Couldn't open
 * timidity.cfg" - the game plays no music at all, and until now said nothing
 * about why.
 *
 * So look for a SoundFont among the game's own files, where whoever installs
 * the game can put one, and name it to SDL_mixer. SDL_SOUNDFONTS in the
 * environment still wins, and so does a compiled-in default that really exists.
 */

#if USE_SDL_MIXER

static bool SoundFontIsReadable (const char* pszFile)
{
	FILE* fp = pszFile && *pszFile ? fopen (pszFile, "rb") : NULL;

if (!fp)
	return false;
fclose (fp);
return true;
}

//------------------------------------------------------------------------------

static void SetupSoundFont (void)
{
	static int32_t	bDone = 0;

	const char*		pszFolders [] = {gameFolders.game.szMusic [2], gameFolders.game.szMusic [0], gameFolders.game.szRoot};
	const char*		pszPatterns [] = {"*.sf2", "*.sf3"};
	char				szFilter [FILENAME_LEN];
	char				szSoundFont [FILENAME_LEN];
	FFS				ffs;

if (bDone)
	return;
bDone = 1;
if (getenv ("SDL_SOUNDFONTS"))	// somebody has already said which one to use
	return;
for (int32_t i = 0; i < int32_t (sizeofa (pszFolders)); i++) {
	if (!*pszFolders [i])
		continue;
	for (int32_t j = 0; j < int32_t (sizeofa (pszPatterns)); j++) {
		sprintf (szFilter, "%s%s", pszFolders [i], pszPatterns [j]);
		if (FFF (szFilter, &ffs, 0))
			continue;
		sprintf (szSoundFont, "%s%s", pszFolders [i], ffs.name);
		FFC (&ffs);
		if (Mix_SetSoundFonts (szSoundFont)) {
			PrintLog (0, "playing MIDI music with the SoundFont %s\n", szSoundFont);
			return;
			}
		PrintLog (0, "cannot use the SoundFont %s (%s)\n", szSoundFont, Mix_GetError ());
		}
	}
if (SoundFontIsReadable (Mix_GetSoundFonts ()))
	PrintLog (0, "playing MIDI music with the SoundFont SDL_mixer was built for (%s)\n", Mix_GetSoundFonts ());
else
	PrintLog (0, "no SoundFont found, so MIDI music cannot be played.\n"
					 "   Put a .sf2 file in '%s', or name one in SDL_SOUNDFONTS.\n", gameFolders.game.szMusic [2]);
}

#endif //USE_SDL_MIXER

//------------------------------------------------------------------------------

static Mix_MusicType GetMusicType(const char *file, SDL_RWops *rw)
{
	Mix_MusicType type;
	const char *ext;

	/* Use the extension as a first guess on the file type */
	type = MUS_NONE;
	ext = strrchr(file, '.');
	/* No need to guard these with #ifdef *_MUSIC stuff,
	 * since we simply call Mix_LoadMUSType_RW() later */
	if ( ext ) {
		++ext; /* skip the dot in the extension */
		if ( stricmp(ext, "WAV") == 0 ) {
			type = MUS_WAV;
		} else if ( stricmp(ext, "MID") == 0 ||
		            stricmp(ext, "MIDI") == 0 ||
		            stricmp(ext, "KAR") == 0 ) {
			type = MUS_MID;
		} else if ( stricmp(ext, "OGG") == 0 ) {
			type = MUS_OGG;
		} else if ( stricmp(ext, "FLAC") == 0 ) {
			type = MUS_FLAC;
		} else 	if ( stricmp(ext, "MPG") == 0 ||
		             stricmp(ext, "MPEG") == 0 ||
		             stricmp(ext, "MP3") == 0 ||
		             stricmp(ext, "MAD") == 0 ) {
			type = MUS_MP3;
		}
	}
	return type;
}

//------------------------------------------------------------------------------

int32_t CMidi::PlaySong (const char* pszSong, char* melodicBank, char* drumBank, int32_t bLoop, int32_t bD1Song)
{
if (gameStates.sound.audio.bNoMusic)
	return 0;

#if (defined (_WIN32) || USE_SDL_MIXER)
	int32_t	bCustom;

PrintLog (1, "CMidi::PlaySong (%s)\n", pszSong);
audio.StopCurrentSong ();
if (!(pszSong && *pszSong)) {
	PrintLog (-1);
	return 0;
	}
if (m_nVolume < 1) {
	PrintLog (0, "the music volume is turned all the way down\n");
	PrintLog (-1);
	return 0;
	}

bCustom = ((strstr (pszSong, ".ogg") != NULL) || strstr (pszSong, ".flac"));
if (bCustom) {
	if (audio.Format () != AUDIO_S16SYS) {
		audio.Shutdown ();
		audio.Setup (1, AUDIO_S16SYS);
		}
	}
else if (!(m_hmp = hmp_open (pszSong, bD1Song))) {
	PrintLog (0, "could not read the song %s\n", pszSong);
	PrintLog (-1);
	return 0;
	}

#	if USE_SDL_MIXER
if (gameOpts->sound.bUseSDLMixer) {
	char			fnSong [FILENAME_LEN];
	const char*	pfnSong;

	if (bCustom) {
		pfnSong = pszSong;
		}
	else {
#if defined (_WIN32)
		sprintf (fnSong, "%sd2x-temp.mid", *gameFolders.var.szCache ? gameFolders.var.szCache : gameFolders.user.szCache);
#else
		sprintf (fnSong, "%sd2x-temp.mid", *gameFolders.var.szCache ? gameFolders.var.szCache : gameFolders.user.szCache);
#endif
		if (!hmp_to_midi (m_hmp, fnSong)) {
			PrintLog (-1, "SDL_mixer failed to load %s\n(%s)\n", fnSong, Mix_GetError ());
			return 0;
			}
		pfnSong = fnSong;
		}
	if (!bCustom)
		SetupSoundFont ();
	try {
		SDL_RWops* rw = CFileOpenRWOps (pfnSong, NULL);
		m_music = Mix_LoadMUSType_RW (rw, GetMusicType(pfnSong, rw), 1);
		}
	catch (...) {	// critical problem in midi playback -> turn it off
		SetVolume (gameConfig.nMidiVolume = 0);
		}
	if (!m_music) {
		PrintLog (0, "SDL_mixer failed to load %s\n(%s)\n", pfnSong, Mix_GetError ());
		PrintLog (-1);
		return 0;
		}
	if (-1 == Mix_FadeInMusicPos (m_music, bLoop ? -1 : 1, !gameOpts->sound.bFadeMusic ? 0 : songManager.Pos () ? 1000 : 1500, (double) songManager.Pos () / 1000.0)) {
		PrintLog (0, "SDL_mixer cannot play %s\n(%s)\n", pszSong, Mix_GetError ());
		songManager.SetPos (0);
		PrintLog (-1);
		return 0;
		}
	PrintLog (0, "SDL_mixer playing %s\n", pszSong);
	if (songManager.Pos ())
		songManager.SetPos (0);
	else
		songManager.SetStart (SDL_GetTicks ());
	
	songManager.SetPlaying (bCustom ? -1 : 1);
	SetVolume (m_nVolume);
	PrintLog (-1);
	return songManager.Playing ();
	}
#	endif
#	if defined (_WIN32)
if (bCustom) {
	PrintLog (-1, "Cannot play %s - enable SDL_mixer\n", pszSong);
	return 0;
	}
hmp_play (m_hmp, bLoop);
songManager.SetPlaying (1);
SetVolume (m_nVolume);
#	endif
#endif
PrintLog (-1);
return 1;
}

//------------------------------------------------------------------------------

void CMidi::Pause (void)
{
if (gameStates.sound.audio.bNoMusic)
	return;

if (!m_nPaused) {
#if USE_SDL_MIXER
	if (gameOpts->sound.bUseSDLMixer)
		Mix_PauseMusic ();
#endif
	}
m_nPaused++;
}

//------------------------------------------------------------------------------

void CMidi::Resume (void)
{
if (gameStates.sound.audio.bNoMusic)
	return;

if (m_nPaused == 1) {
#if USE_SDL_MIXER
	if (gameOpts->sound.bUseSDLMixer)
		Mix_ResumeMusic ();
#endif
	}
m_nPaused--;
}

//------------------------------------------------------------------------------
