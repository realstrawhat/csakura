#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _XOPEN_SOURCE 700
#endif

#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define VERSION "2.0.0"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_PETALS  768
#define MAX_SOURCES 4096
#define MAX_BLOBS   28

typedef struct {
    const char *glyph;
    short pair;
    bool bold;
} Cell;

typedef struct {
    double x, y;
    double vy;
    double phase, freq, amp;
    const char *glyph;
    short pair;
    double rest;
    bool active;
} Petal;

typedef struct { short x, y; } Coord;
typedef struct { double x, y, rx, ry; } Blob;

static Cell *grid = NULL;
static int gw = 0, gh = 0;

static Coord sources[MAX_SOURCES];
static int nsources = 0;

static Blob blobs[MAX_BLOBS];
static int nblobs = 0;

static Petal petals[MAX_PETALS];
static int npetals = 0;

static double wind = 0.0, wind_target = 0.0;

static volatile sig_atomic_t running = 1;

static int    opt_fps     = 20;
static int    opt_density = 5;
static double opt_wind    = 1.0;
static bool   opt_ascii   = false;
static int    opt_palette = 0;

static double frand(void) { return rand() / ((double)RAND_MAX + 1.0); }
static double frange(double a, double b) { return a + frand() * (b - a); }
static double clampd(double v, double a, double b)
{
    return v < a ? a : (v > b ? b : v);
}

static void on_signal(int sig) { (void)sig; running = 0; }

enum {
    C_P0 = 1,
    C_P1, C_P2, C_P3, C_P4, C_P5, C_P6,
    C_P7,
    C_TRUNK_D, C_TRUNK_M, C_TRUNK_L,
    C_GRASS, C_FADED,
};

typedef struct {
    const char *name;
    short ramp[8];
} Palette;

static const Palette PALETTES[] = {
    { "sakura",   { 225, 224, 218, 212, 211, 175, 168, 132 } },
    { "rose",     { 224, 218, 211, 204, 203, 161, 125,  88 } },
    { "blush",    { 225, 218, 217, 211, 210, 168, 131,  95 } },
    { "magenta",  { 219, 213, 207, 200, 163, 127,  90,  53 } },
    { "peach",    { 223, 216, 215, 209, 173, 130,  94,  58 } },
    { "coral",    { 217, 210, 209, 203, 167, 131,  88,  52 } },
    { "sunset",   { 223, 216, 209, 203, 167, 131,  88,  52 } },
    { "gold",     { 229, 222, 221, 179, 136,  94,  58,  52 } },
    { "lavender", { 189, 183, 147, 141, 140,  98,  61,  54 } },
    { "violet",   { 183, 177, 141, 135,  98,  91,  54,  53 } },
    { "sky",      { 153, 117, 111,  75,  68,  31,  25,  24 } },
    { "mint",     { 158, 122, 115,  79,  72,  35,  29,  22 } },
    { "matcha",   { 193, 150, 149, 107,  70,  64,  28,  22 } },
    { "white",    { 255, 225, 224, 218, 211, 175, 168, 132 } },
    { "ink",      { 255, 252, 251, 248, 245, 242, 239, 236 } },
};
#define NPALETTES ((int)(sizeof(PALETTES) / sizeof(PALETTES[0])))

static int find_palette(const char *name)
{
    for (int i = 0; i < NPALETTES; i++)
        if (strcmp(PALETTES[i].name, name) == 0)
            return i;
    return -1;
}

static void apply_palette(void)
{
    if (!has_colors())
        return;

    const Palette *p = &PALETTES[opt_palette];

    if (COLORS >= 256) {
        for (int i = 0; i < 8; i++)
            init_pair((short)(C_P0 + i), p->ramp[i], -1);
        init_pair(C_TRUNK_D, 52,  -1);
        init_pair(C_TRUNK_M, 94,  -1);
        init_pair(C_TRUNK_L, 137, -1);
        init_pair(C_GRASS, 108, -1);
        init_pair(C_FADED, p->ramp[5], -1);
    } else {

        short base = COLOR_MAGENTA;
        if (opt_palette == 7)  base = COLOR_YELLOW;
        if (opt_palette == 10) base = COLOR_CYAN;
        if (opt_palette == 11 || opt_palette == 12) base = COLOR_GREEN;
        if (opt_palette == 14) base = COLOR_WHITE;

        init_pair(C_P0, COLOR_WHITE, -1);
        init_pair(C_P1, COLOR_WHITE, -1);
        for (int i = 2; i < 6; i++)
            init_pair((short)(C_P0 + i), base, -1);
        init_pair(C_P6, COLOR_RED, -1);
        init_pair(C_P7, COLOR_RED, -1);
        init_pair(C_TRUNK_D, COLOR_YELLOW, -1);
        init_pair(C_TRUNK_M, COLOR_YELLOW, -1);
        init_pair(C_TRUNK_L, COLOR_YELLOW, -1);
        init_pair(C_GRASS, COLOR_GREEN, -1);
        init_pair(C_FADED, base, -1);
    }
}

