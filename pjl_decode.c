/* (c) 2016 Thomas BERNARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif /* MIN */

struct pjl_data {
	size_t upgrade_size;
	char name[256];
};

void parse_pjl_cmd(struct pjl_data * prop, const char * line, size_t len)
{
	const char * p;
	const char * cmd = NULL;
	size_t cmd_len;
	const char * name = NULL;
	size_t name_len;
	const char * value = NULL;
	size_t value_len;
	if(len < 5) return;
	p = line + 4;	/* skip @PJL */
	if(*p != ' ') return;
	p++;
	cmd = p;
	while(!isspace(*p)) {
		if(p >= line + len) return;
		p++;	
	}
	cmd_len = p - cmd;
	while(isspace(*p)) {
		if(p >= line + len) return;
		p++;	
	}
	name = p;
	while(!isspace(*p) && *p != '=') {
		if(p >= line + len) return;
		p++;	
	}
	name_len = p - name;
	while(isspace(*p) || *p == '=') {
		if(p >= line + len) return;
		p++;	
	}
	value = p;
	value_len = len - (value - line);
	while(value_len > 1 && isspace(value[value_len-1]))
		value_len--;
	printf("%.*s %.*s=%.*s\n",
	       (int)cmd_len, cmd,
	       (int)name_len, name,
	       (int)value_len, value);
	if(cmd_len == 7 && 0 == memcmp(cmd, "UPGRADE", 7)) {
		if(name_len == 4 && 0 == memcmp(name, "SIZE", 4))
			prop->upgrade_size = strtoul(value, NULL, 10);
	} else if(cmd_len == 7 && 0 == memcmp(cmd, "COMMENT", 7)) {
		if(name_len == 5 && 0 == memcmp(name, "MODEL", 5))
			memcpy(prop->name, value, MIN(sizeof(prop->name), value_len));
			prop->name[sizeof(prop->name) - 1] = '\0';
	}
}

void pjl_decode(const char * data, size_t len)
{
	const char * p;
	struct pjl_data prop;
	int fake_exit;
	int i;

	p = data;
	memset(&prop, 0, sizeof(prop));
	/* Format #1
	 * <ESC>%-12345X
	 * => Universal Exit Language (UEL) Command
	 *
	 * Format #2
	 * @PJL [<CR>]<LF>
	 *
	 * Format #3
	 * @PJL command [<words>] [<CR>]<LF>
	 *
	 * Format #4
	 * @PJL command [command modifier : value] [option name [= value]] [<CR>]<LF>
	 */
	while(*p != 033) { /* ESC */
		if(p >= data + len)	{
			fprintf(stderr, "no ESC command\n");
			return;
		}
		p++;
	}
	if(memcmp(p, "\033%-12345X", 9) == 0) {
		p += 9;
	} else {
		fprintf(stderr, "unknown ESC command at offset %lx\n", (p - data));
		return;
	}
	fake_exit = 0;
	while(p < data + len) {
		const char * line = p;
		size_t l;
		if(memcmp(p, "\033%-12345X", 9) == 0) {
			/* end of PJL */
			p += 9;
			if(memcmp(p, "@PJL", 4) != 0) break;
			else fake_exit = 1;	/* PJL commands continue after the UEL ! */
		}
		if(memcmp(p, "@PJL", 4) != 0) {
			if(fake_exit) break;
			fprintf(stderr, "unknown PJL command at offset %06lx\n", (p - data));
			return;
		}
		p += 4;
		while(*p != '\r' && *p != '\n') {
			if(p >= data + len) {
				fprintf(stderr, "prematurate end of file\n");
				return;
			}
			p++;
		}
		l = p - line;
		/*printf("> %.*s\n", (int)l, line);*/
		parse_pjl_cmd(&prop, line, l);
		if(*p == '\r') p++;
		if(*p == '\n') p++;
	}
	if(*p == '\r') p++;
	if(*p == '\n') p++;
/*
	printf("%02x ", *p & 0xff);
	p++;
	printf("%02x ", *p & 0xff);
	p++;
	printf("\n");
*/
	printf("%lu bytes left\n", len - (p - data));
	printf("upgrade_size=%lu 0x%lx\n", prop.upgrade_size, prop.upgrade_size);
	printf("name='%s'\n", prop.name);
	if(prop.upgrade_size > len - (p - data)) {
		fprintf(stderr, "not enough bytes left\n");
		return;
	} else {
		char fname[256];
		/*size_t towrite = prop.upgrade_size;*/
		size_t towrite = len - (p - data);
		ssize_t n;
		int fd;
		if(prop.name[0] == '\0') {
			strncpy(fname, "firmware.bin", sizeof(fname));
		} else {
			snprintf(fname, sizeof(fname), "%s_firmware.bin", prop.name);
			for(i = 0; i < (int)sizeof(fname) && fname[i] != '\0'; i++)
				if(fname[i] == ' ') fname[i] = '_';
		}
		printf("writting to %s\n", fname);
		fd = creat(fname, 0666);
		if(fd < 0) {
			perror("creat");
			return;
		}
		while(towrite > 0) {
			n = write(fd, p, towrite);
			if(n < 0) {
				perror("write");
				return;
			}
			towrite -= n;
		}
		close(fd);
		p += prop.upgrade_size;
	}
	i = 0;
	while(p < data + len) {
		printf("%02x ", *p & 0xff);
		p++;
		i++;
		if(i >= 16 || (p == data + len)) {
			printf("| ");
			for(i = -i; i < 0; i++) {
				putchar((p[i] < 32) ? ' ' : p[i]);
			}
			printf("\n");
		}
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

	printf("%s: %lu bytes\n", argv[1], st.st_size);

	data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED|MAP_FILE, fd, 0);
	if(data==NULL) {
		perror("mmap");
		close(fd);
		return 4;
	}

	/* work ! */
	pjl_decode(data, st.st_size);

	close(fd);
	if(munmap(data, st.st_size) < 0)
		perror("munmap");
	return 0;
}
