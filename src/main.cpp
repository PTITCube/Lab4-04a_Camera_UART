/**
 * VÍ DỤ 4-4a: CHỤP ẢNH BẰNG GIAO THỨC UART THUẦN ĐẾN MODULE CAM
 * 
 * ESP32-CAM sử dụng UART2 (GPIO 14 TX / GPIO 15 RX) để giao tiếp.
 * ESP32 chính (OBC) kết nối qua UART2 (GPIO 16 RX / GPIO 17 TX).
 * 
 * Kết nối dây:
 * ESP32-CAM GPIO14 (TX2) -> ESP32 GPIO16 (RX2)
 * ESP32-CAM GPIO15 (RX2) -> ESP32 GPIO17 (TX2)
 * GND                     -> GND
 */
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // UART2 kết nối với ESP32-CAM (baudrate 115200)
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    
    Serial.println("\n[RAW] Khởi động module UART Camera...");
    delay(2000); // Chờ cam khởi động
    
    Serial.println("[RAW] Gửi tín hiệu 'p' qua UART2 đến ESP32-CAM...");
    Serial2.println("p");
}

void loop() {
    // Chuyển tiếp luồng dữ liệu (log và dữ liệu ảnh hex) từ Camera về Serial màn hình
    while (Serial2.available()) {
        Serial.write(Serial2.read());
    }
}
