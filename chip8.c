// vim: fdm=marker

#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <bsd/sys/time.h>

#include "chip8.h"

typedef uint16_t Opcode;
typedef void Opimpl(Chip8 *, Opcode);
typedef void Handler(Chip8 *);

/*
  OPCODE CHEAT SHEET

| chip8_cpu_cls   | 00E0 | CLS
| chip8_cpu_ret   | 00EE | RET
| chip8_cpu_jmp   | 1NNN | JMP addr
| chip8_cpu_call  | 2NNN | CALL addr
| chip8_cpu_se    | 3XNN | SE Vx, byte
| chip8_cpu_sne   | 4XNN | SNE Vx, byte
| chip8_cpu_ser   | 5XY0 | SE Vx, Vy
| chip8_cpu_ld    | 6XKK | LD Vx, byte
| chip8_cpu_add   | 7XKK | ADD Vx, byte
| chip8_cpu_ldr   | 8XY0 | LD Vx, Vy
| chip8_cpu_or    | 8XY1 | OR Vx, Vy
| chip8_cpu_and   | 8XY2 | AND Vx, Vy
| chip8_cpu_xor   | 8XY3 | XOR Vx, Vy
| chip8_cpu_addr  | 8XY4 | ADD Vx, Vy
| chip8_cpu_sub   | 8XY5 | SUB Vx, Vy
| chip8_cpu_shr   | 8XY6 | SHR Vx
| chip8_cpu_subn  | 8XY7 | SUBN Vx, Vy
| chip8_cpu_shl   | 8XYE | SHL Vx
| chip8_cpu_sner  | 9XY0 | SNE Vx, Vy
| chip8_cpu_ldi   | ANNN | LD I, addr
| chip8_cpu_jmpv0 | BNNN | JMP V0, addr
| chip8_cpu_rnd   | CXKK | RND Vx, byte
| chip8_cpu_drw   | DXYN | DRW Vx, Vy, nibble
| chip8_cpu_sknp  | EXA1 | SKNP Vx
| chip8_cpu_skp   | EX9E | SKP Vx
| chip8_cpu_ldrdt | FX07 | LD Vx, DT
| chip8_cpu_ldk   | FX0A | LD Vx, K
| chip8_cpu_lddtr | FX15 | LD DT, Vx
| chip8_cpu_ldstr | FX18 | LD ST, Vx
| chip8_cpu_addi  | FX1E | ADD I, Vx
| chip8_cpu_ldf   | FX29 | LD F, Vx
| chip8_cpu_bcd   | FX33 | BDC Vx
| chip8_cpu_ldir  | FX55 | LD [I], Vx
| chip8_cpu_ldri  | FX65 | LD Vx, [I]
*/

// Opcode Tables
static void chip8_cpu_func(Chip8 *, Opcode);  // 0 Function Opcodes
static void chip8_cpu_arith(Chip8 *, Opcode); // 8 Arithmetic Opcodes
static void chip8_cpu_key(Chip8 *, Opcode);   // E Key Opcodes
static void chip8_cpu_vx(Chip8 *, Opcode);    // F Misc Opcodes

// Jumps
static void chip8_cpu_jmp(Chip8 *, Opcode);   // 1NNN | JMP addr
static void chip8_cpu_jmpv0(Chip8 *, Opcode); // BNNN | JMP V0, addr

// Subroutines
static void chip8_cpu_call(Chip8 *, Opcode);  // 2NNN | CALL addr
static void chip8_cpu_ret(Chip8 *, Opcode);   // 00EE | RET

// Skips
static void chip8_cpu_se(Chip8 *, Opcode);    // 3XNN | SE Vx, byte
static void chip8_cpu_sne(Chip8 *, Opcode);   // 4XNN | SNE Vx, byte
static void chip8_cpu_ser(Chip8 *, Opcode);   // 5XY0 | SE Vx, Vy
static void chip8_cpu_sner(Chip8 *, Opcode);  // 9XY0 | SNE Vx, Vy