static void init_colors(void)
{
    start_color();
    use_default_colors();
    apply_palette();
}

static const char *G_FULL   = "\u2588";
static const char *G_DARK   = "\u2593";
static const char *G_MED    = "\u2592";
static const char *G_DOT    = "\u00B7";

static const char *bloom_uni[] = { "\u2740", "\u273F", "\u2741", "\u273D" };
static const char *bloom_asc[] = { "&", "%", "@", "*" };

static const char *petal_uni[] = { "\u2740", "\u273F", "*", "\u00B7", "\u2218" };
static const char *petal_asc[] = { "*", "*", "o", ".", "'" };

static const char *g_full(void) { return opt_ascii ? "@" : G_FULL; }
static const char *g_dark(void) { return opt_ascii ? "%" : G_DARK; }
static const char *g_med(void)  { return opt_ascii ? ":" : G_MED;  }

static void put(int x, int y, const char *glyph, short pair, bool bold)
{
    if (x < 0 || y < 0 || x >= gw || y >= gh)
        return;
    Cell *c = &grid[(size_t)y * gw + x];
    c->glyph = glyph;
    c->pair  = pair;
    c->bold  = bold;
}

static double field(double x, double y)
{
    double s = 0.0;
    for (int i = 0; i < nblobs; i++) {
        double dx = (x - blobs[i].x) / blobs[i].rx;
        double dy = (y - blobs[i].y) / blobs[i].ry;
        s += exp(-(dx * dx + dy * dy) * 2.2);
    }
    return s;
}

static void add_source(int x, int y)
{
    if (nsources < MAX_SOURCES) {
        sources[nsources].x = (short)x;
        sources[nsources].y = (short)y;
        nsources++;
    }
}

static void gen_canopy(double cx, double cy, double rx, double ry)
{
    const char **blooms = opt_ascii ? bloom_asc : bloom_uni;

    double bx0 = 1e9, bx1 = -1e9, by0 = 1e9, by1 = -1e9;
    for (int i = 0; i < nblobs; i++) {
        if (blobs[i].x - blobs[i].rx < bx0) bx0 = blobs[i].x - blobs[i].rx;
        if (blobs[i].x + blobs[i].rx > bx1) bx1 = blobs[i].x + blobs[i].rx;
        if (blobs[i].y - blobs[i].ry < by0) by0 = blobs[i].y - blobs[i].ry;
        if (blobs[i].y + blobs[i].ry > by1) by1 = blobs[i].y + blobs[i].ry;
    }
    int x0 = (int)(bx0 - 3), x1 = (int)(bx1 + 3);
    int y0 = (int)(by0 - 2), y1 = (int)(by1 + 3);

    cy = (by0 + by1) / 2.0;
    ry = fmax((by1 - by0) / 2.0, 2.0);
    (void)cx; (void)rx;

    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= gh) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= gw) continue;

            double f = field(x, y);
            if (f < 0.30)
                continue;

            if (f < 0.42 && frand() < 0.35)
                continue;

            double h = clampd((y - (cy - ry)) / (2.0 * ry), 0.0, 1.0);

            if (h > 0.62 && f < 0.85) {
                double openness = (h - 0.62) * 1.3;
                if (frand() < openness)
                    continue;
            }

            double shade = h * 6.0 + frange(-0.9, 0.9);

            double fu = field(x, y - 1.6);
            if (fu > f * 1.12) shade += 1.7;
            else if (fu < f * 0.88) shade -= 1.5;

            const char *g;
            bool bold = false;
            if (f > 0.92)
                g = frand() < 0.80 ? g_full() : g_dark();
            else if (f > 0.55)
                g = frand() < 0.60 ? g_dark() : g_med();
            else {
                double r = frand();
                if (r < 0.45)      g = g_med();
                else if (r < 0.85) g = blooms[rand() % 4];
                else               g = opt_ascii ? "." : G_DOT;
                bold = frand() < 0.3;
            }

            if (f > 0.55 && frand() < 0.07) {
                g = blooms[rand() % 4];
                bold = true;
                shade -= 2.0;
            }

            int idx = (int)clampd(shade, 0.0, 7.0);
            put(x, y, g, (short)(C_P0 + idx), bold);

            if ((f < 0.60 || fu > f * 1.12) && frand() < 0.5)
                add_source(x, y);
        }
    }
}

