#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_XOPEN_SOURCE)
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
#define MAX_BLOBS   32

/*
 * A terminal cell is roughly twice as tall as it is wide. The tree is laid out
 * in *square* units and divided back down, so the crown keeps one silhouette
 * whatever the terminal's rows-to-columns ratio happens to be.
 */
#define CELL_AR 2.0

enum { K_NONE = 0, K_WOOD, K_CANOPY, K_GROUND };

/*
 * Foreground tones. The eight ramp steps run light (0) to deep (7); a cell
 * picks one by how much light reaches it.
 */
enum {
    F_P0 = 0, F_P1, F_P2, F_P3, F_P4, F_P5, F_P6, F_P7,
    F_TRUNK_D, F_TRUNK_M, F_TRUNK_L,
    F_GRASS, F_GRASS_M, F_GRASS_D,
    F_FADED,
    NFG
};

/*
 * Background tones. Canopy cells sit on a haze — a dark tint of the blossom
 * colour — so that partial block glyphs (░▒▓) blend the ramp step toward the
 * mass instead of toward the bare terminal. Cells deep inside the crown sit on
 * a ramp step directly, so a blossom drawn there stays flush with its
 * neighbours rather than punching a hole in the mass.
 */
enum {
    B_DEF = 0, B_HAZE,
    B_P0, B_P1, B_P2, B_P3, B_P4, B_P5, B_P6, B_P7,
    NBG
};

#define NPAIRS (1 + NFG * NBG)

typedef struct {
    const char   *glyph;
    unsigned char fg, bg, kind;
    bool          bold;
} Cell;