// Data Register
static void chip8_cpu_ld(Chip8 *, Opcode);    // 6XKK | LD Vx, byte
static void chip8_cpu_ldr(Chip8 *, Opcode);   // 8XY0 | LD Vx, Vy
static void chip8_cpu_or(Chip8 *, Opcode);    // 8XY1 | OR Vx, Vy
static void chip8_cpu_xor(Chip8 *, Opcode);   // 8XY3 | XOR Vx, Vy
static void chip8_cpu_and(Chip8 *, Opcode);   // 8XY2 | AND Vx, Vy
static void chip8_cpu_add(Chip8 *, Opcode);   // 7XKK | ADD Vx, byte
static void chip8_cpu_addr(Chip8 *, Opcode);  // 8XY4 | ADD Vx, Vy
static void chip8_cpu_sub(Chip8 *, Opcode);   // 8XY5 | SUB Vx, Vy
static void chip8_cpu_subn(Chip8 *, Opcode);  // 8XY7 | SUBN Vx, Vy
static void chip8_cpu_shr(Chip8 *, Opcode);   // 8XY6 | SHR Vx
static void chip8_cpu_shl(Chip8 *, Opcode);   // 8XYE | SHL Vx
static void chip8_cpu_rnd(Chip8 *, Opcode);   // CXKK | RND Vx, byte

// I
static void chip8_cpu_ldi(Chip8 *, Opcode);   // ANNN | LD I, addr
static void chip8_cpu_addi(Chip8 *, Opcode);  // FX1E | ADD I, Vx

// Storage
static void chip8_cpu_ldir(Chip8 *, Opcode);  // FX55 | LD [I], Vx
static void chip8_cpu_ldri(Chip8 *, Opcode);  // FX65 | LD Vx, [I]

// Drawing
static void chip8_cpu_drw(Chip8 *, Opcode);   // DXYN | DRW Vx, Vy, nibble
static void chip8_cpu_cls(Chip8 *, Opcode);   // 00E0 | CLS

// Font
static void chip8_cpu_ldf(Chip8 *, Opcode);   // FX1E | LD F, Vx

// Keypad
static void chip8_cpu_ldk(Chip8 *, Opcode);   // FX0A | LD Vx, K
static void chip8_cpu_skp(Chip8 *, Opcode);   // EX9E | SKP Vx
static void chip8_cpu_sknp(Chip8 *, Opcode);  // EXA1 | SKNP Vx

// BCD
static void chip8_cpu_bcd(Chip8 *, Opcode);   // FX33 | BDC Vx

// Timers
static void chip8_cpu_lddtr(Chip8 *, Opcode); // FX15 | LD DT, Vx
static void chip8_cpu_ldstr(Chip8 *, Opcode); // FX18 | LD ST, Vx
static void chip8_cpu_ldrdt(Chip8 *, Opcode); // FX07 | LD Vx, DT

// Bad Instruction
static void chip8_cpu_bdi(Chip8 *, Opcode);   // BDI

#ifdef DEBUG
    #define LOG(...) printf(__VA_ARGS__); printf("\n")
#else
    #define LOG(...)
#endif

// Opcode Table - General Operations
Opimpl *chip8_op_all[16] = {
    chip8_cpu_func,  // 00--
    chip8_cpu_jmp,   // 1NNN | JMP addr
    chip8_cpu_call,  // 2NNN | CALL addr
    chip8_cpu_se,    // 3XNN | SE Vx, byte
    chip8_cpu_sne,   // 4XNN | SNE Vx, byte
    chip8_cpu_ser,   // 5XY0 | SE Vx, Vy
    chip8_cpu_ld,    // 6XKK | LD Vx, byte
    chip8_cpu_add ,  // 7XKK | ADD Vx, byte
    chip8_cpu_arith, // 8XY-
    chip8_cpu_sner,  // 9XY0 | SNE Vx, Vy
    chip8_cpu_ldi,   // ANNN | LD I, addr
    chip8_cpu_jmpv0, // BNNN | JMP V0, addr
    chip8_cpu_rnd,   // CXKK | RND Vx, byte
    chip8_cpu_drw,   // DXYN | DRW Vx, Vy, nibble
    chip8_cpu_key,   // EX--
    chip8_cpu_vx     // FX--
};

