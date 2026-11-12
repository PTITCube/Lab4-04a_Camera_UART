# Lab4-04a_Camera_UART

**Chụp ảnh bằng giao thức UART thuần và lưu trữ trực tiếp vào thẻ nhớ MicroSD (Black Box)**

Bài thực hành thuộc Module 4 (Cảm biến & Tải trọng quang học) trong hệ thống CubeSat-LMS.

---

## 1. Mục tiêu bài học
- Hiểu và lập trình cơ chế bắt tay mức nhị phân (Binary Handshake Protocol) giữa On-Board Computer (ESP32) và module phụ tải trọng Camera (ESP32-CAM) qua UART2 mà không dùng thư viện đóng gói sẵn.
- Thực hiện ghi dữ liệu ảnh JPEG nhị phân theo luồng trực tiếp (Data Streaming) từ UART vào hệ thống tập tin trên thẻ nhớ MicroSD qua bus SPI.
- Mô phỏng nguyên lý lưu trữ hộp đen (Black Box Non-Volatile Storage) trong hệ thống vệ tinh CubeSat trước khi thực hiện downlink về trạm mặt đất.

---

## 2. Sơ đồ kết nối phần cứng

### A. Giao tiếp UART2 giữa ESP32 (OBC) và ESP32-CAM
| Chân ESP32 (OBC) | Chân ESP32-CAM | Chức năng |
| :--- | :--- | :--- |
| **GPIO 16 (RX2)** | **GPIO 14 (TX2)** | Nhận luồng dữ liệu byte ảnh từ Camera |
| **GPIO 17 (TX2)** | **GPIO 15 (RX2)** | Gửi tín hiệu điều khiển (`p`, `OK`, `DONE`) |
| **GND** | **GND** | Nối chung mass |

### B. Giao tiếp SPI giữa ESP32 (OBC) và khe thẻ nhớ MicroSD
| Chân ESP32 (OBC) | Khe thẻ MicroSD | Chức năng |
| :--- | :--- | :--- |
| **GPIO 13** | **CS** | Chip Select thẻ nhớ |
| **GPIO 18** | **SCK** | SPI Clock |
| **GPIO 19** | **MISO** | SPI Master In Slave Out |
| **GPIO 23** | **MOSI** | SPI Master Out Slave In |
| **3V3 / 5V** | **VCC** | Nguồn cấp |
| **GND** | **GND** | Mass |

### C. Bộ chuyển mạch MUX 74HC4066 (nếu bo mạch dùng chung UART2 với GPS)
- `MUX_CAM_PIN` (GPIO 14): Kích hoạt `HIGH` để mở thông kênh Camera.
- `MUX_GPS_PIN` (GPIO 12): Kích hoạt `LOW` để ngắt kênh GPS.

---

## 3. Giao thức bắt tay UART (Raw Handshake Protocol)

```
        OBC (ESP32)                              ESP32-CAM
             |                                       |
             | ---------- "p\n" (Chụp ảnh) --------> |
             |                                       | (Chụp cảm biến OV2640)
             | <-------- "SIZE:<bytes>\n" ---------- |
             |                                       |
    (Mở file trên SD)                                |
             |                                       |
             | ------------ "OK\n" (Sẵn sàng) ------> |
             |                                       |
             | <------- [Binary JPEG Stream] ------- |
    (Ghi khối buffer vào SD)                         |
             |                                       |
             | ---------- "DONE\n" (Hoàn tất) -----> |
             v                                       v
```

---

## 4. Hướng dẫn thực hành
1. Lắp thẻ nhớ MicroSD đã định dạng **FAT32** vào khe thẻ nhớ trên bo mạch.
2. Kết nối OBC ESP32 với máy tính qua cổng USB.
3. Mở Serial Monitor ở tốc độ baud **115200**.
4. Khi hệ thống khởi động và mount thành công thẻ nhớ, nhập ký tự `p` hoặc `P` rồi gửi:
   - ESP32 gửi lệnh `p\n` tới ESP32-CAM.
   - ESP32-CAM phản hồi `SIZE:<dung_luong>`.
   - ESP32 mở file `/photos/uart_img_<id>.jpg` trên MicroSD và xác nhận `OK`.
   - Toàn bộ luồng byte JPEG được ghi trực tiếp vào thẻ nhớ.
   - Sau khi hoàn thành, file được đóng và kiểm tra tính toàn vẹn dung lượng.
5. Tháo thẻ nhớ cắm vào máy tính để kiểm tra các file ảnh đã chụp trong thư mục `/photos/`.