static void gen_trunk(double bx, double tx, double ty)
{
    double base_y = gh - 2.0;
    double h = base_y - ty;
    if (h < 2.0) h = 2.0;

    double maxw = clampd(gw * 0.028, 2.0, 5.0);
    double bend = frange(-1.0, 1.0) * clampd(gw * 0.02, 1.0, 4.0);

    int steps = (int)(h * 2.0) + 2;
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        double y = base_y - t * h;
        double x = bx + (tx - bx) * t + sin(t * M_PI) * bend;

        double w = maxw * pow(1.0 - t, 1.10) * (1.0 + 1.1 * exp(-t * 10.0)) + 0.6;

        for (int dx = (int)-w; dx <= (int)w; dx++) {
            short pair;
            if (dx < -w * 0.35)     pair = C_TRUNK_D;
            else if (dx > w * 0.45) pair = C_TRUNK_L;
            else                    pair = C_TRUNK_M;
            put((int)(x + dx), (int)y, g_full(), pair, false);
        }
    }
}

static Coord tips[64];
static int ntips = 0;
static double br_xmin, br_xmax, br_ymin;

static void gen_branch(double x, double y, double angle, double len, int depth)
{
    double t = 0.0;

    while (t < len) {
        double dx = cos(angle), dy = sin(angle);
        x += dx * 1.7;
        y -= dy * 0.85;
        t += 1.0;
        angle += frange(-0.10, 0.10);
        angle = clampd(angle, 0.15, M_PI - 0.15);

        if (x < br_xmin) { x = br_xmin; angle = M_PI - angle; }
        if (x > br_xmax) { x = br_xmax; angle = M_PI - angle; }
        if (y < br_ymin) {
            y = br_ymin;
            angle = angle > M_PI / 2.0 ? M_PI - 0.20 : 0.20;
        }

        put((int)x, (int)y, g_full(), depth == 0 ? C_TRUNK_M : C_TRUNK_L, false);
        if (depth == 0) {
            put((int)x + 1, (int)y, g_full(), C_TRUNK_D, false);
        } else if (frand() < 0.35) {
            put((int)x + (frand() < 0.5 ? -1 : 1), (int)y, g_full(), C_TRUNK_M, false);
        }
    }

    if (depth >= 2 || len < 3.0) {
        if (ntips < 64) {
            tips[ntips].x = (short)x;
            tips[ntips].y = (short)y;
            ntips++;
        }
        return;
    }

    int kids = 2 + (frand() < 0.5 ? 1 : 0);
    for (int i = 0; i < kids; i++) {
        double spread = frange(0.40, 0.80);
        double na = i == 0 ? angle + spread
                  : i == 1 ? angle - spread
                  : angle + frange(-0.25, 0.25);
        gen_branch(x, y, na, len * frange(0.55, 0.75), depth + 1);
    }
}

static void gen_ground(double cx, double rx)
{
    const char **blooms = opt_ascii ? bloom_asc : bloom_uni;
    int y = gh - 1;

    for (int x = 0; x < gw; x++) {
        double dx = (x - cx) / (rx * 1.25);
        double p = exp(-dx * dx * 2.2);
        double r = frand();

        if (r < p * 0.50) {
            double rr = frand();
            const char *g = rr < 0.25 ? blooms[rand() % 4]
                          : rr < 0.60 ? (opt_ascii ? "." : G_DOT)
                          : ",";
            put(x, y, g, (short)(C_P3 + rand() % 4), false);
        } else if (r < p * 0.50 + 0.10) {
            put(x, y, "\"", C_GRASS, false);
        } else if (r < p * 0.50 + 0.16) {
            put(x, y, ",", C_GRASS, false);
        } else {
            put(x, y, "_", C_GRASS, false);
        }

        if (gh > 3 && frand() < p * 0.12)
            put(x, y - 1, opt_ascii ? "." : G_DOT, C_FADED, false);
    }
}

