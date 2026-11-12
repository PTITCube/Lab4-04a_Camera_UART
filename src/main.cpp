/**
 * VÍ DỤ 4-4a: CHỤP ẢNH BẰNG GIAO TIẾP UART THUẦN VÀ LƯU VÀO THẺ NHỚ MICROSD
 * 
 * Mục đích:
 * - Minh họa giao thức bắt tay nhị phân mức thấp (Raw Handshake Protocol) giữa
 *   On-Board Computer (ESP32) và module Camera phụ (ESP32-CAM) qua UART2.
 * - Dữ liệu ảnh JPEG nhị phân nhận được từ UART2 sẽ được ghi trực tiếp theo luồng (streaming)
 *   vào thẻ nhớ MicroSD thông qua giao tiếp SPI, đóng vai trò như Hộp đen lưu trữ dữ liệu (Black Box).
 * 
 * Sơ đồ kết nối phần cứng:
 * 1. ESP32-CAM <-> ESP32 OBC (UART2):
 *    - CAM TX2 (GPIO 14) -> ESP32 RX2 (GPIO 16)
 *    - CAM RX2 (GPIO 15) -> ESP32 TX2 (GPIO 17)
 *    - GND               -> GND
 * 2. MicroSD Card <-> ESP32 OBC (SPI Bus):
 *    - SD_CS   -> GPIO 13
 *    - SD_SCK  -> GPIO 18
 *    - SD_MISO -> GPIO 19
 *    - SD_MOSI -> GPIO 23
 * 3. MUX 74HC4066 (Bộ chia kênh UART2 dùng chung với GPS - nếu có):
 *    - MUX_GPS_PIN -> GPIO 12 (LOW khi chụp ảnh)
 *    - MUX_CAM_PIN -> GPIO 14 (HIGH khi chụp ảnh)
 * 
 * Giao thức bắt tay UART giữa OBC và ESP32-CAM:
 *    OBC                          ESP32-CAM
 *     | --- "p\n" -------------->    |  (OBC yêu cầu chụp)
 *     | <--- "SIZE:<bytes>\n" ---    |  (CAM chụp xong và báo kích thước JPEG)
 *     | --- "OK\n" ------------->    |  (OBC sẵn sàng nhận luồng dữ liệu)
 *     | <--- [Binary JPEG Stream] -  |  (CAM truyền byte nhị phân ảnh)
 *     | (Ghi trực tiếp vào SD)       |
 *     | --- "DONE\n" ----------->    |  (OBC báo đã nhận đủ)
 */

#include <Arduino.h>

#include <PTITCube.h>
#include <SPI.h>
#include <SD.h>

PTIT_CDH cdh;

// Định nghĩa chân SPI thẻ nhớ MicroSD (OBC)
#define SD_CS_PIN   13
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

// Định nghĩa chân UART2 giao tiếp với ESP32-CAM
#define CAM_RX_PIN  16
#define CAM_TX_PIN  17
#define CAM_BAUD    115200

// Định nghĩa chân chuyển mạch MUX 74HC4066
#define MUX_GPS_PIN 12
#define MUX_CAM_PIN 14

static int photoCounter = 0;
static bool sdReady = false;

// Hàm chọn kênh giao tiếp cho Camera trên MUX 74HC4066 (Break-Before-Make)
void selectCameraChannel() {
    pinMode(MUX_GPS_PIN, OUTPUT);
    pinMode(MUX_CAM_PIN, OUTPUT);

    // Tắt GPS trước
    digitalWrite(MUX_GPS_PIN, LOW);
    delayMicroseconds(50);
    // Bật kênh Camera sau
    digitalWrite(MUX_CAM_PIN, HIGH);
}

// Khởi tạo thẻ nhớ MicroSD qua SPI
bool initMicroSD() {
    cdh.println("[SD] Khoi tao giao tiep SPI cho the nho MicroSD...");
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
        cdh.println("[SD] LOI: Khong the mount the nho MicroSD!");

        return false;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    cdh.printf("[SD] Mount thanh cong! Dung luong the: %llu MB\n", cardSize);

    // Đảm bảo thư mục /photos tồn tại
    if (!SD.exists("/photos")) {
        if (SD.mkdir("/photos")) {
            cdh.println("[SD] Da tao thu muc /photos.");
        } else {
            cdh.println("[SD] Canh bao: Khong the tao thu muc /photos!");
        }
    }

    return true;
}

