#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libarflac.h"

FLAC__StreamDecoderWriteStatus ArFLAC__write_hook(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
void ArFLAC__metadata_hook(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
void ArFLAC__error_hook(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
int ArFLAC__sumDigits(int n);
void ArFLAC__appendArResult(ArFLAC_trackInfo* track, unsigned char confidence, ArFLAC_uint32 CRC, ArFLAC_uint32 OffsetFindCRC);

ArFLAC_trackInfo* ArFLAC_openTrack(const char* filename, ArFLAC_bool isFirst, ArFLAC_bool isLast, ArFLAC_msgCallback logger)
{
	ArFLAC_trackInfo* trackData = malloc(sizeof(ArFLAC_trackInfo));
	
	if(!trackData)
	{
		logger("Failed to allocate ArFLAC_trackInfo structure.");
		return NULL;
	}
	
	trackData->decoder = NULL;
	trackData->CRC = 0;
	
	trackData->ArConfidence = malloc(4*sizeof(unsigned char));
	trackData->ArCRC = malloc(4*sizeof(ArFLAC_uint32));
	trackData->ArOffsetFindCRC = malloc(4*sizeof(ArFLAC_uint32));
	trackData->ArResultCount = 0;
	trackData->ArResultSpaceAllocated = 4;
	
	trackData->sampleCount = trackData->sampleOffset = 0;
	trackData->isFirst = isFirst;
	trackData->isLast = isLast;
	trackData->statusGood = true;
	trackData->error = "";
	trackData->err = logger;

	if(!(trackData->decoder = FLAC__stream_decoder_new()))
	{
		trackData->err("Failed to initalise FLAC decoder");
		free(trackData);
		return NULL;
	}
	
	/* The last parameter can be arbitrary data. */
	switch(FLAC__stream_decoder_init_file(trackData->decoder, filename, ArFLAC__write_hook, ArFLAC__metadata_hook, ArFLAC__error_hook, trackData))
	{
		case FLAC__STREAM_DECODER_INIT_STATUS_OK:
			/* trackData->err("Decoder initialisation was successful."); */
			return trackData;
		case FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
			trackData->err("The library was not compiled with support for the given container format.");
			break;
		case FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS:
			trackData->err("A required callback was not supplied.");
			break;
		case FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR:
			trackData->err("An error occurred allocating memory.");
			break;
		case FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE:
			trackData->err("fopen() failed in FLAC__stream_decoder_init_file().");
			break;
		case FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED:
			trackData->err("FLAC__stream_decoder_init_*() was called when the decoder was already initialized, usually because FLAC__stream_decoder_finish() was not called.");
	}
	
	free(trackData);
	return NULL;
}
	
ArFLAC_bool ArFLAC_readMetadata(ArFLAC_trackInfo* trackData)
{
	if(FLAC__stream_decoder_process_until_end_of_metadata(trackData->decoder) && trackData->statusGood)
	{
		/* trackData->err("Successfully read FLAC metadata: trackData.sampleCount = %lu.", trackData->sampleCount); */
		return true;
	}
	else
	{
		trackData->err("Error while decoding stream: %s", trackData->error);
		return false;
	}
}
	
ArFLAC_bool ArFLAC_generateCRC(ArFLAC_trackInfo* trackData)
{
	if(FLAC__stream_decoder_process_until_end_of_stream(trackData->decoder) && trackData->statusGood)
	{
		/* trackData->err("Successfully decoded FLAC stream!"); */
		return true;
	}
	else
	{
		trackData->err("Error while decoding stream: %s", trackData->error);
		return false;
	}
}

void ArFLAC_closeTrack(ArFLAC_trackInfo* trackData)
{
	FLAC__stream_decoder_finish(trackData->decoder);
	FLAC__stream_decoder_delete(trackData->decoder);
	free(trackData->ArConfidence);
	free(trackData->ArCRC);
	free(trackData->ArOffsetFindCRC);
	free(trackData);
}
	
FLAC__StreamDecoderWriteStatus ArFLAC__write_hook(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	ArFLAC_trackInfo* trackData = client_data;
	if(trackData->statusGood)
	{
		int i;
		
		if((frame->header.channels != 2) || (frame->header.sample_rate != 44100) || (frame->header.bits_per_sample != 16))
		{
			trackData->error = "Only stereo 44.1kHz/16bit audio (CDDA) is supported";
			trackData->statusGood = false;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		
		if(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)
		{
			trackData->error = "Frame number passed to FLAC_write_hook, only sample number should be here.";
			trackData->statusGood = false;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		
		for(i = 0; i < frame->header.blocksize; i++)
		{
			/* FLAC__int32 sample = (buffer[1][i] << 16) + (buffer[0][i] & 0x0000FFFF); */
			trackData->sampleOffset++;
			
			if((!(trackData->isFirst) && !(trackData->isLast)) || (trackData->isFirst && (trackData->sampleOffset >= (588*5))) || (trackData->isLast && ((trackData->sampleCount - trackData->sampleOffset) >= (5*588))))
			{
				trackData->CRC += (((buffer[1][i] << 16) + (buffer[0][i] & 0x0000FFFF)) * trackData->sampleOffset);
			}
		}

		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}
	else
	{
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
}

void ArFLAC__metadata_hook(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	ArFLAC_trackInfo* trackData = client_data;
	
	/*switch(metadata->type)
	{*/
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		/*trackData->err("STREAMINFO block found."); */
		
		trackData->sampleCount = metadata->data.stream_info.total_samples;
		
		if((trackData->sampleCount%588) != 0)
		{
			trackData->error = "FLAC file does not contain a whole number of CDDA frames!";
			trackData->statusGood = false;
		}		
		else if((metadata->data.stream_info.channels != 2) || (metadata->data.stream_info.sample_rate != 44100) || (metadata->data.stream_info.bits_per_sample != 16))
		{
			trackData->error = "Only stereo 44.1kHz/16bit audio (CDDA) is supported";
			trackData->statusGood = false;
		}
	}
		
		/*	break;
		case FLAC__METADATA_TYPE_PADDING:
			printMsg("PADDING block found.");
			break;
		case FLAC__METADATA_TYPE_APPLICATION:
			printMsg("APPLICATION block found.");
			break;
		case FLAC__METADATA_TYPE_SEEKTABLE:
			printMsg("SEEKTABLE block found.");
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			printMsg("VORBISCOMMENT block (a.k.a. FLAC tags) found.");
			break;
		case FLAC__METADATA_TYPE_CUESHEET:
			printMsg("CUESHEET block found.");
			break;
		case FLAC__METADATA_TYPE_PICTURE:
			printMsg("PICTURE block found.");
			break;
		case FLAC__METADATA_TYPE_UNDEFINED:
			printMsg("UNDEFINED data block found.");
	}*/
}

void ArFLAC__error_hook(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
}

ArFLAC_discInfo* ArFLAC_openDisc(ArFLAC_msgCallback logger)
{
	ArFLAC_discInfo* disc = malloc(sizeof(ArFLAC_discInfo));
	
	if(disc)
	{
		disc->trackCount = disc->ArId1 = disc->ArId2 = disc->CDDBId = 0;
		disc->err = logger;
		disc->file[0] = disc->url[0] = '\0';
	}

	return disc;
}

ArFLAC_bool ArFLAC_appendTrack(ArFLAC_discInfo* disc, ArFLAC_trackInfo* track)
{
	if(disc->trackCount <= ARFLAC_MAXTRACKS)
	{
		disc->tracks[disc->trackCount] = track;
		disc->trackCount++;
		return true;
	}
	else
	{
		disc->err("Max number of tracks (%d) reached. Your CD is nonstandard!", ARFLAC_MAXTRACKS);
		return false;
	}
}

ArFLAC_bool ArFLAC_appendTracks(ArFLAC_discInfo* disc, ArFLAC_trackInfo** tracks, short newTrackCount)
{
	if(disc->trackCount+newTrackCount <= ARFLAC_MAXTRACKS)
	{
		int i;
		
		for(i = 0; i < newTrackCount; i++)
		{
			disc->tracks[disc->trackCount] = tracks[i];
			disc->trackCount++;
		}
		
		return true;
	}
	else
	{
		disc->err("Max number of tracks (%d) will be exceeded. Your CD is nonstandard!", ARFLAC_MAXTRACKS);
		return false;
	}
}

ArFLAC_bool ArFLAC_generateDiscIds(ArFLAC_discInfo* disc)
{
	int trackNo;
	int sampleOffset = 0;
	
	if(!disc->trackCount)
	{
		disc->err("No tracks to calculate Disc IDs from!");
		return false;
	}
	
	/* This is for the first track.  */
	disc->ArId2 = 1;
	
	/* DiscID1 is the sum of the frame offsets from the start of the CD of all the tracks + the length of the CD in frames */
	for(trackNo = 0; trackNo < disc->trackCount; trackNo++)
	{
		/* This one doesn't take the total CD length into account, so it comes before sampleOffset += ... */
		disc->CDDBId += ArFLAC__sumDigits((sampleOffset/(75*588)) + 2);
		
		/* This will now hold the offset - in samples - from the start of the CD to the start of disc->tracks[trackNo+1] */
		sampleOffset += disc->tracks[trackNo]->sampleCount;		
		
		/* Because this is after sampleOffset += ... then it it's really operating on trackNo+1. This is good, because it needs to include the length
		 * of the CD in frames (e.g. the offset to the track after the last track) */
		disc->ArId1 += (sampleOffset/588);		
		disc->ArId2 += (sampleOffset/588) * (trackNo+2);
	}

	disc->CDDBId = ((disc->CDDBId % 255) << 24) + ((sampleOffset/(588*75)) << 8) + disc->trackCount;

	disc->ArId1 &= 0xFFFFFFFF;
	disc->ArId2 &= 0xFFFFFFFF;
	disc->CDDBId &= 0xFFFFFFFF;
	
	return true;
}

ArFLAC_bool ArFLAC_generateArURL(ArFLAC_discInfo* disc)
{
	if(disc->ArId1 && disc->ArId2 && disc->CDDBId)
	{
		snprintf(disc->file, 40, "dBAR-%.3d-%.8x-%.8x-%.8x.bin", disc->trackCount, disc->ArId1, disc->ArId2, disc->CDDBId);
		snprintf(disc->url, 85, "http://www.accuraterip.com/accuraterip/%.1x/%.1x/%.1x/%s", disc->ArId1 & 0xF, disc->ArId1>>4 & 0xF, disc->ArId1>>8 & 0xF, disc->file);
		return true;
	}
	else
	{
		disc->err("ArFLAC_generateArURL called before IDs have been generated.");
		return false;
	}
}

ArFLAC_bool ArFLAC_processArData(ArFLAC_discInfo* disc, const unsigned char* buffer, ArFLAC_uint32 length)
{
	int dataPtr;
	int resultNo;
						
	for(dataPtr = resultNo = 0; dataPtr < length; resultNo++)
	{
		unsigned char chunkTrackCount	= buffer[dataPtr];
		ArFLAC_uint32 chunkArId1		= *(ArFLAC_uint32*)&buffer[dataPtr+1];
		ArFLAC_uint32 chunkArId2		= *(ArFLAC_uint32*)&buffer[dataPtr+5];
		ArFLAC_uint32 chunkCDDBId		= *(ArFLAC_uint32*)&buffer[dataPtr+9];

		dataPtr += 13;
						
		if((chunkTrackCount == disc->trackCount) && (chunkArId1 == disc->ArId1) && (chunkArId2 == disc->ArId2) && (chunkCDDBId == disc->CDDBId))
		{
			int trackNo;
							
			/* disc->err("IDs and track count match!"); */
								
			for(trackNo = 0; trackNo < disc->trackCount; trackNo++)
			{
				ArFLAC_trackInfo* track = disc->tracks[trackNo];
								
				ArFLAC__appendArResult(track, buffer[dataPtr], *(ArFLAC_uint32*)&buffer[dataPtr+1], *(ArFLAC_uint32*)&buffer[dataPtr+5]);
				dataPtr += 9;
			}
		}								
		else
		{
			if(chunkTrackCount != disc->trackCount)
			{
				disc->err("Chunk track count (%d) does not match ours (%d)", chunkTrackCount, disc->trackCount);
			}
							
			if(chunkArId1 != disc->ArId1)
			{
				disc->err("Chunk ArId1 (%08x) does not match ours (%08x)", chunkArId1, disc->ArId1);
			}
						
			if(chunkArId2 != disc->ArId2)
			{
				disc->err("Chunk ArId2 (%08x) does not match ours (%08x)", chunkArId2, disc->ArId2);
			}
							
			if(chunkCDDBId != disc->CDDBId)
			{
				disc->err("Chunk CDDBId (%08x) does not match ours (%08x)", chunkCDDBId, disc->CDDBId);
			}
							
			return false;
		}
	}
	
	return true;
}

void ArFLAC__appendArResult(ArFLAC_trackInfo* track, unsigned char confidence, ArFLAC_uint32 CRC, ArFLAC_uint32 OffsetFindCRC)
{
	track->ArResultCount++;
	
	if(track->ArResultSpaceAllocated > track->ArResultCount)
	{
		void *realloc(void *ptr, size_t size);
		track->ArConfidence = realloc(track->ArConfidence, (track->ArResultSpaceAllocated + 4)*sizeof(unsigned char));
		track->ArCRC = realloc(track->ArCRC, (track->ArResultSpaceAllocated + 4)*sizeof(ArFLAC_uint32));
		track->ArOffsetFindCRC = realloc(track->ArOffsetFindCRC, (track->ArResultSpaceAllocated + 4)*sizeof(ArFLAC_uint32));
	}
	
	track->ArConfidence[track->ArResultCount-1] = confidence;
	track->ArCRC[track->ArResultCount-1] = CRC;
	track->ArOffsetFindCRC[track->ArResultCount-1] = OffsetFindCRC;
}

int ArFLAC__sumDigits(int n)
{
	int r = 0;

	while(n > 0)
	{
		r += (n%10);
		n /= 10;
	}

	return r;
}


void ArFLAC_closeDisc(ArFLAC_discInfo* disc)
{
	free(disc);
}