typedef struct {
    double x, y;
    double vy;
    double phase, freq, amp;
    const char *glyph;
    unsigned char fg;
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

static Coord tips[64];
static int ntips = 0;
static double br_xmin, br_xmax, br_ymin;
static double br_w = 1.45;   /* limb half-width at the fork, in cells */

static Petal petals[MAX_PETALS];
static int npetals = 0;

static double wind = 0.0, wind_target = 0.0;

static volatile sig_atomic_t running = 1;

static int    opt_fps     = 20;
static int    opt_density = 5;
static double opt_wind    = 1.0;
static bool   opt_ascii   = false;
static bool   opt_flat    = false;   /* keep the terminal background visible */
static int    opt_palette = 0;

/* Whether cell backgrounds are actually painted (needs 256 colours). */
static bool shaded = false;

static double frand(void) { return rand() / ((double)RAND_MAX + 1.0); }
static double frange(double a, double b) { return a + frand() * (b - a); }
static double clampd(double v, double a, double b)
{
    return v < a ? a : (v > b ? b : v);
}

static void on_signal(int sig) { (void)sig; running = 0; }

/*
 * With backgrounds off there is only one background tone, so the pair table
 * collapses to one entry per foreground and stays inside the 64-pair limit of
 * a plain 8-colour terminal.
 */
static short pair_of(int fg, int bg)
{
    return shaded ? (short)(1 + fg * NBG + bg) : (short)(1 + fg);
}

typedef struct {
    const char *name;
    short ramp[8];
    short haze;     /* canopy shadow: a dark tone the ramp can sink into */
} Palette;

static const Palette PALETTES[] = {
    { "sakura",   { 225, 224, 218, 212, 211, 175, 168, 132 }, 235 },
    { "rose",     { 224, 218, 211, 204, 203, 161, 125,  88 }, 235 },
    { "blush",    { 225, 218, 217, 211, 210, 168, 131,  95 }, 236 },
    { "magenta",  { 219, 213, 207, 200, 163, 127,  90,  53 }, 234 },
    { "peach",    { 223, 216, 215, 209, 173, 130,  94,  58 }, 235 },
    { "coral",    { 217, 210, 209, 203, 167, 131,  88,  52 }, 235 },
    { "sunset",   { 223, 216, 209, 203, 167, 131,  88,  52 }, 235 },
    { "gold",     { 229, 222, 221, 179, 136,  94,  58,  52 }, 235 },
    { "lavender", { 189, 183, 147, 141, 140,  98,  61,  54 }, 234 },
    { "violet",   { 183, 177, 141, 135,  98,  91,  54,  53 }, 234 },
    { "sky",      { 153, 117, 111,  75,  68,  31,  25,  24 }, 234 },
    { "mint",     { 158, 122, 115,  79,  72,  35,  29,  22 }, 234 },
    { "matcha",   { 193, 150, 149, 107,  70,  64,  28,  22 }, 234 },
    { "white",    { 255, 225, 224, 218, 211, 175, 168, 132 }, 236 },
    { "ink",      { 255, 252, 251, 248, 245, 242, 239, 236 }, 234 },
};
#define NPALETTES ((int)(sizeof(PALETTES) / sizeof(PALETTES[0])))

static int find_palette(const char *name)
{
    for (int i = 0; i < NPALETTES; i++)
        if (strcmp(PALETTES[i].name, name) == 0)
            return i;
    return -1;
}

static void update_shaded(void)
{
    shaded = !opt_flat && has_colors() && COLORS >= 256 && COLOR_PAIRS >= NPAIRS;
}

static void apply_palette(void)
{
    if (!has_colors())
        return;

    const Palette *p = &PALETTES[opt_palette];
    short fg[NFG], bg[NBG];

    if (COLORS >= 256) {
        for (int i = 0; i < 8; i++)
            fg[F_P0 + i] = p->ramp[i];
        fg[F_TRUNK_D] = 52;  fg[F_TRUNK_M] = 94;  fg[F_TRUNK_L] = 137;
        fg[F_GRASS]   = 108; fg[F_GRASS_M] = 65;  fg[F_GRASS_D] = 238;
        fg[F_FADED]   = p->ramp[5];

        bg[B_DEF]  = -1;
        bg[B_HAZE] = p->haze;
        for (int i = 0; i < 8; i++)
            bg[B_P0 + i] = p->ramp[i];
    } else {
        short base = COLOR_MAGENTA;
        if (opt_palette == 7)  base = COLOR_YELLOW;
        if (opt_palette == 10) base = COLOR_CYAN;
        if (opt_palette == 11 || opt_palette == 12) base = COLOR_GREEN;
        if (opt_palette == 14) base = COLOR_WHITE;

        fg[F_P0] = COLOR_WHITE;
        fg[F_P1] = COLOR_WHITE;
        for (int i = 2; i < 6; i++)
            fg[F_P0 + i] = base;
        fg[F_P6] = COLOR_RED;
        fg[F_P7] = COLOR_RED;
        fg[F_TRUNK_D] = COLOR_YELLOW;
        fg[F_TRUNK_M] = COLOR_YELLOW;
        fg[F_TRUNK_L] = COLOR_YELLOW;
        fg[F_GRASS]   = COLOR_GREEN;
        fg[F_GRASS_M] = COLOR_GREEN;
        fg[F_GRASS_D] = COLOR_GREEN;
        fg[F_FADED]   = base;

        for (int i = 0; i < NBG; i++)
            bg[i] = -1;
    }

    if (shaded) {
        for (int f = 0; f < NFG; f++)
            for (int b = 0; b < NBG; b++)
                init_pair((short)(1 + f * NBG + b), fg[f], bg[b]);
    } else {
        for (int f = 0; f < NFG; f++)
            init_pair((short)(1 + f), fg[f], -1);
    }
}

static void init_colors(void)
{
    start_color();
    use_default_colors();
    update_shaded();
    apply_palette();
}

static const char *G_FULL = "█";
static const char *G_DARK = "▓";
static const char *G_MED  = "▒";
static const char *G_LITE = "░";
static const char *G_DOT  = "·";

static const char *bloom_uni[] = { "❀", "✿", "❁", "✽" };
static const char *bloom_asc[] = { "&", "%", "@", "*" };

static const char *petal_uni[] = { "❀", "✿", "*", "·", "∘" };
static const char *petal_asc[] = { "*", "*", "o", ".", "'" };

static const char *g_full(void) { return opt_ascii ? "@" : G_FULL; }
static const char *g_dark(void) { return opt_ascii ? "%" : G_DARK; }
static const char *g_med(void)  { return opt_ascii ? ":" : G_MED;  }
static const char *g_lite(void) { return opt_ascii ? "." : G_LITE; }
static const char *g_dot(void)  { return opt_ascii ? "." : G_DOT;  }

/* ---------- generation ---------- */

static unsigned char cur_kind = K_NONE;

static void put_bg(int x, int y, const char *glyph, int fg, int bg, bool bold)
{
    if (x < 0 || y < 0 || x >= gw || y >= gh)
        return;
    Cell *c = &grid[(size_t)y * gw + x];
    c->glyph = glyph;
    c->fg    = (unsigned char)fg;
    c->bg    = (unsigned char)bg;
    c->bold  = bold;
    c->kind  = cur_kind;
}

static void put(int x, int y, const char *glyph, int fg, bool bold)
{
    put_bg(x, y, glyph, fg, B_DEF, bold);
}

static unsigned char kind_at(int x, int y)
{
    if (x < 0 || y < 0 || x >= gw || y >= gh)
        return K_NONE;
    return grid[(size_t)y * gw + x].kind;
}

/*
 * Falloff sits between a tight 2.2 and a soft 1.25. Too tight and the coverage
 * ramp is crossed inside a single row, giving a hard horizontal cut across the
 * crown; too soft and the per-tip blobs smear together into one smooth dome
 * instead of reading as separate clumps of blossom.
 */
static double field(double x, double y)
{
    double s = 0.0;
    for (int i = 0; i < nblobs; i++) {
        double dx = (x - blobs[i].x) / blobs[i].rx;
        double dy = (y - blobs[i].y) / blobs[i].ry;
        s += exp(-(dx * dx + dy * dy) * 1.70);
    }
    return s;
}

static double hash01(int i)
{
    unsigned h = (unsigned)i * 2654435761u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (double)(h & 0xffff) / 65535.0;
}

static double hash2(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (double)((h ^ (h >> 16)) & 0xffff) / 65535.0;
}

/* Smooth value noise in [0,1]; inputs are already divided by the feature size. */
static double vnoise(double x, double y)
{
    int xi = (int)floor(x), yi = (int)floor(y);
    double tx = x - xi, ty = y - yi;
    tx = tx * tx * (3.0 - 2.0 * tx);
    ty = ty * ty * (3.0 - 2.0 * ty);
    double a = hash2(xi, yi),     b = hash2(xi + 1, yi);
    double c = hash2(xi, yi + 1), d = hash2(xi + 1, yi + 1);
    return (a + (b - a) * tx) * (1.0 - ty) + (c + (d - c) * tx) * ty;
}

/*
 * Per-column wobble in [-0.5, 0.5], one control point every `period` cells.
 * Sampling the field at y + wobble breaks the canopy's contour: the crown is
 * only a dozen or so rows tall, so an unperturbed outline flattens into a
 * straight cut across the top.
 */
static double wobble(int x, double period)
{
    double p = x / period;
    int c = (int)floor(p);
    double t = p - c;
    double a = hash01(c) - 0.5, b = hash01(c + 1) - 0.5;
    t = t * t * (3.0 - 2.0 * t);
    return a + (b - a) * t;
}

static void add_source(int x, int y)
{
    if (nsources < MAX_SOURCES) {
        sources[nsources].x = (short)x;
        sources[nsources].y = (short)y;
        nsources++;
    }
}

static void gen_canopy(double cx, double rx, double ry_design)
{
    const char **blooms = opt_ascii ? bloom_asc : bloom_uni;

    double bx0 = 1e9, bx1 = -1e9, by0 = 1e9, by1 = -1e9;
    for (int i = 0; i < nblobs; i++) {
        if (blobs[i].x - blobs[i].rx < bx0) bx0 = blobs[i].x - blobs[i].rx;
        if (blobs[i].x + blobs[i].rx > bx1) bx1 = blobs[i].x + blobs[i].rx;
        if (blobs[i].y - blobs[i].ry < by0) by0 = blobs[i].y - blobs[i].ry;
        if (blobs[i].y + blobs[i].ry > by1) by1 = blobs[i].y + blobs[i].ry;
    }
    if (by0 < 0.0)
        by0 = 0.0;
    int x0 = (int)(bx0 - 3), x1 = (int)(bx1 + 3);
    int y0 = (int)(by0 - 1), y1 = (int)(by1 + 3);
    if (y0 < 0)
        y0 = 0;

    double cy = (by0 + by1) / 2.0;
    double ry = fmax((by1 - by0) / 2.0, 2.0);

    /* Feature sizes tied to the tree, so texture scales with the terminal. */
    double wob_amp = fmax(ry_design * 0.18, 0.8);
    double wob_per = fmax(rx * 0.11, 2.0);
    double clump_x = fmax(rx * 0.22, 2.0);
    double clump_y = fmax(ry_design * 0.26, 1.2);

    cur_kind = K_CANOPY;

    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= gh) continue;
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || x >= gw) continue;

            double wob = wobble(x, wob_per) * wob_amp;
            double f = field(x, y + wob);
            if (f < 0.20)
                continue;

            /* Coverage: 1 deep inside the mass, falling to 0 at the rim. */
            double cov0 = clampd((f - 0.20) / 0.60, 0.0, 1.0);
            cov0 = cov0 * cov0 * (3.0 - 2.0 * cov0);
            double cov = cov0;

            /*
             * Break the mass into blossom clumps. Without this the crown is a
             * single smooth blob — a generic tree. Two octaves, stretched
             * horizontally, give the layered clusters a cherry reads as.
             */
            double cl = 0.62 * vnoise(x / clump_x, y / clump_y)
                      + 0.38 * vnoise(x / (clump_x * 0.45), y / (clump_y * 0.5));
            cov = clampd(cov * (0.50 + 1.00 * cl), 0.0, 1.0);

            /*
             * Ragged silhouette, and gaps where the clumps thin out. Gaps deep
             * inside the crown fall back to a shadow tone — dropping straight
             * to the terminal background punches visible holes in the mass.
             */
            if (cov < 0.55 && frand() > cov * 1.5) {
                if (cov0 > 0.60 && kind_at(x, y) != K_WOOD)
                    put_bg(x, y, g_lite(), F_P6, B_HAZE, false);
                continue;
            }

            double h = clampd((y - (cy - ry)) / (2.0 * ry), 0.0, 1.0);

            /* Thin the underside so the canopy trails off into wisps. */
            if (h > 0.72) {
                double open = (h - 0.72) * 2.2 * (1.3 - cov);
                if (frand() < open)
                    continue;
            }

            /*
             * Let limbs show through between clusters — dark branches
             * threading the blossom is most of the character. Driven by the
             * noise field, not a coin flip, so a limb emerges as a continuous
             * run rather than a scatter of disconnected dashes. The exposed
             * wood still takes the canopy's shadow tone, so it reads as being
             * inside the crown rather than a gap punched through it.
             */
            if ((cov < 0.50 || cl < 0.30) && kind_at(x, y) == K_WOOD) {
                grid[(size_t)y * gw + x].bg = B_HAZE;
                continue;
            }

            /*
             * Light falls from the upper left; the mass shades what's below.
             * Thin rim cells take a deep, saturated tone on purpose: a pale
             * tint blended toward a dark background turns grey, not pink.
             */
            double shade = 1.1
                         + h * 5.2
                         + ((x - cx) / fmax(rx, 1.0)) * 0.70
                         + (1.0 - cov) * 2.0
                         + (0.5 - cl) * 2.8       /* clumps read as clusters  */
                         + frange(-1.05, 1.05);   /* dither across ramp steps */

            double fu = field(x, y + wob - 1.6);
            if (fu > f * 1.10)      shade += 1.6;
            else if (fu < f * 0.85) shade -= 1.4;

            int idx = (int)clampd(shade, 0.0, 7.0);

            /*
             * Solid well before the rim, so the mass stays saturated and only
             * the outer fringe dissolves. The partial blocks are doing real
             * work here: ▓ over the haze is three-quarters of the ramp step,
             * ░ is a quarter, which is how the crown fades out smoothly with
             * only eight colours to spend.
             */
            const char *g = cov > 0.62 ? g_full()
                          : cov > 0.42 ? g_dark()
                          : cov > 0.24 ? g_med()
                          : g_lite();
            int fg = F_P0 + idx, bg = B_HAZE;
            bool bold = false;

            /*
             * Blossoms ride on top of the mass, densest around the rim, and
             * step away from the local tone rather than always lightening — a
             * pale flower on the pale crown is invisible. Inside the mass the
             * blossom sits on the tone its neighbours already render as.
             */
            if (frand() < 0.06 + 0.16 * cl + 0.14 * (1.0 - cov)) {
                g   = blooms[rand() % 4];
                bg  = cov > 0.62 ? B_P0 + idx
                    : cov > 0.38 ? B_P0 + (idx < 7 ? idx + 1 : 7)
                    : B_HAZE;
                fg  = F_P0 + (idx <= 2 ? idx + 3 : idx - 3);
                if (idx > 2 && frand() < 0.35)
                    bold = true;
            }

            put_bg(x, y, g, fg, bg, bold);

            if ((cov < 0.55 || fu > f * 1.10) && frand() < 0.5)
                add_source(x, y);
        }
    }

    cur_kind = K_NONE;
}

