#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bfvm.h"

#ifdef DEBUG
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

static uint16_t *
read_file(const char *file_name, long *size)
{
	FILE *fp;
	uint16_t *buf;
	long s;

	if(!(fp = fopen(file_name, "r")))
		goto err;
	if (fseek(fp, 0, SEEK_END))
		goto err;
	if (0 > (s = ftell(fp)))
		goto err;
	if (!(buf = (uint16_t *)malloc(s)))
		goto err;
	if (fseek(fp, 0, SEEK_SET))
		goto err;
	if (fread(buf, 1, (size_t)s, fp) < s) {
		fclose(fp);
		printf("can't read file\n");
		return NULL;
	}
	fclose(fp);
	*size = s / sizeof(uint16_t);
	return buf;

err:
	if (fp)
		fclose(fp);
	err(1, NULL);
}

static void
interpret(uint16_t *bytecode, long size)
{
	int stack[32768], *sp = stack;
	int i = 0;

	memset(stack, 0, 32768);
	DPRINTF("size:%lu\n", size);
	for (;;) {
		uint16_t byte = bytecode[i];
		if (i >= size) {
			DPRINTF("i >= size\n");
			break;
		}
//		DPRINTF("%d:", i);
		switch(byte & 0xff00) {
		case BF_PINC:
			DPRINTF(">");
			++sp;
//			DPRINTF(" sp:%p", sp);
			++i;
			break;
		case BF_PDEC:
			DPRINTF("<");
			--sp;
//			DPRINTF(" sp:%p", sp);
			++i;
			break;
		case BF_VINC:
			DPRINTF("+");
			++(*sp);
//			DPRINTF(" *sp:%x %c", *sp, *sp);
			++i;
			break;
		case BF_VDEC:
			DPRINTF("-");
			--(*sp);
//			DPRINTF(" *sp:%x %c", *sp, *sp);
			++i;
			break;
		case BF_PUTC:
			DPRINTF(".");
//			DPRINTF(" *sp:%x %c", *sp, *sp);
			putchar(*sp);
			++i;
			break;
		case BF_GETC:
			DPRINTF(",");
//			DPRINTF(" *sp:%x %c", *sp, *sp);
			*sp = getchar();
			++i;
			break;
		case BF_BEQZ:
			DPRINTF("[");
			if (!*sp) {
				uint8_t skip = 0x00ff & byte;
//				DPRINTF("skip:%u", skip);
				i += skip;
			}else
				++i;
			break;
		case BF_BNEZ:
			DPRINTF("]");
			if (*sp) {
				uint8_t skip = 0x00ff & byte;
//				DPRINTF("skip:%u", skip);
				i -= skip;
			}else
				++i;
			break;
		default:
			printf("unknown operation:%x\n", byte);
			return;
		}
//		DPRINTF("\n");
	}
	
}

int main(int argc, char **argv)
{
	long size;
	uint16_t *bytecode;

	if (argc < 2) {
		fprintf(stderr, "%s bytecode\n", argv[0]);
		return 1;
	}
	if (!(bytecode = read_file(argv[1], &size)))
		return 1;
	interpret(bytecode, size);
	free(bytecode);
	return 0;
}
