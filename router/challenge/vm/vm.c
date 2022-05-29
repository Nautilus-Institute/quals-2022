#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define RAM_SIZE 32768
#define DATA_REGION_START 4096

#define LOAD_REG 1
#define CLEAR_REG 2
#define MOVE_REG 3
#define MOVE_MEM 4
#define SYSCALL_FOPEN 30
#define SYSCALL_FREAD 31
#define SYSCALL_FCLOSE 32
#define SYSCALL_PRINT 33
#define SYSCALL_MAKEGOD 34
#define SYSCALL_HALT 0xcc
#define ADD 100 
#define SUB 101

int is_god = 0;

struct CPU {
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t PC;
  uint8_t RAM[RAM_SIZE];
};


void halt()
{
  const char oops[10] = "Oops\n";
  fprintf(stdout, oops);
  fflush(stdout);
  _exit(1);
}

void load_reg(struct CPU* cpu)
{
  uint8_t reg_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t val = cpu->RAM[cpu->PC];
  cpu->PC++;
  switch (reg_dst) {
    case 0:
      cpu->A = (cpu->A << 8) | val;
      break;
    case 1:
      cpu->B = (cpu->B << 8) | val;
      break;
    case 2:
      cpu->C = (cpu->C << 8) | val;
      break;
    default:
      halt();
  }
}

void move_reg(struct CPU* cpu)
{
  uint8_t reg_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t reg_src = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint32_t val;
  switch (reg_src) {
    case 0:
      val = cpu->A;
      break;
    case 1:
      val = cpu->B;
      break;
    case 2:
      val = cpu->C;
      break;
    default:
      halt();
  }

  switch (reg_dst) {
    case 0:
      cpu->A = val;
      break;
    case 1:
      cpu->B = val;
      break;
    case 2:
      cpu->C = val;
      break;
    default:
      halt();
  }
}

void clear_reg(struct CPU* cpu)
{
  uint8_t reg_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  switch (reg_dst) {
    case 0:
      cpu->A = 0;
      break;
    case 1:
      cpu->B = 0;
      break;
    case 2:
      cpu->C = 0;
      break;
    default:
      halt();
  }
}

void move_mem(struct CPU* cpu)
{
  uint32_t mem_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  mem_dst = (mem_dst << 8) | cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t reg_src = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t val = 0;
  switch (reg_src) {
    case 0:
      val = cpu->A;
      break;
    case 1:
      val = cpu->B;
      break;
    case 2:
      val = cpu->C;
      break;
    default:
      halt();
  }
  if (val > 0xff) {
#ifdef DEBUG
    printf("val[%d] > 0xff\n", val);
#endif
    halt();
  }
  if (mem_dst >= RAM_SIZE) {
#ifdef DEBUG
    printf("mem_dst[%d] >= RAM_SIZE\n", mem_dst);
#endif
    halt();
  }
  if (mem_dst < DATA_REGION_START) {
#ifdef DEBUG
    printf("mem_dst[%d] < DATA_REGION_START\n", mem_dst);
#endif
    halt();
  }
  cpu->RAM[mem_dst] = val;
}

void syscall_fopen(struct CPU* cpu)
{
  if (cpu->A >= RAM_SIZE) {
    halt();
  }
  if (cpu->A < DATA_REGION_START) {
    halt();
  }
  char* path_src = (char*)&cpu->RAM[cpu->A];
  char path[20] = {0};
  char path_with_slash[20] = {0};
  strncpy(path, path_src, 19);
  path[19] = 0;
  for (size_t i = 0; i < strlen(path); ++i) {
    if (path[i] == '.')
      halt();
    if (path[i] == '/')
      halt();
  }
  // Adding a slash in the front so that it can access the flag file
  path_with_slash[0] = '/';
  strncpy(path_with_slash + 1, path, 18);
  path_with_slash[19] = 0; // Make sure it's null-terminated
  if (strstr(path_with_slash, "flag") != NULL) {
    if (!is_god) {
      halt();
    }
  }
  int fd = open(path_with_slash, 0);
  cpu->A = fd;
}

void syscall_fread(struct CPU* cpu)
{
  if (cpu->C >= RAM_SIZE)
    halt();
  if (cpu->C < DATA_REGION_START)
    halt();
  if (cpu->C + cpu->B < cpu->C || cpu->C + cpu->B >= RAM_SIZE)
    halt();
  cpu->A = read(cpu->A, &cpu->RAM[cpu->C], cpu->B);
}

void syscall_fclose(struct CPU* cpu)
{
  cpu->A = close(cpu->A);
}

void syscall_print(struct CPU* cpu)
{
  if (cpu->A >= RAM_SIZE)
    halt();
  if (cpu->A + cpu->B < cpu->A || cpu->A + cpu->B >= RAM_SIZE)
    halt();
  for (uint32_t i = 0; i < cpu->B; ++i) {
    char ch = (char)cpu->RAM[cpu->A + i];
    printf("%c", ch);
  }
  printf("\n");
}

