//---------------------------------------------------------------------------------------------
//
// Copyright (c) 2019 fmad engineering llc 
//
// The MIT License (MIT) see LICENSE file for details 
//
// file wrapper 
//
//---------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <locale.h>
#include <assert.h>
#include <errno.h>

#include "fTypes.h"
#include "fProfile.h"
#include "fFile.h"

//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------

typedef struct fFile_t
{
	u8		Path[1024];							// path to file

	FILE*	File;								// usual file handle if its created

	bool	IsBuffer;							// file in buffer or IO mode
	u8*		Buffer;								// temp packet buffer
	u64		BufferPos;							// current position in packet buffer
	u64		BufferMax;							// max buffer size

	u64		TotalByte;
	u64		TotalPayload;						// total payload bytes

} fFile_t;

static u64		s_TotalFile			= 0;		// total number of files allocated
static u64		s_TotalFileActive	= 0;		// total number of files active 
static u64		s_TotalFileClose	= 0;		// number of files closed 

static u64 		s_FileSizeMin		= 128;		// minimum file size
static bool		s_ForceFlush		= false;	// force flushing after every write 

//---------------------------------------------------------------------------------------------

struct fFile_t* fFile_Open(u8* Path, u8* Mode)
{
	fFile_t* F = (fFile_t*)malloc( sizeof(fFile_t) );
	memset(F, 0, sizeof(fFile_t));

	strncpy(F->Path, Path, sizeof(F->Path));

	// initial output buffer 
	F->IsBuffer		= true;
	F->BufferMax	= s_FileSizeMin;
	F->BufferPos	= 0;
	F->Buffer		= malloc( F->BufferMax );		
	memset(F->Buffer, 0, F->BufferMax );

	F->TotalByte	= 0;

	// if file existed before get total length
	struct stat fstat;	
	if (stat(F->Path, &fstat) >= 0) 
	{
		F->TotalByte = fstat.st_size;
	}

	// new file allocated 
	s_TotalFile++;

	return F;
}

//---------------------------------------------------------------------------------------------

void fFile_Write(struct fFile_t* F, void* Buffer, u32 Length, bool IsPayload)
{
	// should never write with a null file handle
	assert(F != NULL);

	// need to create a file 
	if (F->IsBuffer)
	{
		if (F->BufferPos + Length > F->BufferMax)
		{
			F->File = fopen(F->Path, "a");
			assert(F->File != NULL);

			fwrite(F->Buffer, 1, F->BufferPos, F->File);
			F->TotalByte	+= F->BufferPos;

			F->IsBuffer 	= false;
			F->BufferPos	= 0;

			s_TotalFileActive++;
		}
	}

	// buffer it
	if (F->IsBuffer)
	{
		memcpy(F->Buffer + F->BufferPos, Buffer, Length);
		F->BufferPos += Length;
	}
	else
	{
		assert(F->File != NULL);

		fwrite(Buffer, 1, Length, F->File);
		if (s_ForceFlush) fflush(F->File);
		F->TotalByte	+= Length; 
	}

	// if this is payload
	if (IsPayload) F->TotalPayload += Length;
}

//---------------------------------------------------------------------------------------------

void fFile_Close(struct fFile_t* F)
{
	if (!F) return;

	// flush buffer contents if not written to disk
	if (F->IsBuffer && (F->BufferPos > 0))
	{
		// if theres real substational data the flush it
		if (F->TotalPayload > s_FileSizeMin)
		{
			F->File = fopen(F->Path, "a");
			assert(F->File != NULL);

			fwrite(F->Buffer, 1, F->BufferPos, F->File);
			F->BufferPos = 0;
		}
	}

	// close
	if (F->File)
	{
		fclose(F->File);
		s_TotalFileActive--;
	}

	if (F->Buffer)
	{
		free(F->Buffer);
		F->Buffer = NULL;
	}
	memset(F, 0, sizeof(fFile_t));
	free(F);

	s_TotalFileClose++;
}

//---------------------------------------------------------------------------------------------

void fFile_Flush(struct fFile_t* F)
{
	if (F->File)
	{
		fflush(F->File);
	}
}

//---------------------------------------------------------------------------------------------
// set the minimum file size before creating a file 
void fFile_SizeMin(u32 SizeMin)
{
	s_FileSizeMin = SizeMin;
}

//---------------------------------------------------------------------------------------------
// force flushing data after each write 
void fFile_ForceFlush(void)
{
	s_ForceFlush = true;	
}

//---------------------------------------------------------------------------------------------
