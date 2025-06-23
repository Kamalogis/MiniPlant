#include <WiFi.h>
#include <PubSubClient.h>

//WiFi
const char* ssid = "Samsung A2";
const char* password = "Mulyadi55";

//IP Broker (Mosquitto) -> (Kebetulan Broker pake Laptop jadi make IP laptop)
const char* mqtt_server = "192.168.128.71";

WiFiClient espClient;
PubSubClient client(espClient);

int angka = 1;
int angka2 = 20;

//Setup WIFI
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi terhubung!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

//Connect Broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Terhubung!");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Kirim angka ke MQTT
  char buffer[10];
  char buffer2[10];
  itoa(angka, buffer, 10);
  itoa(angka2, buffer2, 10);
  client.publish("sensor/angka", buffer);
  client.publish("sensor/bukan", buffer2);

  Serial.print("Mengirim angka: ");
  Serial.println(angka);

  angka++;
  if (angka > 100) angka = 1;

  delay(1000);
}
