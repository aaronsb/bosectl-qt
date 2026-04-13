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
#   make clean         # rm -rf ./build/
#
# ── Cutting a release ──────────────────────────────────────────────────
#   make bump-version VERSION=X.Y.Z
#       Rewrite the version strings in CMakeLists.txt, src/main.cpp,
#       src/TrayIcon.cpp (About dialog), PKGBUILD, and PKGBUILD-git.
#       Does NOT commit — leaves the bump in the working tree so you can
#       review the diff before proceeding. Also resets the PKGBUILD tarball
#       sha256 to 'SKIP' since we don't know it until GitHub has published
#       the release tarball.
#
#   make release VERSION=X.Y.Z
#       Assumes bump-version has been run and the working tree has the
#       bumped version strings. Does the github side of the release:
#         1. commit the bump (message "Release X.Y.Z")
#         2. tag vX.Y.Z
#         3. push main + tag
#         4. gh release create with auto-generated notes
#         5. download the published tarball, compute sha256
#         6. patch PKGBUILD with the real sha256
#         7. commit + push "Set vX.Y.Z tarball sha256"
#
#   make aur-publish
#       Publishes the current on-disk PKGBUILD / PKGBUILD-git to the AUR.
#       Refuses to run if PKGBUILD's first sha256 is still 'SKIP' (that
#       would mean the release step was skipped). Reads the version from
#       PKGBUILD so there's no VERSION arg needed.
#         - clones ssh://aur@aur.archlinux.org/bosectl-qt.git
#         - clones ssh://aur@aur.archlinux.org/bosectl-qt-git.git
#         - copies PKGBUILD / PKGBUILD-git into each
#         - regenerates .SRCINFO via `makepkg --printsrcinfo`
#         - commits + pushes to the AUR's `master` branch
#         - leaves the /tmp clones in place for inspection
#
#   make cut-release VERSION=X.Y.Z
#       The whole flow: bump-version → release → aur-publish.
#       Use this when you want one command to do everything. Use the
#       three individual targets if you want to pause between steps.
#
# ── Prerequisites ───────────────────────────────────────────────────────
#   - cmake, make, gcc, Qt6 (base + dbus), bluez      (build)
#   - git, gh                                          (release)
#   - makepkg (from pacman), curl, sha256sum           (release + aur-publish)
#   - SSH key loaded for aur@aur.archlinux.org         (aur-publish)

SHELL := /bin/bash

BUILD_DIR := build
BIN := $(BUILD_DIR)/bosectl-qt

# Current package version, read from PKGBUILD. Used by aur-publish so the
# commit message and context reflect what's actually being shipped.
CURRENT_VERSION := $(shell awk -F= '/^pkgver=/ {print $$2; exit}' PKGBUILD)

REPO := aaronsb/bosectl-qt
TARBALL_URL = https://github.com/$(REPO)/archive/v$(VERSION).tar.gz

AUR_STABLE_DIR := /tmp/aur-bosectl-qt
AUR_GIT_DIR := /tmp/aur-bosectl-qt-git
AUR_STABLE_URL := ssh://aur@aur.archlinux.org/bosectl-qt.git
AUR_GIT_URL := ssh://aur@aur.archlinux.org/bosectl-qt-git.git

.PHONY: help build clean run run-verbose \
        bump-version release aur-publish cut-release \
        _check-version _check-pkgbuild-ready

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
	@printf '  \033[1mCutting a release\033[0m (bosectl-qt tracks upstream bosectl major/minor)\n'
	@printf '    make cut-release VERSION=X.Y.Z   full flow: bump → release → AUR\n'
	@printf '\n'
	@printf '    — or, decomposed if you want to pause between steps —\n'
	@printf '    make bump-version VERSION=X.Y.Z  rewrite version strings in source\n'
	@printf '                                     (CMakeLists.txt, main.cpp,\n'
	@printf '                                     TrayIcon.cpp About dialog,\n'
	@printf '                                     PKGBUILD, PKGBUILD-git). Resets\n'
	@printf '                                     tarball sha256 to SKIP. No commit.\n'
	@printf '    make release VERSION=X.Y.Z       commit bump, tag vX.Y.Z, push,\n'
	@printf '                                     gh release create, fetch tarball,\n'
	@printf '                                     pin sha256, second commit/push.\n'
	@printf '    make aur-publish                 clone both AUR repos, copy\n'
	@printf '                                     PKGBUILD/PKGBUILD-git, regenerate\n'
	@printf '                                     .SRCINFO, push master. Reads the\n'
	@printf '                                     version from PKGBUILD.\n'
	@printf '\n'
	@printf '  \033[1mGotchas\033[0m\n'
	@printf '    • AUR uses `master`, not `main`. The Makefile pushes to master.\n'
	@printf '    • PKGBUILD pins _bosectl_commit separately — bump it by hand if\n'
	@printf '      the lib/bosectl submodule advanced with this release.\n'
	@printf '    • aur-publish refuses to run if PKGBUILD'"'"'s first sha256 is still\n'
	@printf '      `SKIP` — that would mean `make release` was skipped.\n'
	@printf '\n'
	@printf '  \033[1mPrerequisites\033[0m\n'
	@printf '    cmake, gcc, Qt6 (base + dbus), bluez   (build)\n'
	@printf '    gh authenticated                        (release)\n'
	@printf '    makepkg (pacman), curl, sha256sum       (release + aur-publish)\n'
	@printf '    SSH key loaded for aur@aur.archlinux.org (aur-publish)\n'
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
	@echo "==> Rewriting version strings to $(VERSION)"
	sed -i -E 's/(project\(bosectl-qt VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1$(VERSION)/' CMakeLists.txt
	sed -i -E 's/(setApplicationVersion\(")[0-9]+\.[0-9]+\.[0-9]+/\1$(VERSION)/' src/main.cpp
	sed -i -E 's|(<p>Version )[0-9]+\.[0-9]+\.[0-9]+(</p>)|\1$(VERSION)\2|' src/TrayIcon.cpp
	sed -i -E 's/^(pkgver=)[0-9]+\.[0-9]+\.[0-9]+$$/\1$(VERSION)/' PKGBUILD
	sed -i -E 's/^(pkgver=)[0-9]+\.[0-9]+\.[0-9]+(\.r0\.g0000000)/\1$(VERSION)\2/' PKGBUILD-git
	sed -i -E 's/(printf ")[0-9]+\.[0-9]+\.[0-9]+(\.r%s\.g%s")/\1$(VERSION)\2/' PKGBUILD-git
	sed -i -E "/^sha256sums=\(/,/^\)/ s/^(    )'[a-f0-9]{64}'/\1'SKIP'/" PKGBUILD
	@echo "==> Bumped. Diff:"
	@git --no-pager diff --stat CMakeLists.txt src/main.cpp src/TrayIcon.cpp PKGBUILD PKGBUILD-git
	@echo ""
	@echo "Next: review with 'git diff' then run 'make release VERSION=$(VERSION)'"

