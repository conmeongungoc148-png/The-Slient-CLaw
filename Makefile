# Makefile for The Forest (Integrated Map System)

CC = gcc
CFLAGS = -Wall -Iraylib/include -Iboss/src -std=c99 -Wno-missing-braces
LDFLAGS = -Lraylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm

# Tệp nguồn chính
SRC = src/main.c src/game.c src/camera.c src/map.c \
      boss/src/boss.c boss/src/boss_player.c boss/src/projectile.c \
      boss/src/orb.c boss/src/boom.c src/collision.c src/gamestate.c \
      boss/src/skill/claw.c boss/src/skill/laser.c boss/src/skill/slam.c \
      boss/src/skill/hazard.c boss/src/skill/rain.c boss/src/skill/projectile_attack.c
OBJ = $(SRC:.c=.o)
EXE = theforest.exe

# Shell detection for cross-platform clean command
ifeq ($(OS),Windows_NT)
    ifeq ($(findstring sh,$(SHELL)),sh)
        RM = rm -f
        RM_FILES = src/*.o boss/src/*.o boss/src/skill/*.o $(EXE)
    else
        RM = del /f /q
        RM_FILES = src\*.o boss\src\*.o boss\src\skill\*.o $(EXE)
    endif
else
    RM = rm -f
    RM_FILES = src/*.o boss/src/*.o boss/src/skill/*.o $(EXE)
endif

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

run: $(EXE)
	./$(EXE)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	$(RM) $(RM_FILES)

# Standalone Boss Fight targets
boss:
	$(MAKE) -C boss

boss-run:
	$(MAKE) -C boss run

boss-clean:
	$(MAKE) -C boss clean

.PHONY: all run clean boss boss-run boss-clean
