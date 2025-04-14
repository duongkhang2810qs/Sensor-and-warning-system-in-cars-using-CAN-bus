#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>

// Địa chỉ I2C của LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // 16 cột, 2 dòng

// Khai báo Servo
#define SERVO1_PIN PA1  // Chân điều khiển servo 1 (PWM)
#define SERVO2_PIN PA2  // Chân điều khiển servo 2 (PWM)
Servo myServo1;         // Đối tượng điều khiển Servo 1
Servo myServo2;         // Đối tượng điều khiển Servo 2

// Khai báo LED
#define LED1_PIN PB0          // LED1
#define LED2_PIN PB1          // LED2
#define WARNING_LED_PIN PB10  // LED cảnh báo khi vật thể gầnS


// Khai báo Buzzer
#define BUZZER_PIN PA0  // Chân điều khiển còi buzzer

#define SPI_CS_PIN PA4  // Chân CS của MCP2515 nối với PA4
MCP2515 mcp2515(SPI_CS_PIN);  // Khởi tạo đối tượng MCP2515

struct can_frame canMsg;  // Thông điệp CAN

void setup() {
  // Khởi tạo giao tiếp Serial
  Serial.begin(115200);

  // Khởi tạo LCD và bật đèn nền
  lcd.init();
  lcd.backlight();

  // In thông điệp ban đầu trên LCD
  lcd.setCursor(0, 0);
  lcd.print("Waiting for data...");

  // Khởi tạo 2 servo
  myServo1.attach(SERVO1_PIN);  // Gắn servo 1 vào chân PA1
  myServo2.attach(SERVO2_PIN);  // Gắn servo 2 vào chân PA2
  myServo1.write(0);  // Đặt Servo 1 về vị trí ban đầu (0 độ)
  myServo2.write(0);  // Đặt Servo 2 về vị trí ban đầu (0 độ)

  // Cấu hình LED
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(WARNING_LED_PIN, OUTPUT);

  // Cấu hình Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // Khởi tạo MCP2515
  mcp2515.reset(); // Reset module MCP2515
  mcp2515.setBitrate(CAN_125KBPS); // Đặt tốc độ CAN là 125Kbps
  mcp2515.setNormalMode(); // Đặt chế độ bình thường của MCP2515

  Serial.println("Receiver: Waiting for messages");
  
  delay(1000);
}

void loop() {
  // Kiểm tra nếu có thông điệp CAN đến
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x100) {  // Kiểm tra ID
      // Đọc trạng thái mưa
      int rainStatus = canMsg.data[0];  // Trạng thái mưa (0: không mưa, 1: mưa)
      // Hiển thị trạng thái mưa trên LCD
      lcd.setCursor(0, 1);
      if (rainStatus == 1) {
        lcd.print("Rain    "); // In trạng thái mưa
        Serial.println("Received Rain Status: Rain"); // In ra Serial Monitor
        // Điều khiển servo quay qua quay lại (mô phỏng cần gạt nước)
        for (int pos = 0; pos <= 180; pos += 2) {
          myServo1.write(pos);  // Quay servo từ 0 đến 180 độ
          myServo2.write(pos);
          delay(15);
        }
        for (int pos = 180; pos >= 0; pos -= 2) {
          myServo1.write(pos);  // Quay servo từ 180 đến 0 độ
          myServo2.write(pos);
          delay(15);
        }
      } else {
        lcd.print("Dry     ");  // In trạng thái không mưa
        Serial.println("Received Rain Status: Dry"); // In ra Serial Monitor
        myServo1.write(0);  // Đặt servo về vị trí 0 độ (tắt gạt nước)
        myServo2.write(0);
      }

      // Đọc dữ liệu nhiệt độ và độ ẩm
      int temperature = canMsg.data[1];  // Nhiệt độ
      int humidity = canMsg.data[2];     // Độ ẩm

      // Hiển thị nhiệt độ và độ ẩm trên LCD
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temperature);
      lcd.print((char)223); 
      lcd.print("C  ");
      lcd.setCursor(8, 0);
      lcd.print("RH:");
      lcd.print(humidity);
      lcd.print("%   ");

      // In ra Serial Monitor
      Serial.print("Received Temperature: ");
      Serial.print(temperature);
      Serial.print("°C, Humidity: ");
      Serial.print(humidity);
      Serial.println("%");

      // Đọc trạng thái ánh sáng
      int lightStatus = canMsg.data[3];  // Trạng thái ánh sáng (0: sáng, 1: tối)
      // Hiển thị trạng thái ánh sáng trên LCD
      if (lightStatus == 1) {
        lcd.setCursor(8, 1);
        lcd.print("Dark    ");
        digitalWrite(LED1_PIN, HIGH);
        digitalWrite(LED2_PIN, HIGH);  // Bật LED2 nếu trời tối
        Serial.println("Received Light Status: Dark");
      } else {
        lcd.setCursor(8, 1);
        lcd.print("Bright  ");
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);   // Tắt LED2 nếu trời sáng
        Serial.println("Received Light Status: Bright");
      }

      // Đọc trạng thái khoảng cách
      int distanceStatus = canMsg.data[4];   // Trạng thái còi (0: không bíp, 1: bíp)
      // Điều khiển còi
      if (distanceStatus == 1) {
        digitalWrite(BUZZER_PIN, HIGH);  // Bật còi khi có vật thể quá gần
        Serial.println("Received Distance Status: Dangerous");
        // Nhấp nháy Warning LED khi vật thể gần
        digitalWrite(WARNING_LED_PIN, HIGH); // Bật LED cảnh báo
        delay(500); // Nhấp nháy trong 500ms
        digitalWrite(WARNING_LED_PIN, LOW);  // Tắt LED cảnh báo
      } else {
        digitalWrite(BUZZER_PIN, LOW);   // Tắt còi khi không có vật thể gần
        Serial.println("Received Distance Status: Safe");
      }
    }
  }

  delay(500);
}
