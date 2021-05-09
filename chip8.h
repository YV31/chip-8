#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define SCREEN_W 64
#define SCREEN_H 32

#define RAM_SIZE     0x1000
#define ROM_START    0x200
#define ROM_SIZE     (RAM_SIZE - ROM_START)
#define KEYPAD_SIZE  16
#define CHAR_SIZE    5
#define FONT_START   0x000
#define FONT_SIZE    KEYPAD_SIZE * CHAR_SIZE
#define STACK_SIZE   16
#define SCREEN_SIZE  (SCREEN_W * SCREEN_H)

typedef struct Chip8 {
    uint8_t ram[RAM_SIZE];       // Memory
    uint8_t screen[SCREEN_SIZE]; // Display Memory
    uint16_t stack[STACK_SIZE];  // 16-bit Stack

    // Registers
    uint8_t v[16];
    uint16_t i;

    // Pseudo-registers
    uint8_t dt, st; // Delay Timer, Sound Timer
    uint8_t sp;     // Stack Pointer
    uint16_t pc;    // Program Counter

    // Interpreter usage
    int keys[KEYPAD_SIZE];
    void *userdata;
    void (*key_handle)(struct Chip8 *);
} Chip8;

Chip8 *chip8_create(void (*)(Chip8 *), void *);
void chip8_destroy(Chip8 *);
void chip8_reset(Chip8 *chip);

int chip8_load_rom(Chip8 *, char const *);

void chip8_cycle(Chip8 *);
void chip8_tick(Chip8 *);

void chip8_ram_dump(Chip8 *);

#endif // CHIP8_H