static void gen_tree(void)
{
    memset(grid, 0, sizeof(Cell) * (size_t)gw * gh);
    nsources = 0;
    nblobs = 0;
    ntips = 0;

    double rx = clampd(gw * 0.26, 6.0, 36.0);
    double ry = clampd(gh * 0.26, 3.0, rx * 0.55);
    double cx = gw * 0.5 + frange(-1.5, 1.5);
    double cy = ry + 2.5;

    double bx = cx + frange(-2.0, 2.0);
    double tx = cx + frange(-2.0, 2.0);
    double ty = cy + ry * 1.15;
    if (ty > gh - 4.0) ty = gh - 4.0;

    br_xmin = cx - rx * 0.80;
    br_xmax = cx + rx * 0.80;
    br_ymin = fmax(1.0, cy - ry * 0.55);

    gen_ground(cx, rx);
    gen_trunk(bx, tx, ty);

    int limbs = 3 + rand() % 2;
    double reach = (ty - br_ymin) * 0.60 + 2.0;
    for (int i = 0; i < limbs; i++) {
        double na = M_PI / 2.0
                  + (i - (limbs - 1) / 2.0) * frange(0.55, 0.75)
                  + frange(-0.15, 0.15);
        gen_branch(tx + frange(-1.0, 1.0), ty + frange(0.0, 1.5),
                   na, reach * frange(0.75, 1.0), 0);
    }

    blobs[nblobs++] = (Blob){ cx, cy - ry * 0.10, rx * 0.50, ry * 0.50 };
    for (int i = 0; i < ntips && nblobs < MAX_BLOBS - 4; i++) {
        blobs[nblobs++] = (Blob){
            tips[i].x + frange(-1.5, 1.5),
            tips[i].y - frange(0.0, 1.5),
            rx * frange(0.18, 0.30),
            ry * frange(0.24, 0.38),
        };
    }

    for (int i = 0; i < 5 && nblobs < MAX_BLOBS; i++) {
        blobs[nblobs++] = (Blob){
            cx + frange(-0.70, 0.70) * rx,
            cy + frange(0.25, 0.60) * ry,
            rx * frange(0.18, 0.28),
            ry * frange(0.24, 0.34),
        };
    }

    gen_canopy(cx, cy, rx, ry);
}

static void spawn_petal(Petal *p, bool scatter)
{
    const char **glyphs = opt_ascii ? petal_asc : petal_uni;

    p->active = true;
    p->rest   = -1.0;

    if (nsources > 0 && frand() < 0.85) {
        Coord c = sources[rand() % nsources];
        p->x = c.x + frange(-1.0, 1.0);
        p->y = c.y + frange(0.0, 1.0);
    } else {
        p->x = frand() * gw;
        p->y = -frange(0.0, 3.0);
    }
    if (scatter)
        p->y = frange(0.0, gh - 2.0);

    p->vy    = frange(0.10, 0.28);
    p->amp   = frange(0.10, 0.45);
    p->freq  = frange(0.05, 0.18);
    p->phase = frand() * 2.0 * M_PI;
    p->glyph = glyphs[rand() % 5];
    p->pair  = (short)(C_P1 + rand() % 5);
}

static void reset_petals(bool scatter)
{
    npetals = gw * opt_density / 4;
    if (npetals < 16)          npetals = 16;
    if (npetals > MAX_PETALS)  npetals = MAX_PETALS;

    for (int i = 0; i < MAX_PETALS; i++)
        petals[i].active = false;
    for (int i = 0; i < npetals; i++)
        if (frand() < 0.6)
            spawn_petal(&petals[i], scatter);
}

static void update_petals(double dt)
{

    if (frand() < 0.008)
        wind_target = frange(-0.12, 0.45) * opt_wind;
    wind += (wind_target - wind) * 0.02;

    for (int i = 0; i < npetals; i++) {
        Petal *p = &petals[i];

        if (!p->active) {
            if (frand() < 0.03)
                spawn_petal(p, false);
            continue;
        }

        if (p->rest >= 0.0) {
            p->rest -= dt;
            if (p->rest < 0.0)
                p->active = false;
            continue;
        }

        p->phase += p->freq;
        p->x += wind + p->amp * sin(p->phase);
        p->y += p->vy;

        if (p->x < -2.0)          p->x = gw + 1.0;
        else if (p->x > gw + 2.0) p->x = -1.0;

        if (p->y >= gh - 1.0) {
            p->y    = gh - 1.0;
            p->rest = frange(2.0, 7.0);
            p->pair = C_FADED;
            p->glyph = opt_ascii ? "." : G_DOT;
        }
    }
}

