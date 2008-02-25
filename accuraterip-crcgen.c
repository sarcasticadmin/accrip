#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h> 
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "libarflac.h"

void printMsg(const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[2049];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, 2048, text, argsPtr);
	va_end(argsPtr);

	fprintf(stderr, "%s\n", textbuffer);
}

void bailOut(const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[2049];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, 2048, text, argsPtr);
	va_end(argsPtr);

	fprintf(stderr, "%s\n", textbuffer);
	exit(EXIT_FAILURE);	
}

int FLACfilter(const struct dirent* file)
{
	return strstr(file->d_name, ".flac") ? 1 : 0;
}

int main(int argc, char** argv)
{
	int i;
	
	if(argc <= 1)
		bailOut("%s: /path/to/album/flacs [/path/to/more/album/flacs]", argv[0]);
	
	for(i = 1; i < argc; i++)
	{
		ArFLAC_discInfo* disc;
		struct dirent** namelist;
		int n;
		
		if(!(disc = ArFLAC_openDisc(printMsg)))
		{
			bailOut("Failed to create disc struct.");
		}

		n = scandir(argv[i], &namelist, FLACfilter, alphasort);
		
		if(n < 0)
		{
			bailOut("scandir(%s) failed: %s", argv[i], strerror(errno));
		}
		else
		{
			struct stat fileStat;
			FILE* file;
			int k;

			for(k = 0; k < n; k++)
			{
				ArFLAC_trackInfo* track;
				char path[1024];
				
				snprintf(path, 1024, "%s/%s", argv[i], namelist[k]->d_name);
			
				if(!(track = ArFLAC_openTrack(path, (k == 0), (k == n-1), printMsg)))
				{
					bailOut("Failed to open track");
				}
				
				if(!ArFLAC_readMetadata(track))
				{
					bailOut("Failed to read track metadata.");
				}
				
				ArFLAC_appendTrack(disc, track);
							
				free(namelist[k]);
			}
			
			free(namelist);
			
			if(ArFLAC_generateDiscIds(disc))
			{
				printMsg("Track\tRipping Status\t\t[Disc ID: %08x-%08x-%08x]", disc->ArId1, disc->ArId2, disc->CDDBId);
			}
			
			if(ArFLAC_generateArURL(disc))
			{
				char command[1024];
				/*printMsg("Fetching AccurateRip data from: %s ...", disc->url);*/
				snprintf(command, 1024, "wget -nc --quiet \"%s\"", disc->url);
				system(command);
			}
	
			if(stat(disc->file, &fileStat))
			{
				bailOut("Failed to stat %s: %s", disc->file, strerror(errno));
			}
			
			if(!fileStat.st_size)
			{
				bailOut("Disc not present in database.");
			}
			
			if(fileStat.st_size > 16384)
			{
				bailOut("AccurateRip datafile too large (%d bytes)", fileStat.st_size);
			}
			
			if((file = fopen(disc->file, "r")))
			{
				int bytesRead = 0;
				unsigned char* buffer = malloc(fileStat.st_size);
				
				while(bytesRead < fileStat.st_size)
				{
					int readstatus = fread(buffer, sizeof(unsigned char), fileStat.st_size - bytesRead, file);

					if(readstatus)
					{
						bytesRead += readstatus;
					}
					else
					{
						bailOut("Read error in fread()");
					}
				}
				
				if(!ArFLAC_processArData(disc, buffer, bytesRead))
				{
					printMsg("Failed to read AccurateRip information about the rip");
				}				
			}
			else
			{
				bailOut("Failed to open %s: %s", disc->file, strerror(errno));
			}
			
			for(k = 0; k < disc->trackCount; k++)
			{
				ArFLAC_trackInfo* track = disc->tracks[k];
				if(ArFLAC_generateCRC(track))
				{
					int m;
					unsigned char bestConfidence = 0;
					ArFLAC_uint32 bestCRC = 0;
					ArFLAC_bool gotResult = false;
					
					for(m = 0; m < track->ArResultCount; m++)
					{
						if(track->ArConfidence[m] > bestConfidence)
						{
							bestConfidence = track->ArConfidence[m];
							bestCRC = track->ArCRC[m];
						}
						
						if(track->CRC == track->ArCRC[m])
						{
							printMsg(" %d\tAccurately Ripped    (confidence %2d)     [%08x]", k+1, track->ArConfidence[m], track->ArCRC[m]);
							gotResult = true;
						}
					}
					
					if(!gotResult)
					{
						printMsg(" %d\t** Rip not accurate **   (confidence %2d)     [%08x] [%08x]", k+1, bestConfidence, bestCRC, track->CRC);
					}
				}
				else
				{
					bailOut("Failed to generate CRC.");
				}
			}
		}
		
		ArFLAC_closeDisc(disc);
	}
			
	return 0;
}
