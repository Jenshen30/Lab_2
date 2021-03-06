#include "return_codes.h"
#include <malloc.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define abs(X) (((X) < 0) ? -(X) : (X))
#if defined(ZLIB)
	#include <zlib.h>
	#define PRECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) ;
	#define DECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN)  (uncompress(OUT, &SIZE_OUT, IN, (uLong)SIZE_IN) != Z_OK)

#elif defined(LIBDEFLATE)
	#include <libdeflate.h>
	#define DECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) \
		(libdeflate_zlib_decompress(libdeflate_alloc_decompressor(), IN, SIZE_IN, OUT, SIZE_OUT, NULL) != LIBDEFLATE_SUCCESS)
	#define PRECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) ;
#elif defined(ISAL)
	#include <include/igzip_lib.h>
	#define PRECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) \
		struct inflate_state isa;                      \
		isal_inflate_init(&isa);                       \
		isa.crc_flag = IGZIP_ZLIB;                     \
		isa.next_in = IN;                              \
		isa.avail_in = SIZE_IN;                        \
		isa.next_out = OUT;                            \
		isa.avail_out = SIZE_OUT;

	#define DECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) (isal_inflate(&isa) != ISAL_DECOMP_OK)

#else
	#error("Did't choose any right lib or it is a ISA-L (")
	#define PRECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN) ;
	#define DECOMPRESSING(OUT, SIZE_OUT, IN, SIZE_IN)  false
#endif

#define START_BUFF_LEN 32
unsigned const char IHDR[4] = { 0x49, 0x48, 0x44, 0x52 };
unsigned const char IDAT[4] = { 0x49, 0x44, 0x41, 0x54 };
unsigned const char IEND[4] = { 0x49, 0x45, 0x4E, 0x44 };

typedef union
{
	unsigned int converted;
	unsigned char mini[4];
} ConverterCharsToInt;

typedef struct
{
	unsigned int height;
	unsigned int width;
	unsigned char bitdepth;
	unsigned char typeofcolor;
} MetaOfAllPNG;

typedef struct
{
	size_t size;
	size_t realsize;
} MetaOfBuf;

bool skipUselessInf(FILE *f, unsigned int bytestoskip)
{
	unsigned char tmp[1];
	for (unsigned int i = 0; i < bytestoskip; i++)
	{
		if (fread(tmp, sizeof(unsigned char), 1, f) != 1)
		{
			return false;
		}
	}

	return true;
}

bool readBufOfChars(FILE *f, unsigned char *buf, size_t size)
{
	return fread(buf, sizeof(unsigned char), size, f) == size;
}

bool oneByteToShort(FILE *f, unsigned char *future)
{
	unsigned char a[1];
	if (!readBufOfChars(f, a, 1))
	{
		return false;
	}
	*future = a[0];
	return true;
}

bool fourBytesToInt(FILE *f, unsigned int *futureint)
{
	ConverterCharsToInt m;
	unsigned char tmp;
	if (!readBufOfChars(f, m.mini, 4))
	{
		return false;
	}

	for (int i = 0; i < 2; i++)
	{
		tmp = m.mini[i];
		m.mini[i] = m.mini[3 - i];
		m.mini[3 - i] = tmp;
	}
	*futureint = m.converted;
	return true;
}

bool nameOfChank(FILE *f, unsigned char *futurename)
{
	return readBufOfChars(f, futurename, 4);
}

bool resize(unsigned char **buf, MetaOfBuf *met, unsigned int tolen)
{
	while (met->size - met->realsize < tolen + 2)
	{
		unsigned char *tmp = realloc(*buf, sizeof(unsigned char) * met->size * 2);
		if (tmp == NULL)
		{
			free(buf);
			return false;
		}
		met->size *= 2;
		*buf = tmp;
	}
	return true;
}

