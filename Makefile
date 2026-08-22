# bosectl-qt — build / run / release helper
#
# CMake drives the actual compile; this Makefile sits on top as a convenience
# wrapper for the full lifecycle (daily dev loop + cutting releases + pushing
# to the AUR). CMakeLists.txt is still the source of truth for how the binary
# is built.
#
# ── Daily development ──────────────────────────────────────────────────
#   make               # same as `make build`
#   make build         # cmake configure + compile to ./build/
#   make run           # build, then run ./build/bosectl-qt
#   make run-verbose   # build, then run with --verbose logging
#   make test          # build, then run ctest (bmap suite + app suite)
#   make clean         # rm -rf ./build/
#
# ── Cutting a release ──────────────────────────────────────────────────
#   make bump-version VERSION=X.Y.Z
#       Rewrite this project's own version strings — CMakeLists.txt,
#       src/main.cpp, src/TrayIcon.cpp (About dialog). Does NOT commit, so
#       the diff can be reviewed first. It no longer touches PKGBUILD:
#       arch-repo owns pkgver, pkgrel and sha256sums and overwrites all
#       three, so a value written here would only be wrong until it did.
#
#   make release VERSION=X.Y.Z
#       Commit the bump, tag vX.Y.Z, push, and cut the GitHub release.
#       That is the end of it. arch-repo watches this repository, reads
#       ./PKGBUILD from the default branch, takes the version and checksum
#       from the release, builds in a clean container, lints, signs, and
#       pushes to the AUR and the [aaronsb] pacman repository.
#
#       There used to be four more steps here: download the published
#       tarball, compute its sha256, patch PKGBUILD, commit again. That
#       second commit existed because a PKGBUILD cannot carry the checksum
#       of the tarball its own tag produces — the hash does not exist until
#       the tag does. arch-repo removes the circularity by reading the
#       recipe from the branch and the two moving values from the release,
#       rather than automating a walk around it.
#
# ── Checking before you release ─────────────────────────────────────────
#   make check         cmake configure + compile, and report the version
#   make package       build ./PKGBUILD in a clean chroot and namcap it
#   make version       report the version this repository would release
#
# ── Prerequisites ───────────────────────────────────────────────────────
#   - cmake, make, gcc, Qt6 (base + dbus), bluez      (build)
#   - git, gh                                          (release)
#   - devtools, pacman-contrib, namcap                 (package)

SHELL := /bin/bash

BUILD_DIR := build
BIN := $(BUILD_DIR)/bosectl-qt

# The AUR package name, and the version this project reports about itself.
# PKGBUILD's pkgver is a placeholder arch-repo overwrites, so it is not read
# here — CMakeLists.txt is where the version actually lives.
NAME    := $(shell sed -n 's/^pkgname=//p' PKGBUILD)
SRCNAME := $(or $(shell sed -n 's/^_repo=//p' PKGBUILD),$(NAME))
# The dot stands in for the literal paren: make counts parentheses inside
# $(shell ...) and an escaped one in a regex reads as unbalanced to it.
CURRENT_VERSION := $(shell awk '/^project.bosectl-qt VERSION/ {print $$3}' CMakeLists.txt)

.PHONY: help build clean run run-verbose test \
        bump-version release check package version \
        _check-version

# Default: show help. Typing `make` with no target lands on a usage screen
# rather than silently starting a build — discoverable, and the frequent
# "make build / run / clean" flow is one extra word of typing.
.DEFAULT_GOAL := help

