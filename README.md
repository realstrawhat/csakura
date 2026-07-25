# csakura 2.0 🌸

A sakura tree with falling petals for your terminal — in the spirit of
`cmatrix` and `cava`, but prettier.

![csakura preview](assets/preview.png)

- Procedurally grown cherry-blossom tree (different every run, press `r` to regrow)
- Petals drift down on a wandering wind, settle on the ground, and fade
- Auto-adjusts to any terminal size, regenerates on resize
- Written in C99 + ncurses, low CPU (default 20 FPS)
- 15 blossom color palettes (`-c` / press `c`)
- [Live keyboard shortcuts](#shortcuts) for every setting — palette, petals,
  wind, speed, ASCII and flat mode, all without restarting
- Matching HTML demo in [`web/`](web/)

## What's new in 2.0

The tree is grown rather than stamped. The canopy now follows the limbs that
were actually generated instead of a fixed arrangement of blobs, so no two
crowns share a silhouette:

- **Clumped blossom.** Two octaves of value noise break the crown into
  clusters, with a per-column wobble roughing up the outline — a cherry, not a
  smooth dome.
- **Directional light.** Light falls from the upper left and the mass shades
  what hangs below it, so the crown has volume instead of a flat tint.
- **Limbs through the blossom.** Branches show through where the clusters
  thin, following the noise field so a limb emerges as a continuous run rather
  than a scatter of dashes.
- **Smooth taper.** Trunk and branches use partial block glyphs (`░▒▓`) at
  their edges, so the wood curves instead of stepping, and the trunk keeps its
  proportion to the crown at every terminal size.
- **Shaded cells.** Canopy cells sit on a dark tint of the blossom colour, and
  the block glyphs blend the ramp step against it — this is what lets eight
  colours read as a solid, saturated mass with a fringe that dissolves.
- **Sane at any shape.** A short split, a tall narrow pane, or a full screen
  all get a correctly proportioned tree rather than a pancake or a lamppost.

The shaded cells mean csakura paints a background behind the canopy, so a
transparent terminal no longer shows through it. Pass `-t` (or press `t`) for
flat mode, which never paints cell backgrounds and keeps your terminal — or
your wallpaper — visible behind the tree.

## Install

### Arch Linux

From the AUR, with `yay` (or `paru`):

```sh
yay -S csakura        # latest release
yay -S csakura-git    # builds from main
```

Or build the PKGBUILD by hand:

```sh
sudo pacman -S --needed base-devel ncurses
git clone https://github.com/realstrawhat/csakura.git
cd csakura
makepkg -si
```

Or without pacman at all:

```sh
make && sudo make install
```

Uninstall: `sudo pacman -R csakura` or `sudo make uninstall`.

### macOS

With [Homebrew](https://brew.sh):

```sh
brew install ncurses
git clone https://github.com/realstrawhat/csakura.git
cd csakura
brew install --HEAD --formula ./Formula/csakura.rb
```

Or build yourself:

```sh
brew install ncurses
git clone https://github.com/realstrawhat/csakura.git
cd csakura
make
sudo make install
```

Then run `csakura`. Uninstall with `brew uninstall csakura` or `sudo make uninstall`.

### Other Linux

Need a C compiler + wide-char ncurses (`libncursesw5-dev` / `ncurses-devel`):

```sh
git clone https://github.com/realstrawhat/csakura.git
cd csakura
make && sudo make install
```
## Usage

```
csakura [options]

  -f FPS    frames per second, 5-60 (default: 20)
  -p NUM    petal density, 1-10 (default: 5)
  -w NUM    wind strength, 0-10 (default: 1)
  -c NAME   blossom palette (default: sakura)
  -a        ASCII glyphs only (no unicode blossoms)
  -t        flat mode: never paint cell backgrounds, so a
            transparent terminal shows through the canopy
  -h        help
  -v        version

palettes:
  sakura, rose, blush, magenta, peach,
  coral, sunset, gold, lavender, violet,
  sky, mint, matcha, white, ink
```

Examples:

```sh
csakura                 # default
csakura -f 12 -p 3      # chill / low CPU (nice in a side pane)
csakura -p 10 -w 6      # windy petal storm
csakura -c mint         # mint palette
csakura -t              # let a transparent terminal show through
```

## Shortcuts

Everything is tunable live — you never need to restart to try a setting.

| Key       | Does                                                  |
|-----------|-------------------------------------------------------|
| `q`/`Esc` | quit                                                  |
| `r`       | regrow the tree (new shape every time)                |
| `c` / `C` | next / previous color palette (15 of them)            |
| `p` / `P` | more / fewer petals                                   |
| `w` / `W` | more / less wind                                      |
| `+` / `-` | faster / slower                                       |
| `a`       | toggle ASCII glyphs — for fonts without the blossoms  |
| `t`       | toggle flat mode — lets a transparent terminal through |

Uppercase letters are shifted, so `C` is <kbd>Shift</kbd>+<kbd>c</kbd>.

The same keys work in the [web version](web/), which also has a click-through
panel behind the faint ⋮ in the corner.

## Web version

Open [`web/index.html`](web/index.html) in a browser for the same animation.
The faint ⋮ in the corner opens the full set of controls — palette, petal
density, wind, speed, ASCII glyphs and flat mode — and the terminal keys all
work too (`r` `c`/`C` `a` `t` `p`/`P` `w`/`W` `+`/`-`). Settings persist across
visits. You can also enable GitHub Pages on the `/web` folder for a live demo.

## Notes

- Best with a 256-color terminal (kitty, iTerm2, or modern Terminal.app). The
  shaded canopy needs 256 colors; on an 8-color terminal csakura falls back to
  flat mode automatically.
- Unicode blossoms need a font that has them (most Nerd Fonts do). Use `-a` if not.
- On macOS, Homebrew’s `ncurses` is recommended for the best color support.
- Running over a transparent or image background? Use `-t`.

## Attribution

If you use csakura, please credit **realstrawhat**  
(https://github.com/realstrawhat/csakura).

## License

MIT
