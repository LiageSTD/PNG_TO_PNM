
#include "return_codes.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ZLIB
#include <zlib.h>
#endif
#ifdef LIBDEFLATE
#include "libdeflate.h"
#endif
typedef unsigned int uint32;
typedef unsigned char uchar;

typedef struct
{
	uint32 len;
	uchar *defs;
	uchar name[4];
	uint32 contr;
} chunk;

typedef struct
{
	uint32 width;
	uint32 height;
	uchar colorType;
} pngData;

void rotate(uint32 *v)
{
	*v = (*v >> 24) | ((*v >> 8) & 0xff00) | ((*v << 8) & 0xff0000) | (*v << 24);
}
int parseChunk(chunk *curr, FILE *in)
{
	if (fread(&curr->len, 4, 1, in) != 1)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	rotate(&curr->len);
	if (fread(curr->name, 1, 4, in) != 4)
	{
		if (curr->len != 0)
		{
			fprintf(stderr, "Wrong File format");
			return ERROR_DATA_INVALID;
		}
	}
	curr->defs = malloc(sizeof(uchar) * curr->len);

	if (fread(curr->defs, 1, curr->len, in) != curr->len)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	if (fread(&curr->contr, 4, 1, in) != 1)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	rotate(&curr->contr);
	return 0;
}

chunk iDAT;
int copy(chunk *main, chunk *curr)
{
	main->defs = realloc(main->defs, main->len + curr->len);
	if (main->defs == NULL)
	{
		fprintf(stderr, "Not enough memory");
		return ERROR_OUT_OF_MEMORY;
	}
	for (int i = 0; i < curr->len; i++)
	{
		main->defs[iDAT.len + i] = curr->defs[i];
	}
	main->len += curr->len;
	return 0;
}

uchar paethPredictor(uchar a, uchar b, uchar c)
{
	int p = a + b - c;
	int pa = abs(p - a);
	int pb = abs(p - b);
	int pc = abs(p - c);

	if (pa <= pb && pa <= pc)
	{
		return a;
	}
	else if (pb <= pc)
	{
		return b;
	}
	else
	{
		return c;
	}
}
int filter(unsigned char *decompressed, uint32 width, uint32 height, uint32 depth)
{
	for (uint32 top = 0; top < height; top++)
	{
		uint32 control = decompressed[top * (width * depth + 1)];
		if (control == 0)
		{
			continue;
		}
		else if (control == 1)
		{
			for (uint32 bot = 1; bot < width * depth + 1; bot++)
			{
				decompressed[top * (width * depth + 1) + bot] +=
					(bot < depth + 1) ? 0 : decompressed[top * (width * depth + 1) + bot - depth];
			}
		}
		else if (control == 2)
		{
			for (uint32 bot = 1; bot < width * depth + 1; bot++)
			{
				decompressed[top * (width * depth + 1) + bot] +=
					(top == 0) ? 0 : decompressed[(top - 1) * (width * depth + 1) + bot];
			}
		}
		else if (control == 3)
		{
			for (uint32 bot = 1; bot < width * depth + 1; bot++)
			{
				decompressed[top * (width * depth + 1) + bot] +=
					((top == 0 ? 0 : decompressed[(top - 1) * (width * depth + 1) + bot]) +
					 ((bot < depth + 1) ? 0 : decompressed[(top) * (width * depth + 1) + bot - depth])) /
					2;
			}
		}
		else if (control == 4)
		{
			for (uint32 bot = 1; bot < width * depth + 1; bot++)
			{
				decompressed[top * (width * depth + 1) + bot] += paethPredictor(
					(bot < depth + 1) ? 0 : decompressed[(top) * (width * depth + 1) + bot - depth],
					(top == 0) ? 0 : decompressed[(top - 1) * (width * depth + 1) + bot],
					((bot < depth + 1) || (top == 0)) ? 0 : decompressed[(top - 1) * (width * depth + 1) + bot - depth]);
			}
		}
	}
	return 0;
}
int main(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "Need more parameters. Have %d", argc);
		return ERROR_PARAMETER_INVALID;
	}
	FILE *in;
	if ((in = fopen(argv[1], "rb")) == NULL)
	{
		fprintf(stderr, "Can't open a file");
		return ERROR_CANNOT_OPEN_FILE;
	}
	uchar *buff = malloc(sizeof(uchar) * 8);
	if (fread(buff, 1, 8, in) != 8)
	{
		fprintf(stderr, "Wrong File format");
		free(buff);
		return ERROR_DATA_INVALID;
	}
	if (buff[0] != 0x89 || buff[1] != 0x50 || buff[2] != 0x4E || buff[3] != 0x47 || buff[4] != 0x0D ||
		buff[5] != 0x0A || buff[6] != 0x1A || buff[7] != 0x0A)
	{
		fprintf(stderr, "It's not a pnf file");
		free(buff);
		return ERROR_DATA_INVALID;
	}
	free(buff);
	pngData mainData;
	chunk iHDR;
	if (fread(&iHDR.len, 4, 1, in) != 1)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	rotate(&iHDR.len);
	if (iHDR.len != 13)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	if (fread(iHDR.name, 1, 4, in) != 4)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	uint32 iBuff = 0;
	if (fread(&iBuff, 4, 1, in) != 1)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	rotate(&iBuff);
	mainData.width = iBuff;
	if (fread(&iBuff, 4, 1, in) != 1)
	{
		fprintf(stderr, "Wrong File format");
		return ERROR_DATA_INVALID;
	}
	iHDR.defs = malloc(sizeof(uchar) * 5);
	if (fread(iHDR.defs, 1, 5, in) != 5)
	{
		fprintf(stderr, "Wrong File format");
		free(iHDR.defs);
		return ERROR_DATA_INVALID;
	}
	mainData.colorType = iHDR.defs[1];
	if (!(mainData.colorType == 0x00 || mainData.colorType == 0x02 || mainData.colorType == 0x03))
	{
		fprintf(stderr, "Unsupported type");
		free(iHDR.defs);
		return ERROR_UNSUPPORTED;
	}

	rotate(&iBuff);
	mainData.height = iBuff;
	if (fread(iHDR.defs, 1, 4, in) != 4)
	{
		fprintf(stderr, "Wrong File format");
		free(iHDR.defs);
		return ERROR_DATA_INVALID;
	}
	chunk curr;
	chunk PLTE;
	int n = 0;
	int log = 0;
	int logForS = 0;
	free(iHDR.defs);
	free(curr.defs);
	int nums = 0;
	do
	{
		n = parseChunk(&curr, in);
		if (n != 0)
		{
			free(curr.defs);
			return n;
		}
		if (curr.name[0] == 'I' && curr.name[1] == 'D' && curr.name[2] == 'A' && curr.name[3] == 'T')
		{
			nums++;
			log = 1;
			if (copy(&iDAT, &curr) != 0)
			{
				free(curr.defs);
				return ERROR_OUT_OF_MEMORY;
			}
		}
		else if (curr.name[0] == 0x50 && curr.name[1] == 0x4C && curr.name[2] == 0x54 && curr.name[3] == 0x45)
		{
			if (log == 1)
			{
				fprintf(stderr, "Invalid data");
				free(curr.defs);
				return ERROR_DATA_INVALID;
			}
			log = 1;
			logForS = 1;
			if (copy(&PLTE, &curr) != 0)
			{
				free(curr.defs);
				return ERROR_OUT_OF_MEMORY;
			}
		}
	} while (curr.name[0] != 'I' || curr.name[1] != 'E' || curr.name[2] != 'N' || curr.name[3] != 'D');
	if (fread(curr.defs, curr.len, curr.len, in) != 0)
	{
		fprintf(stderr, "Invalid file");
		free(curr.defs);
		return ERROR_DATA_INVALID;
	}

	free(curr.defs);

	fclose(in);

	unsigned long size = (mainData.width * 3 + 1) * mainData.height;
	unsigned long sizeP = size;
	unsigned char *decompressed = malloc(sizeof(unsigned char) * size);