/*
 * Lay down one horizontal slice of wood centred on x with half-width w, using
 * partial block glyphs where the edge falls inside a cell so the taper reads as
 * a smooth curve rather than a staircase.
 */
static void wood_span(double x, int y, double w, int dark, int mid, int lite)
{
    int i0 = (int)floor(x - w), i1 = (int)ceil(x + w);
    for (int xi = i0; xi <= i1; xi++) {
        double l  = fmax((double)xi, x - w);
        double rr = fmin(xi + 1.0, x + w);
        double cov = rr - l;
        if (cov <= 0.06)
            continue;

        double d = (xi + 0.5 - x) / fmax(w, 0.5);
        int fg = d < -0.30 ? dark : (d > 0.40 ? lite : mid);
        const char *g = cov > 0.85 ? g_full()
                      : cov > 0.55 ? g_dark()
                      : cov > 0.28 ? g_med()
                      : g_lite();
        put(xi, y, g, fg, false);
    }
}

static void gen_trunk(double bx, double tx, double ty, double rx)
{
    double base_y = gh - 2.0;
    double h = base_y - ty;
    if (h < 2.0) h = 2.0;

    /* Sized off the crown, so the trunk keeps its proportion at every size. */
    double maxw = fmax(rx * 0.085, 1.4);
    double bend = frange(-1.0, 1.0) * fmax(rx * 0.06, 1.0);

    for (int y = (int)ty; y <= (int)base_y; y++) {
        double t = clampd((base_y - y) / h, 0.0, 1.0);
        double x = bx + (tx - bx) * t + sin(t * M_PI) * bend;
        /* Gentle root flare — a steep one reads as a wedge, not a trunk. */
        double w = maxw * pow(1.0 - t, 1.10) * (1.0 + 0.60 * exp(-t * 6.0)) + 0.55;
        wood_span(x, y, w, F_TRUNK_D, F_TRUNK_M, F_TRUNK_L);
    }
}