static void draw(void)
{
    erase();

    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            const Cell *c = &grid[(size_t)y * gw + x];
            if (!c->glyph)
                continue;
            attr_t a = COLOR_PAIR(c->pair) | (c->bold ? A_BOLD : 0);
            attron(a);
            mvaddstr(y, x, c->glyph);
            attroff(a);
        }
    }

    for (int i = 0; i < npetals; i++) {
        const Petal *p = &petals[i];
        if (!p->active)
            continue;
        int px = (int)p->x, py = (int)p->y;
        if (px < 0 || px >= gw || py < 0 || py >= gh)
            continue;
        attr_t a = COLOR_PAIR(p->pair);
        attron(a);
        mvaddstr(py, px, p->glyph);
        attroff(a);
    }

    refresh();
}

static void resize_grid(void)
{
    gw = COLS;
    gh = LINES;
    free(grid);
    grid = calloc((size_t)gw * gh, sizeof(Cell));
    if (!grid) {
        endwin();
        fprintf(stderr, "csakura: out of memory\n");
        exit(1);
    }
}

static void usage(FILE *out)
{
    fprintf(out,
        "usage: csakura [options]\n"
        "\n"
        "a sakura tree with falling petals for your terminal\n"
        "\n"
        "options:\n"
        "  -f FPS    frames per second, 5-60 (default: 20)\n"
        "  -p NUM    petal density, 1-10 (default: 5)\n"
        "  -w NUM    wind strength, 0-10 (default: 1)\n"
        "  -c NAME   blossom palette (default: sakura)\n"
        "  -a        ASCII glyphs only (no unicode blossoms)\n"
        "  -h        show this help\n"
        "  -v        show version\n"
        "\n"
        "palettes:\n"
        "  ");
    for (int i = 0; i < NPALETTES; i++) {
        fprintf(out, "%s%s", PALETTES[i].name, i + 1 < NPALETTES ? ", " : "\n");
        if ((i + 1) % 5 == 0 && i + 1 < NPALETTES)
            fprintf(out, "\n  ");
    }
    fprintf(out,
        "\n"
        "keys:\n"
        "  q / Esc   quit\n"
        "  r         regrow the tree\n"
        "  c         next color palette\n"
        "  C         previous color palette\n");
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "f:p:w:c:ahv")) != -1) {
        switch (opt) {
        case 'f':
            opt_fps = atoi(optarg);
            if (opt_fps < 5)  opt_fps = 5;
            if (opt_fps > 60) opt_fps = 60;
            break;
        case 'p':
            opt_density = atoi(optarg);
            if (opt_density < 1)  opt_density = 1;
            if (opt_density > 10) opt_density = 10;
            break;
        case 'w':
            opt_wind = atof(optarg);
            if (opt_wind < 0.0)  opt_wind = 0.0;
            if (opt_wind > 10.0) opt_wind = 10.0;
            break;
        case 'c': {
            int idx = find_palette(optarg);
            if (idx < 0) {
                fprintf(stderr, "csakura: unknown palette '%s'\n", optarg);
                fprintf(stderr, "try: ");
                for (int i = 0; i < NPALETTES; i++)
                    fprintf(stderr, "%s%s", PALETTES[i].name,
                            i + 1 < NPALETTES ? ", " : "\n");
                return 1;
            }
            opt_palette = idx;
            break;
        }
        case 'a':
            opt_ascii = true;
            break;
        case 'v':
            printf("csakura %s\n", VERSION);
            return 0;
        case 'h':
            usage(stdout);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    setlocale(LC_ALL, "");
    srand((unsigned)(time(NULL) ^ getpid()));

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    if (has_colors())
        init_colors();

    resize_grid();
    gen_tree();
    reset_petals(true);

    const double dt = 1.0 / opt_fps;

    while (running) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27)
            break;
        if (ch == 'r' || ch == 'R') {
            gen_tree();
            reset_petals(false);
        }
        if (ch == 'c') {
            opt_palette = (opt_palette + 1) % NPALETTES;
            apply_palette();
            gen_tree();
            reset_petals(false);
        }
        if (ch == 'C') {
            opt_palette = (opt_palette + NPALETTES - 1) % NPALETTES;
            apply_palette();
            gen_tree();
            reset_petals(false);
        }
        if (ch == KEY_RESIZE) {
            resize_grid();
            gen_tree();
            reset_petals(true);
        }

        update_petals(dt);
        draw();
        napms(1000 / opt_fps);
    }

    endwin();
    free(grid);
    return 0;
}
