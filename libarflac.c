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

//BUGBUG only for debug
void printMsg(const char* text, ...);

FLAC__StreamDecoderWriteStatus ArFLAC__write_hook(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
void ArFLAC__metadata_hook(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
void ArFLAC__error_hook(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
int ArFLAC__sumDigits(int n);
void InitCRC32Table();
unsigned int Reflect(unsigned int ref, char ch);

unsigned int CRC32Table[256];


ArFLAC_trackInfo* ArFLAC_openTrack(const char* filename, ArFLAC_msgCallback logger)
{

	ArFLAC_trackInfo* trackData = malloc(sizeof(ArFLAC_trackInfo));
	
	if(!trackData)
	{
		logger("Failed to allocate ArFLAC_trackInfo structure.");
		return NULL;
	}
	
	trackData->decoder = NULL;
	trackData->CRC = 0;
	trackData->CRCv2 = 0;
	trackData->MD5checked = false;
	trackData->CRCoffset = 0;
	trackData->matchedOffset = NULL;
	trackData->trimSamples = 0;
	trackData->eaccrcFinalized = false;
	trackData->eaccrc=0xffffffff;
	trackData->eaccrcnslr=0xffffffff;
	trackData->prevTrack = NULL;
	trackData->nextTrack = NULL;
	trackData->filename = strdup(filename);
	if(!trackData->filename)
	{
		logger("Failed to allocate ArFLAC_trackInfo structure.");
		return NULL;
	}
	
	trackData->ArConfidence = NULL;
	trackData->ArCRC = NULL;
	trackData->ArOffsetFindCRC = NULL;
	trackData->ArResultCount = 0;
	
	trackData->sampleCount = trackData->sampleOffset = 0;
	trackData->statusGood = true;
	trackData->error = "";
	trackData->err = logger;

	if(!(trackData->decoder = FLAC__stream_decoder_new()))
	{
		trackData->err("Failed to initialise FLAC decoder");
		free(trackData);
		return NULL;
	}

	if (!FLAC__stream_decoder_set_md5_checking(trackData->decoder, true))
	{
		trackData->statusGood = false;
		trackData->err("Could not enable MD5 checking");
		return NULL;
	}

	FLAC__StreamDecoderInitStatus r = FLAC__stream_decoder_init_file(trackData->decoder, 
																	 trackData->filename, 
																	 ArFLAC__write_hook, 
																	 ArFLAC__metadata_hook, 
																	 ArFLAC__error_hook, 
																	 trackData);
	if(r==FLAC__STREAM_DECODER_INIT_STATUS_OK)
		return trackData;
	else
	{
		trackData->err(FLAC__StreamDecoderInitStatusString[r]);
		free(trackData);
		return NULL;
	}
}
	
/* func to reopen and already open file so that we can look for other matches at different offsets */
ArFLAC_bool ArFLAC_reopenTrack(ArFLAC_trackInfo* trackData)
{
	trackData->sampleOffset=0;
	trackData->CRC = 0;
	trackData->CRCv2 = 0;
	FLAC__StreamDecoderInitStatus r = FLAC__stream_decoder_init_file(trackData->decoder, 
																	 trackData->filename, 
																	 ArFLAC__write_hook, 
																	 ArFLAC__metadata_hook, 
																	 ArFLAC__error_hook, 
																	 trackData);
	if(r!=FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		trackData->err("Reopen error: %s", FLAC__StreamDecoderInitStatusString[r]);
		return false;
	}
	
	return true;
}

void ArFLAC_closeTrack(ArFLAC_trackInfo* trackData)
{
	FLAC__stream_decoder_delete(trackData->decoder);
	free(trackData->ArConfidence);
	free(trackData->ArCRC);
	free(trackData->ArOffsetFindCRC);
	free(trackData->matchedOffset);
	free(trackData->filename);
	free(trackData);
}

ArFLAC_bool ArFLAC_readMetadata(ArFLAC_trackInfo* trackData)
{
	if(FLAC__stream_decoder_process_until_end_of_metadata(trackData->decoder) && trackData->statusGood)
	{
		return true;
	}
	else
	{
		trackData->err("Error while decoding metadata: %s", trackData->error);
		return false;
	}
}

ArFLAC_bool ArFLAC_generateCRC(ArFLAC_trackInfo* trackData)
{

	if(FLAC__stream_decoder_process_until_end_of_stream(trackData->decoder) && trackData->statusGood)
	{
		// only check MD5 once
		if (!trackData->MD5checked)
		{
			if (!FLAC__stream_decoder_get_md5_checking(trackData->decoder))
			{
				trackData->statusGood = false;
				trackData->err("MD5 check was disabled by FLAC decoder");
				return false;
			}
		}
		if (!FLAC__stream_decoder_finish(trackData->decoder))
		{
			trackData->statusGood = false;
			trackData->err("MD5 mismatch");
			return false;
		}
		trackData->MD5checked = true;
		//Don't delete, we're gonna reuse if needed: FLAC__stream_decoder_delete(trackData->decoder);

		return true;
	}
	else
	{
		trackData->err("Error while decoding stream: %s", trackData->error);
		trackData->err("Decoder state: %s", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(trackData->decoder)]);
		return false;
	}
}

FLAC__StreamDecoderWriteStatus ArFLAC__write_hook(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	ArFLAC_trackInfo* trackData = client_data;
	if(trackData->statusGood)
	{
		int i,j;
		
		if((frame->header.channels != 2) || (frame->header.sample_rate != 44100) || (frame->header.bits_per_sample != 16))
		{
			trackData->error = "[Stream] Only stereo 44.1kHz/16bit audio (CDDA) is supported";
			trackData->statusGood = false;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		
		if(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)
		{
			trackData->error = "[Stream] Frame number passed to FLAC_write_hook, only sample number should be here.";
			trackData->statusGood = false;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}

		FLAC__uint64 startSampleNum = 0;
		FLAC__uint64 stopSampleNum = frame->header.blocksize;
		if (!trackData->sampleOffset) // beginning of track
		{
			// initialize CRC based on negative offset, using data from previous track
			// and don't worry about first track, since first 5 frames are ignored by AR
			if (trackData->CRCoffset<0)
			{
				for(j = trackData->CRCoffset; j<0;j++)
				{
					trackData->sampleOffset++;
					if (trackData->prevTrack)
					{
						CalcCRC(trackData->CRC,trackData->prevTrack->lastFrames[5*588+j],trackData->sampleOffset);
						CalcCRCv2(trackData->CRCv2,trackData->prevTrack->lastFrames[5*588+j],trackData->sampleOffset);
					}
					else
					{
						// on first track, just loop "filling" with silence, ie nulls
					}
				}
			}	
			// for positive offset, skip so many samples
			else if (trackData->CRCoffset>0)
			{
				startSampleNum = trackData->CRCoffset;
			}
		}
		else // not beginning of track
		{
			if (trackData->CRCoffset>0 && (trackData->sampleCount-(trackData->sampleOffset+trackData->CRCoffset))<stopSampleNum)
			{
				stopSampleNum = trackData->sampleCount-(trackData->sampleOffset+trackData->CRCoffset);
			}
			else if ((trackData->sampleCount-trackData->sampleOffset)<stopSampleNum)
			{
				stopSampleNum = trackData->sampleCount-trackData->sampleOffset;
			}
		}
		
		unsigned char nSample[4];
		FLAC__uint32 sample;
		
		for(i = startSampleNum; i < stopSampleNum; i++)
		{
            trackData->sampleOffset++;
			sample = (buffer[1][i] << 16) + (buffer[0][i] & 0x0000FFFF);

			// do not calc this after the first pass thru the whole stream
			if (!trackData->eaccrcFinalized)
			{
				// Compute EAC CRCs... but only on the first pass, ie !MD5checked
				*(unsigned short int*)(nSample) = (unsigned short)buffer[0][i];
				*(unsigned short int*)(nSample+2) = (unsigned short)buffer[1][i];
				// calculate CRC skipping nulsamples on left/right channel
				if (*(unsigned short int*)(nSample))
					for (j = 0; j < 2; j++)
						trackData->eaccrcnslr = (trackData->eaccrcnslr >> 8) ^ CRC32Table[(trackData->eaccrcnslr & 0xFF) ^ nSample[j]];
				if (*(unsigned short int*)(nSample+2))
					for (j = 2; j < 4; j++)
						trackData->eaccrcnslr = (trackData->eaccrcnslr >> 8) ^ CRC32Table[(trackData->eaccrcnslr & 0xFF) ^ nSample[j]];
				// calculate standard CRC
				for (j = 0; j < 4; j++)
					trackData->eaccrc = (trackData->eaccrc >> 8) ^ CRC32Table[(trackData->eaccrc & 0xFF) ^ nSample[j]];
			}

			// only do this on the first pass, ie when !MD5checked
			if(!trackData->MD5checked)
			{
				// Keep copy of special areas: first 5 frames, frame 450 area, and last 5 frames
				if(trackData->sampleOffset <= 5*588)
				{
					trackData->firstFrames[trackData->sampleOffset - 1] = sample;
				}
				else if(trackData->sampleOffset > 588*445 && trackData->sampleOffset <= 588*455)
				{
					trackData->frame450[trackData->sampleOffset - 588*445 - 1] = sample;
				}
				else if ((trackData->sampleCount - trackData->sampleOffset) < 5*588)
				{
					trackData->lastFrames[5*588 - (trackData->sampleCount - trackData->sampleOffset) - 1] = sample;
				}
			}

			if(	(trackData->prevTrack && trackData->nextTrack) || 
			    ((!trackData->prevTrack) && (trackData->sampleOffset >= 588*5)) ||
			   	((!trackData->nextTrack) && ((trackData->sampleCount - trackData->sampleOffset) >= 5*588)) )
			{
				// skip first 5 frames of first track, final 5 frames of last track
				CalcCRC(trackData->CRC,sample,trackData->sampleOffset);
				CalcCRCv2(trackData->CRCv2,sample,trackData->sampleOffset);
			}

		} // for (i)

		// if this was a positive CRC offset, add samples for following track
		if ((trackData->sampleOffset+trackData->CRCoffset)==trackData->sampleCount)
		{
			if (trackData->nextTrack)
			{
				for(j = 0; j<trackData->CRCoffset;j++)
				{
					trackData->sampleOffset++;
					CalcCRC(trackData->CRC,trackData->nextTrack->firstFrames[j],trackData->sampleOffset);
					CalcCRCv2(trackData->CRCv2,trackData->nextTrack->firstFrames[j],trackData->sampleOffset);
				}
			}
		}

		// are we done with all samples? note that we may reach here again if there is a large number of trimmed samples
		if (!trackData->MD5checked && trackData->sampleOffset==trackData->sampleCount && !trackData->eaccrcFinalized)
		{
			trackData->eaccrc = trackData->eaccrc^0xffffffff;
			trackData->eaccrcnslr = trackData->eaccrcnslr^0xffffffff;
			trackData->eaccrcFinalized = true;
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
	
	switch(metadata->type)
	{
	/*if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)*/
	case FLAC__METADATA_TYPE_STREAMINFO:
	{

		trackData->sampleCount = metadata->data.stream_info.total_samples;
		if(trackData->trimSamples)
		{
			// Remove the famous trailing 4608 FLAC sample pad...
			// note that this amount varies, especially on first and last tracks
			// 4608 will leave 492 unaligned samples.  
			// we do a small correction here for the common cases of 480 and 462 unaligned samples, others will likely 
			// just produce garbage
			if (trackData->sampleCount % 588)
			{
				trackData->trimSamples = 4608 + (trackData->sampleCount%588 - 492);
				trackData->sampleCount -= trackData->trimSamples;
			}
			else
			{
				trackData->trimSamples = 0;
			}
		}
		if((trackData->sampleCount%588) != 0)
		{
			trackData->error = "[Metadata] FLAC file does not contain a whole number of CDDA frames!";
			trackData->statusGood = false;
		}		
		else if((metadata->data.stream_info.channels != 2) || (metadata->data.stream_info.sample_rate != 44100) || (metadata->data.stream_info.bits_per_sample != 16))
		{
			trackData->error = "[Metadata] Only stereo 44.1kHz/16bit audio (CDDA) is supported";
			trackData->statusGood = false;
		}
		break;
	}
	
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
	}
}

void ArFLAC__error_hook(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	ArFLAC_trackInfo* trackData = client_data;
	trackData->error = FLAC__StreamDecoderErrorStatusString[status];
	trackData->statusGood = false;
}




ArFLAC_discInfo* ArFLAC_openDisc(ArFLAC_msgCallback logger)
{
	ArFLAC_discInfo* disc = malloc(sizeof(ArFLAC_discInfo));
	
	if(disc)
	{
		for (int k=0;k<ARFLAC_MAXTRACKS;k++)
		{
			disc->tracks[k] = NULL;
		}
		disc->trackCount = disc->ArIdAddn = disc->ArIdMult = disc->CDDBId = 0;
		disc->pregap=0;
		disc->shiftSamples=0;
		disc->err = logger;
		disc->ArFile[0] = disc->url[0] = '\0';
	}

	InitCRC32Table();
	return disc;
}

ArFLAC_bool ArFLAC_appendTrack(ArFLAC_discInfo* disc, ArFLAC_trackInfo* track)
{
	if(disc->trackCount <= ARFLAC_MAXTRACKS)
	{
		// setup doubly-linked list, a bit redundant since *disc has the master array of tracks, but easier for code-blocks that do not have access to *disc
		if (disc->trackCount>0)
		{
			track->prevTrack = disc->tracks[disc->trackCount-1];
			track->prevTrack->nextTrack = track;
		}
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


ArFLAC_bool ArFLAC_generateDiscIds(ArFLAC_discInfo* disc)
{
	int trackNo;
	unsigned int sampleOffset = 0;
	
	if(!disc->trackCount)
	{
		disc->err("No tracks to calculate Disc IDs from!");
		return false;
	}
	
	/* This is for the first track.  */
	// disc->ArIdMult = 1;
	sampleOffset = disc->pregap*588;
	disc->CDDBId = 0;
	disc->ArIdAddn = abs(disc->pregap);
	disc->ArIdMult = (disc->pregap ? abs(disc->pregap) : 1);
	
	/* DiscID1 is the sum of the frame offsets from the start of the CD of all the tracks + the length of the CD in frames */
	for(trackNo = 0; trackNo < disc->trackCount; trackNo++)
	{
		/* This one doesn't take the total CD length into account, so it comes before sampleOffset += ... */
		disc->CDDBId += ArFLAC__sumDigits((sampleOffset/44100) + 2); // the +2 is for 150frame padding at beginning of disc
		
		/* This will now hold the offset - in samples - from the start of the CD to the start of disc->tracks[trackNo+1] */
		sampleOffset += disc->tracks[trackNo]->sampleCount;
		
		/* Because this is after sampleOffset += ... then it it's really operating on trackNo+1. 
		 * This is good, because it needs to include the length of the CD in frames
		 * (e.g. the offset to the track after the last track) */
		disc->ArIdAddn += (sampleOffset/588);
		disc->ArIdMult += (sampleOffset/588) * (trackNo+2);
	}

    disc->CDDBId = ((disc->CDDBId%0xFF) << 24) + ((sampleOffset/44100) << 8) + disc->trackCount;

	disc->ArIdAddn &= 0xFFFFFFFF;
	disc->ArIdMult &= 0xFFFFFFFF;
	disc->CDDBId &= 0xFFFFFFFF;
	
	return true;
}

ArFLAC_bool ArFLAC_generateArURL(ArFLAC_discInfo* disc)
{
	if(disc->ArIdAddn && disc->ArIdMult && disc->CDDBId)
	{
		snprintf(disc->ArFile, 40, "dBAR-%.3d-%.8x-%.8x-%.8x.bin", disc->trackCount, disc->ArIdAddn, disc->ArIdMult, disc->CDDBId);
		snprintf(disc->url, 85, "http://www.accuraterip.com/accuraterip/%.1x/%.1x/%.1x/%s", disc->ArIdAddn & 0xF, disc->ArIdAddn>>4 & 0xF, disc->ArIdAddn>>8 & 0xF, disc->ArFile);
		//BUGBUG...relocate to /tmp
		snprintf(disc->ArFile, 45, "/tmp/dBAR-%.3d-%.8x-%.8x-%.8x.bin", disc->trackCount, disc->ArIdAddn, disc->ArIdMult, disc->CDDBId);
		return true;
	}
	else
	{
		disc->err("ArFLAC_generateArURL called before IDs have been generated.");
		return false;
	}
}

ArFLAC_bool ArFLAC_processArData(ArFLAC_discInfo* disc, const unsigned char* buffer, FLAC__uint32 length)
{
	unsigned int dataPtr;
	unsigned int resultNo,trackNo;
	unsigned int ArResultCount;

	if(length % (disc->trackCount*9 + 13))
	{
		disc->err("dBAR file has illegal length %u",length);
		return false;
	}
	ArResultCount=(length / (disc->trackCount*9 + 13));
  
	for(trackNo = 0; trackNo < disc->trackCount; trackNo++)
	{
		disc->tracks[trackNo]->ArResultCount = ArResultCount;
		disc->tracks[trackNo]->ArConfidence = malloc(ArResultCount*sizeof(unsigned char));
		disc->tracks[trackNo]->ArCRC = malloc(ArResultCount*sizeof(FLAC__uint32));
		disc->tracks[trackNo]->ArOffsetFindCRC = malloc(ArResultCount*sizeof(FLAC__uint32));
		disc->tracks[trackNo]->matchedOffset = malloc(ArResultCount*sizeof(FLAC__int32));
		if (disc->tracks[trackNo]->ArConfidence==NULL || 
			disc->tracks[trackNo]->ArCRC==NULL || 
			disc->tracks[trackNo]->ArOffsetFindCRC==NULL ||
			disc->tracks[trackNo]->matchedOffset==NULL)
		{
			disc->err("Failed to allocate memory for AR results");
			return false;
		}
	}
	
	for(dataPtr = resultNo = 0; dataPtr < length; resultNo++)
	{

		unsigned char chunkTrackCount	= buffer[dataPtr];
		FLAC__uint32 chunkArIdAddn		= *(FLAC__uint32*)&buffer[dataPtr+1];
		FLAC__uint32 chunkArIdMult		= *(FLAC__uint32*)&buffer[dataPtr+5];
		FLAC__uint32 chunkCDDBId		= *(FLAC__uint32*)&buffer[dataPtr+9];

		dataPtr += 13;
						
		if((chunkTrackCount == disc->trackCount) && 
		   (chunkArIdAddn == disc->ArIdAddn) && 
		   (chunkArIdMult == disc->ArIdMult) && 
		   (chunkCDDBId == disc->CDDBId))
		{
			for(trackNo = 0; trackNo < disc->trackCount; trackNo++)
			{
				disc->tracks[trackNo]->ArConfidence[resultNo] = buffer[dataPtr];
				disc->tracks[trackNo]->ArCRC[resultNo] = *(FLAC__uint32*)&buffer[dataPtr+1];
				disc->tracks[trackNo]->ArOffsetFindCRC[resultNo] = *(FLAC__uint32*)&buffer[dataPtr+5];
				disc->tracks[trackNo]->matchedOffset[resultNo] = 0;
				if (disc->tracks[trackNo]->ArOffsetFindCRC[resultNo] && (resultNo>0) && disc->tracks[trackNo]->ArOffsetFindCRC[resultNo-1]==0)
				{
					//BUGBUG once we've seen a zero are we at end of offsetCRC results?
					disc->err("Result #%u has bogus non-zero offsetCRC", resultNo+1);
					return false;
				}
				dataPtr += 9;
			}
		}
		else
		{
			if(chunkTrackCount != disc->trackCount)
			{
				disc->err("Result #%u Chunk track count (%d) does not match ours (%d)", resultNo+1, chunkTrackCount, disc->trackCount);
			}
							
			if(chunkArIdAddn != disc->ArIdAddn)
			{
				disc->err("Result #%u Chunk ArIdAddn (%08x) does not match ours (%08x)", resultNo+1, chunkArIdAddn, disc->ArIdAddn);
			}
						
			if(chunkArIdMult != disc->ArIdMult)
			{
				disc->err("Result #%u Chunk ArIdMult (%08x) does not match ours (%08x)", resultNo+1, chunkArIdMult, disc->ArIdMult);
			}
							
			if(chunkCDDBId != disc->CDDBId)
			{
				disc->err("Result #%u Chunk CDDBId (%08x) does not match ours (%08x)", resultNo+1, chunkCDDBId, disc->CDDBId);
			}
							
			return false;
		}
	}
	
	return true;
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
	for (int k=0;k<disc->trackCount;k++)
	{
		ArFLAC_closeTrack(disc->tracks[k]);
		disc->tracks[k]=NULL;
	}
	free(disc);
}

int ArFLAC_getDiscNumber(const char* filename)
{
	FLAC__StreamMetadata *tags;
	int discNumber = 0;
	if (!FLAC__metadata_get_tags(filename, &tags))
		printMsg("No tags.");
	else {
		FLAC__StreamMetadata_VorbisComment_Entry *comments;
		char *discNumberStr, *fieldname;
		comments = tags->data.vorbis_comment.comments;
		if(FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(
			comments[FLAC__metadata_object_vorbiscomment_find_entry_from(tags,0,"DISCNUMBER")],
			&fieldname,&discNumberStr)) {
			discNumber = atoi(discNumberStr);
			if (discNumber<1)
				printMsg("Found bad DISCNUMBER tag");
			free(discNumberStr);
			free(fieldname);
		}
	}
	FLAC__metadata_object_delete(tags);
	return discNumber;
}


// Call this function only once to initialize the CRC table
void InitCRC32Table()
{
	unsigned int ulPolynomial = 0x04c11db7;
	int i,j;
	// 256 values representing ASCII character codes.
	for(i = 0; i <= 0xFF; i++)
	{
		CRC32Table[i] = Reflect(i, 8) << 24;
		for (j = 0; j < 8; j++)
			CRC32Table[i] = (CRC32Table[i] << 1) ^ (CRC32Table[i] & (1 << 31) ? ulPolynomial : 0);
		CRC32Table[i] = Reflect(CRC32Table[i], 32);
	}
}
/*
	Reflection is a requirement for the official CRC-32 standard.
	You can create CRCs without it, but they won't conform to the standard.
	Used only by InitCRC32Table()
*/
unsigned int Reflect(unsigned int ref, char ch)
{
	unsigned int value=0;
	int i;
	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for (i = 1; i < (ch + 1); i++)
	{
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}
