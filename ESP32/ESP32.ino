#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configuración WiFi
const char* ssid = "Totalplay-97AF";         // Reemplaza con tu SSID WiFi
const char* password = "97AF3390WNTt3CaF"; // Reemplaza con tu contraseña WiFi

// URL del servidor API
const char* serverUrl = "http://192.168.1.100:5000/api/esp32/data"; // Reemplaza con la IP de tu servidor

// Configuración de la pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pines del joystick para el ESP32
#define JOYSTICK_X 35
#define JOYSTICK_Y 34
#define JOYSTICK_BUTTON 32

// Pin de datos para el sensor DS18B20
#define ONE_WIRE_BUS 25
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Variables para el menú
int opcionActual = 0;
int primeraOpcion = 0;
const int numeroDeOpciones = 6;
const int opcionesVisibles = 3;
bool seleccion = false;
bool enSubMenu = false;
int sensorActual = 0;

// Opciones del menú principal
String menuOptions[numeroDeOpciones] = {
  "EcoGrow", "Temp y Hum", "Temp H2O", "Sensor pH", "Bomba H2O", "LEDs"
};

// Configuración para los sensores AHT10
Adafruit_AHTX0 aht1, aht2;

// Variables para almacenar lecturas de sensores
float temp1, hum1, temp2, hum2, waterTemp;

// Variable para controlar el tiempo de envío de datos
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;  // Enviar datos cada 5 segundos

void setup() {
  Serial.begin(115200);

  pinMode(JOYSTICK_X, INPUT);
  pinMode(JOYSTICK_Y, INPUT);
  pinMode(JOYSTICK_BUTTON, INPUT_PULLUP);

  ds18b20.begin();

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Fallo en la asignación del SSD1306"));
    for (;;);
  }
  display.clearDisplay();
  display.display();

  // Inicialización de los sensores AHT10
  if (!aht1.begin()) {
    Serial.println("No se encontró AHT10 en la dirección 0x38");
  }
  if (!aht2.begin(0x39)) {  // Dirección 0x39 para el segundo sensor
    Serial.println("No se encontró AHT10 en la dirección 0x39");
  }

  // Conectar a WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo se pudo conectar a WiFi");
  }
}

void loop() {
  // Leer los valores del joystick
  int yPosition = analogRead(JOYSTICK_Y);
  int buttonState = digitalRead(JOYSTICK_BUTTON);

  // Gestionar la interfaz del menú
  if (enSubMenu) {
    controlSubMenu(yPosition, buttonState);
  } else {
    controlMainMenu(yPosition, buttonState);
  }

  // Leer datos de los sensores (independientemente del menú)
  readSensorData();

  // Enviar datos a la API periódicamente
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    sendDataToAPI();
    lastSendTime = currentTime;
  }
}

void readSensorData() {
  // Leer datos de los sensores AHT10
  sensors_event_t tempEvent1, humEvent1, tempEvent2, humEvent2;

  if (aht1.getEvent(&humEvent1, &tempEvent1)) {
    temp1 = tempEvent1.temperature;
    hum1 = humEvent1.relative_humidity;
  }

  if (aht2.getEvent(&humEvent2, &tempEvent2)) {
    temp2 = tempEvent2.temperature;
    hum2 = humEvent2.relative_humidity;
  }

  // Leer temperatura del agua (DS18B20)
  ds18b20.requestTemperatures();
  waterTemp = ds18b20.getTempCByIndex(0);

  // Imprimir datos en serial (para depuración)
  Serial.println("Lecturas de sensores:");
  Serial.print("Sensor 1: Temp = "); Serial.print(temp1); Serial.print("°C, Hum = "); Serial.print(hum1); Serial.println("%");
  Serial.print("Sensor 2: Temp = "); Serial.print(temp2); Serial.print("°C, Hum = "); Serial.print(hum2); Serial.println("%");
  Serial.print("Temp agua = "); Serial.print(waterTemp); Serial.println("°C");
}

