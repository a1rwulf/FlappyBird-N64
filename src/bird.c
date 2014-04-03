#include <math.h>

#include "bird.h"

#include "audio.h"
#include "background.h"
#include "global.h"

static sprite_t *bird_sprite = NULL;
static float bird_half_w = 0.0;
static float bird_half_h = 0.0;

bird_t bird_setup(u8 color_type)
{
    if (bird_sprite == NULL)
    {
        /* Load the sprite and pre-calculate some size details */
        bird_sprite = read_dfs_sprite( "/gfx/bird.sprite" );
        /* Calculate the half-hypotenuse of the slice diagonal */
        float slice_w = (bird_sprite->width / bird_sprite->hslices) - 1.0;
        float slice_h = (bird_sprite->height / bird_sprite->vslices) - 1.0;
        bird_half_w = (slice_w / 2.0) * GRAPHICS_SCALE;
        bird_half_h = (slice_h / 2.0) * GRAPHICS_SCALE;
    }
    bird_t bird = {
        .state = BIRD_STATE_TITLE,
        .color_type = color_type,
        .score = 0,
        .dead_ms = 0,
        .anim_ms = 0,
        .anim_frame = 0,
        .x = BIRD_TITLE_X,
        .y = 0.0,
        .dx = 0.0,
        .rot = 0.0,
        .dy = 0.0,
        .dy_ms = 0,
        .sine_ms = 0,
        .sine_x = 0.0,
        .sine_y = 0.0
    };
    return bird;
}

void bird_draw(const bird_t bird)
{
    /* Calculate player space center position */
    int cx = g_graphics->width * bird.x;
    int cy = GROUND_TOP_Y >> 1;
    /* Calculate bird Y position */
    float bird_y = bird.y;
    switch (bird.state)
    {
        case BIRD_STATE_READY:
        case BIRD_STATE_TITLE:
            bird_y += bird.sine_y;
            break;
    }
    if (bird_y > BIRD_MAX_Y) bird_y = BIRD_MAX_Y;
    if (bird_y < BIRD_MIN_Y) bird_y = BIRD_MIN_Y;
    cy += bird_y * cy;
    /* TODO Calculate rotation from center point */
    u16 tx = cx - bird_half_w, bx = cx + bird_half_w,
        ty = cy - bird_half_h, by = cy + bird_half_h;
    /* Load the current animation sprite slice as a texture */
    graphics_rdp_texture_fill( g_graphics );
    rdp_sync( SYNC_PIPE );
    u8 stride = (bird.color_type * BIRD_NUM_COLORS) + bird.anim_frame;
    rdp_load_texture_stride( 0, 0, MIRROR_DISABLED, bird_sprite, stride );
    /* Draw the rotated rectangle */
    rdp_draw_textured_rectangle_scaled( 0,
        tx, ty, bx, by, GRAPHICS_SCALE, GRAPHICS_SCALE );
}

inline static void bird_tick_animation(bird_t *bird)
{
    u64 ticks_ms = get_ticks_ms(),
        anim_ms = bird->anim_ms;
    u8 anim_frame = bird->anim_frame;
    if (bird->state != BIRD_STATE_DEAD)
    {
        if (ticks_ms - anim_ms >= BIRD_ANIM_RATE)
        {
            /* Update animation state */
            if (++anim_frame >= BIRD_ANIM_FRAMES)
            {
                anim_frame = 0;
            }
            anim_ms = ticks_ms;
        }
    } else {
        /* Dead birds don't animate */
        anim_ms = ticks_ms;
        anim_frame = BIRD_ANIM_FRAMES - 1;
    }
    bird->anim_ms = anim_ms;
    bird->anim_frame = anim_frame;
}

inline static void bird_tick_dx(bird_t *bird)
{
    /* Move the bird over if needed */
    if (bird->state != BIRD_STATE_TITLE && bird->x > BIRD_PLAY_X)
    {
        bird->dx += BIRD_ACCEL_X;
        bird->x -= bird->dx;
        if (bird->x < BIRD_PLAY_X) bird->x = BIRD_PLAY_X;
    }
}

inline static void bird_tick_sine_wave(bird_t *bird)
{
    /* Center the bird in the sky */
    bird->y = 0.0;
    bird->rot = 0.0;
    /* Periodically update the "floating" effect */
    u64 ticks_ms = get_ticks_ms();
    if (ticks_ms - bird->sine_ms >= BIRD_SINE_RATE)
    {
        bird_tick_dx( bird );
        /* Increment the "floating" effect sine wave */
        bird->sine_ms = ticks_ms;
        bird->sine_x += BIRD_SINE_INCREMENT;
        bird->sine_y = sinf( bird->sine_x ) * BIRD_SINE_DAMPEN;
        while (bird->sine_x >= BIRD_SINE_CYCLE)
        {
            bird->sine_x -= BIRD_SINE_CYCLE;
        }
    }
}

static void bird_tick_velocity(bird_t *bird, gamepad_state_t gamepad)
{
    /* Flap when the player presses A */
    if ( gamepad.A )
    {
        bird->dy = -BIRD_FLAP_VELOCITY;
        bird->anim_frame = BIRD_ANIM_FRAMES - 1;
        audio_play_sfx( g_audio, SFX_WING );
    }
    u64 ticks_ms = get_ticks_ms();
    if ( ticks_ms - bird->dy_ms >= BIRD_VELOCITY_RATE )
    {
        bird_tick_dx( bird );
        float y = bird->y;
        float dy = bird->dy;
        dy += BIRD_GRAVITY_ACCEL;
        y += dy;
        /* Did the bird hit the ceiling? */
        if (y < BIRD_MIN_Y)
        {
            y = BIRD_MIN_Y;
        }
        /* Did the bird hit the ground? */
        if (y > BIRD_MAX_Y)
        {
            y = BIRD_MAX_Y;
            dy = 0.0;
            bird->state = BIRD_STATE_DEAD;
            bird->dead_ms = ticks_ms;
            audio_play_sfx( g_audio, SFX_HIT );
        }
        bird->y = y;
        bird->dy = dy;
        bird->dy_ms = ticks_ms;
    }
}

void bird_tick(bird_t *bird, gamepad_state_t gamepad)
{
    u64 ticks_ms = get_ticks_ms();
    /* State transitions based on button input */
    switch (bird->state)
    {
        case BIRD_STATE_TITLE:
        case BIRD_STATE_DEAD:
            if (( gamepad.A || gamepad.start ) &&
                ( ticks_ms - bird->dead_ms > BIRD_DEAD_DELAY ))
            {
                bird->state = BIRD_STATE_READY;
                bird->score = 0;
                audio_play_sfx( g_audio, SFX_SWOOSH );
            }
            break;
        case BIRD_STATE_READY:
            if ( gamepad.A )
            {
                bird->state = BIRD_STATE_PLAY;
            }
    }
    /* Cycle through bird colors with right trigger */
    if ( gamepad.R )
    {
        if (++bird->color_type >= BIRD_NUM_COLORS)
        {
            bird->color_type = 0;
        }
    }
    /* State-specific tick updates */
    switch (bird->state)
    {
        case BIRD_STATE_READY:
        case BIRD_STATE_TITLE:
            bird_tick_sine_wave( bird );
            break;
        case BIRD_STATE_PLAY:
            bird_tick_velocity( bird, gamepad );
            // bird_tick_rotation( bird );
            break;
    }
    bird_tick_animation( bird );
}
