// SPDX-License-Identifier: GPL-2.0
/*
 * tetris - falling blocks (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: the classic falling-blocks game. Pieces drop into a 10 x 15
 * well; complete rows disappear and score points, and the pieces fall
 * faster as you clear lines.
 *
 * Suggested controls: SW1 = move left, SW2 = move right, SW3 = rotate,
 *                     SW4 = hard drop
 *
 * This skeleton only draws the initial scene: the well with a few
 * settled blocks, one piece near the top, and the score/next panel.
 * Nothing moves yet - the game is yours to write:
 *
 *   TODO 1: make the piece fall (an lv_timer that does py++, redraw())
 *   TODO 2: a collision test - walls, floor and settled board[] cells
 *   TODO 3: buttons - move/rotate only when the result still fits
 *   TODO 4: settle the piece into board[] when it lands, spawn the next
 *   TODO 5: clear full rows - score, speed up, LED milestones
 *
 * Peek at pong/snake for the timer + button pattern.
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs (common/hal.h):
 *   bool hal_button_pressed(int btn);
 *       True exactly once per press (edge) - perfect for moves.
 *   void hal_leds(uint32_t mask);
 *       All LEDs from a bitmask: bit0=red, bit1=green, bit2=blue.
 *
 * LVGL:
 *   lv_timer_create(cb, period_ms, NULL);   call cb every period_ms
 *   lv_timer_set_period(timer, ms);         change the fall speed
 *   lv_label_set_text_fmt(lbl, "%d", v);    update a label
 *
 * Data model: board[y][x] holds 0 for an empty cell, or piece index + 1
 * for a settled block (the +1 keeps the color). The falling piece is
 * NOT in board[]: it is (cur, rot, px, py), where shapes[cur][rot] is a
 * 4x4 bitmap and (px, py) is the position of that box on the board.
 * shape_cell(piece, rot, cx, cy) tells whether a box cell is solid.
 * redraw() paints board[] plus the falling piece - call it after every
 * change.
 *
 * -----------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hal.h"

#define CELL    16
#define COLS    10			/* playfield: 10 x 15 cells... */
#define ROWS    15			/* ...= 160 x 240 px */
#define PANEL_X (COLS * CELL)		/* score/next panel to the right */

#define NUM_PIECES 7

/* One 4x4 bitmap per rotation, bit (row * 4 + col), row 0 on top */
static const uint16_t shapes[NUM_PIECES][4] = {
	{ 0x00F0, 0x4444, 0x0F00, 0x2222 },	/* I */
	{ 0x0660, 0x0660, 0x0660, 0x0660 },	/* O */
	{ 0x0072, 0x0262, 0x0270, 0x0232 },	/* T */
	{ 0x0036, 0x0462, 0x0360, 0x0231 },	/* S */
	{ 0x0063, 0x0264, 0x0630, 0x0132 },	/* Z */
	{ 0x0071, 0x0226, 0x0470, 0x0322 },	/* J */
	{ 0x0074, 0x0622, 0x0170, 0x0223 },	/* L */
};

static const uint32_t piece_color[NUM_PIECES] = {
	0x00FFFF,	/* I cyan   */
	0xFFD500,	/* O yellow */
	0xA020F0,	/* T purple */
	0x00E060,	/* S green  */
	0xFF3030,	/* Z red    */
	0x4060FF,	/* J blue   */
	0xFF9000,	/* L orange */
};

static uint8_t board[ROWS][COLS];	/* 0 = empty, else piece + 1 */

static int cur, rot, px, py;		/* the falling piece */
static int next_piece;

static lv_obj_t *cell_obj[ROWS][COLS];	/* object pool, one per cell */
static lv_obj_t *next_obj[4][4];	/* "next piece" preview */
static lv_obj_t *score_lbl, *lines_lbl;

/* Is box cell (cx, cy) of the given piece/rotation solid? */
static bool shape_cell(int piece, int r, int cx, int cy)
{
	return shapes[piece][r] >> (cy * 4 + cx) & 1;
}

