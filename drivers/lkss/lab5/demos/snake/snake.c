// SPDX-License-Identifier: GPL-2.0
/*
 * snake - classic Snake (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: steer a growing snake to the food without hitting the walls or
 * yourself. Only relative turns are available, which makes it tricky.
 *
 * Controls: SW1 = turn left (counter-clockwise), SW4 = turn right
 *           (clockwise), SW3 = restart after game over
 *
 * The object pool, the drawing (redraw()) and the game reset are
 * already written. You implement:
 *
 *   TODO 1: place the food on a random free cell
 *   TODO 2: handle the SW1/SW4 turns (change 'heading')
 *   TODO 3: advance the snake one cell and detect wall/self collision
 *   TODO 4: eat the food - grow, score, speed up
 *   TODO 5: LED milestones (red at length 5, +green at 10, +blue at 15)
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs (common/hal.h):
 *   bool hal_button_pressed(int btn);
 *       True exactly once per press (edge) - perfect for turns.
 *   void hal_leds(uint32_t mask);
 *       All LEDs from a bitmask: bit0=red, bit1=green, bit2=blue.
 *
 * LVGL:
 *   lv_obj_set_pos(obj, x, y);              move an object
 *   lv_label_set_text_fmt(lbl, "%d", v);    update a label
 *   lv_timer_set_period(timer, ms);         change the game speed
 *
 * Useful C:
 *   rand() % GRID                           random cell coordinate
 *
 * Data model: body[0] is the head; body[i] holds grid coordinates.
 * Moving = shift every element one slot towards the tail, then write
 * the new head into body[0]. Growing = do the same but keep the old
 * tail (shift one element further).
 *
 * -----------------------------------------------------------------------
 */
#include <stdlib.h>
#include <time.h>
#include "hal.h"

#define CELL      16
#define GRID      15			/* 15 x 16 = 240 px */
#define MAX_LEN   (GRID * GRID)
#define START_LEN 3

enum dir { UP, RIGHT, DOWN, LEFT };

/* Movement deltas indexed by enum dir */
static const int dx[] = { 0, 1, 0, -1 };
static const int dy[] = { -1, 0, 1, 0 };

struct cell {
	int x, y;
};

static struct cell body[MAX_LEN];	/* body[0] = head */
static int length;
static enum dir heading;
static struct cell food;
static bool running;
static int score;

static lv_obj_t *seg_obj[MAX_LEN];	/* object pool for body segments */
static lv_obj_t *food_obj;
static lv_obj_t *score_lbl, *msg_lbl;
static lv_timer_t *tick_timer;
static uint32_t tick_period;

static void place_food(void)
{
	/* TODO 1: pick a random cell (rand() % GRID for x and y) that is
	 * not covered by the snake body; loop until you find a free one.
	 * For now the food always sits on a fixed cell.
	 */
	food.x = 3;
	food.y = 3;

	lv_obj_set_pos(food_obj, food.x * CELL + 2, food.y * CELL + 2);
}

static void update_leds(void)
{
	/* TODO 5: build a bitmask from the snake length and hal_leds() it:
	 * length >= 5 -> red, >= 10 -> also green, >= 15 -> also blue.
	 */
}

/* Move every segment object to its cell; hide the unused pool entries */
static void redraw(void)
{
	for (int i = 0; i < length; i++) {
		lv_obj_remove_flag(seg_obj[i], LV_OBJ_FLAG_HIDDEN);
		lv_obj_set_pos(seg_obj[i], body[i].x * CELL + 1,
			       body[i].y * CELL + 1);
		lv_obj_set_style_bg_color(seg_obj[i],
			i == 0 ? lv_color_hex(0x00FF00)   /* head */
			       : lv_color_hex(0x00A000),  /* body */
			0);
	}
	for (int i = length; i < MAX_LEN; i++)
		lv_obj_add_flag(seg_obj[i], LV_OBJ_FLAG_HIDDEN);
}

static void new_game(void)
{
	length = START_LEN;
	heading = RIGHT;
	score = 0;
	for (int i = 0; i < length; i++) {
		body[i].x = GRID / 2 - i;
		body[i].y = GRID / 2;
	}
	lv_label_set_text(score_lbl, "0");
	lv_label_set_text(msg_lbl, "");
	tick_period = 220;
	lv_timer_set_period(tick_timer, tick_period);
	place_food();
	update_leds();
	redraw();
	running = true;
}

void game_over(void)
{
	running = false;
	lv_label_set_text_fmt(msg_lbl,
			      "GAME OVER\nscore %d\nSW3 = restart", score);
}

/* Called once per game step (the period shrinks as the snake grows) */
static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	/* TODO 2: turning.
	 * hal_button_pressed(HACKPAD_BTN_SW1) -> heading = (heading+3) % 4
	 * hal_button_pressed(HACKPAD_BTN_SW4) -> heading = (heading+1) % 4
	 */

	if (hal_button_pressed(HACKPAD_BTN_SW3) && !running)
		new_game();

	if (!running)
		return;

	/* The cell the head moves into this step */
	struct cell head = {
		body[0].x + dx[heading],
		body[0].y + dy[heading],
	};

	/* TODO 3: collision.
	 * If head is outside 0..GRID-1 (x or y), or matches any body[]
	 * cell, call game_over() and return. Right now the snake happily
	 * leaves the screen - try it!
	 */

	/* TODO 4: eat and grow.
	 * grow = (head.x == food.x && head.y == food.y).
	 * When growing, keep the old tail: shift starting from index
	 * 'length' instead of 'length - 1', then length++, score += 10,
	 * update score_lbl, place_food(), update_leds(), and speed up:
	 *   if (tick_period > 80) { tick_period -= 10;
	 *       lv_timer_set_period(tick_timer, tick_period); }
	 */

	/* Move: shift the body towards the tail, new head in front */
	for (int i = length - 1; i > 0; i--)
		body[i] = body[i - 1];
	body[0] = head;

	redraw();
}

int main(void)
{
	srand(time(NULL));
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	for (int i = 0; i < MAX_LEN; i++) {
		seg_obj[i] = lv_obj_create(scr);
		lv_obj_set_size(seg_obj[i], CELL - 2, CELL - 2);
		lv_obj_set_style_border_width(seg_obj[i], 0, 0);
		lv_obj_set_style_radius(seg_obj[i], 2, 0);
		lv_obj_add_flag(seg_obj[i], LV_OBJ_FLAG_HIDDEN);
	}

	food_obj = lv_obj_create(scr);
	lv_obj_set_size(food_obj, CELL - 4, CELL - 4);
	lv_obj_set_style_bg_color(food_obj, lv_color_hex(0xFFFF00), 0);
	lv_obj_set_style_border_width(food_obj, 0, 0);
	lv_obj_set_style_radius(food_obj, LV_RADIUS_CIRCLE, 0);

	score_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(score_lbl, lv_color_hex(0x808080), 0);
	lv_obj_align(score_lbl, LV_ALIGN_TOP_RIGHT, -4, 2);

	msg_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(msg_lbl, lv_color_hex(0xFF4040), 0);
	lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_18, 0);
	lv_obj_align(msg_lbl, LV_ALIGN_CENTER, 0, 0);

	tick_timer = lv_timer_create(game_tick, 220, NULL);
	new_game();

	hal_run();
	return 0;
}
