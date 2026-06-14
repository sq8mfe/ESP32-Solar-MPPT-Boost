#include <WiFi.h>
#include <WebServer.h>

// Piny
#define PWM_PIN     25
#define V_ADC_PIN   34
#define I_ADC_PIN   35

WebServer server(80);

float Vpv, Ipv, Ppv, Pprev = 0;
float duty = 0.5;
float step = 0.01;

void setup() {
  Serial.begin(115200);
  pinMode(PWM_PIN, OUTPUT);
  WiFi.softAP("Solar_MPPT");
  server.on("/", []() {
    server.send(200, "text/plain", "MPPT Online");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  // Odczyt ADC
  Vpv = analogRead(V_ADC_PIN) * (40.0 / 4095.0);
  Ipv = analogRead(I_ADC_PIN) * (30.0 / 4095.0);
  Ppv = Vpv * Ipv;

  // P&O MPPT
  if (Ppv > Pprev) {
    duty += step;
  } else {
    duty -= step;
  }
  duty = constrain(duty, 0.1, 0.8);
  Pprev = Ppv;

  // PWM
  ledcWrite(0, duty * 255);  // 8-bit PWM

  delay(100);
}