void syscall_makegod(struct CPU* cpu)
{
  is_god = 0;
  // Make sure "flag" or "god" does not exist anywhere in the code region
  for (int i = 0; i < DATA_REGION_START; ++i) {
    if (cpu->RAM[i] == 'f' || cpu->RAM[i] == 'l' || cpu->RAM[i] == 'a' || cpu->RAM[i] == 'g') {
#ifdef DEBUG
      printf("Found \"flag\" at %d\n", i);
#endif
      cpu->A = 0xffffffff;
      return;
    }
    if (cpu->RAM[i] == 'g' || cpu->RAM[i] == 'o' || cpu->RAM[i] == 'd') {
#ifdef DEBUG
      printf("Found \"god\" at %d\n", i);
#endif
      cpu->A = 0xffffffff;
      return;
    }
  }
  // Check god signature
#ifdef DEBUG
  printf("A = %x, B = %x\n", cpu->A, cpu->B);
#endif
  if (cpu->A == 0x676f6400 && cpu->B == 0x676f6401) {
    is_god = 1;
    cpu->A = 0;
  }
}

void syscall_halt(struct CPU* cpu)
{
  fflush(stdout);
  _exit(cpu->A & 0xff);
}

void add(struct CPU* cpu)
{
  uint8_t reg_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t reg_src = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint32_t val;
  switch (reg_src) {
    case 0:
      val = cpu->A;
      break;
    case 1:
      val = cpu->B;
      break;
    case 2:
      val = cpu->C;
      break;
    default:
      halt();
  }

  switch (reg_dst) {
    case 0:
      cpu->A += val;
      break;
    case 1:
      cpu->B += val;
      break;
    case 2:
      cpu->C += val;
      break;
    default:
      halt();
  } 
}

void sub(struct CPU* cpu)
{
  uint8_t reg_dst = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint8_t reg_src = cpu->RAM[cpu->PC];
  cpu->PC++;
  uint32_t val;
  switch (reg_src) {
    case 0:
      val = cpu->A;
      break;
    case 1:
      val = cpu->B;
      break;
    case 2:
      val = cpu->C;
      break;
    default:
      halt();
  }

  switch (reg_dst) {
    case 0:
      cpu->A -= val;
      break;
    case 1:
      cpu->B -= val;
      break;
    case 2:
      cpu->C -= val;
      break;
    default:
      halt();
  } 
}

void run_instr(struct CPU* cpu)
{
  // fetch
  uint8_t opcode = cpu->RAM[cpu->PC];
  cpu->PC++;
  switch (opcode) {
    case LOAD_REG:
#ifdef DEBUG
      printf("LOAD_REG\n");
#endif
      load_reg(cpu);
      break;
    case CLEAR_REG:
#ifdef DEBUG
      printf("CLEAR_REG\n");
#endif
      clear_reg(cpu);
      break;
    case MOVE_REG:
#ifdef DEBUG
      printf("MOVE_REG\n");
#endif
      move_reg(cpu);
      break;
    case ADD:
#ifdef DEBUG
      printf("ADD\n");
#endif
      add(cpu);
      break;
    case SUB:
#ifdef DEBUG
      printf("SUB\n");
#endif
      sub(cpu);
      break;
    case MOVE_MEM:
#ifdef DEBUG
      printf("MOVE_MEM\n");
#endif
      move_mem(cpu);
      break;
    case SYSCALL_FOPEN:
#ifdef DEBUG
      printf("SYSCALL_FOPEN\n");
#endif
      syscall_fopen(cpu);
      break;
    case SYSCALL_FREAD:
#ifdef DEBUG
      printf("SYSCALL_FREAD\n");
#endif
      syscall_fread(cpu);
      break;
    case SYSCALL_FCLOSE:
#ifdef DEBUG
      printf("SYSCALL_FCLOSE\n");
#endif
      syscall_fclose(cpu);
      break;
    case SYSCALL_PRINT:
#ifdef DEBUG
      printf("SYSCALL_PRINT\n");
#endif
      syscall_print(cpu);
      break;
    case SYSCALL_MAKEGOD:
#ifdef DEBUG
      printf("SYSCALL_MAKEGOD\n");
#endif
      syscall_makegod(cpu);
      break;
    case SYSCALL_HALT:
#ifdef DEBUG
      printf("SYSCALL_HALT\n");
#endif
      syscall_halt(cpu);
      break;
    default:
#ifdef DEBUG
      printf("Unknown opcode %d\n", opcode);
#endif
      break;
  }
}


void next_pc(struct CPU* cpu)
{
  cpu->PC = rand() % 4096;
#ifdef DEBUG
  printf("PC %d\n", cpu->PC);
#endif
}


void run_vm(struct CPU* cpu)
{
  for (int i = 0; i < 2048; ++i) {
    run_instr(cpu);
    next_pc(cpu);
  }
}


int main(int argc, char** argv)
{
  char BUF[2000000] = {0};
  for (int i = 0; i < 8; ++i) {
    BUF[rand() % 1000] = rand();
  }

  if (argc != 2) {
    const char usage[100] = "Usage: %s seed\n";
    printf(usage, argv[0], BUF);
    fflush(stdout);
    _exit(2);
  }

  int seed = atoi(argv[1]);
#ifdef DEBUG
  printf("Seed = %d\n", seed);
#endif
  srand(seed);

  struct CPU cpu;

  cpu.PC = 0;
  memset(&cpu.RAM, SYSCALL_HALT, sizeof(cpu.RAM));
  fread(&cpu.RAM, DATA_REGION_START, 1, stdin);
  run_vm(&cpu);
}
