#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Arduino_JSON.h>
#include <SPI.h>
#include <mcp2515.h>
#define SPI_CS_PIN 5  // Chân CS của MCP2515 nối với PA4
MCP2515 mcp2515(SPI_CS_PIN);  // Khởi tạo đối tượng MCP2515

struct can_frame canMsg;  // Thông điệp CAN

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Car Sensor Dashboard - Team 2</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      text-align: center;
      background-color: #f7f7f7;
    }
    h1 {
      background-color: #4CAF50;
      color: white;
      padding: 20px;
      margin: 0;
    }
    .container {
      display: grid;
      grid-template-columns: repeat(2, 1fr); /* 2 cột */
      grid-gap: 20px; /* Khoảng cách giữa các phần tử */
      margin: 20px auto;
      padding: 0 20px;
      max-width: 600px; /* Giới hạn chiều rộng tối đa */
    }
    .card {
      background-color: white;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);
      border-radius: 8px;
      padding: 20px;
      text-align: center;
    }
    .card .title {
      font-size: 1.2rem;
      font-weight: bold;
      margin-bottom: 10px;
    }
    .card .value {
      font-size: 2rem;
      font-weight: bold;
    }
    .temperature {
      border-top: 5px solid #FF5722;
    }
    .humidity {
      border-top: 5px solid #2196F3;
    }
    .light {
      border-top: 5px solid #FFC107;
    }
    .rain {
      border-top: 5px solid #9C27B0;
    }
  </style>
</head>
<body>
  <h1>Car Sensor Dashboard - Team 2</h1>
  <div class="container">
    <div class="card temperature">
      <div class="title">Temperature</div>
      <div class="value"><span id="temperature">0</span> &deg;C</div>
    </div>
    <div class="card humidity">
      <div class="title">Humidity</div>
      <div class="value"><span id="humidity">0</span> %</div>
    </div>
    <div class="card light">
      <div class="title">Light</div>
      <div class="value"><span id="light">0</span></div>
    </div>
    <div class="card rain">
      <div class="title">Rain</div>
      <div class="value"><span id="rain">0</span></div>
    </div>
  </div>

  <script>
    if (!!window.EventSource) {
      var source = new EventSource('/events');
      source.addEventListener('sensor_data', function(e) {
        var data = JSON.parse(e.data);
        document.getElementById("temperature").innerHTML = data.temperature;
        document.getElementById("humidity").innerHTML = data.humidity;
        document.getElementById("light").innerHTML = data.light;
        document.getElementById("rain").innerHTML = data.rain;
      }, false);
    }
  </script>
</body>
</html>
)rawliteral";




// JSON object to hold sensor data
JSONVar sensorData;

// Create server
AsyncWebServer server(80);
AsyncEventSource events("/events");


void connectWifi(){
  // Connect to Wi-Fi
  String ssid="Long";
  String password = "00000001";

  Serial.begin(115200);

  // Kết nối Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

}
void disconnectWifi(){
  // Ngắt kết nối Wi-Fi vì không cần nữa
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  connectWifi();
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    Serial.println("Client connected!");
  });
  server.addHandler(&events);
  server.begin();
  SPI.begin();
  // Khởi tạo MCP2515
  mcp2515.reset(); // Reset module MCP2515
  mcp2515.setBitrate(CAN_125KBPS); // Đặt tốc độ CAN là 125Kbps
  mcp2515.setNormalMode(); // Đặt chế độ bình thường của MCP2515

  Serial.println("Receiver: Waiting for messages");
  
  delay(1000);
}
int temperature, humidity, rainStatus, lightStatus;
void loop() {
  // Kiểm tra nếu có thông điệp CAN đến
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x100) {  // Kiểm tra ID
      // Đọc trạng thái mưa
      rainStatus = canMsg.data[0];  // Trạng thái mưa (0: không mưa, 1: mưa)
      if (rainStatus == 1) {
        Serial.println("Received Rain Status: Rain"); // In ra Serial Monitor
      } else {
        Serial.println("Received Rain Status: Dry"); // In ra Serial Monitor
      }

      // Đọc dữ liệu nhiệt độ và độ ẩm
      temperature = canMsg.data[1];  // Nhiệt độ
      humidity = canMsg.data[2];     // Độ ẩm

      // In ra Serial Monitor
      Serial.print("Received Temperature: ");
      Serial.print(temperature);
      Serial.print("°C, Humidity: ");
      Serial.print(humidity);
      Serial.println("%");

      // Đọc trạng thái ánh sáng
      lightStatus = canMsg.data[3];  // Trạng thái ánh sáng (0: sáng, 1: tối)
      if (lightStatus == 1) {
        Serial.println("Received Light Status: Dark");
      } else {
        Serial.println("Received Light Status: Bright");
      }

      // Đọc trạng thái khoảng cách
      int distanceStatus = canMsg.data[4];   // Trạng thái còi (0: không bíp, 1: bíp)
      // Điều khiển còi
      if (distanceStatus == 1) {
        Serial.println("Received Distance Status: Dangerous");
      } else {
        Serial.println("Received Distance Status: Safe");
      }
    }
  }

  String temp = String(temperature);
  String hum = String(humidity);
  String rain = String(rainStatus?"Raining":"Dry");
  String light = String(lightStatus? "Dark":"Bright");
  // Gửi dữ liệu lên web server
    sensorData["temperature"] = temp;
    sensorData["humidity"] = hum;
    sensorData["light"] = light;
    sensorData["rain"] = rain;
    static unsigned long lastTime = 0;
    unsigned long now = millis();
    if (now - lastTime > 1000) {
      String jsonString = JSON.stringify(sensorData);
      events.send(jsonString.c_str(), "sensor_data", millis());
      lastTime = now;
    }
    delay(500);
}
