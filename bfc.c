#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bfvm.h"

#ifdef DEBUG
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

static char *
read_file(const char *file_name, long *size)
{
	FILE *fp;
	char *buf;
	long s;

	if(!(fp = fopen(file_name, "r")))
		goto err;
	if (fseek(fp, 0, SEEK_END))
		goto err;
	if (0 > (s = ftell(fp)))
		goto err;
	if (!(buf = calloc(s, sizeof(char))))
		goto err;
	if (fseek(fp, 0, SEEK_SET))
		goto err;
	if (fread(buf, 1, (size_t)s, fp) < s) {
		fclose(fp);
		printf("can't read file\n");
		return NULL;
	}
	fclose(fp);
	*size = s;
	return buf;

err:
	if (fp)
		fclose(fp);
	err(1, NULL);
}


static char *
parse_ook(char *text, long size, long *nsize)
{
	int *ook_list;
	char *parsed, *cp = text;
	int i = 0, j, ook_list_size;

	if (!(ook_list = (int *)calloc(size / 4, sizeof(int))))
		return NULL;
	while(cp + 4 < text + size) {
		if (*cp == ' ' || *cp == '\n' || *cp == '\r' || *cp == '\t') {
			++cp;
		} else if(!strncmp("Ook", cp, 3)) {
			cp += 3;
			switch (*cp) {
			case '.':
				ook_list[i] = 1;
				++i;
				break;
			case '?':
				ook_list[i] = 2;
				++i;
				break;
			case '!':
				ook_list[i] = 3;
				++i;
				break;
			default:
				printf("Parse error\n");
				free(ook_list);
				return NULL;
			}
			++cp;
		} else {
			printf("Parse error\n");
			free(ook_list);
			return NULL;
		}
	}
	ook_list_size = i;
	if (!(parsed = (char *)calloc((size / 4) / 2, sizeof(char))))
		return NULL;
	for (i = 0, j = 0; i < ook_list_size; i += 2, j++) {
		switch(ook_list[i]) {
		case 1:
			switch(ook_list[i + 1]) {
			case 1:
				parsed[j] = '+';
				break;
			case 2:
				parsed[j] = '>';
				break;
			case 3:
				parsed[j] = ',';
				break;
			default:
				printf("Parse error\n");
				free(ook_list);
				free(parsed);
				return NULL;
			}
			break;
		case 2:
			switch(ook_list[i + 1]) {
			case 1:
				parsed[j] = '<';
				break;
			case 2:
				printf("Ook? Ook? is not defined function\n");
				free(ook_list);
				free(parsed);
				return NULL;
			case 3:
				parsed[j] = ']';
				break;
			default:
				printf("Parse error\n");
				free(ook_list);
				free(parsed);
				return NULL;
			}
			break;
		case 3:
			switch(ook_list[i + 1]) {
			case 1:
				parsed[j] = '.';
				break;
			case 2:
				parsed[j] = '[';
				break;
			case 3:
				parsed[j] = '-';
				break;
			default:
				printf("Parse error\n");
				free(ook_list);
				free(parsed);
				return NULL;
			}
			break;
		default:
			printf("Parse error\n");
			free(ook_list);
			free(parsed);
			return NULL;
		}
	}
	*nsize = j;
	free(ook_list);
	return parsed;
}

static uint16_t *
compile(char *text, long size, long *nsize)
{
	int i, j = 0, p = 0;
	int stack[10000] = {0,};
	uint16_t *bytecode;

	DPRINTF("size:%lu\n", size);
	if (!(bytecode = (uint16_t *)calloc(size, sizeof(uint16_t))))
		return NULL;
	for (i = 0; i < size; i++) {
		DPRINTF("%d ", j);
		switch(text[i]) {
		case '>':
			DPRINTF(">");
			bytecode[j] = BF_PINC;
			++j;
			break;
		case '<':
			DPRINTF("<");
			bytecode[j] = BF_PDEC;
			++j;
			break;
		case '+':
			DPRINTF("+");
			bytecode[j] = BF_VINC;
			++j;
			break;
		case '-':
			DPRINTF("-");
			bytecode[j] = BF_VDEC;
			++j;
			break;
		case '.':
			DPRINTF(".");
			bytecode[j] = BF_PUTC;
			++j;
			break;
		case ',':
			DPRINTF(",");
			bytecode[j] = BF_GETC;
			++j;
			break;
		case '[':
			DPRINTF("[");
			if (p >= 10000) {
				printf("too deep loop\n");
				free(bytecode);
				return NULL;
			}
			bytecode[j] = BF_BEQZ;
			stack[p] = j;
			++j;
			++p;
			break;
		case ']': {
			int from = stack[p - 1];
			 if (j - from > 0xff) {
				printf("too far jump\n");
				free(bytecode);
				return NULL;
			}
			DPRINTF("]");
			DPRINTF("to:%d from:%d jmp:%d",
					j, from, j - from);
			bytecode[from] |= 0xff & (j - from);
			bytecode[j] = BF_BNEZ | 0xff & (j - from);
			++j;
			--p;
			break;
		}
		default:
			DPRINTF("ignore character '%c'\n", text[i]);
		}
		DPRINTF(" %x", bytecode[j - 1]);
		DPRINTF("\n");
	}
	if (p > 0) {
		fprintf(stderr, "unterminated branch\n");
		free(bytecode);
		return NULL;
	}
	*nsize = j * sizeof(uint16_t);
	return bytecode;
}

static long
write_file(const char *file_name, uint16_t *bytecode, long size)
{
	FILE *fp;

	if(!(fp = fopen(file_name, "w")))
		goto err;
	if (fwrite(bytecode, 1, size, fp) < size) {
		fclose(fp);
		printf("can't write file\n");
		return -1;
	}
	fclose(fp);
	return size;
err:
	if (fp)
		fclose(fp);
	err(1, NULL);
}

int 
main(int argc, char **argv)
{
	char *text, *parsed;
	long size;
	uint16_t *bytecode;

	if (argc < 4) {
		fprintf(stderr, "%s [brainfuck|ook|mona] sourcecode bytecode\n", argv[0]);
		return 1;
	}
	if (!(text = read_file(argv[2], &size)))
		return 1;
	if (!strcmp("brainfuck", argv[1])) {
		parsed = text;
	} else if (!strcmp("ook", argv[1])) {
		parsed = parse_ook(text, size, &size);
		free(text);
	} else if (!strcmp("mona", argv[1])) {
	    convert_mona(text, size);
		parsed = text;
	} else {
		printf("unsupported language: %s\n", argv[1]);
		free(text);
		return 1;
	}
	if (!(bytecode = compile(parsed, size, &size))) {
		free(parsed);
		return 1;
	}
	size = write_file(argv[3], bytecode, size);
	free(parsed);
	free(bytecode);
	if (size < 0)
		return 1;
	return 0;
}
