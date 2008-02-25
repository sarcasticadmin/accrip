#include <FLAC/stream_decoder.h>

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

#define ARFLAC_MAXTRACKS 100

typedef unsigned char ArFLAC_bool;
typedef FLAC__uint32 ArFLAC_uint32;
typedef FLAC__uint64 ArFLAC_uint64;
typedef FLAC__StreamDecoder ArFLAC_decoder;
typedef void(*ArFLAC_msgCallback)(const char* text, ...);

typedef struct
{
	ArFLAC_decoder* decoder;
	ArFLAC_uint32 CRC;
	
	unsigned char* ArConfidence;
	ArFLAC_uint32* ArCRC;
	ArFLAC_uint32* ArOffsetFindCRC;
	unsigned short ArResultCount;
	unsigned short ArResultSpaceAllocated;
	
	ArFLAC_uint64 sampleCount;
	ArFLAC_uint64 sampleOffset;
	ArFLAC_bool isFirst;
	ArFLAC_bool isLast;
	ArFLAC_bool statusGood;
	const char* error;
	ArFLAC_msgCallback err;
} ArFLAC_trackInfo;

typedef struct
{
	ArFLAC_trackInfo* tracks[ARFLAC_MAXTRACKS];
	short trackCount;
	ArFLAC_uint32 ArId1;
	ArFLAC_uint32 ArId2;
	ArFLAC_uint32 CDDBId;
	ArFLAC_msgCallback err;
	char url[85]; /* http://www.accuraterip.com/accuraterip/A/B/C/dBAR-NNN-XXXXXXXX-YYYYYYYY-ZZZZZZZZ.bin */
	char file[40]; /* dBAR-NNN-XXXXXXXX-YYYYYYYY-ZZZZZZZZ.bin */
} ArFLAC_discInfo;

ArFLAC_trackInfo* ArFLAC_openTrack(const char* filename, ArFLAC_bool isFirst, ArFLAC_bool isLast, ArFLAC_msgCallback logger);

ArFLAC_bool ArFLAC_readMetadata(ArFLAC_trackInfo* trackData);
ArFLAC_bool ArFLAC_generateCRC(ArFLAC_trackInfo* trackData);

void ArFLAC_closeTrack(ArFLAC_trackInfo* trackData);


ArFLAC_discInfo* ArFLAC_openDisc(ArFLAC_msgCallback logger);

ArFLAC_bool ArFLAC_appendTrack(ArFLAC_discInfo* disc, ArFLAC_trackInfo* track);
ArFLAC_bool ArFLAC_appendTracks(ArFLAC_discInfo* disc, ArFLAC_trackInfo** tracks, short trackCount);
ArFLAC_bool ArFLAC_generateDiscIds(ArFLAC_discInfo* disc);
ArFLAC_bool ArFLAC_generateArURL(ArFLAC_discInfo* disc);
ArFLAC_bool ArFLAC_processArData(ArFLAC_discInfo* disc, const unsigned char* buffer, ArFLAC_uint32 length);

void ArFLAC_closeDisc(ArFLAC_discInfo* disc);
