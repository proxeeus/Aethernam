# Eternam native macOS port
CC       ?= clang
SDL_PREFIX ?= /opt/homebrew
GAME_DATA ?= ../Eternam
# BUNDLE_DATA=1 copies your game files into the app (personal builds only - never distribute that)
BUNDLE_DATA ?= 0
CFLAGS   ?= -O2 -g
CFLAGS   += -std=gnu11 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label \
            -fno-strict-aliasing -fwrapv -I$(SDL_PREFIX)/include -MMD -MP
LDFLAGS  += -L$(SDL_PREFIX)/lib -lSDL3 -Wl,-rpath,@executable_path/../Frameworks

SRC := src/main.c $(wildcard src/core/*.c) $(wildcard src/platform/*.c) $(wildcard src/game/*.c) $(wildcard src/sound/*.c) $(wildcard src/intro/*.c)
CXXSRC := src/sound/opl.cpp src/sound/ymfm/ymfm_opl.cpp src/sound/ymfm/ymfm_adpcm.cpp src/sound/ymfm/ymfm_pcm.cpp
OBJ := $(SRC:%.c=build/obj/%.o) $(CXXSRC:%.cpp=build/obj/%.o)
CXX ?= clang++
SDK := $(shell xcrun --show-sdk-path)
CXXFLAGS ?= -O2 -g -std=c++17 -MMD -MP -nostdinc++ -isystem $(SDK)/usr/include/c++/v1
BIN := build/eternam
APP := build/Eternam.app

all: $(APP)

build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $@

$(APP): $(BIN) packaging/Info.plist
	rm -rf $(APP)
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources/data $(APP)/Contents/Frameworks
	cp $(BIN) $(APP)/Contents/MacOS/Eternam
	cp packaging/Info.plist $(APP)/Contents/Info.plist
	[ -f packaging/Eternam.icns ] && cp packaging/Eternam.icns $(APP)/Contents/Resources/ || true
	cp $(SDL_PREFIX)/lib/libSDL3.0.dylib $(APP)/Contents/Frameworks/
	chmod u+w $(APP)/Contents/Frameworks/libSDL3.0.dylib
	install_name_tool -id @rpath/libSDL3.0.dylib $(APP)/Contents/Frameworks/libSDL3.0.dylib
	install_name_tool -change $$(otool -L $(BIN) | awk '/libSDL3/{print $$1}') @rpath/libSDL3.0.dylib $(APP)/Contents/MacOS/Eternam
	if [ "$(BUNDLE_DATA)" = 1 ]; then \
	  for f in $(GAME_DATA)/*.PAK $(GAME_DATA)/*.CC1 $(GAME_DATA)/*.CC4 $(GAME_DATA)/*.DAT $(GAME_DATA)/*.AVE $(GAME_DATA)/*.TAT; do \
	    [ -f "$$f" ] && cp "$$f" $(APP)/Contents/Resources/data/; done; \
	else rmdir $(APP)/Contents/Resources/data; fi; true
	codesign --force --deep -s - $(APP)

run: $(BIN)
	$(BIN) --data $(GAME_DATA)

clean:
	rm -rf build/obj $(BIN) $(APP)

-include $(OBJ:.o=.d)
.PHONY: all run clean
