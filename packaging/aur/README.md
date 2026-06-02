# AUR packaging — `mat-cat`

Canonical source for the [AUR `mat-cat`](https://aur.archlinux.org/packages/mat-cat)
package. Edit here, then sync to the AUR git repo.

## Notes

- **VCS sources use `SKIP`.** `makepkg` does not checksum git working trees; the
  pinned `#tag=` plus the `paige` submodule gitlink provide integrity.
- **`paige` is declared as its own source** rather than fetched by a recursive
  `git submodule update`, so the build works in a clean/offline chroot. `prepare()`
  points the submodule URL at the makepkg-managed clone and uses
  `protocol.file.allow=always` (git blocks local-path submodule transport by default).
  The submodule name is `lib/paige` (see the repo's `.gitmodules`) — the git config
  key must match exactly.
- **`prepare()` rewrites `LDFLAGS =` to `+=`** so makepkg's hardening flags are
  honored instead of overwritten.

## Syncing to the AUR

```sh
cp packaging/aur/PKGBUILD /path/to/aur/mat-cat/PKGBUILD
cd /path/to/aur/mat-cat
makepkg --printsrcinfo > .SRCINFO   # regenerate; do not hand-edit
git commit -am 'upgpkg: mat-cat 0.7.0-2'
git push
```

Bump `pkgrel` for packaging-only changes; reset it to `1` when `pkgver` changes.
