# NvProfileSwitcher

Ứng dụng C++ GUI nhỏ gọn dùng để chuyển đổi nhanh cấu hình đồ họa NVIDIA Base Profile (thông qua NVAPI DRS) phục vụ cho giả lập Android (BlueStacks) chơi Minecraft hoặc trở về cấu hình mặc định.

## Tính năng

- 🎮 **Minecraft Graphic**: Bật các tùy chọn khử răng cưa, Texture filtering LOD bias, Anisotropic filter, VRR,... tối ưu trải nghiệm đồ họa Minecraft.
- 🔄 **Normal Profile**: Phục hồi các cài đặt NVIDIA về trạng thái thông thường.
- 🎨 **Giao diện hiện đại**: Dark theme, nút bấm bo góc có hiệu ứng hover mượt mà.
- ⚡ **Siêu nhẹ**: Build native C++ Win32 không phụ thuộc runtime cồng kềnh.

## Yêu cầu

- Windows 10/11 (64-bit)
- Card đồ họa NVIDIA & driver NVIDIA đã cài đặt (`nvapi64.dll`)
- Chạy với quyền **Administrator** để lưu thiết lập vào NVIDIA Base Profile.

## Cách build

### Sử dụng Visual Studio (MSVC)
Chạy trực tiếp file `build.bat` hoặc mở Developer Command Prompt for VS và chạy:

```cmd
cl /EHsc /O2 /utf-8 main.cpp /Fe:NvProfileSwitcher.exe /link user32.lib gdi32.lib comctl32.lib shell32.lib /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
```