int defillteringOneHe(unsigned char *buf, size_t *currpos, MetaOfAllPNG meta)
{
	int lenofpixel = (int)(meta.typeofcolor + 1);
	size_t i = 1;
	size_t placetowork = meta.width * lenofpixel + 1;

	if (buf[*currpos] == 0x00)	  // None
	{
		*currpos += placetowork;
		return 0;
	}
	else if (buf[*currpos] == 0x01)	   // sub
	{
		while (i < placetowork)
		{
			if (i >= lenofpixel + 1)
			{
				buf[i + *currpos] += buf[i + *currpos - lenofpixel];
			}
			i++;
		}
		*currpos += placetowork;
		return 0;
	}
	else if (buf[*currpos] == 0x02)	   // Up
	{
		while (i < placetowork)
		{
			if (*currpos - placetowork >= 0)
			{
				buf[i + *currpos] += buf[i + *currpos - (placetowork)];
			}
			else
			{
				break;
			}
			i++;
		}
		*currpos += placetowork;
		return 0;
	}
	else if (buf[*currpos] == 0x03)	   // Average
	{
		int tmp;
		while (i < placetowork)
		{
			tmp = 0;
			if (*currpos - placetowork >= 0)
			{
				tmp += buf[i + *currpos - placetowork];
			}

			if (i >= lenofpixel + 1)
			{
				tmp += buf[i + *currpos - lenofpixel];
			}
			buf[i + *currpos] += (unsigned char)floor(tmp / 2);
			i++;
		}
		*currpos += placetowork;
		return 0;
	}
	else if (buf[*currpos] == 0x04)	   // Paeth
	{
		int a, b, c, pa, pc, pb;
		while (i < placetowork)
		{
			a = b = c = 0;
			if (i >= lenofpixel + 1)
			{
				a = buf[i + *currpos - lenofpixel];
			}
			if (*currpos - placetowork >= 0)
			{
				b = buf[i + *currpos - placetowork];
			}

			if ((*currpos - placetowork >= 0) && (i >= lenofpixel + 1))
			{
				c = buf[i + *currpos - placetowork - lenofpixel];
			}
			else
			{
				buf[i + *currpos] += a + b + c;
				i++;
				continue;
			}

			pa = abs(b - c);
			pb = abs(a - c);
			pc = abs(a + b - 2 * c);

			if (pa <= pb && pa <= pc)
			{
				buf[i + *currpos] += a;
			}
			else if (pb <= pc)
			{
				buf[i + *currpos] += b;
			}
			else
			{
				buf[i + *currpos] += c;
			}
			i++;
		}
		*currpos += placetowork;
		return 0;
	}
	return -1;
}
int compareStrings(unsigned const char *a, unsigned const char *b, size_t num)
{
	for (size_t i = 0; i < num; i++)
	{
		if (a[i] != b[i])
		{
			return -1;
		}
	}
	return 0;
}

// -1 - bad news
// 0 - continue
// 1 - the logical end
int readAllChunk(FILE *f, unsigned char **buf, MetaOfAllPNG *meta, MetaOfBuf *mbuf)
{
	unsigned char name[4];
	unsigned int currlen = 0;

	if (!(fourBytesToInt(f, &currlen) && nameOfChank(f, name)))
	{
		return -1;
	}

	if (compareStrings(name, IDAT, 4) == 0)
	{
		if (!resize(buf, mbuf, currlen) || !readBufOfChars(f, *buf + mbuf->realsize, currlen))
		{
			return -1;
		}
		mbuf->realsize += currlen;
	}
	else if (compareStrings(name, IEND, 4) == 0)
	{
		if (skipUselessInf(f, 4) && !skipUselessInf(f, 1))
			return 1;
		return -1;
	}
	else if (compareStrings(name, IHDR, 4) == 0)
	{
		bool res = true;
		res &= fourBytesToInt(f, &meta->width);
		res &= fourBytesToInt(f, &meta->height);
		res &= oneByteToShort(f, &meta->bitdepth);
		res &= oneByteToShort(f, &meta->typeofcolor);
		res &= skipUselessInf(f, 3);
		if (!res)
		{
			return -1;
		}
	}
	else
	{
		if (!skipUselessInf(f, currlen))
		{
			return -1;
		}
	}

	// skip control sum bytes
	if (!skipUselessInf(f, 4))
	{
		return -1;
	}
	return 0;
}

