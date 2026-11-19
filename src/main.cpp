/**
 * VÍ DỤ 4-4a: CHỤP ẢNH QUA UART THUẦN & LƯU THẺ NHỚ MICROSD
 */
#include <Arduino.h>
#include <PTITCube.h>
#include <SPI.h>
#include <SD.h>

PTIT_CDH cdh;

#define SD_CS_PIN   13
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

#define CAM_RX_PIN  16
#define CAM_TX_PIN  17
#define CAM_BAUD    115200

#define MUX_GPS_PIN 12
#define MUX_CAM_PIN 14

static int photoCounter = 0;
static bool sdReady = false;

void selectCameraChannel() {
    pinMode(MUX_GPS_PIN, OUTPUT);
    pinMode(MUX_CAM_PIN, OUTPUT);
    digitalWrite(MUX_GPS_PIN, LOW);
    delayMicroseconds(50);
    digitalWrite(MUX_CAM_PIN, HIGH);
}

bool captureAndSaveToSD() {
    if (!sdReady) {
        cdh.println("Camera: The nho SD chua san sang.");
        return false;
    }

    selectCameraChannel();
    while (Serial2.available()) Serial2.read();

    // Gửi lệnh chụp đến ESP32-CAM
    Serial2.print("capture\n");
    Serial2.flush();

    // Chờ ESP32-CAM phản hồi SIZE:<bytes>
    unsigned long start = millis();
    long imageSize = 0;
    while (millis() - start < 5000) {
        if (Serial2.available()) {
            String line = Serial2.readStringUntil('\n');
            line.trim();
            if (line.startsWith("SIZE:")) {
                imageSize = line.substring(5).toInt();
                break;
            } else if (line.startsWith("ERROR:")) {
                cdh.printf("Camera: ESP32-CAM bao loi: %s\n", line.c_str());
                return false;
            }
        }
        delay(1);
    }

    if (imageSize <= 0) {
        cdh.println("Camera: Timeout hoac khong nhan duoc phan hoi SIZE.");
        return false;
    }

    char path[32];
    snprintf(path, sizeof(path), "/photos/uart_img_%d.jpg", photoCounter);
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        cdh.printf("Camera: Khong the tao file %s tren the nho.\n", path);
        return false;
    }

    // Bắt đầu nhận luồng ảnh
    Serial2.print("OK\n");
    Serial2.flush();

    uint8_t buf[256];
    long received = 0;
    start = millis();

    while (received < imageSize && (millis() - start < 15000)) {
        if (Serial2.available()) {
            int toRead = min((int)Serial2.available(), (int)sizeof(buf));
            int n = Serial2.readBytes(buf, toRead);
            if (n > 0) {
                file.write(buf, n);
                received += n;
                start = millis();
            }
        }
    }

    file.flush();
    file.close();

    if (received == imageSize) {
        Serial2.print("DONE\n");
        Serial2.flush();
        cdh.printf("Camera: Da luu %s (%ld bytes)\n", path, received);
        photoCounter++;
        return true;
    }

    cdh.printf("Camera: Loi truyen nhan (%ld/%ld bytes)\n", received, imageSize);
    return false;
}

void setup() {
    cdh.begin(115200);

    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdReady = SD.begin(SD_CS_PIN, SPI, 8000000);
    if (sdReady && !SD.exists("/photos")) {
        SD.mkdir("/photos");
    }

    Serial2.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
    cdh.println("Camera: San sang. Go 'capture' de chup anh.");
}

void loop() {
    if (cdh.available()) {
        String cmd = cdh.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() == 0) return;

        if (cmd.equalsIgnoreCase("capture") || cmd.equalsIgnoreCase("photo")) {
            captureAndSaveToSD();
        } else {
            cdh.printf("Camera: Lenh '%s' khong hop le. Go 'capture' de chup anh.\n", cmd.c_str());
        }
    }
}
