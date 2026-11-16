/* The font has every character the firmware shows, the name fits the 240-pixel screen, and scaling is whole pixels.
 * tests/run_host_tests.sh builds and runs it; it also prints the text, to look at. */
#include <stdio.h>
#include <string.h>
#include "app_id.h"

#define GRID_W 320
#define GRID_H 24
static char grid[GRID_H][GRID_W];
static int outside;
static void grid_rect(unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) {
            if (i >= GRID_W || j >= GRID_H)
                outside = 1;
            else
                grid[j][i] = 1;
        }
}
#define FONT_RECT(x, y, w, h, colour) ((void)(colour), grid_rect(x, y, w, h))
#include "font.h"

static int lit(void)
{
    int n = 0, i, j;
    for (j = 0; j < GRID_H; j++)
        for (i = 0; i < GRID_W; i++)
            n += grid[j][i];
    return n;
}

static int failed;
static void check(const char *what, int ok)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        failed = 1;
    }
}

int main(void)
{
    const char *shown = APP_NAME " " APP_VERSION " 0123456789", *c;      /* and the load's digits */
    int at1, at3, j, i;
    for (c = shown; *c; c++)
        if (!font_rows(*c)) {
            printf("FAIL: no glyph for '%c'\n", *c);
            failed = 1;
        }
    check("an unknown character has no glyph", font_rows('~') == 0);
    check("the title at scale 2 fits the screen", font_text_width(APP_NAME " " APP_VERSION, 2) <= 240);
    check("width: 12 characters at scale 3", font_text_width("SINT_FM1_DR1", 3) == 213);
    check("width: nothing", font_text_width("", 3) == 0);

    font_draw_text(0, 0, 1, shown, 1);
    at1 = lit();
    for (j = 0; j < 7; j++) {
        for (i = 0; i < (int)font_text_width(shown, 1); i++)
            putchar(grid[j][i] ? '#' : ' ');
        putchar('\n');
    }
    check("scale 1 stays in 7 rows", !outside && at1 > 0);
    memset(grid, 0, sizeof grid);
    font_draw_text(0, 0, 3, APP_NAME, 1);
    at3 = lit();
    memset(grid, 0, sizeof grid);
    font_draw_text(0, 0, 1, APP_NAME, 1);
    check("scale 3 lights 9 times the pixels", at3 == 9 * lit() && !outside);
    memset(grid, 0, sizeof grid);
    font_draw_text(0, 0, 1, "~", 1);
    check("an unknown character draws a full block", lit() == 35);
    puts(failed ? "font_test: FAILED" : "font_test: ok");
    return failed;
}