bool checkPNGFormat(FILE *f)
{
	unsigned char mini[8];
	unsigned char correctpngheader[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (!readBufOfChars(f, mini, 8))
	{
		return false;
	}
	return compareStrings(mini, correctpngheader, 8) == 0;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		perror("Too small params in input!");
		return ERROR_INVALID_PARAMETER;
	}

	FILE *image = fopen(*(argv + 1), "rb");
	if (image == NULL)
	{
		perror("Couldn't open \"in\" file!");
		return ERROR_FILE_NOT_FOUND;
	}

	if (!checkPNGFormat(image))
	{
		perror("Not PNG file!");
		fclose(image);
		return ERROR_INVALID_DATA;
	}
	MetaOfAllPNG metapng = { .height = 0, .width = 0, .bitdepth = 0, .typeofcolor = 0 };
	MetaOfBuf metaBuf = { .size = START_BUFF_LEN, .realsize = 0 };
	unsigned char *buffer = malloc(sizeof(char) * START_BUFF_LEN);

	if (buffer == NULL)
	{
		perror("Problems with memory!");
		fclose(image);
		return ERROR_MEMORY;
	}

	int error;
	do
	{
		error = readAllChunk(image, &buffer, &metapng, &metaBuf);
	} while (error == 0);

	if (error == -1)
	{
		perror("This file is broken or was not enought memory to use!");
		free(buffer);
		fclose(image);
		return ERROR_INVALID_DATA;
	}

	unsigned long uncompesedlen = (unsigned long)(metapng.width * metapng.height * (metapng.typeofcolor + 1) + metapng.height);
	unsigned char *allundata = malloc(sizeof(unsigned char) * uncompesedlen);

	if (allundata == NULL)
	{
		perror("Memory error!");
		free(buffer);
		fclose(image);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	PRECOMPRESSING(allundata, uncompesedlen, buffer, metaBuf.realsize)
	// decompress
	if (DECOMPRESSING(allundata, uncompesedlen, buffer, metaBuf.realsize))
	{
		perror("This file is broken!\n");
		free(buffer);
		free(allundata);
		fclose(image);
		return ERROR_INVALID_DATA;
	}
	free(buffer);
	fclose(image);

	// defiltering
	size_t pos = 0;
	do
	{
		error = defillteringOneHe(allundata, &pos, metapng);
	} while (error == 0 && pos < uncompesedlen);

	if (error == -1)
	{
		perror("Bag in filter!");
		free(allundata);
		return ERROR_UNKNOWN;
	}
	// pnm writing
	image = NULL;
	image = fopen(argv[2], "wb");

	if (image == NULL)
	{
		perror("Problems with output file");
		free(allundata);
		return ERROR_UNKNOWN;
	}

	if (metapng.typeofcolor == 0)
	{
		fprintf(image, "P5\n");
	}
	else
	{
		fprintf(image, "P6\n");
	}
	fprintf(image, "%i %i\n255\n", metapng.width, metapng.height);

	for (unsigned long i = 1; i < uncompesedlen; i += metapng.width * (metapng.typeofcolor + 1) + 1)
	{
		if (fwrite(&allundata[i], 1, metapng.width * (metapng.typeofcolor + 1), image) != metapng.width * (metapng.typeofcolor + 1))
		{
			perror("Cannot write some bytes");
			fclose(image);
			free(allundata);
			return ERROR_MEMORY;
		}
	}

	fclose(image);
	free(allundata);
	return ERROR_SUCCESS;
}
