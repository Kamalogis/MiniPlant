#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include <ESP32Ping.h>

//Konfigurasi WiFi
const char* ssid = "KAMALOGIS123";
const char* password = "kamalogis123";

//Konfigurasi IP ESP (wajib satu jaringan dengan PLC)
IPAddress localIP(10, 10, 17, 201);
IPAddress gateway(10, 10, 17, 1);
IPAddress subnet(255, 0, 0, 0);

//IP PLC 
IPAddress plcIP(10, 10, 17, 210);

//Modbus TCP
ModbusIP mb;

void setup() {
  Serial.begin(115200);
  delay(1000);

  //Konfigurasi IP statis untuk ESP
  WiFi.config(localIP, gateway, subnet);

  //Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Terhubung!");
  Serial.print("Alamat IP ESP: ");
  Serial.println(WiFi.localIP());

  delay(1000);

  // Inisialisasi sebagai Modbus client
  mb.client();

  // Ping ke PLC
  if (Ping.ping(plcIP)) {
    Serial.println("Ping ke PLC berhasil!");
  } else {
    Serial.println("Ping ke PLC gagal!");
  }

  // Hubungkan ke PLC (Modbus server)
  if (mb.connect(plcIP)) {
    Serial.println("Modbus TCP connect ke PLC berhasil");
  } else {
    Serial.println("Gagal connect ke PLC (Modbus TCP)");
  }
}

void loop() {
  mb.task();

  Serial.println("🔁 Mencoba menulis ke PLC...");

  delay(2000);
  // Menulis TRUE %M1)
  bool success = mb.writeCoil(plcIP, 1, true);  // writeCoil(PLCIp, Address, value)

  if (success) {
    Serial.println("Berhasil menulis TRUE ke %M0 di PLC!");
    delay(1000);
    mb.writeCoil(plcIP, 1, false);  
  } else {
    Serial.println("Gagal menulis ke PLC. Periksa koneksi atau mapping.");
  }

  delay(1000); 
}
