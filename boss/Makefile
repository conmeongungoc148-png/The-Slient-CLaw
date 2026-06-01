# Makefile for Boss Fight (Agis)

CC = gcc
CFLAGS = -Wall -I../raylib/include -std=c99 -Wno-missing-braces -DSTANDALONE_BOSS
LDFLAGS = -L../raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm

# Source files
SRC = src/main.c src/player.c src/boss.c src/projectile.c src/orb.c src/collision.c src/gamestate.c src/map.c src/boom.c \
      src/skill/projectile_attack.c src/skill/claw.c src/skill/hazard.c src/skill/laser.c src/skill/rain.c src/skill/slam.c
OBJ = $(SRC:.c=.o)
EXE = bossfight.exe

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

run: $(EXE)
	./$(EXE)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f src/*.o src/skill/*.o $(EXE)

.PHONY: all run clean
