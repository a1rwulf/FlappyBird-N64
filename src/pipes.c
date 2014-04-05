#include <stdlib.h>
#include <math.h>

#include "pipes.h"

#include "background.h"
#include "global.h"

pipes_t pipes_setup(void)
{
    pipes_t pipes = {
        .color = PIPE_COLOR_GREEN,
        .reset_ms = 0, .scroll_ms = get_ticks_ms(),
        .cap_sprite = read_dfs_sprite( "/gfx/pipe-cap.sprite" ),
        .tube_sprite = read_dfs_sprite( "/gfx/pipe-tube.sprite" )
    };
    return pipes;
}

void pipes_free(pipes_t *pipes)
{
    free(pipes->cap_sprite);
    pipes->cap_sprite = NULL;
    free(pipes->tube_sprite);
    pipes->tube_sprite = NULL;
}

inline static pipe_colors_t pipes_random_color(void)
{
    return ((float) rand() / (float) RAND_MAX) * PIPE_NUM_COLORS;
}

inline static u8 pipe_prev_index(u8 i)
{
    return (i > 0) ? i - 1 : PIPES_MAX_NUM - 1;
}

inline static float pipe_random_y(void)
{
    float y = ((float) rand() / (float) RAND_MAX) * PIPE_MAX_Y;
    if ( roundf( (float) rand() / (float) RAND_MAX ) ) y *= -1.0;
    return y;
}

inline static float pipe_random_bias_y(const float prev_y)
{
    float bias_y = ((float) rand() / (float) RAND_MAX) * PIPE_MAX_BIAS_Y;
    if ( roundf( (float) rand() / (float) RAND_MAX ) ) bias_y *= -1.0;
    float y = prev_y + bias_y;
    if ( y > PIPE_MAX_Y ) y = PIPE_MAX_Y;
    if ( y < -PIPE_MAX_Y ) y = -PIPE_MAX_Y;
    return y;
}

void pipes_reset(pipes_t *pipes)
{
    if (pipes->reset_ms == pipes->scroll_ms) return;
    float y = pipe_random_y(), prev_y = y;
    for (u8 i = 0; i < PIPES_MAX_NUM; i++)
    {
        if (i > 0)
        {
            y = pipe_random_bias_y( prev_y );
        }
        pipe_t pipe = {
            .x = PIPE_START_X + (i * PIPE_GAP_X),
            .y = y
        };
        pipes->n[i] = pipe;
        prev_y = y;
    }
    pipes->color = pipes_random_color();
    pipes->reset_ms = pipes->scroll_ms = get_ticks_ms();
}

void pipes_tick(pipes_t *pipes)
{
    const u64 ticks_ms = get_ticks_ms();
    if (ticks_ms - pipes->scroll_ms >= PIPES_SCROLL_RATE)
    {
        for (u8 i = 0, j; i < PIPES_MAX_NUM; i++)
        {
            pipes->n[i].x += PIPES_SCROLL_DX;
            if (pipes->n[i].x < PIPE_MIN_X)
            {
                j = pipe_prev_index( i );
                pipes->n[i].x = pipes->n[j].x + PIPE_GAP_X;
                pipes->n[i].y = pipe_random_bias_y( pipes->n[j].y );
            }
        }
        pipes->scroll_ms = ticks_ms;
    }
}

void pipes_draw(const pipes_t pipes)
{
    sprite_t *tube = pipes.tube_sprite;
    sprite_t *cap = pipes.cap_sprite;
    const u8 color = pipes.color, cap_hslices = cap->hslices;
    pipe_t pipe;
    s16 cx, cy, tx, ty, bx, by, gap_y;

    graphics_rdp_texture_fill( g_graphics );
    mirror_t mirror = MIRROR_DISABLED;
    rdp_sync( SYNC_PIPE );
    for (u8 i = 0; i < PIPES_MAX_NUM; i++)
    {
        pipe = pipes.n[i];
        /* Calculate X position */
        cx = g_graphics->width * pipe.x;
        tx = cx - (PIPE_TUBE_WIDTH >> 1);
        bx = cx + (PIPE_TUBE_WIDTH >> 1) - 1;
        if (bx < 0 || tx >= g_graphics->width) continue;
        /* Calculate Y position */
        cy = (BG_GROUND_TOP_Y >> 1);
        gap_y = cy + pipe.y * cy;
        /* Load tube texture */
        rdp_load_texture_stride( 0, 0, mirror, tube, color );
        /* Top tube */
        ty = 0;
        by = gap_y - (PIPE_GAP_Y >> 1);
        rdp_draw_textured_rectangle( 0, tx, ty, bx, by );
        /* Bottom tube */
        ty = gap_y + (PIPE_GAP_Y >> 1);
        by = BG_GROUND_TOP_Y - 1;
        rdp_draw_textured_rectangle( 0, tx, ty, bx, by );
        /* Load top cap texture */
        rdp_load_texture_stride( 0, 0, mirror, cap, color + cap_hslices );
        /* Top cap */
        ty = gap_y - (PIPE_GAP_Y >> 1);
        by = ty + PIPE_CAP_HEIGHT - 1;
        rdp_draw_textured_rectangle( 0, tx, ty, bx, by );
        /* Load bottom cap texture */
        rdp_load_texture_stride( 0, 0, mirror, cap, color );
        /* Bottom cap */
        ty = gap_y + (PIPE_GAP_Y >> 1) - PIPE_CAP_HEIGHT;
        by = ty + PIPE_CAP_HEIGHT - 1;
        rdp_draw_textured_rectangle( 0, tx, ty, bx, by );
    }

}