void sendDataToAPI() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No hay conexión WiFi");
    return;
  }

  // Crear objeto JSON para los datos
  StaticJsonDocument<256> jsonDoc;

  // Añadir datos del sensor AHT10 #1 (temp y humedad)
  JsonObject aht10_1 = jsonDoc.createNestedObject("aht10_1");
  aht10_1["temperatura"] = temp1;
  aht10_1["humedad"] = hum1;

  // Añadir datos del sensor AHT10 #2 (temp y humedad)
  JsonObject aht10_2 = jsonDoc.createNestedObject("aht10_2");
  aht10_2["temperatura"] = temp2;
  aht10_2["humedad"] = hum2;

  // Añadir datos del sensor DS18B20 (temp agua)
  JsonObject ds18b20_data = jsonDoc.createNestedObject("ds18b20");
  ds18b20_data["temperatura"] = waterTemp;

  // Serializar el JSON
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Enviar la petición HTTP POST
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Código HTTP: " + String(httpResponseCode));
    Serial.println("Respuesta: " + response);
  } else {
    Serial.print("Error en petición HTTP: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void controlMainMenu(int yPosition, int buttonState) {
  // Navegación en el menú principal usando el joystick
  if (yPosition < 750) {
    opcionActual--;
    if (opcionActual < 0) opcionActual = numeroDeOpciones - 1;
    adjustScroll();
    delay(200);
  }
  else if (yPosition > 3500) {
    opcionActual++;
    if (opcionActual >= numeroDeOpciones) opcionActual = 0;
    adjustScroll();
    delay(200);
  }

  // Entrar o salir del submenú con el botón del joystick
  if (buttonState == LOW && !seleccion) {
    seleccion = true;
    enSubMenu = !enSubMenu;

    if (enSubMenu) {
      displaySubMenu(opcionActual);
    } else {
      displayMenu();
    }
  } else if (buttonState == HIGH) {
    seleccion = false;
  }

  // Mostrar el menú principal con scroll si no está en el submenú
  if (!enSubMenu) {
    displayMenu();
  }
}

void controlSubMenu(int yPosition, int buttonState) {
  if (opcionActual == 1) {  // Si la opción seleccionada es "Temp y Hum"
    // Navegación en el submenú de lectura de sensores
    if (yPosition < 750) {
      sensorActual = 0;
      displaySensorData();
      delay(200);
    }
    else if (yPosition > 3500) {
      sensorActual = 1;
      displaySensorData();
      delay(200);
    }
  }
  // Salir del submenú y regresar al menú principal al presionar el botón nuevamente
  if (buttonState == LOW && !seleccion) {
    seleccion = true;
    enSubMenu = false;
    displayMenu();
    delay(200);
  } else if (buttonState == HIGH) {
    seleccion = false;
  }
}

void adjustScroll() {
  if (opcionActual < primeraOpcion) {
    primeraOpcion = opcionActual;
  }
  else if (opcionActual >= primeraOpcion + opcionesVisibles) {
    primeraOpcion = opcionActual - opcionesVisibles + 1;
  }
}

void displayMenu() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  for (int i = 0; i < opcionesVisibles; i++) {
    int optionIndex = primeraOpcion + i;
    if (optionIndex >= numeroDeOpciones) break;

    if (optionIndex == opcionActual) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(0, i * 20);
    display.print(menuOptions[optionIndex]);
  }

  display.display();
}

void displaySensorData() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  if (sensorActual == 0) {
    display.setCursor(0, 0);
    display.println("Sensor 1:");
    display.setCursor(0, 25);
    display.print("T:");
    display.print(temp1, 1);
    display.print("C");
    display.setCursor(0, 45);
    display.print("H:");
    display.print(hum1, 1);
    display.println("%");
  } else {
    display.setCursor(0, 0);
    display.println("Sensor 2:");
    display.setCursor(0, 25);
    display.print("T:");
    display.print(temp2, 1);
    display.print("C");
    display.setCursor(0, 45);
    display.print("H:");
    display.print(hum2, 1);
    display.println("%");
  }

  display.display();
}

void displaySubMenu(int optionIndex) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (optionIndex == 1) {
    displaySensorData();
  } else if (optionIndex == 2) {
    ds18b20.requestTemperatures();
    float waterTemp = ds18b20.getTempCByIndex(0);
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Temperatur");
    display.println("del agua:");
    display.setCursor(0, 40);
    display.print(waterTemp, 2);
    display.print(" C");
    display.display();
  } else {
    display.setCursor(0, 0);
    switch (optionIndex) {
      case 0:
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Innova");
        display.println("     TecNM");
        display.setCursor(0, 40);
        display.println("---ITSA---");
        break;
      case 3:
        display.println("Submenu 4 actions");
        break;
      case 4:
        display.println("Submenu 5 actions");
        break;
      case 5:
        display.println("Submenu 6 actions");
        break;
    }
    display.display();
  }
}
