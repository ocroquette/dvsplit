/*
    dvsplit.c

    Copyright (C) Olivier Croquette, 2008

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libdv/dv.h>
#include <libdv/dv_types.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

/* Basically the same as dv_get_recording_datetime, but gives 
   a struct tm as output.
*/
void
my_dv_get_recording_datetime(dv_decoder_t * dv, struct tm * dt)
{
	int             id1, id2, year;

	if ((id1 = dv->ssyb_pack[0x62]) != 0xff &&
	    (id2 = dv->ssyb_pack[0x63]) != 0xff) {
		year = dv->ssyb_data[id1][3];
		year = (year & 0x0f) + 10 * ((year >> 4) & 0x0f);
		year += (year < 25) ? 2000 : 1900;
		dt->tm_year = year;
		dt->tm_mon = ((dv->ssyb_data[id1][2] >> 4) & 0x01) * 10 +
			(dv->ssyb_data[id1][2] & 0x0f);
		dt->tm_mday = ((dv->ssyb_data[id1][1] >> 4) & 0x03) * 10 +
			(dv->ssyb_data[id1][1] & 0x0f);
		dt->tm_hour = ((dv->ssyb_data[id2][3] >> 4) & 0x03) * 10 +
			(dv->ssyb_data[id2][3] & 0x0f);
		dt->tm_min = ((dv->ssyb_data[id2][2] >> 4) & 0x07) * 10 +
			(dv->ssyb_data[id2][2] & 0x0f);
		dt->tm_sec = ((dv->ssyb_data[id2][1] >> 4) & 0x07) * 10 +
			(dv->ssyb_data[id2][1] & 0x0f);
	} else {
		/* it may also be found in vaux for some... */
		if ((id1 = dv->vaux_pack[0x62]) != 0xff &&
		    (id2 = dv->vaux_pack[0x63]) != 0xff) {
			year = dv->vaux_data[id1][3];
			year = (year & 0x0f) + 10 * ((year >> 4) & 0x0f);
			year += (year < 25) ? 2000 : 1900;

			dt->tm_year = year;
			dt->tm_mon = ((dv->vaux_data[id1][2] >> 4) & 0x01) * 10 +
				(dv->vaux_data[id1][2] & 0x0f);
			dt->tm_mday = ((dv->vaux_data[id1][1] >> 4) & 0x03) * 10 +
				(dv->vaux_data[id1][1] & 0x0f);
			dt->tm_hour = ((dv->vaux_data[id2][3] >> 4) & 0x03) * 10 +
				(dv->vaux_data[id2][3] & 0x0f);
			dt->tm_min = ((dv->vaux_data[id2][2] >> 4) & 0x07) * 10 +
				(dv->vaux_data[id2][2] & 0x0f);
			dt->tm_sec = ((dv->vaux_data[id2][1] >> 4) & 0x07) * 10 +
				(dv->vaux_data[id2][1] & 0x0f);

		} else
			return;
	}
	//Convert to strange time.h convention.See time.h manual
    dt->tm_year -= 1900;
    dt->tm_mon -= 1;
}



int
read_frame(FILE * in_vid, unsigned char *frame_buf, int *isPAL)
{
	if (fread(frame_buf, 1, 120000, in_vid) != 120000) {
		return 0;
	}
	*isPAL = (frame_buf[3] & 0x80);

	if (*isPAL) {
		if (fread(frame_buf + 120000, 1, 144000 - 120000, in_vid) !=
		    144000 - 120000) {
			return 0;
		}
	}
	return 1;
}


void 
closeOutputFile(FILE ** outputFile)
{
	if (*outputFile)
		fclose(*outputFile);
	outputFile = NULL;
}
void 
openNextOutputFile(FILE ** outputFile, char *prefix, int *fileCount, struct tm * frameTm, double percent)
{
	char           *newFileName = malloc(strlen(prefix) + 20);
	(*fileCount)++;
	sprintf(newFileName, "%s_%03d.dv", prefix, *fileCount);
	char            timeStr[100];
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", frameTm);
	fprintf(stdout, "[%04.1f%%] Opening %s for output at: %s\n", percent, newFileName, timeStr);
	closeOutputFile(outputFile);
	//close old file if open
		*outputFile = fopen(newFileName, "w");
	if ( ! *outputFile ) {
		fprintf(stderr, "Warning: could not open %s for output\n", newFileName);
	}
}

off_t 
getFileSize(char *filename)
{
	struct stat     stbuf;
	stat(filename, &stbuf);
	return stbuf.st_size;
}

int
main(int argc, char **argv)
{
	unsigned char   dv_buffer[144000];
	dv_decoder_t   *decoder = NULL;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s INPUT.dv  OUTPUTPREFIX\n", argv[0]);
		return (1);
	}
	char           *inputFileName = argv[1];
	FILE           *inputFile = fopen(inputFileName, "r");
	if (!inputFile) {
		fprintf(stderr, "Error: could not open %s\n", argv[1]);
		return (1);
	}
	off_t           inputBytes = getFileSize(inputFileName);

	char           *outputPrefix = argv[2];

	int             outputFileCount = 0;
	FILE           *outputFile = NULL;

	/* assume PAL */
	decoder = dv_decoder_new(FALSE, FALSE, FALSE);
	decoder->clamp_luma = FALSE;
	decoder->clamp_chroma = FALSE;
	dv_reconfigure(FALSE, FALSE);

	int             isPAL = FALSE;
	unsigned long int frames = 0;
	time_t          lastFrameT;
	while (read_frame(inputFile, dv_buffer, &isPAL)) {
		assert(isPAL);
		frames++;
		dv_parse_header(decoder, dv_buffer);
		dv_parse_packs(decoder, dv_buffer);
		struct tm       frameTm;
		my_dv_get_recording_datetime(decoder, &frameTm);
		//fprintf(stderr, "%s", asctime(&frameTm));

		time_t          frameT = mktime(&frameTm);
		//open new output file on first frame or any time jump
			if (frames == 0 || frameT - lastFrameT > 2) {
			if (outputFile) {
				fclose(outputFile);
				outputFile = NULL;
			}
			double          percent = 100.0 * ftello(inputFile) / (double) inputBytes;
			openNextOutputFile(&outputFile, outputPrefix, &outputFileCount, &frameTm, percent);
		}
		if ( outputFile )
			fwrite(dv_buffer, 1, (isPAL ? 144000 : 120000), outputFile);

		lastFrameT = frameT;
	}

	dv_decoder_free(decoder);
	closeOutputFile(&outputFile);

	return 0;
}
