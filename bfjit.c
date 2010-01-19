#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
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
	int stack[32768];
	int i = 0, j = 0;
	long psize;
	unsigned char *program;
	void *func_table[2] = {(void *)putchar, (void *)getchar};
	int address_list[65536];
	int start_address;

	if (size > 65536) {
		printf("bytecode too big\n");
		return;
	}
	
#ifdef BSD
	psize = getpagesize();
#else
	psize = sysconf(_SC_PAGE_SIZE);
#endif
	if((posix_memalign((void **)&program, psize, psize)))
		err(1, "posix_memalign");
	if(mprotect((void*)program, psize, PROT_READ | PROT_WRITE | PROT_EXEC))
		err(1, "mprotect");

	program[j++] = 0x55;// push   %ebp
	program[j++] = 0x89; program[j++] = 0xe5;// mov    %esp,%ebp
	program[j++] = 0x83; program[j++] = 0xec; program[j++] = 0x18;// sub    $0x18,%esp
	program[j++] = 0x8b; program[j++] = 0x5d; program[j++] = 0x08;// mov    0x8(%ebp),%ebx
	program[j++] = 0x8b; program[j++] = 0x75; program[j++] = 0x0c;// mov    0xc(%ebp),%esi
	start_address = j;
	
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
			address_list[i] = j;
			program[j++] = 0x43; // inc    %ebx
			i++;
			break;
		case BF_PDEC:
			address_list[i] = j;
			program[j++] = 0x4b; // dec    %ebx
			i++;
			break;
		case BF_VINC:
			address_list[i] = j;
			program[j++] = 0xff; program[j++] = 0x03;// incl    (%ebx)
			i++;
			break;
		case BF_VDEC:
			address_list[i] = j;
			program[j++] = 0xff; program[j++] = 0x0b;// decl    (%ebx)
			i++;
			break;
		case BF_PUTC:
			address_list[i] = j;
			program[j++] = 0x8b; program[j++] = 0x03; // mov    (%ebx),%eax
			program[j++] = 0x89; program[j++] = 0x04; program[j++] = 0x24;// mov    %eax,(%esp)
			program[j++] = 0xff; program[j++] = 0x16; // call *(%esi)
			i++;
			break;
		case BF_GETC:
			address_list[i] = j;
			program[j++] = 0x89; program[j++] = 0xf0; // mov    %esi,%eax
			program[j++] = 0xff; program[j++] = 0x50; program[j++] = 0x04; // call   *0x4(%eax)
			program[j++] = 0x89; program[j++] = 0x03; // mov    %eax,(%ebx)
			i++;
			break;
		case BF_BEQZ:
			address_list[i] = j;
			program[j++] = 0x83; program[j++] = 0x3b; program[j++] = 0x00; // cmpl   $0x0,(%ebx)
			program[j++] = 0x74; program[j++] = 0x00; // je    0
			i++;
			break;
		case BF_BNEZ:
			address_list[i] = j;
			program[j++] = 0x83; program[j++] = 0x3b; program[j++] = 0x00; // cmpl   $0x0,(%ebx)
			program[j++] = 0x75; program[j++] = 0x00; // jne    0
			i++;
			break;
		default:
			printf("unknown operation:%x\n", byte);
			return;
		}
//		DPRINTF("\n");
	}
	program[j++] = 0xc9;// leave  
	program[j++] = 0xc3;// ret
	
	j = start_address;
	i = 0;
	for (;;) {
		uint16_t byte = bytecode[i];
		if (i >= size) {
			DPRINTF("i >= size\n");
			break;
		}
//		DPRINTF("%d:", i);
		switch(byte & 0xff00) {
		case BF_PINC:
		case BF_PDEC:
		case BF_VINC:
		case BF_VDEC:
		case BF_PUTC:
		case BF_GETC:
			i++;
			break;
		case BF_BEQZ: {
			uint8_t skip = 0x00ff & byte;
			j = address_list[i];
			j++; j++; j++; // cmpl   $0x0,(%ebx)
			j++; program[j++] = address_list[i+skip] - j; // je
			DPRINTF("je %d\n", address_list[i+skip] - j);
			i++;
			break;
		}
		case BF_BNEZ: {
			uint8_t skip = 0x00ff & byte;
			j = address_list[i];
			j++; j++; j++; // cmpl   $0x0,(%ebx)
			j++; program[j++] = 0xff - (j - address_list[i-skip]); // jne
			DPRINTF("jne %d\n", 0xff - (j - address_list[i-skip]));
			i++;
			break;
		}
		default:
			printf("unknown operation:%x\n", byte);
			return;
		}
//		DPRINTF("\n");
	}

	memset(stack, 0, 32768);
	((void (*)(int *, void**))program)(stack, func_table);
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