help:
	@printf '\n'
	@printf '  \033[1mbosectl-qt\033[0m — Qt6 system tray app for Bose headphones (BMAP)\n'
	@printf '\n'
	@printf '  \033[1mDaily development\033[0m\n'
	@printf '    make build         cmake configure + compile to ./build/\n'
	@printf '    make run           build, then run ./build/bosectl-qt\n'
	@printf '    make run-verbose   build, then run with --verbose logging\n'
	@printf '    make clean         rm -rf ./build/\n'
	@printf '\n'
	@printf '  \033[1mBefore you release\033[0m\n'
	@printf '    make check         compile, and report the version\n'
	@printf '    make version       report the version this repo would release\n'
	@printf '    make package       build ./PKGBUILD in a clean chroot + namcap\n'
	@printf '\n'
	@printf '  \033[1mCutting a release\033[0m (bosectl-qt tracks upstream bosectl major/minor)\n'
	@printf '    make bump-version VERSION=X.Y.Z  rewrite this project'"'"'s version\n'
	@printf '                                     strings (CMakeLists.txt,\n'
	@printf '                                     main.cpp, TrayIcon.cpp About\n'
	@printf '                                     dialog). No commit.\n'
	@printf '    make release VERSION=X.Y.Z       commit, tag, push, gh release\n'
	@printf '                                     create. arch-repo does the rest.\n'
	@printf '\n'
	@printf '  \033[1mGotchas\033[0m\n'
	@printf '    • PKGBUILD pins _bosectl_commit separately — bump it by hand if\n'
	@printf '      the lib/bosectl submodule advanced with this release.\n'
	@printf '    • pkgver, pkgrel and sha256sums in PKGBUILD are placeholders.\n'
	@printf '      arch-repo overwrites all three; do not maintain them.\n'
	@printf '    • A packaging fix needs no release at all. Change the recipe on\n'
	@printf '      the default branch and arch-repo ships it as a pkgrel bump.\n'
	@printf '\n'
	@printf '  \033[1mPrerequisites\033[0m\n'
	@printf '    cmake, gcc, Qt6 (base + dbus), bluez     (build)\n'
	@printf '    gh authenticated                          (release)\n'
	@printf '    devtools, pacman-contrib, namcap          (package)\n'
	@printf '\n'

# ─── Build / run ────────────────────────────────────────────────────────────

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -Wno-dev
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: build
	$(BIN)

run-verbose: build
	$(BIN) --verbose

test: build ## Build, then run both test suites via ctest
	ctest --test-dir $(BUILD_DIR) --output-on-failure

# ─── Release ceremony ───────────────────────────────────────────────────────

_check-version:
	@if [ -z "$(VERSION)" ]; then \
		echo "error: VERSION is required. Example: make $(MAKECMDGOALS) VERSION=0.4.0"; \
		exit 1; \
	fi
	@if ! echo "$(VERSION)" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
		echo "error: VERSION='$(VERSION)' does not look like semver X.Y.Z"; \
		exit 1; \
	fi

bump-version: _check-version
	@echo "==> Rewriting this project'"'"'s version strings to $(VERSION)"
	sed -i -E 's/(project\(bosectl-qt VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1$(VERSION)/' CMakeLists.txt
	sed -i -E 's/(setApplicationVersion\(")[0-9]+\.[0-9]+\.[0-9]+/\1$(VERSION)/' src/main.cpp
	sed -i -E 's|(<p>Version )[0-9]+\.[0-9]+\.[0-9]+(</p>)|\1$(VERSION)\2|' src/TrayIcon.cpp
	@echo "==> Bumped. Diff:"
	@git --no-pager diff --stat CMakeLists.txt src/main.cpp src/TrayIcon.cpp
	@echo ""
	@echo "PKGBUILD is deliberately untouched: arch-repo owns pkgver, pkgrel and"
	@echo "sha256sums and overwrites all three from the release."
	@echo ""
	@echo "Next: review with '"'"'git diff'"'"' then '"'"'make release VERSION=$(VERSION)'"'"'"

