#include <WiFi.h>
#include <WebServer.h>
#include <driver/adc.h>
#include <driver/ledc.h>

// ----------------- Konfiguracja WiFi -----------------
const char* ssid = "Solar_MPPT";
const char* password = "password123";
WebServer server(80);

// ----------------- Konfiguracja Hardware -----------------
#define PWM_PIN         25
#define V_PV_ADC_PIN    34
#define I_PV_ADC_PIN    35
#define PWM_FREQ        20000
#define PWM_RESOLUTION  10

// ----------------- Zmienne MPPT -----------------
float V_pv = 0.0, I_pv = 0.0, P_pv = 0.0, P_prev = 0.0;
float D = 0.5;
float delta_D = 0.01;
bool mppt_enabled = true;

// ----------------- Strona HTML do monitoringu -----------------
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Solar MPPT Monitor</title>
  <meta http-equiv="refresh" content="2">
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    .data { font-size: 24px; color: #0066cc; }
  </style>
</head>
<body>
  <h1>ESP32 Solar MPPT</h1>
  <p>PV Voltage: <span class="data">%V_pv% V</span></p>
  <p>PV Current: <span class="data">%I_pv% A</span></p>
  <p>PV Power: <span class="data">%P_pv% W</span></p>
  <p>Duty Cycle: <span class="data">%D%</span></p>
  <p>MPPT Status: <span class="data">%MPPT_STATUS%</span></p>
  <form action="/toggle" method="post">
    <input type="submit" value="Toggle MPPT">
  </form>
</body>
</html>
)rawliteral";

// ----------------- Funkcje pomiarowe -----------------
float read_avg_adc(adc1_channel_t channel) {
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) {
    sum += adc1_get_raw(channel);
    delayMicroseconds(100);
  }
  return (sum / 32.0) * 3.3 / 4095.0;
}

void update_mppt() {
  if (!mppt_enabled) return;

  V_pv = read_avg_adc(ADC1_CHANNEL_6) * 100.0;  // Skalowanie dzielnika 100:1
  I_pv = read_avg_adc(ADC1_CHANNEL_7) * 10.0;   // Skalowanie dla shunta 0.01Ω + INA240
  P_pv = V_pv * I_pv;

  if (P_pv > P_prev) {
    if (D + delta_D < 0.8) D += delta_D;
  } else {
    if (D - delta_D > 0.1) D -= delta_D;
  }
  P_prev = P_pv;
  ledcWrite(0, (uint32_t)(D * 1023));
}

// ----------------- Obsługa żądań HTTP -----------------
void handle_root() {
  String page = html_page;
  page.replace("%V_pv%", String(V_pv, 2));
  page.replace("%I_pv%", String(I_pv, 2));
  page.replace("%P_pv%", String(P_pv, 2));
  page.replace("%D%", String(D, 2));
  page.replace("%MPPT_STATUS%", mppt_enabled ? "ON" : "OFF");
  server.send(200, "text/html", page);
}

void handle_toggle() {
  mppt_enabled = !mppt_enabled;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ----------------- Setup & Loop -----------------
void setup() {
  Serial.begin(115200);

  // Konfiguracja ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

  // Konfiguracja PWM
  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, 0);

  // Start WiFi w trybie Access Point
  WiFi.softAP(ssid, password);
  Serial.println("AP Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Konfiguracja serwera HTTP
  server.on("/", handle_root);
  server.on("/toggle", handle_toggle);
  server.begin();
}

void loop() {
  update_mppt();
  server.handleClient();
  delay(100);
}