// Hàm chụp ảnh qua UART thuần và ghi trực tiếp vào thẻ nhớ
bool captureAndSaveToSD() {
    if (!sdReady) {
        cdh.println("[LOI] The nho SD chua san sang! Khong the luu anh.");
        return false;
    }

    // 1. Chuyển kênh MUX sang Camera
    selectCameraChannel();

    // 2. Dọn sạch bộ đệm UART2
    while (Serial2.available()) {
        Serial2.read();
    }

    // 3. Gửi lệnh chụp 'p\n' đến ESP32-CAM
    cdh.println("\n[UART-CAM] Gui lenh chup anh ('p') toi ESP32-CAM...");
    Serial2.print("p\n");
    Serial2.flush();

    // 4. Chờ ESP32-CAM phản hồi kích thước ảnh: "SIZE:<bytes>"
    cdh.println("[UART-CAM] Cho phan hoi SIZE tu ESP32-CAM (Timeout: 5s)...");
    unsigned long startTime = millis();
    long imageSize = 0;
    bool sizeReceived = false;

    while (millis() - startTime < 5000) {
        if (Serial2.available()) {
            String line = Serial2.readStringUntil('\n');
            line.trim();

            if (line.startsWith("SIZE:")) {
                imageSize = line.substring(5).toInt();
                sizeReceived = true;
                break;
            } else if (line.startsWith("ERROR:")) {
                cdh.printf("[UART-CAM] ESP32-CAM bao loi: %s\n", line.c_str());
                return false;
            }
        }
        delay(1);
    }

    if (!sizeReceived || imageSize <= 0) {
        cdh.println("[UART-CAM] LOI: Khong nhan duoc phan hoi SIZE hoac kich thuoc = 0!");
        return false;
    }

    cdh.printf("[UART-CAM] ESP32-CAM san sang truyen anh, kich thuoc: %ld bytes (%.2f KB)\n", 
                  imageSize, imageSize / 1024.0);

    // 5. Mở file mới trên thẻ MicroSD để ghi dữ liệu JPEG
    char filePath[32];
    snprintf(filePath, sizeof(filePath), "/photos/uart_img_%d.jpg", photoCounter);
    
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
        cdh.printf("[SD] LOI: Khong the tao file ghi tren the nho: %s\n", filePath);
        return false;
    }

    // 6. Gửi xác nhận "OK\n" để ESP32-CAM bắt đầu truyền luồng nhị phân
    Serial2.print("OK\n");
    Serial2.flush();
    cdh.printf("[SD] Bat dau ghi du lieu truc tiep vao file: %s ...\n", filePath);

    // 7. Tiếp nhận luồng byte qua UART2 và ghi theo khối vào thẻ nhớ
    uint8_t buffer[256];
    long totalBytesReceived = 0;
    startTime = millis();
    unsigned long lastProgressTime = 0;

    while (totalBytesReceived < imageSize && (millis() - startTime < 15000)) {
        if (Serial2.available()) {
            int availableBytes = Serial2.available();
            if (availableBytes > (int)sizeof(buffer)) {
                availableBytes = sizeof(buffer);
            }

            int bytesRead = Serial2.readBytes(buffer, availableBytes);
            if (bytesRead > 0) {
                file.write(buffer, bytesRead);
                totalBytesReceived += bytesRead;
                startTime = millis(); // Reset timeout khi dang nhan du lieu lien tuc
            }
        }

        // In tiến độ mỗi 500ms
        if (millis() - lastProgressTime >= 500) {
            float percent = (float)totalBytesReceived / imageSize * 100.0f;
            cdh.printf("  -> Da nhan: %ld / %ld bytes (%.1f%%)\n", totalBytesReceived, imageSize, percent);
            lastProgressTime = millis();
        }
    }

    // 8. Đóng file và kiểm tra tính toàn vẹn
    file.flush();
    file.close();

    bool success = false;
    if (totalBytesReceived == imageSize) {
        Serial2.print("DONE\n");
        Serial2.flush();

                cdh.printf("[THANH CONG] Da luu anh vao the nho: %s\n", filePath);
        cdh.printf("             Tong dung luong: %ld bytes\n", totalBytesReceived);
                photoCounter++;
        success = true;
    } else {
        cdh.printf("[LOI] Timeout truyen nhan! Chi nhan duoc %ld / %ld bytes.\n", 
                      totalBytesReceived, imageSize);
        success = false;
    }

    return success;
}

void setup() {
    cdh.begin(115200);
    delay(1000);

        cdh.println("   LAB 4-04a: CHUP ANH UART THUAN & LUU VAO THE NHO SD  ");
    
    // Khởi tạo thẻ nhớ MicroSD
    sdReady = initMicroSD();
    if (!sdReady) {
        cdh.println("[CANH BAO] Thao tac ghi file se khong kha dung neu khong co the nho!");
    }

    // Khởi tạo UART2 kết nối với module ESP32-CAM
    Serial2.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
    cdh.printf("[UART-CAM] UART2 da khoi dong (Baudrate: %d, RX: %d, TX: %d)\n", 
                  CAM_BAUD, CAM_RX_PIN, CAM_TX_PIN);

    cdh.println("[READY] Nhap 'p' tren cdh Monitor de chup anh va luu vao MicroSD.");
}

void loop() {
    // Lắng nghe lệnh từ cdh Monitor
    if (cdh.available()) {
        char cmd = cdh.read();

        // Bỏ qua ký tự xuống dòng
        if (cmd == '\r' || cmd == '\n') {
            return;
        }

        if (cmd == 'p' || cmd == 'P') {
            captureAndSaveToSD();
        } else {
            cdh.printf("[INFO] Ky tu '%c' khong hop le. Vui long nhap 'p' de chup anh.\n", cmd);
        }
    }
}
