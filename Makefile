WIN_DIR := $(shell wslpath -w '$(CURDIR)')
BUILD_EXE_W := $(WIN_DIR)\\build\\handmade-hero.exe
DEVENV := $(shell for v in 18 2022 2019; do \
	p="/mnt/c/Program Files/Microsoft Visual Studio/$$v/Community/Common7/IDE/devenv.exe"; \
	test -f "$$p" && wslpath -w "$$p" && break; \
	done)

.PHONY: build clean open clangd

build:
	cmd.exe /c "cd /d $(WIN_DIR) && misc\\build.bat"

clean:
	cmd.exe /c "cd /d $(WIN_DIR) && if exist build rmdir /s /q build"

open: build
	cmd.exe /c start "" "$(DEVENV)" /debugexe "$(BUILD_EXE_W)"

clangd:
	bash misc/generate-clangd.sh
