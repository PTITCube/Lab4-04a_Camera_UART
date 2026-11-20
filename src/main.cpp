/**
 * VÍ DỤ 4-4a: CHỤP ẢNH QUA UART & LƯU THẺ NHỚ MICROSD
 */
#include <Arduino.h>
#include <PTITCube.h>
#include <SD.h>

PTIT_CDH cdh;
PTIT_EPS eps;
PTIT_Storage storage;

// Chân Camera và IC chuyển mạch 74HC4066
#define CAM_RX_PIN  16
#define CAM_TX_PIN  17
#define CAM_BAUD    115200

#define MUX_GPS_PIN 12
#define MUX_CAM_PIN 14

static int photoCounter = 0;

void selectCamera() {
    digitalWrite(MUX_GPS_PIN, LOW);
    delayMicroseconds(50);
    digitalWrite(MUX_CAM_PIN, HIGH);
}

void selectGPS() {
    digitalWrite(MUX_CAM_PIN, LOW);
    delayMicroseconds(50);
    digitalWrite(MUX_GPS_PIN, HIGH);
}

bool captureImage() {
    if (!storage.isReady()) {
        cdh.println("Camera: The nho chua san sang.");
        return false;
    }

    selectCamera();
    while (Serial2.available()) Serial2.read();

    Serial2.print("capture\n");
    Serial2.flush();

    unsigned long start = millis();
    long imageSize = 0;
    while (millis() - start < 5000) {
        if (Serial2.available()) {
            String line = Serial2.readStringUntil('\n');
            line.trim();
            if (line.startsWith("SIZE:")) {
                imageSize = line.substring(5).toInt();
                break;
            }
        }
    }

    if (imageSize <= 0) {
        cdh.println("Camera: Timeout hoac khong nhan duoc SIZE.");
        selectGPS();
        return false;
    }

    char path[32];
    snprintf(path, sizeof(path), "/photos/uart_img_%d.jpg", photoCounter);
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        cdh.printf("Camera: Khong the tao file %s.\n", path);
        selectGPS();
        return false;
    }

    Serial2.print("OK\n");
    Serial2.flush();

    uint8_t buf[256];
    long received = 0;
    start = millis();

    while (received < imageSize && (millis() - start < 15000)) {
        if (Serial2.available()) {
            int n = Serial2.readBytes(buf, min((int)Serial2.available(), (int)sizeof(buf)));
            if (n > 0) {
                file.write(buf, n);
                received += n;
                start = millis();
            }
        }
    }
    file.close();

    bool success = (received == imageSize);
    if (success) {
        Serial2.print("DONE\n");
        Serial2.flush();
        cdh.printf("Camera: Da luu %s (%ld bytes)\n", path, received);
        photoCounter++;
    } else {
        cdh.printf("Camera: Loi truyen nhan (%ld/%ld bytes)\n", received, imageSize);
    }

    selectGPS();
    return success;
}

void setup() {
    cdh.begin(115200);

    // Kích hoạt hạ áp cấp nguồn ngoại vi trên PCB vệ tinh qua thư viện EPS
    eps.setBuckState(true);

    storage.init();
    if (storage.isReady() && !SD.exists("/photos")) {
        SD.mkdir("/photos");
    }

    pinMode(MUX_GPS_PIN, OUTPUT);
    pinMode(MUX_CAM_PIN, OUTPUT);
    selectGPS();

    Serial2.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
    cdh.println("Camera: San sang. Go 'capture' de chup anh.");
}

void loop() {
    if (cdh.available()) {
        String cmd = cdh.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() == 0) return;

        if (cmd.equalsIgnoreCase("capture") || cmd.equalsIgnoreCase("photo")) {
            captureImage();
        } else {
            cdh.printf("Camera: Lenh '%s' khong hop le. Go 'capture' de chup anh.\n", cmd.c_str());
        }
    }
}
