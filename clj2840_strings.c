#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* BIG endian */
#define READU16(p) (p[0] << 8)|p[1]
#define READU32(p) (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3]


size_t find_word(const uint8_t * p, const uint8_t * word, size_t off, size_t len)
{
	while(off < len) {
		if(memcmp(p + off, word, 4) == 0)
			return off;
		off += 4;
	}
	return len;
}

const uint8_t locl[4] = {'L', 'O', 'C', 'L'};

void extract_strings(uint8_t * buffer, size_t len)
{
	size_t off = 0;
	int i, n;
	uint16_t id;
	uint32_t l;
	while((off = find_word(buffer, locl, off, len)) < len) {
		/*printf("%08lx\n", off);*/
/*
002e3c4c: 4c4f434c
002e3c50: 00000000
002e3c54: 00010000
002e3c58: 002e3c4c LOCL
002e3c5c: 002e3c04 (C) Copyright Hewlett-Packard Company 1987-1994. All Rights Reserved.
002e3c60: 01309572
002e3c64: 00000101
002e3c68: 002e7c84 en
002e3c6c: 002e7c87 US
002e3c70: 000007d4
002e3c74: adf80000
002e3c78: 002e7c8c Insert memory card
002e3c7c: adf90000
002e3c80: 002e7ca0 Printing proof sheet: %d/%d
002e3c84: adfa0000
002e3c88: 002e7cbc Canceling proof sheet
*/
		if((READU32((buffer+off+4))) == 0) {
			for(i = 0; i < 64; i += 4) {
				l = READU32((buffer+off+i));
				printf("%08lx: %08x", off+i, l);
				if(l > 0x10000 && l < len) printf(" %s", buffer + l);
				printf("\n");
			}
			n = READU32((buffer+off+0x24));
			printf("%d strings\n", n);
			for(i = 0; i < n; i++) {
				id = READU16((buffer+off+0x28+i*8)); 
				l = READU32((buffer+off+0x28+i*8+4));
				printf("%04x: %s\n", id, buffer + l);
			}
			printf("\n");
		}
		off += 4;
	}
}

int main(int argc, char * * argv)
{
	FILE * f;
	size_t n, off, size;
	uint8_t * buffer;

	if(argc < 3) {
		fprintf(stderr, "usage : %s out00_chunk.bin out01_chunk\n", argv[0]);
		return 1;
	}

	f = fopen(argv[1], "rb");
	if(!f) {
		fprintf(stderr, "cannot open '%s'\n", argv[1]);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(n);
	if(buffer == NULL) {
		fprintf(stderr, "malloc() error\n");
		return 3;
	}
	if(n != fread(buffer, 1, n, f))
		fprintf(stderr, "fread() error\n");
	fclose(f);
	off = n;
	f = fopen(argv[2], "rb");
	if(!f) {
		fprintf(stderr, "cannot open '%s'\n", argv[2]);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	size = off + n;
	buffer = realloc(buffer, size);
	if(buffer == NULL) {
		fprintf(stderr, "realloc() error\n");
		return 3;
	}
	if(n != fread(buffer + off, 1, n, f))
		fprintf(stderr, "fread() error\n");
	fclose(f);

	extract_strings(buffer, size);

	free(buffer);
	return 0;
}
