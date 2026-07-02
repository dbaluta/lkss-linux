// SPDX-License-Identifier: GPL-2.0
/*
 * tetris - falling blocks for the LKSS hackathon
 *
 * Controls: SW1 = move left, SW2 = move right, SW3 = rotate
 *           (and restart after game over), SW4 = hard drop
 * Clear full rows to score (40/100/300/1200 points for 1/2/3/4 rows);
 * the pieces fall faster as you clear lines.
 * LED milestones: red at 5 lines, +green at 10, +blue at 15.
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
static int score, lines;
static bool running;

static lv_obj_t *cell_obj[ROWS][COLS];	/* object pool, one per cell */
static lv_obj_t *next_obj[4][4];	/* "next piece" preview */
static lv_obj_t *score_lbl, *lines_lbl, *msg_lbl;
static lv_timer_t *fall_timer;
static uint32_t fall_period;

/* Is box cell (cx, cy) of the given piece/rotation solid? */
static bool shape_cell(int piece, int r, int cx, int cy)
{
	return shapes[piece][r] >> (cy * 4 + cx) & 1;
}

/* Would the piece fit at (x, y) with rotation r? */
static bool piece_fits(int piece, int r, int x, int y)
{
	for (int cy = 0; cy < 4; cy++)
		for (int cx = 0; cx < 4; cx++) {
			if (!shape_cell(piece, r, cx, cy))
				continue;

			int bx = x + cx, by = y + cy;

			/* walls and floor (above the top is fine) */
			if (bx < 0 || bx >= COLS || by >= ROWS)
				return false;
			/* settled blocks */
			if (by >= 0 && board[by][bx])
				return false;
		}
	return true;
}

/* Apply a move/rotation if the result still fits */
static bool try_move(int dx, int dy, int drot)
{
	int r = (rot + drot) % 4;

	if (!piece_fits(cur, r, px + dx, py + dy))
		return false;
	px += dx;
	py += dy;
	rot = r;
	return true;
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

static void game_over(void)
{
	running = false;
	lv_label_set_text_fmt(msg_lbl,
			      "GAME\nOVER\nscore %d\nSW3 = restart", score);
}

/* Bring in the next piece; game over if it does not fit */
static void spawn_piece(void)
{
	cur = next_piece;
	next_piece = rand() % NUM_PIECES;
	rot = 0;
	px = COLS / 2 - 2;
	py = -1;			/* enters from above the top row */
	update_next();
	if (!piece_fits(cur, rot, px, py))
		game_over();
}

static void update_leds(void)
{
	uint32_t mask = 0;

	if (lines >= 5)
		mask |= 1 << HACKPAD_LED_RED;
	if (lines >= 10)
		mask |= 1 << HACKPAD_LED_GREEN;
	if (lines >= 15)
		mask |= 1 << HACKPAD_LED_BLUE;
	hal_leds(mask);
}

/* Remove full rows, update the score and speed up */
static void clear_lines(void)
{
	static const int points[] = { 0, 40, 100, 300, 1200 };
	int n = 0;

	for (int y = 0; y < ROWS; y++) {
		bool full = true;

		for (int x = 0; x < COLS; x++)
			if (!board[y][x])
				full = false;
		if (!full)
			continue;

		/* pull everything above one row down */
		memmove(board[1], board[0], y * sizeof(board[0]));
		memset(board[0], 0, sizeof(board[0]));
		n++;
	}

	if (!n)
		return;

	score += points[n];
	lines += n;
	lv_label_set_text_fmt(score_lbl, "%d", score);
	lv_label_set_text_fmt(lines_lbl, "%d", lines);
	update_leds();

	/* Speed up, but never below 120 ms per row */
	if (fall_period > 120) {
		fall_period -= 20 * n;
		if (fall_period < 120)
			fall_period = 120;
		lv_timer_set_period(fall_timer, fall_period);
	}
}

/* The piece can no longer fall: settle it into board[] */
static void lock_piece(void)
{
	for (int cy = 0; cy < 4; cy++)
		for (int cx = 0; cx < 4; cx++)
			if (shape_cell(cur, rot, cx, cy) && py + cy >= 0)
				board[py + cy][px + cx] = cur + 1;
	clear_lines();
	spawn_piece();
}

/* Settled blocks + the falling piece -> the cell object pool */
static void redraw(void)
{
	uint8_t view[ROWS][COLS];

	memcpy(view, board, sizeof(view));
	if (running)
		for (int cy = 0; cy < 4; cy++)
			for (int cx = 0; cx < 4; cx++)
				if (shape_cell(cur, rot, cx, cy) &&
				    py + cy >= 0)
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

static void new_game(void)
{
	memset(board, 0, sizeof(board));
	score = 0;
	lines = 0;
	fall_period = 500;
	lv_timer_set_period(fall_timer, fall_period);
	lv_label_set_text(score_lbl, "0");
	lv_label_set_text(lines_lbl, "0");
	lv_label_set_text(msg_lbl, "");
	next_piece = rand() % NUM_PIECES;
	running = true;
	spawn_piece();
	update_leds();
	redraw();
}

/* One gravity step: fall one row, or settle and bring the next piece */
static void fall_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	if (!running)
		return;
	if (!try_move(0, 1, 0))
		lock_piece();
	redraw();
}

/* Every 30 ms: poll the buttons */
static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	bool moved = false;

	if (!running) {
		if (hal_button_pressed(HACKPAD_BTN_SW3))
			new_game();
		return;
	}

	if (hal_button_pressed(HACKPAD_BTN_SW1))
		moved |= try_move(-1, 0, 0);
	if (hal_button_pressed(HACKPAD_BTN_SW2))
		moved |= try_move(1, 0, 0);
	if (hal_button_pressed(HACKPAD_BTN_SW3))
		moved |= try_move(0, 0, 1);
	if (hal_button_pressed(HACKPAD_BTN_SW4)) {
		while (try_move(0, 1, 0))
			;
		lock_piece();
		moved = true;
	}

	if (moved)
		redraw();
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

	msg_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(msg_lbl, lv_color_hex(0xFF4040), 0);
	lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_18, 0);
	lv_obj_align(msg_lbl, LV_ALIGN_CENTER, -(240 - PANEL_X) / 2, 0);

	fall_timer = lv_timer_create(fall_tick, 500, NULL);
	lv_timer_create(game_tick, 30, NULL);
	new_game();

	hal_run();
	return 0;
}
