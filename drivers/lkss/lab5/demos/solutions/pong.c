// SPDX-License-Identifier: GPL-2.0
/*
 * pong - retro Pong for the LKSS hackathon
 *
 * Controls: SW1 = paddle up, SW2 = paddle down (hold), SW3 = restart
 * You play the left paddle; the right paddle is a software-controlled player.
 * First to 5 points wins. Green LED flashes when you score,
 * red LED when the software player scores.
 */
#include <stdlib.h>
#include "hal.h"

#define SCREEN      240
#define PADDLE_W    6
#define PADDLE_H    40
#define BALL_SIZE   8
#define PADDLE_SPD  4
#define SW_PLAYER_SPD 3
#define WIN_SCORE   5

static lv_obj_t *lpad, *rpad, *ball;
static lv_obj_t *lscore_lbl, *rscore_lbl, *msg_lbl;

static int lpad_y, rpad_y;          /* paddle centers */
static int ball_x, ball_y;          /* ball top-left  */
static int vx, vy;                  /* ball velocity  */
static int lscore, rscore;
static bool running;
static int led_off_count;           /* frames until score LED turns off */

static lv_obj_t *make_rect(int w, int h, uint32_t color)
{
	lv_obj_t *o = lv_obj_create(lv_screen_active());

	lv_obj_set_size(o, w, h);
	lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
	lv_obj_set_style_border_width(o, 0, 0);
	lv_obj_set_style_radius(o, 0, 0);
	return o;
}

static void serve(int dir)
{
	ball_x = SCREEN / 2 - BALL_SIZE / 2;
	ball_y = SCREEN / 2 - BALL_SIZE / 2;
	vx = 3 * dir;
	vy = (rand() % 2) ? 2 : -2;
}

static void new_game(void)
{
	lscore = rscore = 0;
	lpad_y = rpad_y = SCREEN / 2;
	lv_label_set_text(lscore_lbl, "0");
	lv_label_set_text(rscore_lbl, "0");
	lv_label_set_text(msg_lbl, "");
	serve((rand() % 2) ? 1 : -1);
	running = true;
}

static void score(bool player)
{
	if (player) {
		lscore++;
		lv_label_set_text_fmt(lscore_lbl, "%d", lscore);
		hal_led(HACKPAD_LED_GREEN, true);
	} else {
		rscore++;
		lv_label_set_text_fmt(rscore_lbl, "%d", rscore);
		hal_led(HACKPAD_LED_RED, true);
	}
	led_off_count = 20;	/* ~320 ms at 16 ms/frame */

	if (lscore >= WIN_SCORE || rscore >= WIN_SCORE) {
		running = false;
		lv_label_set_text(msg_lbl, lscore >= WIN_SCORE ?
				  "YOU WIN!\nSW3 = play again" :
				  "CPU WINS!\nSW3 = play again");
	} else {
		serve(player ? -1 : 1);
	}
}

static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	uint32_t btns = hal_buttons();

	if (led_off_count && --led_off_count == 0)
		hal_leds(0);

	if (hal_button_pressed(HACKPAD_BTN_SW3) && !running)
		new_game();

	if (!running)
		return;

	/* Player paddle: SW1 up, SW2 down (held) */
	if (btns & (1 << HACKPAD_BTN_SW1))
		lpad_y -= PADDLE_SPD;
	if (btns & (1 << HACKPAD_BTN_SW2))
		lpad_y += PADDLE_SPD;
	lpad_y = LV_CLAMP(PADDLE_H / 2, lpad_y, SCREEN - PADDLE_H / 2);

	/* Software-controlled paddle follows the ball */
	if (rpad_y < ball_y + BALL_SIZE / 2 - SW_PLAYER_SPD)
		rpad_y += SW_PLAYER_SPD;
	else if (rpad_y > ball_y + BALL_SIZE / 2 + SW_PLAYER_SPD)
		rpad_y -= SW_PLAYER_SPD;
	rpad_y = LV_CLAMP(PADDLE_H / 2, rpad_y, SCREEN - PADDLE_H / 2);

	/* Ball */
	ball_x += vx;
	ball_y += vy;

	if (ball_y <= 0 || ball_y >= SCREEN - BALL_SIZE)
		vy = -vy;

	/* Left paddle collision */
	if (vx < 0 && ball_x <= PADDLE_W &&
	    ball_y + BALL_SIZE >= lpad_y - PADDLE_H / 2 &&
	    ball_y <= lpad_y + PADDLE_H / 2) {
		vx = -vx;
		/* Add spin depending on where the paddle was hit */
		vy += (ball_y + BALL_SIZE / 2 - lpad_y) / 8;
		vy = LV_CLAMP(-5, vy, 5);
	}
	/* Right paddle collision */
	if (vx > 0 && ball_x + BALL_SIZE >= SCREEN - PADDLE_W &&
	    ball_y + BALL_SIZE >= rpad_y - PADDLE_H / 2 &&
	    ball_y <= rpad_y + PADDLE_H / 2) {
		vx = -vx;
		vy += (ball_y + BALL_SIZE / 2 - rpad_y) / 8;
		vy = LV_CLAMP(-5, vy, 5);
	}

	if (ball_x < -BALL_SIZE)
		score(false);
	else if (ball_x > SCREEN)
		score(true);

	lv_obj_set_pos(lpad, 0, lpad_y - PADDLE_H / 2);
	lv_obj_set_pos(rpad, SCREEN - PADDLE_W, rpad_y - PADDLE_H / 2);
	lv_obj_set_pos(ball, ball_x, ball_y);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* Dashed center line */
	for (int y = 0; y < SCREEN; y += 20) {
		lv_obj_t *dash = make_rect(2, 10, 0x505050);
		lv_obj_set_pos(dash, SCREEN / 2 - 1, y);
	}

	lpad = make_rect(PADDLE_W, PADDLE_H, 0xFFFFFF);
	rpad = make_rect(PADDLE_W, PADDLE_H, 0xFFFFFF);
	ball = make_rect(BALL_SIZE, BALL_SIZE, 0xFFFF00);

	lscore_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(lscore_lbl, lv_color_white(), 0);
	lv_obj_set_style_text_font(lscore_lbl, &lv_font_montserrat_32, 0);
	lv_obj_align(lscore_lbl, LV_ALIGN_TOP_MID, -40, 8);

	rscore_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(rscore_lbl, lv_color_white(), 0);
	lv_obj_set_style_text_font(rscore_lbl, &lv_font_montserrat_32, 0);
	lv_obj_align(rscore_lbl, LV_ALIGN_TOP_MID, 40, 8);

	msg_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(msg_lbl, lv_color_hex(0x00FF00), 0);
	lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(msg_lbl, LV_ALIGN_CENTER, 0, 40);

	new_game();
	lv_timer_create(game_tick, 16, NULL);

	hal_run();
	return 0;
}