static void gen_branch(double x, double y, double angle, double len, int depth)
{
    double t = 0.0;
    double w0 = br_w * (depth == 0 ? 1.0 : depth == 1 ? 0.66 : depth == 2 ? 0.42 : 0.28);

    while (t < len) {
        double dx = cos(angle), dy = sin(angle);
        /* Half-cell steps: a full step skips cells and breaks the limb apart.
         * The 0.85 / 0.42 split is the cell aspect — a limb at 45° has to look
         * like 45° on screen, not like a shallow diagonal. */
        x += dx * 0.85;
        y -= dy * 0.42;
        t += 0.5;
        angle += frange(-0.05, 0.05);
        angle = clampd(angle, 0.15, M_PI - 0.15);

        if (x < br_xmin) { x = br_xmin; angle = M_PI - angle; }
        if (x > br_xmax) { x = br_xmax; angle = M_PI - angle; }
        /* Stop at the ceiling. Deflecting sideways instead makes every limb
         * run along it, leaving a row of horizontal dashes in the crown. */
        if (y < br_ymin) {
            y = br_ymin;
            break;
        }

        /* The finest twigs are sub-cell width; drawing them only scatters
         * disconnected specks through the blossom. They still seed tips. */
        if (depth < 3) {
            double w = w0 * (1.0 - 0.45 * (t / fmax(len, 1.0)));
            wood_span(x, (int)y, w, F_TRUNK_D,
                      depth == 0 ? F_TRUNK_M : F_TRUNK_L, F_TRUNK_L);
        }
    }

    /* Two levels only: deeper recursion lets the first limb's subtree consume
     * every tip slot, so the crown only forms on one side. */
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

    cur_kind = K_GROUND;

    for (int x = 0; x < gw; x++) {
        double dx = (x - cx) / (rx * 1.25);
        double p = exp(-dx * dx * 2.2);
        double rr = frand();
        /* Grass fades out with distance instead of ruling a hard line. */
        int grass = p > 0.55 ? F_GRASS : p > 0.20 ? F_GRASS_M : F_GRASS_D;

        if (rr < p * 0.55) {
            double r2 = frand();
            const char *g = r2 < 0.28 ? blooms[rand() % 4]
                          : r2 < 0.62 ? g_dot()
                          : ",";
            put(x, y, g, F_P2 + rand() % 4, false);
        } else if (rr < p * 0.55 + 0.10) {
            put(x, y, "\"", grass, false);
        } else if (rr < p * 0.55 + 0.16) {
            put(x, y, ",", grass, false);
        } else {
            put(x, y, "_", grass, false);
        }

        /* A drift of fallen petals gathering around the roots. */
        if (gh > 4 && frand() < p * 0.28)
            put(x, y - 1, frand() < 0.35 ? blooms[rand() % 4] : g_dot(),
                F_P2 + rand() % 3, false);
    }

    cur_kind = K_NONE;
}

