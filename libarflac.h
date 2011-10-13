#include <FLAC/stream_decoder.h>
#include <FLAC/metadata.h>

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#define ARFLAC_MAXTRACKS 100
#define CalcCRC(CRC,sample,offset) CRC += (sample)*(offset);
#define CalcCRCv2(CRCv2,sample,offset) CRCv2 += (((FLAC__uint64)sample * (FLAC__uint64)offset) & 0xffffffff) + (((FLAC__uint64)sample * (FLAC__uint64)offset)>>32);

typedef unsigned char ArFLAC_bool;
//typedef FLAC__uint32 ArFLAC_uint32;
//typedef FLAC__uint64 ArFLAC_uint64;
//typedef FLAC__int64 ArFLAC_int64;
//typedef FLAC__StreamDecoder ArFLAC_decoder;
typedef void(*ArFLAC_msgCallback)(const char* text, ...);

typedef struct ArFLAC_trackInfo_struct
{
	FLAC__StreamDecoder* decoder;
	FLAC__uint32	CRC;
	//TRR
	FLAC__uint32	CRCv2;
	ArFLAC_bool   	MD5checked;
	ArFLAC_bool   	eaccrcFinalized;
	unsigned int	eaccrc;
	unsigned int	eaccrcnslr;
	FLAC__int32		CRCoffset;
	FLAC__int32* 	matchedOffset;
	FLAC__uint32	firstFrames[588*5];
	FLAC__uint32	frame450[588*10];
	FLAC__uint32	lastFrames[588*5];
	FLAC__uint32	trimSamples;
	struct ArFLAC_trackInfo_struct *prevTrack;
	struct ArFLAC_trackInfo_struct *nextTrack;
	char*			filename;
	//TRR

	unsigned short ArResultCount;
	unsigned char* ArConfidence;
	FLAC__uint32* 	ArCRC;
	FLAC__uint32* 	ArOffsetFindCRC;
	
	FLAC__uint64	sampleCount;
	FLAC__uint64	sampleOffset;
	ArFLAC_bool 	statusGood;
	const char*	error;
	ArFLAC_msgCallback err;
} ArFLAC_trackInfo;

typedef struct
{
	ArFLAC_trackInfo* tracks[ARFLAC_MAXTRACKS];
	//TRR
	FLAC__int32 pregap;
	int shiftSamples;
	//TRR
	short trackCount;
	FLAC__uint32 ArIdAddn;
	FLAC__uint32 ArIdMult;
	FLAC__uint32 CDDBId;
	ArFLAC_msgCallback err;
	char url[85]; /* http://www.accuraterip.com/accuraterip/A/B/C/dBAR-NNN-XXXXXXXX-YYYYYYYY-ZZZZZZZZ.bin */
	char ArFile[256]; /* dBAR-NNN-XXXXXXXX-YYYYYYYY-ZZZZZZZZ.bin */
} ArFLAC_discInfo;

ArFLAC_trackInfo* ArFLAC_openTrack(const char* filename, ArFLAC_msgCallback logger);
ArFLAC_bool ArFLAC_reopenTrack(ArFLAC_trackInfo* trackData); //TRR new func

ArFLAC_bool ArFLAC_readMetadata(ArFLAC_trackInfo* trackData);
ArFLAC_bool ArFLAC_generateCRC(ArFLAC_trackInfo* trackData);

void ArFLAC_closeTrack(ArFLAC_trackInfo* trackData);

ArFLAC_discInfo* ArFLAC_openDisc(ArFLAC_msgCallback logger);

ArFLAC_bool ArFLAC_appendTrack(ArFLAC_discInfo* disc, ArFLAC_trackInfo* track);
//ArFLAC_bool ArFLAC_appendTracks(ArFLAC_discInfo* disc, ArFLAC_trackInfo** tracks, short trackCount);
ArFLAC_bool ArFLAC_generateDiscIds(ArFLAC_discInfo* disc);
ArFLAC_bool ArFLAC_generateArURL(ArFLAC_discInfo* disc);
ArFLAC_bool ArFLAC_processArData(ArFLAC_discInfo* disc, const unsigned char* buffer, FLAC__uint32 length);

void ArFLAC_closeDisc(ArFLAC_discInfo* disc);
int ArFLAC_getDiscNumber(const char* filename);