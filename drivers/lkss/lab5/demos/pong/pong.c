// SPDX-License-Identifier: GPL-2.0
/*
 * pong - retro Pong (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: a ball bounces between two paddles. You play the left paddle,
 * a software-controlled player runs the right one. First to 5 points wins.
 *
 * Controls: SW1 = paddle up, SW2 = paddle down (hold), SW3 = restart
 *
 * The UI (paddles, ball, score labels) is already created in main()
 * and game_tick() already runs 60 times per second. You implement the
 * game logic:
 *
 *   TODO 1: move the player paddle while SW1/SW2 are held
 *   TODO 2: make the software-controlled paddle follow the ball
 *   TODO 3: move the ball and bounce it off the top/bottom walls
 *   TODO 4: bounce the ball off the paddles
 *   TODO 5: detect a miss, count the score, flash a LED, end at 5
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs (common/hal.h):
 *   uint32_t hal_buttons(void);
 *       Bitmask of buttons held down *right now*. Test a button with
 *       (hal_buttons() & (1 << HACKPAD_BTN_SW1)). Use for movement.
 *   bool hal_button_pressed(int btn);
 *       True exactly once per press (edge). Use for restart.
 *   void hal_led(int led, bool on);       one LED  (HACKPAD_LED_GREEN...)
 *   void hal_leds(uint32_t mask);         all LEDs (bit0=R bit1=G bit2=B)
 *
 * LVGL (only these are needed here):
 *   lv_obj_set_pos(obj, x, y);            move an object
 *   lv_label_set_text_fmt(lbl, "%d", v);  update a label
 *   LV_CLAMP(min, v, max);                clamp a value
 *
 * -----------------------------------------------------------------------
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

static lv_obj_t *make_rect(int w, int h, uint32_t color)
{
	lv_obj_t *o = lv_obj_create(lv_screen_active());

	lv_obj_set_size(o, w, h);
	lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
	lv_obj_set_style_border_width(o, 0, 0);
	lv_obj_set_style_radius(o, 0, 0);
	return o;
}

/* Put the ball in the center, moving towards dir (+1 right, -1 left) */
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

/* Called at 60 fps - the whole game lives here */
static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	if (hal_button_pressed(HACKPAD_BTN_SW3) && !running)
		new_game();

	if (!running)
		return;

	/* TODO 1: player paddle.
	 * While SW1 is held, decrease lpad_y by PADDLE_SPD; while SW2 is
	 * held, increase it. Clamp so the paddle stays on screen:
	 *   lpad_y = LV_CLAMP(PADDLE_H / 2, lpad_y, SCREEN - PADDLE_H / 2);
	 */

	/* TODO 2: software-controlled paddle.
	 * Move rpad_y at most SW_PLAYER_SPD pixels per frame towards the ball
	 * center (ball_y + BALL_SIZE / 2). Clamp like the player paddle.
	 * (Moving at full ball speed would make the software player unbeatable!)
	 */

	/* TODO 3: ball movement and wall bounce.
	 * Add vx/vy to ball_x/ball_y. If the ball touches the top
	 * (ball_y <= 0) or bottom (ball_y >= SCREEN - BALL_SIZE),
	 * negate vy.
	 */

	/* TODO 4: paddle collisions.
	 * Left paddle: if the ball moves left (vx < 0), reaches
	 * ball_x <= PADDLE_W and overlaps the paddle vertically
	 * (lpad_y - PADDLE_H/2 .. lpad_y + PADDLE_H/2), negate vx.
	 * Mirror the same check for the right paddle at
	 * ball_x + BALL_SIZE >= SCREEN - PADDLE_W.
	 * Bonus: add "spin" - adjust vy by where the paddle was hit.
	 */

	/* TODO 5: scoring.
	 * If the ball leaves the screen on the left, the software player scores; on
	 * the right, you score. Update lscore/rscore and the labels,
	 * flash the green (you) or red (software player) LED with hal_led(), and
	 * serve() towards the loser. When someone reaches WIN_SCORE,
	 * set running = false and show a message in msg_lbl.
	 */

	/* Push the state to the screen */
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