// Opcode Table - Arithmetic Operations
Opimpl *chip8_op_arith[16] = {
    chip8_cpu_ldr,  // 8XY0 | LD Vx, Vy
    chip8_cpu_or,   // 8XY1 | OR Vx, Vy
    chip8_cpu_and,  // 8XY2 | AND Vx, Vy
    chip8_cpu_xor,  // 8XY3 | XOR Vx, Vy
    chip8_cpu_addr, // 8XY4 | ADD Vx, Vy
    chip8_cpu_sub,  // 8XY5 | SUB Vx, Vy
    chip8_cpu_shr,  // 8XY6 | SHR Vx
    chip8_cpu_subn, // 8XY7 | SUBN Vx, Vy
    chip8_cpu_bdi,  // 8XY8 | BDI
    chip8_cpu_bdi,  // 8XY9 | BDI
    chip8_cpu_bdi,  // 8XYA | BDI
    chip8_cpu_bdi,  // 8XYB | BDI
    chip8_cpu_bdi,  // 8XYC | BDI
    chip8_cpu_bdi,  // 8XYD | BDI
    chip8_cpu_shl,  // 8XYE | SHL Vx
    chip8_cpu_bdi   // 8XYF | BDI
};

static uint8_t const font[FONT_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};

Chip8 *chip8_create(Handler *key_handle, void *userdata)
{
    Chip8 *chip = malloc(sizeof(Chip8));

    if (chip == NULL)
        return chip;

    memset(chip, 0, sizeof(*chip));

    chip->pc = 0x200;
    chip->key_handle = key_handle;
    chip->userdata = userdata;

    memcpy(chip->ram + FONT_START, font, FONT_SIZE);

    return chip;
}

void chip8_destroy(Chip8 *chip)
{
    free(chip);
}

void chip8_reset(Chip8 *chip)
{
    chip->pc = 0x200;
    chip->i = 0;
    chip->dt = 0;
    chip->st = 0;
    chip->sp = 0;

    memset(chip->v, 0, sizeof(chip->v));
    memset(chip->stack, 0, sizeof(chip->stack));
    memset(chip->screen, 0, sizeof(chip->screen));
}

int chip8_load_rom(Chip8 *chip, const char *filename)
{
    FILE *f = fopen(filename, "rb");

    if (f == NULL) {
        perror("Failed to open ROM");
        return -1;
    }

    fread(chip->ram + ROM_START, 1, ROM_SIZE, f);
    fclose(f);

    return 0;
}

void chip8_cycle(Chip8 *chip)
{
    chip->key_handle(chip);
    Opcode op = (chip->ram[chip->pc] << 8) | (chip->ram[chip->pc + 1]);
    chip->pc += 2;
    chip8_op_all[(op & 0xF000) >> 3 * 4](chip, op);
}

void chip8_tick(Chip8 *chip)
{
    if (chip->dt)
        chip->dt--;

    if (chip->st)
        chip->st--;
}

void chip8_ram_dump(Chip8 *chip)
{
    printf(  "\n┌─ RAM ─────────────────────────────────────────────────┐\n");

    for (int i = 0; i < RAM_SIZE; i++) {

        if ((i % 16) == 0) {
            if (i != 0)
                printf (" │\n");

            printf ("│ %04x ", i);
        }

        printf (" %02x", chip->ram[i]);
    }

    printf(" │\n└───────────────────────────────────────────────────────┘ \n");

}


#define NNN (op & 0x0FFF)
#define N   (op & 0x000F)
#define KK  (op & 0x00FF)
#define X   ((op & 0x0F00) >> 8)
#define Y   ((op & 0x00F0) >> 4)

// Jump Tables {{{

void chip8_cpu_func(Chip8 *chip, Opcode op)
{
    switch(KK) {
        case 0xE0:
            chip8_cpu_cls(chip, op);
            break;
        case 0xEE:
            chip8_cpu_ret(chip, op);
            break;
        default:
            chip8_cpu_bdi(chip, op);
    }
}

void chip8_cpu_arith(Chip8 *chip, Opcode op)
{
    chip8_op_arith[N](chip, op);
}

void chip8_cpu_key(Chip8 *chip, Opcode op)
{
    switch(KK) {
        case 0x9E:
            chip8_cpu_skp(chip, op);
            break;
        case 0xA1:
            chip8_cpu_sknp(chip, op);
            break;
        default:
            chip8_cpu_bdi(chip, op);
    }
}

