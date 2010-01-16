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
	if (!(buf = malloc(s)))
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

static uint16_t *
compile(char *text, long size, long *nsize)
{
	int i, j = 0, p = 0;
	int stack[10000] = {0,};
	uint16_t *bytecode;

	DPRINTF("size:%lu\n", size);
	if (!(bytecode = (uint16_t *)malloc(size * sizeof(uint16_t))))
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
	char *text;
	long size;
	uint16_t *bytecode;

	if (argc < 3) {
		fprintf(stderr, "%s sourcecode bytecode\n", argv[0]);
		return 1;
	}
	if (!(text = read_file(argv[1], &size)))
		return 1;
	if (!(bytecode = compile(text, size, &size))) {
		free(text);
		return 1;
	}
	size = write_file(argv[2], bytecode, size);
	free(text);
	free(bytecode);
	if (size < 0)
		return 1;
	return 0;
}