/* Paint the 4x4 "next piece" preview */
static void update_next(void)
{
	for (int cy = 0; cy < 4; cy++)
		for (int cx = 0; cx < 4; cx++) {
			lv_obj_t *c = next_obj[cy][cx];

			if (shape_cell(next_piece, 0, cx, cy)) {
				lv_obj_remove_flag(c, LV_OBJ_FLAG_HIDDEN);
				lv_obj_set_style_bg_color(c,
					lv_color_hex(piece_color[next_piece]),
					0);
			} else {
				lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
			}
		}
}

/* Settled blocks + the falling piece -> the cell object pool */
static void redraw(void)
{
	uint8_t view[ROWS][COLS];

	memcpy(view, board, sizeof(view));
	for (int cy = 0; cy < 4; cy++)
		for (int cx = 0; cx < 4; cx++)
			if (shape_cell(cur, rot, cx, cy) && py + cy >= 0)
				view[py + cy][px + cx] = cur + 1;

	for (int y = 0; y < ROWS; y++)
		for (int x = 0; x < COLS; x++) {
			lv_obj_t *c = cell_obj[y][x];

			if (view[y][x]) {
				lv_obj_remove_flag(c, LV_OBJ_FLAG_HIDDEN);
				lv_obj_set_style_bg_color(c,
					lv_color_hex(piece_color[view[y][x] - 1]),
					0);
			} else {
				lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
			}
		}
}

static lv_obj_t *make_panel_label(const char *txt, int y)
{
	lv_obj_t *l = lv_label_create(lv_screen_active());

	lv_obj_set_style_text_color(l, lv_color_hex(0x808080), 0);
	lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
	lv_obj_set_pos(l, PANEL_X + 10, y);
	lv_label_set_text(l, txt);
	return l;
}

static lv_obj_t *make_panel_value(int y)
{
	lv_obj_t *l = make_panel_label("0", y);

	lv_obj_set_style_text_color(l, lv_color_white(), 0);
	lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
	return l;
}

int main(void)
{
	srand(time(NULL));
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* the playfield cell pool */
	for (int y = 0; y < ROWS; y++)
		for (int x = 0; x < COLS; x++) {
			lv_obj_t *c = lv_obj_create(scr);

			lv_obj_set_size(c, CELL - 2, CELL - 2);
			lv_obj_set_pos(c, x * CELL + 1, y * CELL + 1);
			lv_obj_set_style_border_width(c, 0, 0);
			lv_obj_set_style_radius(c, 2, 0);
			lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
			cell_obj[y][x] = c;
		}

	/* the right-hand panel: wall, score, lines, next preview */
	lv_obj_t *wall = lv_obj_create(scr);

	lv_obj_set_size(wall, 2, 240);
	lv_obj_set_pos(wall, PANEL_X, 0);
	lv_obj_set_style_bg_color(wall, lv_color_hex(0x404040), 0);
	lv_obj_set_style_border_width(wall, 0, 0);
	lv_obj_set_style_radius(wall, 0, 0);

	make_panel_label("SCORE", 8);
	score_lbl = make_panel_value(24);
	make_panel_label("LINES", 64);
	lines_lbl = make_panel_value(80);
	make_panel_label("NEXT", 120);

	for (int cy = 0; cy < 4; cy++)
		for (int cx = 0; cx < 4; cx++) {
			lv_obj_t *c = lv_obj_create(scr);

			lv_obj_set_size(c, 10, 10);
			lv_obj_set_pos(c, PANEL_X + 14 + cx * 12,
				       140 + cy * 12);
			lv_obj_set_style_border_width(c, 0, 0);
			lv_obj_set_style_radius(c, 2, 0);
			lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
			next_obj[cy][cx] = c;
		}

	/* --- the initial scene --- */

	/* a few settled blocks, so the board[] data model is visible */
	for (int x = 0; x < COLS - 3; x++)
		board[ROWS - 1][x] = x % NUM_PIECES + 1;

	/* one piece near the top, one waiting in the preview */
	cur = rand() % NUM_PIECES;
	rot = 0;
	px = COLS / 2 - 2;
	py = 0;
	next_piece = rand() % NUM_PIECES;

	update_next();
	redraw();

	/* TODO 1..5: bring the scene to life (see the header comment) */

	hal_run();
	return 0;
}