static void add_blob_clamped(double x, double y, double brx, double bry,
                             double y_top)
{
    if (nblobs >= MAX_BLOBS)
        return;
    if (y - bry < y_top)
        y = y_top + bry;
    blobs[nblobs++] = (Blob){ x, y, brx, bry };
}

static void gen_tree(void)
{
    memset(grid, 0, sizeof(Cell) * (size_t)gw * gh);
    nsources = 0;
    nblobs = 0;
    ntips = 0;

    /*
     * Size the crown in square units, not cells. Cell-based radii with a fixed
     * cap made the canopy round in short wide terminals (the width clamped
     * while the height kept growing with the row count) and left the trunk
     * hairline thin. Working in square units keeps one silhouette everywhere.
     */
    double rw_sq = gw * 1.0;
    double rh_sq = gh * CELL_AR;

    double hw_sq = rw_sq * 0.27;                        /* crown half-width  */
    double hh_sq = fmin(hw_sq / 1.95, rh_sq * 0.27);    /* crown half-height */

    double top_pad = clampd(gh * 0.05, 2.0, 4.0);
    double y_top   = top_pad;

    /*
     * Reserve room under the crown. A short terminal — a tmux split, a side
     * pane — otherwise budgets every row to the canopy and clamps the trunk
     * away to nothing, leaving a pancake of blossom resting on the grass.
     */
    double trunk_min = fmax(3.0, gh * 0.18);
    double ry_fit    = (gh - 2.0 - trunk_min - y_top - 0.8) / 2.15;

    double ry = fmax(fmin(hh_sq / CELL_AR, ry_fit), 3.0);
    /*
     * Re-derive the width from the height that actually survived, so a crown
     * squeezed vertically narrows with it rather than smearing across the
     * whole terminal at nine-to-one.
     */
    double rx = fmax(fmin(hw_sq, ry * CELL_AR * 2.8), 6.0);
    br_w = fmax(rx * 0.038, 0.8);

    double cx = gw * 0.5 + frange(-1.5, 1.5);
    double cy = y_top + ry + 0.8;
    double ty = cy + ry * 1.15;

    /*
     * Cap how far the trunk runs. A tall narrow terminal sizes the crown from
     * the column count, then hands every remaining row to the trunk — growing a
     * lamppost with a flower on top. Drop the whole tree down the screen
     * instead and leave sky above it.
     */
    double max_trunk = 2.0 * rx * 0.85 / CELL_AR;
    double shift     = (gh - 2.0 - ty) - max_trunk;
    if (shift > 0.0) {
        y_top += shift;
        cy    += shift;
        ty    += shift;
    }
    if (ty > gh - 4.0) ty = gh - 4.0;

    double bx = cx + frange(-2.0, 2.0);
    double tx = cx + frange(-2.0, 2.0);

    /* Limbs spread wide and reach above the crown's centre, so their tips are
     * distributed through the whole canopy volume. */
    br_xmin = cx - rx * 0.80;
    br_xmax = cx + rx * 0.80;
    br_ymin = fmax(y_top, cy - ry * 0.55);

    gen_ground(cx, rx);

    cur_kind = K_WOOD;
    gen_trunk(bx, tx, ty, rx);

    int limbs = 3 + rand() % 2;
    double reach = (ty - br_ymin) * 0.60 + 2.0;
    for (int i = 0; i < limbs; i++) {
        double na = M_PI / 2.0
                  + (i - (limbs - 1) / 2.0) * frange(0.55, 0.75)
                  + frange(-0.15, 0.15);
        gen_branch(tx + frange(-1.0, 1.0), ty + frange(0.0, 1.5),
                   na, reach * frange(0.75, 1.0), 0);
    }
    cur_kind = K_NONE;

    /*
     * The canopy grows out of the limbs: one core blob, then a puff of blossom
     * at every branch tip. Laying the crown out as a fixed arrangement of blobs
     * instead gives a smooth, generic dome — the silhouette has to follow the
     * tree that was actually grown.
     */
    add_blob_clamped(cx, cy - ry * 0.10, rx * 0.50, ry * 0.50, y_top);
    for (int i = 0; i < ntips && nblobs < MAX_BLOBS - 5; i++) {
        add_blob_clamped(
            tips[i].x + frange(-1.5, 1.5),
            tips[i].y - frange(0.0, 1.5),
            rx * frange(0.18, 0.30),
            ry * frange(0.24, 0.38),
            y_top);
    }
    /* Low clumps so the underside hangs down in wisps. */
    for (int i = 0; i < 5 && nblobs < MAX_BLOBS; i++) {
        add_blob_clamped(
            cx + frange(-0.70, 0.70) * rx,
            cy + frange(0.25, 0.60) * ry,
            rx * frange(0.18, 0.28),
            ry * frange(0.24, 0.34),
            y_top);
    }

    gen_canopy(cx, rx, ry);
}