release: _check-version
	@echo "==> Verifying clean build at $(VERSION)"
	$(MAKE) build
	@echo "==> Committing version bump"
	git add CMakeLists.txt src/main.cpp src/TrayIcon.cpp PKGBUILD PKGBUILD-git
	git commit -m "Release $(VERSION)"
	@echo "==> Tagging v$(VERSION)"
	git tag -a v$(VERSION) -m "bosectl-qt v$(VERSION)"
	git push
	git push origin v$(VERSION)
	@echo "==> Creating GitHub release"
	gh release create v$(VERSION) \
		--title "bosectl-qt v$(VERSION)" \
		--generate-notes
	@echo "==> Fetching tarball and computing sha256"
	@set -e; \
	tmp=$$(mktemp); \
	curl -fsSL $(TARBALL_URL) -o "$$tmp"; \
	sha=$$(sha256sum "$$tmp" | awk '{print $$1}'); \
	rm -f "$$tmp"; \
	echo "    sha256 = $$sha"; \
	sed -i -E "0,/^(    )'SKIP'/ s//\1'$$sha'/" PKGBUILD
	@echo "==> Committing sha256"
	git add PKGBUILD
	git commit -m "Set v$(VERSION) tarball sha256"
	git push
	@echo "==> GitHub release done. Run 'make aur-publish' next."

_check-pkgbuild-ready:
	@first_sha=$$(awk '/^sha256sums=\(/{f=1; next} f && /^\)/{exit} f' PKGBUILD | head -1 | tr -d "', \t"); \
	if [ "$$first_sha" = "SKIP" ]; then \
		echo "error: PKGBUILD tarball sha256 is still 'SKIP'."; \
		echo "  Run 'make release VERSION=$(CURRENT_VERSION)' first so the sha is pinned."; \
		exit 1; \
	fi
	@if [ -z "$(CURRENT_VERSION)" ]; then \
		echo "error: could not read pkgver from PKGBUILD"; \
		exit 1; \
	fi

aur-publish: _check-pkgbuild-ready
	@echo "==> Publishing bosectl-qt $(CURRENT_VERSION) to the AUR"
	@echo ""
	@echo "--- bosectl-qt (stable) ---"
	rm -rf $(AUR_STABLE_DIR)
	git clone $(AUR_STABLE_URL) $(AUR_STABLE_DIR)
	cp PKGBUILD $(AUR_STABLE_DIR)/PKGBUILD
	cd $(AUR_STABLE_DIR) && \
		makepkg --printsrcinfo > .SRCINFO && \
		git add PKGBUILD .SRCINFO && \
		(git diff --cached --quiet && echo "    (no changes, skipping push)" || \
			(git commit -m "Release $(CURRENT_VERSION)" && git push origin master))
	@echo ""
	@echo "--- bosectl-qt-git ---"
	rm -rf $(AUR_GIT_DIR)
	git clone $(AUR_GIT_URL) $(AUR_GIT_DIR)
	cp PKGBUILD-git $(AUR_GIT_DIR)/PKGBUILD
	cd $(AUR_GIT_DIR) && \
		makepkg --printsrcinfo > .SRCINFO && \
		git add PKGBUILD .SRCINFO && \
		(git diff --cached --quiet && echo "    (no changes, skipping push)" || \
			(git commit -m "Release $(CURRENT_VERSION)" && git push origin master))
	@echo ""
	@echo "==> Done. Packages:"
	@echo "    https://aur.archlinux.org/packages/bosectl-qt"
	@echo "    https://aur.archlinux.org/packages/bosectl-qt-git"

cut-release: _check-version
	$(MAKE) bump-version VERSION=$(VERSION)
	$(MAKE) release VERSION=$(VERSION)
	$(MAKE) aur-publish
