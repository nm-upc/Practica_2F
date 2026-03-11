#include <WiFi.h>
#include <WebServer.h>

// Configuración WiFi
const char* ssid = "MEDIDOR_FREC";
const char* password = "12345678";

// Pines
const int INPUT_PIN = 18;  // Pin para medir frecuencia

// Estructura para la cola circular
struct CircularQueue {
  static const int SIZE = 100;  // Tamaño de la cola
  unsigned long times[SIZE];
  int head = 0;
  int tail = 0;
  int count = 0;
  
  void push(unsigned long value) {
    times[head] = value;
    head = (head + 1) % SIZE;
    if (count < SIZE) count++;
    else tail = (tail + 1) % SIZE;  // Sobrescribir el más antiguo
  }
  
  bool pop(unsigned long &value) {
    if (count == 0) return false;
    value = times[tail];
    tail = (tail + 1) % SIZE;
    count--;
    return true;
  }
  
  int getCount() {
    return count;
  }
  
  void clear() {
    head = 0;
    tail = 0;
    count = 0;
  }
};

// Variables volátiles para ISR
volatile unsigned long lastTime = 0;
volatile unsigned long currentTime = 0;
volatile unsigned long period = 0;
volatile bool newPeriod = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Cola circular para almacenar períodos
CircularQueue periodQueue;

// Variables para cálculos
float fMax = 0;
float fMin = 9999999;
float fAvg = 0;
unsigned long lastCalculationTime = 0;
const int CALCULATION_INTERVAL = 1000;  // Calcular cada 1 segundo

// Servidor web
WebServer server(80);

// Temporizador para medición de tiempo
hw_timer_t *timer = NULL;

// ISR para el pin de entrada
void IRAM_ATTR handleInterrupt() {
  currentTime = micros();
  period = currentTime - lastTime;
  lastTime = currentTime;
  
  // Guardar período en cola circular
  if (period > 0) {
    portENTER_CRITICAL_ISR(&mux);
    periodQueue.push(period);
    newPeriod = true;
    portEXIT_CRITICAL_ISR(&mux);
  }
}

// Función para calcular estadísticas de frecuencia
void calculateFrequencies() {
  if (periodQueue.getCount() == 0) return;
  
  unsigned long period;
  float frequency;
  float sum = 0;
  int validSamples = 0;
  
  fMax = 0;
  fMin = 9999999;
  
  // Crear una copia temporal para no bloquear la ISR mucho tiempo
  CircularQueue tempQueue;
  
  portENTER_CRITICAL(&mux);
  // Copiar datos de la cola
  while (periodQueue.pop(period)) {
    tempQueue.push(period);
  }
  portEXIT_CRITICAL(&mux);
  
  // Calcular estadísticas con la copia
  while (tempQueue.pop(period)) {
    if (period > 0) {
      frequency = 1000000.0 / period;  // Convertir a Hz
      sum += frequency;
      validSamples++;
      
      if (frequency > fMax) fMax = frequency;
      if (frequency < fMin) fMin = frequency;
    }
  }
  
  if (validSamples > 0) {
    fAvg = sum / validSamples;
  } else {
    fMax = 0;
    fMin = 0;
    fAvg = 0;
  }
}

// Página web HTML
String getHTML() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Medidor de Frecuencias</title>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin: 50px; background: #f0f0f0; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; }";
  html += ".freq-box { margin: 20px 0; padding: 15px; border-radius: 5px; }";
  html += ".max { background: #ffebee; border-left: 5px solid #f44336; }";
  html += ".min { background: #e3f2fd; border-left: 5px solid #2196f3; }";
  html += ".avg { background: #e8f5e8; border-left: 5px solid #4caf50; }";
  html += ".value { font-size: 2em; font-weight: bold; }";
  html += ".unit { font-size: 0.8em; color: #666; }";
  html += ".stats { margin-top: 30px; padding: 20px; background: #f5f5f5; border-radius: 5px; }";
  html += ".refresh-btn { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  html += ".refresh-btn:hover { background: #45a049; }";
  html += "</style>";
  html += "<script>";
  html += "function updateData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('fmax').innerText = data.fmax.toFixed(2);";
  html += "    document.getElementById('fmin').innerText = data.fmin.toFixed(2);";
  html += "    document.getElementById('favg').innerText = data.favg.toFixed(2);";
  html += "    document.getElementById('samples').innerText = data.samples;";
  html += "  });";
  html += "}";
  html += "setInterval(updateData, 1000);";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>📊 Medidor de Frecuencias</h1>";
  html += "<p>Pin de entrada: GPIO" + String(INPUT_PIN) + "</p>";
  
  html += "<div class='freq-box max'>";
  html += "<h3>Frecuencia Máxima</h3>";
  html += "<div class='value' id='fmax'>0.00</div>";
  html += "<div class='unit'>Hz</div>";
  html += "</div>";
  
  html += "<div class='freq-box min'>";
  html += "<h3>Frecuencia Mínima</h3>";
  html += "<div class='value' id='fmin'>0.00</div>";
  html += "<div class='unit'>Hz</div>";
  html += "</div>";
  
  html += "<div class='freq-box avg'>";
  html += "<h3>Frecuencia Promedio</h3>";
  html += "<div class='value' id='favg'>0.00</div>";
  html += "<div class='unit'>Hz</div>";
  html += "</div>";
  
  html += "<div class='stats'>";
  html += "<h3>Estadísticas</h3>";
  html += "<p>Muestras en cola: <span id='samples'>0</span></p>";
  html += "<p>Tamaño de cola: " + String(CircularQueue::SIZE) + "</p>";
  html += "</div>";
  
  html += "<button class='refresh-btn' onclick='updateData()'>Actualizar ahora</button>";
  html += "</div>";
  html += "</body></html>";
  
  return html;
}

// Manejador para la página principal
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Manejador para datos JSON
void handleData() {
  String json = "{";
  json += "\"fmax\":" + String(fMax) + ",";
  json += "\"fmin\":" + String(fMin) + ",";
  json += "\"favg\":" + String(fAvg) + ",";
  json += "\"samples\":" + String(periodQueue.getCount());
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Iniciando Medidor de Frecuencias...");
  
  // Configurar pin de entrada con pull-up
  pinMode(INPUT_PIN, INPUT_PULLUP);
  
  // Configurar interrupción para el pin
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), handleInterrupt, FALLING);
  
  // Configurar WiFi en modo AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  Serial.println("Punto de acceso WiFi creado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/data", handleData);
  
  server.begin();
  Serial.println("Servidor web iniciado");
  
  lastCalculationTime = millis();
}

void loop() {
  server.handleClient();
  
  // Calcular frecuencias periódicamente
  if (millis() - lastCalculationTime > CALCULATION_INTERVAL) {
    calculateFrequencies();
    lastCalculationTime = millis();
    
    // Mostrar en puerto serie
    Serial.println("=== Estadísticas de Frecuencia ===");
    Serial.print("Máxima: "); Serial.print(fMax); Serial.println(" Hz");
    Serial.print("Mínima: "); Serial.print(fMin); Serial.println(" Hz");
    Serial.print("Promedio: "); Serial.print(fAvg); Serial.println(" Hz");
    Serial.print("Muestras: "); Serial.println(periodQueue.getCount());
    Serial.println("================================");
  }
}