void chip8_cpu_vx(Chip8 *chip, Opcode op)
{
    switch(KK) {
        case 0x07:
            chip8_cpu_ldrdt(chip, op);
            break;
        case 0x0A:
            chip8_cpu_ldk(chip, op);
            break;
        case 0x15:
            chip8_cpu_lddtr(chip, op);
            break;
        case 0x18:
            chip8_cpu_ldstr(chip, op);
            break;
        case 0x1E:
            chip8_cpu_addi(chip, op);
            break;
        case 0x29:
            chip8_cpu_ldf(chip, op);
            break;
        case 0x33:
            chip8_cpu_bcd(chip, op);
            break;
        case 0x55:
            chip8_cpu_ldir(chip, op);
            break;
        case 0x65:
            chip8_cpu_ldri(chip, op);
            break;
        default:
            chip8_cpu_bdi(chip, op);
    }
}

// }}}

// Jumps {{{

void chip8_cpu_jmp(Chip8 *chip, Opcode op)
{
    chip->pc = NNN;

    LOG("JMP %03X", NNN);
}

void chip8_cpu_jmpv0(Chip8 *chip, Opcode op)
{
    chip->pc = NNN + chip->v[0];

    LOG("JMP V[0], %03X", NNN);
}

// }}}

// Subroutines {{{

void chip8_cpu_call(Chip8 *chip, Opcode op)
{
    chip->stack[chip->sp] = chip->pc;
    chip->sp++;
    chip->pc = NNN;

    LOG("CALL %03X", NNN);
}

void chip8_cpu_ret(Chip8 *chip, Opcode op)
{
    (void) op;

    chip->sp--;
    chip->pc = chip->stack[chip->sp];

    LOG("RET");
}

// }}}

// Skips {{{

void chip8_cpu_se(Chip8 *chip, Opcode op)
{
    chip->pc += (chip->v[X] == KK) ? 2 : 0;

    LOG("SE V[%1X], %02X", X, KK);
}

void chip8_cpu_sne(Chip8 *chip, Opcode op)
{
    chip->pc += (chip->v[X] != KK) ? 2 : 0;

    LOG("SNE V[%1X], %02X", X, KK);
}

