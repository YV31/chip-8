#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <bsd/sys/time.h>

#include "chip8.h"

#define PROGRAM_NAME "CHIP-8 Emulator"

static uint8_t const keypad[KEYPAD_SIZE] = {
    SDLK_x, SDLK_1, SDLK_2, SDLK_3,
    SDLK_q, SDLK_w, SDLK_e, SDLK_a,
    SDLK_s, SDLK_d, SDLK_z, SDLK_c,
    SDLK_4, SDLK_r, SDLK_f, SDLK_v,
};

#define HZ_TO_MS(hz) (1000000L / (hz))
#define SCALE 5
#define OFFSET 0
#define WIN_W (SCREEN_W * SCALE) + OFFSET
#define WIN_H (SCREEN_H * SCALE)

bool running = true;
int delay_time = 0;
int ret_val = EXIT_SUCCESS;

void key_handle(Chip8 *chip)
{
    SDL_Event e = *(SDL_Event *) chip->userdata;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            switch(e.key.keysym.sym) {
            case SDLK_y:
                chip8_reset(chip);
                break;
            default:
                break;
            }

            for (int i = 0; i < KEYPAD_SIZE; i++) {
                if (e.key.keysym.sym == keypad[i]) {
                    chip->keys[i] = 1; 
                }
            }
            break;
        case SDL_KEYUP:
            for (int i = 0; i < KEYPAD_SIZE; i++) {
                if (e.key.keysym.sym == keypad[i]) {
                    chip->keys[i] = 0; 
                }
            }
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    srand(time(NULL));

    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    atexit(SDL_Quit);

    win = SDL_CreateWindow(
            PROGRAM_NAME,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            WIN_W, WIN_H, 0
    );
    if (win == NULL) {
        ret_val = EXIT_FAILURE;
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto FAILURE_WIN;
    }

    ren = SDL_CreateRenderer(win, -1, 0);
    if (ren == NULL) {
        ret_val = EXIT_FAILURE;
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto FAILURE_REN;
    }

    SDL_Event e;
    Chip8 *chip = chip8_create(key_handle, &e);

    if (chip == NULL) {
        perror("Failed to allocate Chip8");
        goto FAILURE_CHIP8;
    }

    // TODO: Better argument handling

    if (argc > 1)
        chip8_load_rom(chip, argv[1]);
    if (argc > 2)
        delay_time = atoi(argv[2]);

    struct timeval start, stop, diff;
    gettimeofday(&start, NULL);

    while (running) {
        
#ifdef DEBUG
        printf("V[0..F]: ");
        for (int i = 0; i <= 0xF; i++) {
            printf("%02X ", chip->v[i]);
        }
        printf("| PC: %03X | I: %03X | ", chip->pc, chip->i);
#endif

        gettimeofday(&stop, NULL);
        timersub(&stop, &start, &diff);

        if (timercmp(&diff, &(struct timeval){.tv_usec = HZ_TO_MS(60)}, >=)) {
            chip8_tick(chip);
            start = stop;
        }

        chip8_cycle(chip);

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);

        for (int i = 0; i < SCREEN_H; i++) {
            for (int j = 0; j < SCREEN_W; j++) {
                SDL_Rect const pixel = { j * SCALE, i * SCALE, SCALE, SCALE };
                if (chip->screen[i * SCREEN_W + j] == 1)
                    SDL_RenderFillRect(ren, &pixel);
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(delay_time);
    }
    
    chip8_destroy(chip);
FAILURE_CHIP8:
    SDL_DestroyRenderer(ren);
FAILURE_REN:
    SDL_DestroyWindow(win);
FAILURE_WIN:

    return ret_val;
}
