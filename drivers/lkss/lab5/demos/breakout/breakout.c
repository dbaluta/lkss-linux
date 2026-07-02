// SPDX-License-Identifier: GPL-2.0
/*
 * breakout - brick breaker (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: clear the wall of bricks with ball and paddle. You have 3
 * lives, and the number of lit LEDs is your life counter.
 *
 * Controls: SW1 = paddle left, SW2 = paddle right (hold), SW3 = restart
 *
 * The brick wall, paddle, ball and labels are already created. You
 * implement:
 *
 *   TODO 1: show the remaining lives on the LEDs
 *   TODO 2: move the paddle while SW1/SW2 are held
 *   TODO 3: move the ball, bounce off walls and the paddle
 *   TODO 4: brick collision - hide the brick, bounce, count the score
 *   TODO 5: lose a life when the ball falls off; win/lose conditions
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs (common/hal.h):
 *   uint32_t hal_buttons(void);          held buttons, bit per button
 *   bool hal_button_pressed(int btn);    true once per press (edge)
 *   void hal_leds(uint32_t mask);        bit0=red, bit1=green, bit2=blue
 *
 * LVGL:
 *   lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);     hide (destroyed brick)
 *   lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);  show again (restart)
 *   lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);     already destroyed?
 *   lv_obj_set_pos(obj, x, y);                    move paddle/ball
 *   lv_label_set_text_fmt(lbl, "%d", v);          update the score
 *   LV_CLAMP(min, v, max);                        keep paddle on screen
 *
 * Geometry helper already provided: hit_brick(r, c) returns true when
 * the ball rectangle overlaps brick (r, c).
 *
 * -----------------------------------------------------------------------
 */
#include <stdlib.h>
#include "hal.h"

#define SCREEN     240
#define COLS       8
#define ROWS       5
#define BRICK_W    28		/* 8 * (28 + 2) = 240 */
#define BRICK_H    10
#define BRICK_GAP  2
#define BRICK_TOP  24
#define PADDLE_W   44
#define PADDLE_H   6
#define PADDLE_Y   (SCREEN - 14)
#define BALL_SIZE  6
#define PADDLE_SPD 5

static const uint32_t row_colors[ROWS] = {
	0xFF4040, 0xFF9040, 0xFFFF40, 0x40FF40, 0x4090FF,
};

static lv_obj_t *bricks[ROWS][COLS];
static lv_obj_t *paddle, *ball;
static lv_obj_t *msg_lbl, *score_lbl;

static int paddle_x;		/* paddle center */
static int ball_x, ball_y;	/* ball top-left */
static int vx, vy;
static int lives, score, bricks_left;
static bool running;

static void show_lives(void)
{
	/* TODO 1: light one LED per remaining life with hal_leds().
	 * 3 lives = red+green+blue, 2 = red+green, 1 = red, 0 = dark.
	 */
}

/* Put the ball above the paddle, moving up */
static void reset_ball(void)
{
	ball_x = paddle_x - BALL_SIZE / 2;
	ball_y = PADDLE_Y - 40;
	vx = (rand() % 2) ? 2 : -2;
	vy = -3;
}

static void new_game(void)
{
	lives = 3;
	score = 0;
	bricks_left = ROWS * COLS;
	paddle_x = SCREEN / 2;

	for (int r = 0; r < ROWS; r++)
		for (int c = 0; c < COLS; c++)
			lv_obj_remove_flag(bricks[r][c], LV_OBJ_FLAG_HIDDEN);

	lv_label_set_text(score_lbl, "0");
	lv_label_set_text(msg_lbl, "");
	show_lives();
	reset_ball();
	running = true;
}

void end_game(const char *text)
{
	running = false;
	lv_label_set_text(msg_lbl, text);
}

/* Return true if the ball rectangle overlaps brick cell (r, c) */
bool hit_brick(int r, int c)
{
	int bx = c * (BRICK_W + BRICK_GAP);
	int by = BRICK_TOP + r * (BRICK_H + BRICK_GAP);

	return ball_x + BALL_SIZE > bx && ball_x < bx + BRICK_W &&
	       ball_y + BALL_SIZE > by && ball_y < by + BRICK_H;
}

/* Called at 60 fps */
static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	if (hal_button_pressed(HACKPAD_BTN_SW3) && !running)
		new_game();
	if (!running)
		return;

	/* TODO 2: paddle movement.
	 * While SW1 is held move paddle_x left by PADDLE_SPD, while SW2
	 * is held move it right. Clamp:
	 *   paddle_x = LV_CLAMP(PADDLE_W/2, paddle_x, SCREEN - PADDLE_W/2);
	 */

	/* TODO 3: ball movement.
	 * Add vx/vy to ball_x/ball_y. Bounce off the left/right walls
	 * (negate vx) and the top (negate vy). Bounce off the paddle
	 * when moving down (vy > 0), the ball bottom reaches PADDLE_Y
	 * and it overlaps the paddle horizontally. Bonus: set vx from
	 * where the paddle was hit for controllable rebounds.
	 */

	/* TODO 4: brick collision.
	 * For every non-hidden brick, if hit_brick(r, c):
	 * hide it, negate vy, add (ROWS - r) * 10 to the score, update
	 * score_lbl, and decrement bricks_left - when it reaches zero,
	 * end_game("YOU WIN!\nSW3 = play again"). Only handle one brick
	 * per frame (break out of the loops after a hit).
	 */

	/* TODO 5: ball lost.
	 * When ball_y > SCREEN: lives--, show_lives(); if lives == 0,
	 * end_game("GAME OVER\nSW3 = play again"), else reset_ball().
	 */

	/* Push the state to the screen */
	lv_obj_set_pos(paddle, paddle_x - PADDLE_W / 2, PADDLE_Y);
	lv_obj_set_pos(ball, ball_x, ball_y);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	for (int r = 0; r < ROWS; r++) {
		for (int c = 0; c < COLS; c++) {
			lv_obj_t *b = lv_obj_create(scr);

			lv_obj_set_size(b, BRICK_W, BRICK_H);
			lv_obj_set_pos(b, c * (BRICK_W + BRICK_GAP),
				       BRICK_TOP + r * (BRICK_H + BRICK_GAP));
			lv_obj_set_style_bg_color(b,
					lv_color_hex(row_colors[r]), 0);
			lv_obj_set_style_border_width(b, 0, 0);
			lv_obj_set_style_radius(b, 1, 0);
			bricks[r][c] = b;
		}
	}

	paddle = lv_obj_create(scr);
	lv_obj_set_size(paddle, PADDLE_W, PADDLE_H);
	lv_obj_set_style_bg_color(paddle, lv_color_white(), 0);
	lv_obj_set_style_border_width(paddle, 0, 0);
	lv_obj_set_style_radius(paddle, 3, 0);

	ball = lv_obj_create(scr);
	lv_obj_set_size(ball, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFF00), 0);
	lv_obj_set_style_border_width(ball, 0, 0);
	lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);

	score_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(score_lbl, lv_color_hex(0x808080), 0);
	lv_obj_align(score_lbl, LV_ALIGN_TOP_LEFT, 4, 2);

	msg_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(msg_lbl, lv_color_hex(0x00FF00), 0);
	lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_18, 0);
	lv_obj_align(msg_lbl, LV_ALIGN_CENTER, 0, 30);

	new_game();
	lv_timer_create(game_tick, 16, NULL);

	hal_run();
	return 0;
}