void chip8_cpu_ser(Chip8 *chip, Opcode op)
{
    chip->pc += (chip->v[X] == chip->v[Y]) ? 2 : 0;

    LOG("SE V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_sner(Chip8 *chip, Opcode op)
{
    chip->pc += chip->v[X] != chip->v[Y] ? 2 : 0;

    LOG("SNE V[%1X], V[%1X]", X, Y);
}

// }}}

// Data Register {{{

void chip8_cpu_ld(Chip8 *chip, Opcode op)
{
    chip->v[X] = KK;

    LOG("LD V[%1X], %02X", X, KK);
}

void chip8_cpu_ldr(Chip8 *chip, Opcode op)
{
    chip->v[X] = chip->v[Y];

    LOG("LD V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_or(Chip8 *chip, Opcode op)
{
    chip->v[X] |= chip->v[Y];

    LOG("OR V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_xor(Chip8 *chip, Opcode op)
{
    chip->v[X] ^= chip->v[Y];

    LOG("XOR V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_and(Chip8 *chip, Opcode op)
{
    chip->v[X] &= chip->v[Y];

    LOG("AND V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_add(Chip8 *chip, Opcode op)
{
    chip->v[X] += KK;

    LOG("ADD V[%1X], %02X", X, KK);
}

void chip8_cpu_addr(Chip8 *chip, Opcode op)
{
    chip->v[0xF] = chip->v[X] + chip->v[Y] > 0xFF ? 1 : 0; // Overflow check
    chip->v[X] += chip->v[Y];

    LOG("ADD V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_sub(Chip8 *chip, Opcode op)
{
    chip->v[X] -= chip->v[Y];

    LOG("SUB V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_subn(Chip8 *chip, Opcode op)
{
    chip->v[0xF] = chip->v[Y] > chip->v[X] ? 1 : 0;
    chip->v[X] = chip->v[Y] - chip->v[X];

    LOG("SUBN V[%1X], V[%1X]", X, Y);
}

void chip8_cpu_shr(Chip8 *chip, Opcode op)
{
    chip->v[0xF] = (chip->v[X] & 0x0F) >> 3 == 1 ? 1 : 0;
    chip->v[X] >>= 1;

    LOG("SHR V[%1X]", X);
}

void chip8_cpu_shl(Chip8 *chip, Opcode op)
{
    chip->v[0xF] = chip->v[X] >> 15 == 1 ? 1 : 0;
    chip->v[X] <<= 1;

    LOG("SHL V[%1X]", X);
}

void chip8_cpu_rnd(Chip8 *chip, Opcode op)
{
    chip->v[X] = (uint8_t) rand() & KK;

    LOG("RND V[%1X], %02X", X, KK);
}

// }}}

// I {{{

void chip8_cpu_ldi(Chip8 *chip, Opcode op)
{
    chip->i = NNN;

    LOG("LD I, %03X", NNN);
}

void chip8_cpu_addi(Chip8 *chip, Opcode op)
{
    chip->i += chip->v[X];

    LOG("ADD I, V[%01X]", X);
}

// }}}

// Storage {{{

void chip8_cpu_ldir(Chip8 *chip, Opcode op)
{
    for (uint8_t i = 0; i <= X; i++)
        chip->ram[chip->i + i] = chip->v[i];

    chip->i += X + 1;

    LOG("LDIR %01X", X);
}

void chip8_cpu_ldri(Chip8 *chip, Opcode op)
{
    for (uint8_t i = 0; i <= X; i++)
        chip->v[i] = chip->ram[chip->i + i];

    chip->i += X + 1;

    LOG("LDRI %01X", X);
}

// }}}

// Drawing {{{

void chip8_cpu_drw(Chip8 *chip, Opcode op)
{
    chip->v[0xF] = 0;

    for (uint i = 0; i < N; i++) {
        uint8_t byte = chip->ram[chip->i + i];
        for (uint j = 0; j < 8; j++) {
            if(byte & (0x80 >> j)){
                if(chip->screen[chip->v[X] + j + ((chip->v[Y] + i) * SCREEN_W)])
                    chip->v[0xF] = 0x1;

                chip->screen[chip->v[X] + j + ((chip->v[Y] + i) * SCREEN_W)] ^= 0x1;
            }
        }
    }

    LOG("DRW V[%01X], V[%01X], %01X", X, Y, N);
}

void chip8_cpu_cls(Chip8 *chip, Opcode op)
{
    (void) op;
    memset(chip->screen, 0, sizeof(uint8_t) * SCREEN_SIZE);

    LOG("CLS");
}

// }}}

// Font {{{

void chip8_cpu_ldf(Chip8 *chip, Opcode op)
{
    chip->i = chip->v[X] * CHAR_SIZE;

    LOG("LDF V[%01X]", X);
}

// }}}

// Keypad {{{

void chip8_cpu_ldk(Chip8 *chip, Opcode op)
{
    for (;;) {
        chip->key_handle(chip);
        for (uint8_t i = 0; i < KEYPAD_SIZE; i++) {
            if (chip->keys[i] != 0) {
                chip->v[X] = i;
                goto EXIT;
            }
        }
    }

EXIT:

    LOG("LD V[%01X], K", X);
}

void chip8_cpu_skp(Chip8 *chip, Opcode op)
{
    if (chip->keys[chip->v[X]])
        chip->pc += 2;

    LOG("SKP V[%01X]", X);
}

void chip8_cpu_sknp(Chip8 *chip, Opcode op)
{
    if (!chip->keys[chip->v[X]])
        chip->pc += 2;

    LOG("SKNP V[%01X]", X);
}

// }}}

// BDC {{{

void chip8_cpu_bcd(Chip8 *chip, Opcode op)
{
    chip->ram[chip->i]   = (chip->v[X] % 1000) / 100;
    chip->ram[chip->i+1] = (chip->v[X] % 100) / 10 ;
    chip->ram[chip->i+2] = (chip->v[X] % 10);

    LOG("BCD %01X", X);
}

// }}}

// Timers {{{

void chip8_cpu_ldrdt(Chip8 *chip, Opcode op)
{
    chip->v[X] = chip->dt;

    LOG("LD V[%01X], DT", X);
}

void chip8_cpu_lddtr(Chip8 *chip, Opcode op)
{
    chip->dt = chip->v[X];

    LOG("LD DT, V[%01X]", X);
}

void chip8_cpu_ldstr(Chip8 *chip, Opcode op)
{
    chip->st = chip->v[X];

    LOG("LD ST, V[%01X]", X);
}

// }}}

// Bad Instruction {{{

void chip8_cpu_bdi(Chip8 *chip, Opcode op)
{
    (void) chip;
    fprintf(stderr, "BAD INSTRUCTION: %04X\n", op);
    abort();
}

// }}}

