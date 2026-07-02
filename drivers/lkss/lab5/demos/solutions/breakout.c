// SPDX-License-Identifier: GPL-2.0
/*
 * breakout - brick breaker for the LKSS hackathon
 *
 * Controls: SW1 = paddle left, SW2 = paddle right (hold), SW3 = restart
 * You have 3 lives; the number of lit LEDs shows your remaining lives.
 * Clear all bricks to win.
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
	uint32_t mask = 0;

	if (lives >= 1) mask |= 1 << HACKPAD_LED_RED;
	if (lives >= 2) mask |= 1 << HACKPAD_LED_GREEN;
	if (lives >= 3) mask |= 1 << HACKPAD_LED_BLUE;
	hal_leds(mask);
}

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

static void end_game(const char *text)
{
	running = false;
	lv_label_set_text(msg_lbl, text);
}

/* Return true if the ball rectangle overlaps a brick cell */
static bool hit_brick(int r, int c)
{
	int bx = c * (BRICK_W + BRICK_GAP);
	int by = BRICK_TOP + r * (BRICK_H + BRICK_GAP);

	return ball_x + BALL_SIZE > bx && ball_x < bx + BRICK_W &&
	       ball_y + BALL_SIZE > by && ball_y < by + BRICK_H;
}

static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	uint32_t btns = hal_buttons();

	if (hal_button_pressed(HACKPAD_BTN_SW3) && !running)
		new_game();
	if (!running)
		return;

	if (btns & (1 << HACKPAD_BTN_SW1))
		paddle_x -= PADDLE_SPD;
	if (btns & (1 << HACKPAD_BTN_SW2))
		paddle_x += PADDLE_SPD;
	paddle_x = LV_CLAMP(PADDLE_W / 2, paddle_x, SCREEN - PADDLE_W / 2);

	ball_x += vx;
	ball_y += vy;

	/* Walls */
	if (ball_x <= 0 || ball_x >= SCREEN - BALL_SIZE)
		vx = -vx;
	if (ball_y <= 0)
		vy = -vy;

	/* Paddle */
	if (vy > 0 && ball_y + BALL_SIZE >= PADDLE_Y &&
	    ball_y + BALL_SIZE <= PADDLE_Y + PADDLE_H &&
	    ball_x + BALL_SIZE >= paddle_x - PADDLE_W / 2 &&
	    ball_x <= paddle_x + PADDLE_W / 2) {
		vy = -vy;
		/* Bounce angle depends on hit position */
		vx = (ball_x + BALL_SIZE / 2 - paddle_x) / 6;
		if (vx == 0)
			vx = (rand() % 2) ? 1 : -1;
		vx = LV_CLAMP(-4, vx, 4);
	}

	/* Bricks */
	for (int r = 0; r < ROWS && running; r++) {
		for (int c = 0; c < COLS; c++) {
			if (lv_obj_has_flag(bricks[r][c], LV_OBJ_FLAG_HIDDEN))
				continue;
			if (!hit_brick(r, c))
				continue;

			lv_obj_add_flag(bricks[r][c], LV_OBJ_FLAG_HIDDEN);
			vy = -vy;
			score += (ROWS - r) * 10;
			lv_label_set_text_fmt(score_lbl, "%d", score);
			if (--bricks_left == 0)
				end_game("YOU WIN!\nSW3 = play again");
			goto bricks_done;
		}
	}
bricks_done:

	/* Ball lost */
	if (ball_y > SCREEN) {
		lives--;
		show_lives();
		if (lives == 0) {
			end_game("GAME OVER\nSW3 = play again");
		} else {
			reset_ball();
		}
	}

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
