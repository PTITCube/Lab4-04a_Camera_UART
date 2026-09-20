/**
 * VÍ DỤ 4-4a: CHỤP ẢNH BẰNG GIAO THỨC UART THUẦN ĐẾN MODULE CAM
 */
#include <Arduino.h>

#define CAM_RX 32
#define CAM_TX 33

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, CAM_RX, CAM_TX);
    
    Serial.println("\n[RAW] Khởi động module UART Camera...");
    delay(2000); // Chờ cam khởi động
    
    Serial.println("[RAW] Gửi tín hiệu 'C' (Chụp ảnh) qua UART đến Cam...");
    Serial1.println("C");
}

void loop() {
    // Chuyển tiếp luồng dữ liệu (log và dữ liệu ảnh hex) từ Camera về Serial màn hình
    while (Serial1.available()) {
        Serial.write(Serial1.read());
    }
}