#ifdef ZLIB
	unsigned int chertavaPeremennaya = uncompress(decompressed, &size, iDAT.defs, iDAT.len);

	if (chertavaPeremennaya != Z_OK)
	{
		fprintf(stderr, "Decompression error");
		free(curr.defs);
		free(decompressed);
		return chertavaPeremennaya;
	}
#elif LIBDEFLATE
	int res = libdeflate_zlib_decompress(libdeflate_alloc_decompressor(), iHDR.defs, iHDR.len, decompressed, size, (size_t *)&size);
	if (res != 0)
	{
		fprintf(stderr, "Decompress error");
		free(curr.defs);
		free(decompressed);
		return res;
	}
#else
	fprintf(stderr, "Invalid parameter");
	return ERROR_PARAMETER_INVALID;
#endif
	free(iDAT.defs);
	if (logForS == 1 && mainData.colorType == 0x00)
	{
		fprintf(stderr, "Invalid data");
		free(decompressed);
		return ERROR_DATA_INVALID;
	}
	FILE *out;

	if ((out = fopen(argv[2], "wb")) == NULL)
	{
		fprintf(stderr, "Can't open a file");
		free(decompressed);
		return ERROR_CANNOT_OPEN_FILE;
	}
	if (mainData.colorType == 0x00)
	{
		if (!fprintf(out, "P5\n%u %u\n255\n", mainData.width, mainData.height))
		{
			fprintf(stderr, "Something went wrong while writing in output file");
			free(decompressed);
			fclose(out);
			return ERROR_CANNOT_OPEN_FILE;
		}
	}
	else
	{
		if (!fprintf(out, "P6\n%u %u\n255\n", mainData.width, mainData.height))
		{
			fprintf(stderr, "Something went wrong while writing in output file");
			free(decompressed);
			fclose(out);
			return ERROR_CANNOT_OPEN_FILE;
		}
	}
	if (mainData.colorType == 0x03)
	{
		if (logForS != 1)
		{
			fprintf(stderr, "Invalid data");
			free(decompressed);
			fclose(out);
			return ERROR_DATA_INVALID;
		}
		int bez = 0;
		if (PLTE.len % 3 != 0)
		{
			fprintf(stderr, "Invalid data");
			free(decompressed);
			fclose(out);
			return ERROR_DATA_INVALID;
		}

		for (uint32 i = 1; i < sizeP; i++)
		{
			bez++;
			if (bez == mainData.width)
			{
				bez = 0;
				continue;
			}
			uint32 cur = decompressed[i] * 3;
			if (cur + 2 >= PLTE.len)
			{
				fprintf(stderr, "Invalid data");
				free(decompressed);
				fclose(out);
				return ERROR_DATA_INVALID;
			}
			fwrite(PLTE.defs + cur, 1, 3, out);
		}
	}
	else
	{
		if (mainData.colorType == 0x00)
		{
			filter(decompressed, mainData.width, mainData.height, 1);
		}
		else
		{
			filter(decompressed, mainData.width, mainData.height, 3);
		}
		for (int i = 0; i < mainData.height; i++)
		{
			fwrite(decompressed + 1 + i * (mainData.width * (mainData.colorType != 0x00 ? 3 : 1) + 1),
				   1,
				   mainData.width * (mainData.colorType != 0x00 ? 3 : 1),
				   out);
		}
	}
	free(decompressed);
	fclose(out);
	return 0;
}
