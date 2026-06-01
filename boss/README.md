# Boss Fight - AGIS

Boss fight prototype với gameplay: né projectile + bắt orb để đánh boss.

## Yêu cầu

1. **MinGW-w64** (GCC compiler cho Windows)
   - Download: https://sourceforge.net/projects/mingw-w64/
   - Hoặc cài qua MSYS2: https://www.msys2.org/
   - Sau khi cài, thêm `C:\mingw64\bin` vào PATH

2. **Raylib** (đã có sẵn trong `../raylib/`)

## Cách build

### Option 1: Dùng Makefile (nếu đã cài MinGW)

```bash
cd boss
mingw32-make all
mingw32-make run
```

### Option 2: Compile thủ công (nếu chưa cài MinGW)

Dùng Visual Studio Developer Command Prompt:

```bash
cd boss
build.bat
```

Hoặc compile từng file:

```bash
gcc -c src/collision.c -o src/collision.o -I../raylib/include -std=c99
gcc -c src/player.c -o src/player.o -I../raylib/include -std=c99
gcc -c src/projectile.c -o src/projectile.o -I../raylib/include -std=c99
gcc -c src/orb.c -o src/orb.o -I../raylib/include -std=c99
gcc -c src/boss.c -o src/boss.o -I../raylib/include -std=c99
gcc -c src/gamestate.c -o src/gamestate.o -I../raylib/include -std=c99
gcc -c src/main.c -o src/main.o -I../raylib/include -std=c99

gcc src/*.o -o bossfight.exe -L../raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
```

## Gameplay

- **A/D**: Di chuyển trái/phải
- **Space**: Nhảy
- **Mục tiêu**: Né projectile màu đỏ từ boss, bắt orb màu vàng rơi xuống, orb sẽ bay về đánh boss
- **Win**: Boss HP = 0
- **Lose**: Player HP = 0
- **R**: Retry sau khi win/lose

## Boss Phases

- **Phase 1** (HP 100%-70%): Bắn 1 projectile, orb spawn mỗi 5s
- **Phase 2** (HP 70%-30%): Bắn 3 projectile, orb spawn mỗi 4s
- **Phase 3** (HP 30%-0%): Bắn 5 projectile fan, orb spawn mỗi 3s

## Cấu trúc code

```
boss/
├── src/
│   ├── main.c          - Game loop chính
│   ├── player.c/h      - Player (mèo) movement + HP
│   ├── boss.c/h        - Boss Agis idle + spawn logic
│   ├── projectile.c/h  - Projectile bay về player
│   ├── orb.c/h         - Orb player bắt để đánh boss
│   ├── collision.c/h   - AABB collision detection
│   └── gamestate.c/h   - Win/Lose/Retry UI
├── assets/
│   └── sprites/
│       ├── agis.png    - Boss sprite (3360x240, 14 frames)
│       └── cat/        - Player sprites
├── Makefile
└── README.md
```

## Troubleshooting

**Lỗi: "mingw32-make not found"**
- Cài MinGW-w64 và thêm vào PATH
- Hoặc dùng Visual Studio để compile

**Lỗi: "cannot find -lraylib"**
- Kiểm tra `../raylib/lib/libraylib.a` có tồn tại không
- Nếu không, build raylib từ source

**Lỗi: "cannot open file agis.png"**
- Chạy game từ thư mục `boss/` (không phải `boss/src/`)
- Đảm bảo `assets/boss/sprites/agis.png` tồn tại