/* ---------- petals ---------- */

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
    /* Keep petals in the light half of the ramp — the dark end vanishes
     * against the canopy they fall out of. */
    p->fg    = (unsigned char)(F_P0 + rand() % 4);
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
            p->y     = gh - 1.0;
            p->rest  = frange(2.0, 7.0);
            p->fg    = F_FADED;
            p->glyph = g_dot();
        }
    }
}

/* ---------- rendering ---------- */

static void draw(void)
{
    erase();

    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            const Cell *c = &grid[(size_t)y * gw + x];
            if (!c->glyph)
                continue;
            attr_t a = COLOR_PAIR(pair_of(c->fg, c->bg)) | (c->bold ? A_BOLD : 0);
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
        /* Inherit the cell's background so a petal crossing the crown does not
         * cut a terminal-coloured hole through the blossom. */
        const Cell *c = &grid[(size_t)py * gw + px];
        int bg = c->glyph ? c->bg : B_DEF;
        attr_t a = COLOR_PAIR(pair_of(p->fg, bg));
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

static void regrow(bool scatter)
{
    gen_tree();
    reset_petals(scatter);
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
        "  -t        flat mode: never paint cell backgrounds, so a\n"
        "            transparent terminal shows through the canopy\n"
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
        "  c / C     next / previous color palette\n"
        "  a         toggle ASCII glyphs\n"
        "  t         toggle flat (transparent) mode\n"
        "  p / P     more / fewer petals\n"
        "  w / W     more / less wind\n"
        "  + / -     faster / slower\n");
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "f:p:w:c:athv")) != -1) {
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
        case 't':
            opt_flat = true;
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
    regrow(true);

    while (running) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27)
            break;
        switch (ch) {
        case 'r': case 'R':
            regrow(false);
            break;
        case 'c':
            opt_palette = (opt_palette + 1) % NPALETTES;
            apply_palette();
            regrow(false);
            break;
        case 'C':
            opt_palette = (opt_palette + NPALETTES - 1) % NPALETTES;
            apply_palette();
            regrow(false);
            break;
        case 'a': case 'A':
            opt_ascii = !opt_ascii;
            regrow(false);
            break;
        case 't': case 'T':
            opt_flat = !opt_flat;
            update_shaded();
            apply_palette();
            regrow(false);
            break;
        case 'p':
            if (opt_density < 10) { opt_density++; reset_petals(false); }
            break;
        case 'P':
            if (opt_density > 1)  { opt_density--; reset_petals(false); }
            break;
        case 'w':
            if (opt_wind < 10.0) opt_wind = fmin(10.0, opt_wind + 1.0);
            break;
        case 'W':
            if (opt_wind > 0.0)  opt_wind = fmax(0.0, opt_wind - 1.0);
            break;
        case '+': case '=':
            if (opt_fps < 60) opt_fps += 5;
            break;
        case '-': case '_':
            if (opt_fps > 5)  opt_fps -= 5;
            break;
        case KEY_RESIZE:
            resize_grid();
            regrow(true);
            break;
        }

        update_petals(1.0 / opt_fps);
        draw();
        napms(1000 / opt_fps);
    }

    endwin();
    free(grid);
    return 0;
}
