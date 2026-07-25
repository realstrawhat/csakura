# Publishing csakura to the AUR

`yay` and `paru` are AUR helpers — they don't need anything special. Once these
packages exist on the AUR, `yay -S csakura` just works.

Two packages, because the AUR requires a package built from a moving git branch
to carry the `-git` suffix:

| AUR package   | Source                        | PKGBUILD lives in    |
|---------------|-------------------------------|----------------------|
| `csakura`     | the `vX.Y.Z` release tarball  | repo root            |
| `csakura-git` | `main`, versioned from `git describe` | `aur/csakura-git/` |

`csakura-git` sets `provides`/`conflicts` on `csakura`, so only one can be
installed at a time.

## One-time setup

1. Register at <https://aur.archlinux.org/register>.
2. Add your **public** SSH key under *My Account → SSH Public Key*.
3. Check it works:

   ```sh
   ssh aur@aur.archlinux.org help
   ```

## Cutting a release

Everything below runs on an Arch machine (or an `archlinux` container).

```sh
# 1. Tag upstream first — the stable PKGBUILD downloads this tarball.
git tag -a v2.0.0 -m "csakura 2.0.0"
git push origin v2.0.0
```

Then, in the repo root:

```sh
# 2. Fill in the real checksum (PKGBUILD ships a zeroed placeholder).
updpkgsums                              # pacman-contrib

# 3. Regenerate .SRCINFO — the AUR rejects a push whose .SRCINFO
#    disagrees with the PKGBUILD.
makepkg --printsrcinfo > .SRCINFO

# 4. Build and lint.
makepkg -f
namcap PKGBUILD csakura-*.pkg.tar.zst   # namcap

# 5. Best-effort clean build in a chroot, which catches missing deps that
#    happen to already be installed on your machine.
extra-x86_64-build                      # devtools
```

## Pushing to the AUR

The AUR repo is separate from GitHub and holds **only** `PKGBUILD`, `.SRCINFO`
and small aux files — never sources, tarballs or built packages.

```sh
git clone ssh://aur@aur.archlinux.org/csakura.git aur-csakura
cd aur-csakura
cp ../PKGBUILD ../.SRCINFO .
git add PKGBUILD .SRCINFO
git commit -m "csakura 2.0.0"
git push
```

Same again for the git variant:

```sh
git clone ssh://aur@aur.archlinux.org/csakura-git.git aur-csakura-git
cd aur-csakura-git
cp ../aur/csakura-git/PKGBUILD ../aur/csakura-git/.SRCINFO .
git add PKGBUILD .SRCINFO
git commit -m "csakura-git 2.0.0"
git push
```

Cloning a name nobody has taken gives you an empty repo and claims it on first
push. If the push is rejected, it is almost always a stale `.SRCINFO` — rerun
step 3.

## For later releases

Bump `pkgver`, reset `pkgrel=1`, then repeat steps 1–4 and push. If only the
packaging changed and the sources didn't, keep `pkgver` and bump `pkgrel`
instead.

`csakura-git` needs no version edits at all — its `pkgver()` re-derives the
version from `git describe` on every build. Its committed `pkgver` is only a
placeholder for the AUR's metadata.
