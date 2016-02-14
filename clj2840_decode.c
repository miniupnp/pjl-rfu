/* (c) 2016 Thomas BERNARD */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>


#define READU16(p) (p[0] << 8)|p[1];
#define READU32(p) (p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3];

static unsigned char elfmagic[4] = {'\x7f', 'E', 'L', 'F'};
static unsigned char jpgmagic[3] = {'\xFF', '\xD8', '\xFF'};
static char lzmamagic[3] = { '\x5d', '\x00', '\x00' };

ssize_t search(const void * data, size_t len,
               const void * subj, int subj_len,
               int incr, ssize_t start)
{
	ssize_t ofs;
	for(ofs = start; ofs < (ssize_t)len - subj_len; ofs += incr) {
		if(0 == memcmp(data + ofs, subj, subj_len))
			return ofs;
	}
	return -1;
}

void savefile(const char * data, ssize_t len, const char * filename)
{
	ssize_t n;
	int fd;
	
	printf("%s : 0x%08lx %ld bytes\n", filename, len, len);
	fd = creat(filename, 0666);
	if(fd < 0) {
		perror("creat");
		return;
	}
	while(len > 0) {
		n = write(fd, data, len);
		if(n < 0) {
			perror("write");
			break;
		}
		data += n;
		len -= n;
	}
	close(fd);
}

void clj2840_decode(const void * data, size_t len)
{
	int i;
	char fname[20];
	/*uint32_t u, acc;*/
	size_t ofs;
	const unsigned char * p;
	ssize_t elf_ofs = 0;
	ssize_t ofs1, ofs2;
	p = (const unsigned char*)data;

	if((elf_ofs = search(data, len, elfmagic, 4, 4, 0)) >= 0) {
		printf("ELF found at %08lx\n", elf_ofs);
		savefile(data + elf_ofs, len - elf_ofs, "output.elf");
	}

	i = 0;
	while(ofs < len - 32) {
		uint32_t a, b, c, d;
		uint32_t header_len, chunk_len;
		const char * ext;
		a = READU32((p + ofs));
		b = READU32((p + ofs + 4));
		c = READU32((p + ofs + 8));
		d = READU32((p + ofs + 12));
		printf("%06lx:   %08x   %08x   %08x   %08x", ofs, a, b, c, d);
		printf("\t%06lx left\n", len - ofs);
		printf("        %10u %10u %10u %10u\n", a, b, c, d);
		/* b = size, c = padded size ? d = header length */
		if(d >= 256) {
			header_len = 12;
			chunk_len = 0x10000;
		} else {
			header_len = d;
			chunk_len = b;
		}
		snprintf(fname, sizeof(fname), "out%02d_header.bin", i);
		savefile(data + ofs, header_len, fname);
		ofs += header_len;
		ext = "bin";
		if(memcmp(data + ofs, elfmagic, 4) == 0) ext = "elf";
		else if(memcmp(data + ofs, lzmamagic, 3) == 0) ext = "lzma";
		snprintf(fname, sizeof(fname), "out%02d_chunk.%s", i, ext);
		savefile(data + ofs, chunk_len, fname);
		ofs += chunk_len;
		i++;
	}
	printf("%ld / %ld\n", ofs, len);
	if(ofs < len) {
		snprintf(fname, sizeof(fname), "out%02d_final.bin", i);
		savefile(data + ofs, (len - ofs), fname);
	}

	ofs1 = -4;
	i = 1;
	for(;;) {
		ofs2 = search(data, len, jpgmagic, 3, 1, ofs1 + 4);
		if(ofs1 > 0) {
			snprintf(fname, sizeof(fname), "output%d.jpg", i);
			savefile(data + ofs1, ((ofs2 > 0) ? ofs2 : (ssize_t)len) - ofs1, fname);
			i++;
		}
		if(ofs2 < 0) break;
		printf("JPG found at %08lx (%08lx)\n", ofs2, ofs2 - elf_ofs);
		ofs1 = ofs2;
	} 
}

int main(int argc, char * * argv)
{
	struct stat st;
	int fd;
	void * data;

	if(argc < 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if(fd < 0) {
		perror("open");
		return 2;
	}

	if(fstat(fd, &st) < 0) {
		perror("fstat");
		close(fd);
		return 3;
	}

	printf("%s: %lu bytes (0x%08lx)\n", argv[1], st.st_size, st.st_size);

	data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED|MAP_FILE, fd, 0);
	if(data==NULL) {
		perror("mmap");
		close(fd);
		return 4;
	}

	/* work ! */
	clj2840_decode(data, st.st_size);

	close(fd);
	if(munmap(data, st.st_size) < 0)
		perror("munmap");
	return 0;
}