release: _check-version
	@echo "==> Verifying clean build at $(VERSION)"
	$(MAKE) build
	@echo "==> Committing version bump"
	git add CMakeLists.txt src/main.cpp src/TrayIcon.cpp
	git commit -m "Release $(VERSION)"
	@echo "==> Tagging v$(VERSION)"
	git tag -a v$(VERSION) -m "bosectl-qt v$(VERSION)"
	git push
	git push origin v$(VERSION)
	@echo "==> Creating GitHub release"
	gh release create v$(VERSION) \
		--title "bosectl-qt v$(VERSION)" \
		--generate-notes
	@echo ""
	@echo "==> Done. arch-repo picks this up on its next run and publishes both"
	@echo "    bosectl-qt and bosectl-qt-git to the AUR and [aaronsb]."

# ─── arch-repo's packaging contract ─────────────────────────────────────────
#
# https://github.com/aaronsb/arch-repo/blob/main/docs/packaging-contract.md

check: build version ## Everything CI would run

# Reporting rather than failing: before a release the tag is legitimately
# absent, and after one it is legitimately present, so neither is an error.
version:
	@test -n "$(CURRENT_VERSION)" || { echo "no VERSION in CMakeLists.txt project()" >&2; exit 1; }
	@if git rev-parse -q --verify "refs/tags/v$(CURRENT_VERSION)" >/dev/null; then \
	    echo "$(NAME) $(CURRENT_VERSION) — v$(CURRENT_VERSION) is already tagged"; \
	else \
	    echo "$(NAME) $(CURRENT_VERSION) — not yet tagged; this is what the next release will be"; \
	fi

package: version
	@command -v extra-x86_64-build >/dev/null || { echo "needs devtools" >&2; exit 1; }
	@command -v updpkgsums >/dev/null        || { echo "needs pacman-contrib" >&2; exit 1; }
	@command -v namcap >/dev/null            || { echo "needs namcap" >&2; exit 1; }
	rm -rf pkgbuild-check && mkdir -p pkgbuild-check
	# The tarball the release would carry, built from HEAD and named exactly
	# what source= resolves to, so makepkg uses it instead of fetching
	# archive/v$$pkgver.tar.gz — which GitHub does not generate until the tag
	# exists. A dry run that needs the release to have happened is not one.
	git archive --format=tar.gz --prefix=$(SRCNAME)-$(CURRENT_VERSION)/ \
	    -o pkgbuild-check/$(NAME)-$(CURRENT_VERSION).tar.gz HEAD
	cp PKGBUILD $(wildcard *.install) pkgbuild-check/
	# Slot one only, which is the entry that moves with the version — the same
	# one arch-repo writes. updpkgsums would rewrite every entry, and this
	# recipe's second source is a git checkout carrying SKIP; replacing that
	# with a computed hash would have the dry run build something arch-repo
	# will never publish. The sums array is the only quoted 64-hex in the file.
	cd pkgbuild-check \
	  && sed -i 's/^pkgver=.*/pkgver=$(CURRENT_VERSION)/' PKGBUILD \
	  && sum=$$(sha256sum $(NAME)-$(CURRENT_VERSION).tar.gz | cut -d' ' -f1) \
	  && sed -i "0,/'[0-9a-f]\{64\}'/s//'$$sum'/" PKGBUILD
	cd pkgbuild-check && extra-x86_64-build
	# namcap exits 0 whether or not it found errors, so its output decides —
	# the same rule arch-repo's gate uses. Debug packages are excluded because
	# every .build-id entry in one is a symlink into the main package.
	cd pkgbuild-check && namcap PKGBUILD $$(ls ./*.pkg.tar.zst | grep -v -- '-debug-') | tee namcap.txt
	@cd pkgbuild-check && if [ -f ../.namcap-allow ]; then \
	    bad=$$(grep ' E: ' namcap.txt | grep -vE -f ../.namcap-allow || true); \
	  else \
	    bad=$$(grep ' E: ' namcap.txt || true); \
	  fi; \
	  if [ -n "$$bad" ]; then echo "namcap errors:"; printf '%s\n' "$$bad"; exit 1; fi; \
	  echo "namcap: no errors"
