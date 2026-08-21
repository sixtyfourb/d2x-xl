#include <inttypes.h>
#include <SDL.h>
#include "rwops.h"

// SDL2 widened the RWops callbacks: seek takes and returns a 64-bit offset, and
// read and write count in size_t. The bodies are unchanged - CFile's own types
// are wider than SDL 1.2's int anyway - so only the signatures move.
#if SDL_VERSION_ATLEAST (2, 0, 0)
typedef Sint64 rwops_off_t;
typedef size_t rwops_size_t;
#else
typedef int rwops_off_t;
typedef int rwops_size_t;
#endif

static rwops_off_t rwops_seek(SDL_RWops* rw, rwops_off_t offset, int whence)
{
    CFile* cf = (CFile*)rw->hidden.unknown.data1;
    return (rwops_off_t) cf->Seek (offset, whence) == -1 ? -1 : cf->Tell ();
}

static rwops_size_t rwops_read(SDL_RWops *rw, void *ptr, rwops_size_t size, rwops_size_t maxnum)
{
    CFile* cf = (CFile*)rw->hidden.unknown.data1;
    return (rwops_size_t) cf->Read (ptr, size, maxnum, 0, 1);
}

static rwops_size_t rwops_write(SDL_RWops *rw, const void *ptr, rwops_size_t size, rwops_size_t num)
{
    CFile* cf = (CFile*)rw->hidden.unknown.data1;
    return (rwops_size_t) cf->Write (ptr, size, num);
}

static int rwops_close(SDL_RWops *rw)
{
	CFile* cf = (CFile*)rw->hidden.unknown.data1;
	if (cf->Close ())
		return EOF;
    SDL_FreeRW(rw);
	delete cf;
    return 0;
}

SDL_RWops *CFileOpenRWOps(const char *file, const char *folder)
{
	SDL_RWops *retval = NULL;
	CFile *cf;

	if (!(cf = new CFile()))
		return NULL;

	if (!(cf->Open(file, folder, "rb", 0))) {
		delete cf;
		return NULL;
		}

	if (!(retval = SDL_AllocRW ())) {
		delete cf;
		return NULL;
		}

	retval->seek  = rwops_seek;
	retval->read  = rwops_read;
	retval->write = rwops_write;
	retval->close = rwops_close;
	retval->hidden.unknown.data1 = (void *)cf;

	return retval;
}

