# Boss Map - Thứ tự Layer CHÍNH XÁC

## ✅ Code đã xử lý ĐÚNG thứ tự layer!

### Cách cute_tiled xử lý:
1. **Đọc JSON**: Layers được đọc theo thứ tự trong file (sky → mountain → Group Layer 1)
2. **Tự động REVERSE**: `cute_tiled` tự động reverse linked list → thứ tự ngược lại
3. **Flatten Group**: Hàm `ProcessLayers()` trong `game.c` flatten tất cả group layers
4. **Kết quả**: Layers được vẽ theo đúng thứ tự Z-order từ dưới lên trên

## Thứ tự vẽ và tỷ lệ Parallax (từ xa tới gần):

### 1. Rất xa (Parallax factor: 0.3)
- **sky** (objectgroup) - Bầu trời ngoài cùng, cuộn cực kỳ chậm theo camera.

### 2. Xa (Parallax factor: 0.6)
- **mountain** (objectgroup) - Núi đồi phía sau, cuộn chậm.

### 3. Trung cảnh / Mặc định (Parallax factor: 1.0)
- **fence** (tilelayer) - Hàng rào.
- **building** (objectgroup) - Các tòa nhà.
- **black** (objectgroup) - Lớp phủ tối.
- **abc** (objectgroup) - Các đối tượng đặc biệt.
- **tiles** (tilelayer) - Nền đất chính.
- **props** (tilelayer) - Các chi tiết trang trí tiền cảnh.

### 4. Gần / Cận cảnh (Parallax factor: 0.9)
- **tiles2** (tilelayer) - Các mảng nền đất phụ.
- **statue** (objectgroup) - Các bức tượng đá.

### 5. Player & Boss Layers
- **Player** & **Boss** - Vẽ tương tác trực tiếp theo tọa độ thế giới (Parallax factor: 1.0).

### 6. ground (objectgroup) ⭐ - **KHÔNG VẼ** - chỉ dùng tính toán va chạm vật lý!

## Layer "ground" - Collision Only

Layer "ground" **KHÔNG ĐƯỢC VẼ** ra màn hình:
- Code skip layer này khi vẽ
- Chỉ dùng cho collision detection trong `UpdatePlayer()`
- Chứa 3 collision rectangles:
  - **Sàn chính**: 1217x64 tại (0.67, 370.67 + 50 offset) = (0.67, 420.67)
  - **Platform trái**: 352x20 tại (0, 240 + 50 offset) = (0, 290)
  - **Platform phải**: 350x20 tại (864, 240 + 50 offset) = (864, 290)

## Debug Mode

Nhấn giữ phím **G** trong game để xem collision boxes của layer "ground" (màu xanh lá).

## Code Implementation

```c
// Trong main.c - Vẽ layers
for (int i = 0; i < gameMap.layerCount; i++) {
    TMJLayer *layer = &gameMap.layers[i];
    
    // Skip layer "ground" - chỉ dùng collision
    if (strstr(layer->name, "ground") != NULL) {
        // Debug: Vẽ collision boxes nếu nhấn G
        if (IsKeyDown(KEY_G)) {
            // Vẽ green rectangles...
        }
        continue; // Không vẽ layer ground
    }
    
    // Vẽ các layers khác...
}
```

```c
// Trong game.c - ProcessLayers flatten group layers
static void ProcessLayers(cute_tiled_layer_t *layer, GameMap *map,
                          int *currentLayerIdx, float parentOffsetX,
                          float parentOffsetY) {
    while (layer) {
        if (strcmp(layer->type.ptr, "group") == 0) {
            // Đệ quy vào group, cộng offset của parent
            ProcessLayers(layer->layers, map, currentLayerIdx,
                        parentOffsetX + layer->offsetx,
                        parentOffsetY + layer->offsety);
        } else {
            // Xử lý layer thường, lưu offset tổng
            tmjLayer->offsetx = parentOffsetX + layer->offsetx;
            tmjLayer->offsety = parentOffsetY + layer->offsety;
            // ...
        }
        layer = layer->next;
    }
}
```

## Kết luận

✅ Thứ tự layer **ĐÃ ĐÚNG** - code tự động xử lý:
- Flatten group layers
- Cộng offset của parent vào child
- Vẽ theo đúng thứ tự Z-order
- Skip layer "ground" khi vẽ (chỉ dùng collision)

Nhân vật sẽ đứng trên các collision rectangles trong layer "ground" với offset đã được tính sẵn!
