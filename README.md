# csakura 🌸

A sakura tree with falling petals for your terminal — in the spirit of
`cmatrix` and `cava`, but prettier.

![csakura preview](assets/preview.png)

- Procedurally grown cherry-blossom tree (different every run, press `r` to regrow)
- Petals drift down on a wandering wind, settle on the ground, and fade
- Auto-adjusts to any terminal size, regenerates on resize
- Written in C99 + ncurses, low CPU (default 20 FPS)
- Uses your terminal background — great with transparent kitty / HyDE
- 15 blossom color palettes (`-c` / press `c`)
- Matching HTML demo in [`web/`](web/)

## Install

### Arch Linux

```sh
sudo pacman -S --needed base-devel ncurses
git clone https://github.com/realstrawhat/csakura.git
cd csakura
makepkg -si
```

Or:

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
  -h        help
  -v        version

palettes:
  sakura, rose, blush, magenta, peach,
  coral, sunset, gold, lavender, violet,
  sky, mint, matcha, white, ink

keys:
  q / Esc   quit
  r         regrow the tree
  c         next color palette
  C         previous color palette
```

Examples:

```sh
csakura                 # default
csakura -f 12 -p 3      # chill / low CPU (nice in a side pane)
csakura -p 10 -w 6      # windy petal storm
csakura -c mint         # mint palette
```

## Web version

Open [`web/index.html`](web/index.html) in a browser for the same animation
(color picker via the faint ⋮ in the corner). You can also enable GitHub Pages
on the `/web` folder for a live demo.

## Notes

- Best with a 256-color terminal (kitty, iTerm2, or modern Terminal.app).
- Unicode blossoms need a font that has them (most Nerd Fonts do). Use `-a` if not.
- On macOS, Homebrew’s `ncurses` is recommended for the best color support.

## Attribution

If you use csakura, please credit **realstrawhat**  
(https://github.com/realstrawhat/csakura).

## License

MIT
