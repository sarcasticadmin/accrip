// #include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h> 
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>

#include "libarflac.h"

void printMsg(const char* text, ...)
{
    va_list argsPtr;
    char textbuffer[2049];

    va_start(argsPtr, text);
    vsnprintf(textbuffer, 2048, text, argsPtr);
    va_end(argsPtr);

    printf("%s\n", textbuffer);
}

void printTxt(const char* text, ...)
{
    va_list argsPtr;
    char textbuffer[2049];

    va_start(argsPtr, text);
    vsnprintf(textbuffer, 2048, text, argsPtr);
    va_end(argsPtr);

    printf("%s", textbuffer);
}

void bailOut(const char* text, ...)
{
    va_list argsPtr;
    char textbuffer[2049];

    va_start(argsPtr, text);
    vsnprintf(textbuffer, 2048, text, argsPtr);
    va_end(argsPtr);

    fflush(stdout);
    fprintf(stderr, "%s\n", textbuffer);

    exit(-1);
}

int FLACfilter(const struct dirent* file)
{
    size_t len = strlen(file->d_name);
    if (len<6)
        return 0;
    else
        return strcmp(file->d_name+len-5,".flac") ? 0 : 1;
}

int main(int argc, char** argv)
{
    int r;
    
    if(argc <= 1)
        bailOut("%s: [-d disc#] [-o pregap-offset] [-s shift-samples] [-a disc-id] [-t] [-v] [-r|-x] /path/to/album/flacs [/path/to/more/album/flacs]\n\n" \
                "  -v         verbose\n" \
                "  -x         do not check AccurateRip database\n" \
                "  -r         do not calculate CRC values\n" \
                "  -o offset  first-track offset (sector) for computing DiscID\n" \
                "  -s shift   number of samples to shift\n" \
                "  -t         skip final 4608 samples (old FLAC padding)\n" \
                "  -d num     only use tracks with DISCNUMBER=#\n" \
                "  -a discid  force AccurateRip disc ID\n",
                argv[0]);

    int optcmd,optnum;
    char *optptr=NULL;
    FLAC__int32 pregap = 0;
    int shiftSamples = 0;
    int discNumber = 0;
    char *discId = NULL;
    FLAC__uint32 trimSamples = 0;
    ArFLAC_bool verbose = false;
    ArFLAC_bool only_check_for_record = false;
    ArFLAC_bool no_Ar_compare = false;
        
    opterr = 0;
    while ((optcmd = getopt (argc, argv, "a:o:s:d:rtvx")) != -1) {
        switch (optcmd) {
            case 'o':
                pregap = strtoul(optarg, &optptr, 10);
                if (*optptr || pregap < 0)
                    bailOut("Option -o requires a positive numerical pregap offset (in FRAMES).");
                break;
            case 's':
                shiftSamples = strtoul(optarg, &optptr, 10);
                if (*optptr)
                    bailOut("Option -s requires a numerical value.");
                break;
            case 'r':
                only_check_for_record = true;
                break;
            case 't':
                //BUGBUG clean this up...this gets translated to ~4608 samples removed
                trimSamples=1;
                break;
            case 'v':
                    verbose = true;
                    break;
            case 'x':
                    no_Ar_compare = true;
                    break;
            case 'd':
                discNumber = strtoul(optarg, &optptr, 10);
                if (*optptr || discNumber < 1)
                    bailOut("Option -d requires a disc number.");
                break;
            case 'a':
                discId = optarg;
                break;
            case '?':
                if (optopt == 'o')
                    bailOut("Option -o requires a pregap offset (in FRAMES).");
                else if (optopt == 's')
                    bailOut("Option -s requires number of samples.");
                //else if (optopt == 't')
                //    bailOut("Option -t requires number of samples.");
                else
                    bailOut("Unknown option.");
            default:
                bailOut("getopt() failure");
        }
    } // while (getopt)

    int badTracks=0;
    for(optnum = optind; optnum < argc; optnum++)
    {
        ArFLAC_discInfo* disc;
        struct dirent** namelist;
        int n;
        
        if(!(disc = ArFLAC_openDisc(printMsg)))
        {
            bailOut("Failed to create disc struct.");
        }

        n = scandir(argv[optnum], &namelist, FLACfilter, alphasort);
        
        if(n < 0)
        {
            bailOut("scandir(%s) failed: %s", argv[optnum], strerror(errno));
        }

        /* scan thru directory, presume track# is part of filename and thus filenames are sortable to track order */
        int k;
        int skippedFileCount = 0;
        for(k = 0; k < n; k++)
        {
            ArFLAC_trackInfo* track;
            char path[1024];
            
            snprintf(path, 1024, "%s/%s", argv[optnum], namelist[k]->d_name);

            if (discNumber && ArFLAC_getDiscNumber(path)!=discNumber)
            {
                skippedFileCount++;
                continue;
            }
            
            if(!(track = ArFLAC_openTrack(path, printMsg)))
            {
                bailOut("Failed to open track: %s",path);
            }
            track->trimSamples = trimSamples;

            //BUGBUG should we do something for negative pregap here, to adjust start sampleOffset?
            if(!ArFLAC_readMetadata(track))
            {
                bailOut("Failed to read track metadata: %s",path);
            }
            
            if(!ArFLAC_appendTrack(disc, track))
            {
                bailOut("Failed to append track: %s",path);
            }
                        
            free(namelist[k]);
        } // for (k)
        free(namelist);

        if (skippedFileCount)
            printMsg("Disc %d: ignoring %d tracks from other discs",discNumber,skippedFileCount);

        disc->pregap = pregap;
        if (disc->pregap)
            printMsg ("First track starts at sector offset %d",disc->pregap);
        disc->shiftSamples = shiftSamples;
        if (disc->shiftSamples)
            printMsg ("Offset samples %d BUGBUG",disc->shiftSamples);
        if (trimSamples)
            printMsg ("Ignoring final ~4608 samples");

        if (discId)
        {
            disc->ArIdAddn = strtol(discId, (char**) discId+8, 16);
            disc->ArIdMult = strtol(discId+9, (char**) discId+17, 16);
            disc->CDDBId = strtol(discId+18, (char**) discId+26, 16);
            printTxt("\n[Disc ID: %08x-%08x-%08x] ", disc->ArIdAddn, disc->ArIdMult, disc->CDDBId);
        }
        else if(ArFLAC_generateDiscIds(disc))
        {
            printTxt("\n[Disc ID: %08x-%08x-%08x] ", disc->ArIdAddn, disc->ArIdMult, disc->CDDBId);
        }
        else
        {
            bailOut("Failed to generate AccurateRip disc id");
        }


        if(!no_Ar_compare)
        {
            /* get AccurateRip CRC records */
            struct stat fileStat;
            FILE* file;
            if(ArFLAC_generateArURL(disc))
            {
                char command[1024];
                if (verbose)
                        printMsg("\nFetching AccurateRip data from: %s ...\n", disc->url);
                snprintf(command, 1024, "wget --quiet -O \"%s\" \"%s\"", disc->ArFile, disc->url);
                r=system(command);
                if(WEXITSTATUS(r))
                {
                        bailOut("Not in AccurateRip database\n",WEXITSTATUS(r));
                }
            }
            if(stat(disc->ArFile, &fileStat))
            {
                bailOut("\nNot in AccurateRip database (wget error?)\n");
            }
            if(!fileStat.st_size)
            {
                bailOut("\nNot in AccurateRip database (wget error?)\n");
            }
            if(fileStat.st_size > 16384)
            {
                bailOut("\nAccurateRip datafile too large (%d bytes)\n", fileStat.st_size);
            }
            
            if((file = fopen(disc->ArFile, "r")))
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
                        bailOut("Read error in fread(%s)",disc->ArFile);
                    }
                }
                
                if(!ArFLAC_processArData(disc, buffer, bytesRead))
                {
                    bailOut("Failed to read AccurateRip information about the rip");
                }
                else
                {
                    printTxt("Found %d Result%s. ",
                             disc->tracks[0]->ArResultCount, 
                             disc->tracks[0]->ArResultCount>1 ? "s":"");
                }
            } // if fopen()
            else
            {
                bailOut("Failed to fopen(%s): %s", disc->ArFile, strerror(errno));
            }
        } // get AR record
        
        if (only_check_for_record)
        {
            printMsg("");
            for(k = 0; k < disc->trackCount; k++)
            {
                ArFLAC_trackInfo* track = disc->tracks[k];
                printTxt(" %2d  ",k+1);
                printTxt("%d:%02d.%02d(.%03d)    ",
                         track->sampleCount/44100/60,
                         track->sampleCount/44100%60,
                         track->sampleCount/588%75,
                         (10000*(track->sampleCount/588%75)+375)/750);
                printMsg("%s ",track->filename);
            }
            ArFLAC_closeDisc(disc);
            continue; // skip CRC check, just print existence of AR record
        }
        
        printMsg("Scanning %d tracks",disc->trackCount);
        for(k = 0; k < disc->trackCount; k++)
        {
            ArFLAC_trackInfo* track = disc->tracks[k];
            if(!ArFLAC_generateCRC(track))
            {
                bailOut("Failed to generate CRC: %s",track->filename);
            }
        } // for (k)
            
        printMsg(
            "\n"
            "Tr#  Length           CRC32      w/o NULL       ArCRC      ArCRCv2        Status (confidence)"
            );
        
        for(k = 0; k < disc->trackCount; k++)
        {
            ArFLAC_trackInfo* track = disc->tracks[k];
            unsigned char bestConfidence = 0;
            unsigned char bestConfidencev2 = 0;
            int bestConfidenceIndex = 0;
            int bestConfidenceIndexv2 = 0;
            ArFLAC_bool gotResult = false;
            ArFLAC_bool gotResultv2 = false;
            ArFLAC_bool gotAtLeastOneMatch = false;
            int totalConfidenceAllResults = 0;
            int m,n;

            printTxt(" %2d  ",k+1);
            printTxt("%d:%02d.%02d(.%03d)    ",
                track->sampleCount/44100/60,
                track->sampleCount/44100%60,
                track->sampleCount/588%75,
                (10000*(track->sampleCount/588%75)+375)/750);
            printTxt("[%08X] [%08X]     ", track->eaccrc,track->eaccrcnslr);
            printTxt("[%08X] [%08X]     ", track->CRC,track->CRCv2);
            //printTxt("%s ",track->filename);

            if(no_Ar_compare) {
                printTxt("n/a\n");
                continue;
            }
            
            for(m = 0; m < track->ArResultCount; m++)
            {
                // sometimes a result has zero (0) confidence, so skip it
                if (!track->ArConfidence[m])
                    continue;
                totalConfidenceAllResults += track->ArOffsetFindCRC[m];
                if (track->CRC==track->ArCRC[m] || track->CRCv2==track->ArCRC[m])
                {
                    if (track->CRC==track->ArCRC[m])
                    {
                        if (track->ArConfidence[m] > bestConfidence)
                        {
                            bestConfidence = track->ArConfidence[m];
                            bestConfidenceIndex = m+1;
                        }
                        gotResult = true;
                    }
                    else // if (track->CRCv2==track->ArCRC[m])
                    {
                        if (track->ArConfidence[m] > bestConfidencev2)
                        {
                            bestConfidencev2 = track->ArConfidence[m];
                            bestConfidenceIndexv2 = m+1;
                        }
                        gotResultv2 = true;
                    }
                    gotAtLeastOneMatch = true;
                }
            } // for (m)
            if(gotAtLeastOneMatch)
            {
                if(gotResult && gotResultv2)
                {
                    printMsg("AccurateRip (%u) @%d, ARv2 (%u) @%d", 
                             bestConfidence,
                             bestConfidenceIndex,
                             bestConfidencev2,
                             bestConfidenceIndexv2);
                }
                else
                {
                    // in this case, one of the two versions is zeroed, so adding them just yields back the one that got a result
                    printTxt("AccurateRip%s (%u) ", 
                             bestConfidencev2 ? "V2": "",
                             bestConfidence+bestConfidencev2);
                    if(track->ArResultCount>1)
                        printTxt("@%d ", 
                                 bestConfidenceIndex+bestConfidenceIndexv2);
                    printMsg("");
                }
            }
            else
            {
                //BUGBUG ugly code dup hack to decide whether to print "Partial Match" at offset=0
                unsigned int ii;
                ArFLAC_bool partialMatch = false;
                track->CRC = 0;
                for(ii=0;ii < 588;ii++)
                {
                    // OffsetCRC is based on ARv1
                    CalcCRC(track->CRC,track->frame450[588*5+ii],ii+1);
                }
                for(m = 0; m < track->ArResultCount && track->ArOffsetFindCRC[m]; m++)
                {
                    if (track->CRC==track->ArOffsetFindCRC[m])
                    {
                        partialMatch=true;
                        break;
                    }
                } // for (m=resultNo)
                if(totalConfidenceAllResults == 0)
                {
                    if(partialMatch)
                    {
                        printMsg("Partial Match, But No AccurateRip CRC");
                    }
                    else
                    {
                        printMsg("No AccurateRip CRC");
                    }
                    continue;
                }
                else if(partialMatch)
                    printMsg("Partial Match");
                else
                    printMsg("---");
            }

            if((!gotResult && !gotResultv2) || verbose)
            {
                // make a list of matching offsets where frame450 CRC is in the OffsetFindCRC results
                // we will look starting 5 frames before frame450, per AR spec
                unsigned int ii,jj;
                int matches=0;
                //BUGBUG really should not do per-track offsets but instead build a disc-wide list and then compute new CRCs
                /*#define MAX_POSSIBLE_OFFSETS 100
                FLAC__int32 possibleOffsets[MAX_POSSIBLE_OFFSETS];*/
                for(jj=0;jj<588*9;jj++)
                {
                    track->CRC = 0;
                    for(ii=0;ii < 588;ii++)
                    {
                        // OffsetCRC is based on ARv1
                        CalcCRC(track->CRC,track->frame450[jj+ii],ii+1);
                    }
                    for(m = 0; m < track->ArResultCount && track->ArOffsetFindCRC[m]; m++)
                    {
                        //if (track->ArConfidence[m] && track->CRC==track->ArOffsetFindCRC[m])
                        //BUGBUG don't check confidence for offsetCRC??
                        if (track->CRC==track->ArOffsetFindCRC[m])
                        {
                            track->matchedOffset[matches] = (jj-(588*5));
                            matches++;
                        }
                    } // for (m=resultNo)
                } // for (jj=sample)

                // now loop thru matching offsets.  
                for(n = 0; n < matches; n++)
                {
                    ArFLAC_bool gotAtLeastOneOffsetMatch = false;
                    track->CRCoffset = track->matchedOffset[n];
                    if(!ArFLAC_reopenTrack(track))    // reopenTrack() will zero out track->CRC,CRCv2
                    {
                        bailOut("Failed to reopen track.");
                    }
                    else if(!ArFLAC_generateCRC(track))
                    {
                        bailOut("Failed to generate CRC.");
                    }
                    for(m = 0; m < track->ArResultCount; m++)
                    {
                        if(!track->ArConfidence[m] || !track->ArCRC[m] || (!track->CRCoffset && !verbose))
                        {
                            continue;
                        }
                        else
                        {
                            if (track->CRC==track->ArCRC[m] || track->CRCv2==track->ArCRC[m])
                            {
                                if(track->CRC==track->ArCRC[m])
                                {
                                    printTxt(
                                        "                                                [%08X]                AccurateRip (%u) offset %+d", 
                                        track->CRC,
                                        track->ArConfidence[m],
                                        track->CRCoffset
                                    );
                                }
                                else // if(track->CRCv2==track->ArCRC[m])
                                {
                                    printTxt(
                                        "                                                           [%08X]     AccurateRipv2 (%u) offset %+d", 
                                        track->CRCv2,
                                        track->ArConfidence[m],
                                        track->CRCoffset
                                    );
                                }
                                //if(verbose)
                                if(track->ArResultCount>1)
                                    printTxt(" @%d ", m+1);
                                printMsg("");
                                gotAtLeastOneMatch = true;
                                gotAtLeastOneOffsetMatch = true;
                            }
                        }
                    } // for (m=resultNo)
                    if(!gotAtLeastOneOffsetMatch && track->CRCoffset)
                    {
                        printMsg(
                            "                                                [%08X] [%08X]     Partial Match offset %+d",
                            track->CRC,
                            track->CRCv2,
                            track->CRCoffset
                        );
                    }

                } // for (n)
            }
            if (!gotAtLeastOneMatch)
            {
                printMsg("                                                                          ** Rip NOT accurate **");
                badTracks++;
            }
        } // for (k)
        
        ArFLAC_closeDisc(disc);
    } // for (optnum)
            
    exit(badTracks);
}
