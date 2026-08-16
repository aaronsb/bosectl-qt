# bosectl-qt

## Releasing

`aaronsb/arch-repo` publishes this project. It reads `PKGBUILD` from the default
branch, builds it in a clean container, lints with namcap, signs, and pushes to
the AUR (\`bosectl-qt\` and \`bosectl-qt-git\`) and the `[aaronsb]` pacman repository.

```bash
make package                          # check it builds and lints
make bump-version VERSION=X.Y.Z       # this project's own version strings only
make release VERSION=X.Y.Z            # commit, tag, push, cut the GitHub release
```

Nothing here talks to the AUR. There is no `aur` target and no publish script:
two writers to one AUR ref is how a PKGBUILD and its `.SRCINFO` drift apart.

### Fields arch-repo owns

It overwrites all four before publishing, so a value set here is only wrong
until it does. Do not maintain them, and do not commit a `.SRCINFO`.

| Field | Where it really comes from |
|---|---|
| `pkgver` | the newest published GitHub release |
| `pkgrel` | arch-repo's count of how many times it packaged that release |
| `sha256sums` | computed from the release artifact |
| `.SRCINFO` | regenerated at publish |

This project's own version lives in `CMakeLists.txt`'s `project(... VERSION ...)`, `src/main.cpp`, and the About
dialog in `src/TrayIcon.cpp` — all three rewritten by `make bump-version`, which
deliberately does not touch `PKGBUILD`.

### A packaging fix needs no release

Change the recipe on the default branch and push. arch-repo compares the
rendered recipe against what it last published and ships the difference as a
`pkgrel` bump — `1.2.0-1` becomes `1.2.0-2`, resetting to `-1` at the next
real release. Do not cut a version for a change to packaging alone.

### Check before you tag

`make package` builds the recipe in a clean chroot and runs namcap. It builds
from `HEAD` rather than the published archive, so it works before the release
it precedes, and it fails on a namcap error — namcap exits 0 whether or not it
found one.

### `PKGBUILD-git`

Publishes `bosectl-qt-git` from the default branch at HEAD, so **main must
always build**. Its `pkgver()` derives the base version from `git describe` —
never hardcode it, and never commit a placeholder like `0.3.0.r0.g0000000`,
which names a commit that does not exist and is the string `yay` and `paru`
compare against.

The full contract: https://github.com/aaronsb/arch-repo/blob/main/docs/packaging-contract.md
