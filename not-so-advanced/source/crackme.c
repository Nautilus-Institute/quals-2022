#include <gba_console.h>
#include <stdio.h>
#include <string.h>

char chars[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                 'u', 'v', 'w', 'x', 'y', 'z', '_'};

int inds[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0-f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 10-1f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 20-2f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 30-3f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 40-4f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, // 50-5f
               -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 60-6f
               15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, // 70-7f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 80-8f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 90-9f
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // a0-af
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // b0-bf
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // c0-cf
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // d0-df
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // e0-ef
               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }; // f0-ff

#define REG_SOUNDCNT_X      (*(volatile u16*)0x04000084)
#define REG_SOUNDCNT_L      (*(volatile u16*)0x04000080)
#define REG_SOUNDCNT_H      (*(volatile u16*)0x04000082)
#define REG_SOUND1CNT_L     (*(volatile u16*)0x04000060)
#define REG_SOUND1CNT_H     (*(volatile u16*)0x04000062)
#define REG_SOUND1CNT_X     (*(volatile u16*)0x04000064)

#define REG_KEYINPUT        (*(volatile u16*)0x04000130)
#define KEY_A               (1<<0)
#define KEY_B               (1<<1)
#define KEY_START           (1<<3)
#define KEY_RIGHT           (1<<4)
#define KEY_LEFT            (1<<5)
#define KEY_UP              (1<<6)
#define KEY_DOWN            (1<<7)
#define KEY_R               (1<<8)
#define KEY_L               (1<<9)

#define MEMORY_SIZE         256
#define PROGRAM_SIZE        256
#define PAD_BUFFER_SIZE     256

void printit( char *str )
{
    puts(CON_POS(30, 0));
    puts(str);
}

int doit( char *str )
{
    const u8 *buffer = (const u8 *)str;
    size_t buflength = strlen(str);

    u16 s1 = 1;
    u16 s2 = 0;

    if ( buflength != 9) {
        return -1;
    }

    for ( int i = 0; i < buflength; i++) {
        s1 = (s1 + buffer[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }

    return (s1 ^ s2) & 0xffff;
}

void read_in_flag(char* program) {
    u8 index = 0;
    u8 t;
    u16 old_keys_down = ~REG_KEYINPUT;

    memset(program, 0, PROGRAM_SIZE);

    program[0] = 'a';
    printit(program);

    while (true) { // index is a u8 and the max buffer is 256 bytes long
        u16 keys_down = ~REG_KEYINPUT;
        u16 keys_up = ~old_keys_down & keys_down;
        old_keys_down = keys_down;

        if (keys_up & KEY_LEFT) {

            if ( index > 0 ) {
                index--;
            }

            printit(program);
        }
        else if (keys_up & KEY_RIGHT) {
            if ( index < 40 ) {
                index++;    
            }

            if ( program[index] == 0 ) {
                program[index] = 'a';
            }
            printit(program);
        }
        else if (keys_up & KEY_UP) {
            // Get the index in the character sets of the current character
            t = inds[(u8)program[index]];

            // Move it to the next one, wrapping if necessary
            if ( t == 26 ) {
                t = 0;
            } else {
                t = t + 1;
            }

            program[index] = chars[t];
            printit(program);
        }
        else if (keys_up & KEY_DOWN) {
            // Get the index in the character sets of the current character
            t = inds[(u8)program[index]];

            if ( t == 0 ) {
                t = 26;
            } else {
                t = t - 1;
            }

            program[index] = chars[t];
            printit(program);
        }
        else if (keys_up & KEY_START) {
            return;
        }
    }
}

void beep() {
    REG_SOUNDCNT_X = 0x0080;
    REG_SOUNDCNT_L = 0x1177;
    REG_SOUNDCNT_H = 0x0002;
    REG_SOUND1CNT_L = 0x0056;
    REG_SOUND1CNT_H = 0xf780;
    REG_SOUND1CNT_X = 0x8400;
}

int main(void) {
    consoleDemoInit();

    puts("Read the code, should be easy\n");

    char program[PROGRAM_SIZE + 1]; // It's 1 bigger so they get 256 characters of code.
    program[PROGRAM_SIZE] = 0; // We add an extra 0 on the end so that the interpreter won't run off the end of the program.

    read_in_flag(program);

    puts("Checking it out\n");
    if ( doit(program) == 0x12e1 ) {
        puts("That works");
    } else {
        puts("Nope");
    }

    return 0;
}
