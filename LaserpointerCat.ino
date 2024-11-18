#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Base64.h>

#include <indexhtml.h> // define HTMLO

// WiFi beállítások
const char* ssid = "HGPLSOFT";
const char* password = "7102237203210";

// Servo settings
#define NUM_SERVOS 2
#define SERVO_RESOLUTION 100
#define LED 15
#define _SER

#include "esp_log.h"
static const char* TAG = "DEBUG";

Servo servos[NUM_SERVOS];
const int servoPins[NUM_SERVOS] = {18, 33};
const int buttonPin = 0; // GPIO 0

void taskServoControl(void* parameter);
void taskWiFi(void* parameter);



// Szervó parancssorok
QueueHandle_t servoQueues[NUM_SERVOS];

// Szervó végrehajtás jelzése
SemaphoreHandle_t servoSemaphores[NUM_SERVOS];

// Parancs struktúra
struct Command {
  String cmd;
  int mot;
  int pos;
  int tim;
  int line;
  String cnt;
  String name;
  bool lock;
};

// Program struktúra
struct Program {
  String name;
  std::vector<Command> lines;
};

struct LoopInfo {
  int line;
  int counter;
};

std::vector<LoopInfo> loopiter;

// Programok tárolása
std::vector<Program> programs;

std::vector<std::pair<Program*, int>> callStack;


// Aktuális program állapota
Program* currentProgram = nullptr;
int currentLine = 0;

// LOOP kezelés
int loopCount = 0;
int loopTarget = 0;
int loopStartLine = 0;

// Gomb állapota
bool buttonPressed = false;

// Időzítők a középre állításhoz
unsigned long centerStartTime = 0;
bool centering = false;

AsyncWebServer server(80);
Preferences preferences;


void loop() {
  // Gomb ellenőrzése
  checkButton();

  // Ha középre állítás folyamatban
  if (centering) {
    if (millis() - centerStartTime >= 10000) {
      centering = false;
      // Indítsuk el az első programot
      currentProgram = &programs[0];
      currentLine = 0;
    }
  } else {
    // Parancsok végrehajtása
    executeNextCommand();
  }

  // Egyéb feladatok (pl. WiFi kommunikáció)
  delay(3); // Kis késleltetés a CPU terhelés csökkentése érdekében
}

  String jsondefa = R"rawliteral(
  {
    "progs":[
      {
        "name":"prog1",
        "lines":[
          {"cmd": "MOVE", "mot": 0, "pos": 90, "tim": 2000, "lock": false},
          {"cmd": "MOVE", "mot": 1, "pos": 45, "tim": 2000, "lock": true},
          {"cmd": "WAIT", "tim": 500},
          {"cmd": "MOVE", "mot": 0, "pos": 0, "tim": 2000, "lock": false},
          {"cmd": "MOVE", "mot": 1, "pos": 0, "tim": 2000, "lock": true},
          {"cmd": "WAIT", "tim": 500},
          {"cmd": "LOOP", "line": 0, "cnt": "3"},
          {"cmd": "CALL", "name": "prog2"},
          {"cmd": "GOTO", "name": "prog1"}
        ]
      },
      {
        "name":"prog2",
        "lines":[
          {"cmd": "MOVE", "mot": 0, "pos": 45, "tim": 200, "lock": false},
          {"cmd": "MOVE", "mot": 1, "pos": 45, "tim": 200, "lock": true},
          {"cmd": "WAIT", "tim": 1000}          
        ]
      }
    ]
  }
  )rawliteral";


const uint8_t favicon_ico[] PROGMEM = {
  0x00, 0x00, // Reserved
  0x01, 0x00, // Image type: 1 (ICO)
  0x01, 0x00, // Number of images: 1
  // Image entry
  0x10, 0x10, // Width: 16
  0x10, 0x10, // Height: 16
  0x00,       // Number of colors in palette
  0x00,       // Reserved
  0x01, 0x00, // Color planes
  0x20, 0x00, // Bits per pixel
  0x16, 0x00, 0x00, 0x00, // Size of image data: 22 bytes
  0x22, 0x00, 0x00, 0x00, // Offset of image data: 34 bytes
  // Image data (egyszerű PNG kép)
  0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
  0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
  0x00,0x00,0x10,0x00,0x00,0x10,0x00,0x08,
  0x02,0x00,0x00,0x00,0x90,0x91,0x68,0x36,
  0x00,0x00,0x00,0x0A,0x49,0x44,0x41,0x54,
  0x08,0xD7,0x63,0xF8,0xFF,0xFF,0x3F,0x00,
  0x05,0xFE,0x02,0xFE,0xA7,0x2B,0x2D,0xA0,
  0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,
  0xAE,0x42,0x60,0x82
};
const size_t favicon_ico_len = sizeof(favicon_ico);

void taskWiFi(void * parameter) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_AP");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    #ifdef SER
    Serial.print(".");
    #endif
    delay(500);
  }
  #ifdef SER
  Serial.println(" Csatlakozva a WiFi hálózathoz.");
  #endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      // Retrieve the current SSID and stored program
      String currentSSID = preferences.getString("ssid", ""); // Fetches saved SSID
      String currentProgram = preferences.getString("customProgram", jsondefa); // Fetches custom or default program
      
      // Build the HTML with current SSID and program
      std::vector<String> placeholders = {"@currentSSID", "@currentProgram"};
      std::vector<String> values = {currentSSID, currentProgram};

      String html = replacePlaceholders(HTMLO, placeholders, values);
      
      // Send the response
      //request->send(200, "text/html", html);
      AsyncWebServerResponse *res = request->beginResponse(200, "text/html", html);
      res->addHeader("Access-Control-Allow-Origin", "*");
      res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      res->addHeader("Access-Control-Allow-Headers", "Content-Type");         
      request->send(res); 
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "image/x-icon", favicon_ico, favicon_ico_len);
  });


  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String newSSID = request->getParam("ssid", true)->value();
      String newPassword = request->getParam("password", true)->value();
      preferences.putString("ssid", newSSID);
      preferences.putString("password", newPassword);
      //request->send(200, "text/html", "WiFi credentials updated. Rebooting...");
      AsyncWebServerResponse *res = request->beginResponse(200, "text/html", "WiFi credentials updated. Rebooting...");
      res->addHeader("Access-Control-Allow-Origin", "*");
      res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      res->addHeader("Access-Control-Allow-Headers", "Content-Type");      
      request->send(res); 
      ESP.restart();
    }
  });

  server.on("/uploadProgram", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("program", true)) {
      String program = request->getParam("program", true)->value();
      ESP_LOGI(TAG, "\nProgram hossz: %d", program.length());
      ESP_LOGI(TAG, "uploadProgram: %s", program.c_str());
      preferences.putString("customProgram", program);
      //request->send(200, "text/html", "Custom program uploaded.");
      AsyncWebServerResponse *res = request->beginResponse(200, "text/html", "Custom program uploaded.");
      res->addHeader("Access-Control-Allow-Origin", "*");
      res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      res->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(res);         
      loadPrograms(); // Reload custom program
    }
  });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    preferences.remove("customProgram");
    loadPrograms(); // Reload default program
    //request->send(200, "text/html", "Reset to default program.");
      AsyncWebServerResponse *res = request->beginResponse(200, "text/html", "Reset to default program.");
      res->addHeader("Access-Control-Allow-Origin", "*");
      res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      res->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(res);    
  });

server.on("/getData", HTTP_GET, [](AsyncWebServerRequest *request) {
    // JSON objektum létrehozása
    DynamicJsonDocument jsonDoc(8192); // Elegendő méret a programok tárolására

    // Hozzáadjuk az SSID-t az adatokhoz
    jsonDoc["ssid"] = preferences.getString("ssid", "");

    // Programok hozzáadása JSON-ként
    JsonObject code = jsonDoc.createNestedObject("code");
    JsonArray progsArray = code.createNestedArray("progs");
    for (const auto& prog : programs) {
        JsonObject progObj = progsArray.createNestedObject();
        progObj["name"] = prog.name;

        // **Lines mező létrehozása tömbként**
        JsonArray linesArray = progObj.createNestedArray("lines");

        for (const auto& cmd : prog.lines) {
            JsonObject cmdObj = linesArray.createNestedObject();
            cmdObj["cmd"] = cmd.cmd;
            if (cmd.cmd == "MOVE") {
                cmdObj["mot"] = cmd.mot;
                cmdObj["pos"] = cmd.pos;
                cmdObj["tim"] = cmd.tim;
                cmdObj["lock"] = cmd.lock;
            } else if (cmd.cmd == "WAIT") {
                cmdObj["tim"] = cmd.tim;
            } else if (cmd.cmd == "LOOP") {
                cmdObj["line"] = cmd.line;
                cmdObj["cnt"] = cmd.cnt;
            } else if (cmd.cmd == "CALL" || cmd.cmd == "GOTO") {
                cmdObj["name"] = cmd.name;
            }
        }
    }

    // JSON objektum szerializálása String-re
    String response;
    serializeJson(jsonDoc, response);
    ESP_LOGI(TAG, "\nGETDATA: %s", response.c_str());

    // Válasz küldése
    AsyncWebServerResponse *res = request->beginResponse(200, "application/json", response);
    res->addHeader("Access-Control-Allow-Origin", "*");
    res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(res);
});


  server.onNotFound([](AsyncWebServerRequest *request) {
      request->send(404, "text/plain", "Not Found");
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");


  server.begin();

  for (;;) {
    // WiFi kapcsolódás figyelése vagy újracsatlakozás
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
  #ifdef SER
  Serial.println("Újracsatlakozás...");
  #endif
      
    }
    delay(10000);
  }
}

void taskServoControl(void * parameter) {
  int servoIndex = (int)parameter;
  Command cmd;

  for (;;) {
    // Parancs várása a sorból
    if (xQueueReceive(servoQueues[servoIndex], &cmd, portMAX_DELAY) == pdPASS) {
      if (cmd.cmd == "MOVE") {
        int startPos = servos[servoIndex].read(); // Aktuális pozíció
        int endPos = cmd.pos;
        int totalSteps = SERVO_RESOLUTION;
        int stepDelay = cmd.tim / totalSteps; // Idő lépésenként ms-ban
        float stepSize = (float)(endPos - startPos) / totalSteps; // Pozíció lépésenként


        // Debug üzenetek a soros vonalon
          #ifdef SER

        Serial.print("Servo ");
        Serial.print(servoIndex);
        Serial.print(" - StartPos: ");
        Serial.print(startPos);
        Serial.print(" - EndPos: ");
        Serial.println(endPos);
  #endif


        for (int i = 1; i <= totalSteps; i++) {
          int newPos = startPos + (int)(stepSize * i);
          servos[servoIndex].write(newPos);
          vTaskDelay(stepDelay / portTICK_PERIOD_MS);
        }

        // Biztosítjuk, hogy a végső pozíció pontos legyen
        servos[servoIndex].write(endPos);

        // Végrehajtás jelzése, ha szükséges
        if (cmd.lock) {
          xSemaphoreGive(servoSemaphores[servoIndex]);
        }
      }
    }
  }
}

void checkButton() {
  if (digitalRead(buttonPin) == LOW) { // Gomb megnyomva
    if (!buttonPressed) {
      buttonPressed = true;
      centerServos();
      centerStartTime = millis();
      centering = true;
    }
  } else {
    buttonPressed = false;
  }
}

void centerServos() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].write(90); // Középállás
  }
}



void loadPrograms() {
    String json;
    // Itt betöltheti a programokat JSON formátumban
    if (preferences.isKey("customProgram")) {
        json = preferences.getString("customProgram", "");
    } else {
        json = jsondefa;
    }
    ESP_LOGI(TAG, "\nLoadPrograms: %s", json.c_str());
    // JSON feldolgozása
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json.c_str());

    if (error) {
#ifdef SER
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
  #endif
        return;
    }

    // A programs vektor ürítése, hogy ne halmozódjanak fel a programok
    programs.clear();

    JsonArray progs = doc["progs"].as<JsonArray>(); // Győződj meg arról, hogy progs is JsonArray
    ESP_LOGI(TAG, "\nLoadcode: %s", doc["code"]);
    ESP_LOGI(TAG, "\nLoadProgg: %s", doc["progs"]);
    for (JsonObject prog : progs) {
        Program p;
        p.name = prog["name"].as<String>();
        JsonArray lines = prog["lines"].as<JsonArray>(); // Lines mező JsonArray-ként való kezelése
        for (JsonObject line : lines) {
            Command c;
            c.cmd = line["cmd"].as<String>();
            c.mot = line.containsKey("mot") ? line["mot"] : -1;
            c.pos = line.containsKey("pos") ? line["pos"] : -1;
            c.tim = line.containsKey("tim") ? line["tim"] : 0;
            c.line = line.containsKey("line") ? line["line"] : 0;
            c.cnt = line.containsKey("cnt") ? line["cnt"].as<String>() : "";
            c.name = line.containsKey("name") ? line["name"].as<String>() : "";
            c.lock = line.containsKey("lock") ? line["lock"] : false;
            p.lines.push_back(c);
        }
        programs.push_back(p);
    }
}


void executeNextCommand() {
  if (currentProgram == nullptr) return;

  if (currentLine >= currentProgram->lines.size()) {
      if (!callStack.empty()) {
          // Visszatérés a hívó programba
          currentProgram = callStack.back().first;
          currentLine = callStack.back().second;
          callStack.pop_back();
      } else {
          // Nincs több program a stack-en
          currentProgram = nullptr;
      }
      return;
  }


  Command cmd = currentProgram->lines[currentLine];

  if (cmd.cmd == "MOVE") {
    // Parancs küldése a megfelelő szervó tasknak
    if (cmd.mot >= 0 && cmd.mot < NUM_SERVOS) {
      if (cmd.lock) {
        xQueueSend(servoQueues[cmd.mot], &cmd, portMAX_DELAY);
        xSemaphoreTake(servoSemaphores[cmd.mot], portMAX_DELAY);
      } else {
        xQueueSend(servoQueues[cmd.mot], &cmd, portMAX_DELAY);
      }
    }
    currentLine++;
  } else if (cmd.cmd == "WAIT") {
    vTaskDelay(cmd.tim / portTICK_PERIOD_MS);
    currentLine++;
  } else if (cmd.cmd == "LOOP") {
    // LOOP kezelés
    bool found = false;
    for (auto &loop : loopiter) {
      if (loop.line == currentLine) {
        if (loop.counter > 1) {
          loop.counter--;
          currentLine = cmd.line; // Ugrás a megadott sorra
        } else {
          found = true;
          for (auto it = loopiter.begin(); it != loopiter.end(); ++it) {
            if (it->line == currentLine) {
              // Elem törlése, ha a line értéke megegyezik
              loopiter.erase(it);
              break; // Kilépés a ciklusból, mert csak egy elemet törlünk
            }
          }
          currentLine++;
        }
        return;
      }
    }

    // Ha nem találtunk ilyen line értéket, új loop hozzáadása
    if (!found) {
      int cnt = parseCnt(cmd.cnt);
      loopiter.push_back({currentLine, cnt});
      currentLine = cmd.line;
    }
  } else if (cmd.cmd == "CALL") {
      Program* calledProg = findProgramByName(cmd.name);
      if (calledProg != nullptr) {
          // Mentjük a jelenlegi programot és sor számot a stack-re
          callStack.push_back({currentProgram, currentLine + 1});
          // Átváltunk a hívott programra
          currentProgram = calledProg;
          currentLine = 0;
      } else {
          currentLine++;
      }
  } else if (cmd.cmd == "GOTO") {
      Program* targetProg = findProgramByName(cmd.name);
      if (targetProg != nullptr) {
          // Átváltunk a megadott programra, nincs visszatérés
          currentProgram = targetProg;
          currentLine = 0;
      } else {
          currentLine++;
      }
  } else {
    currentLine++;
  }
}


int parseCnt(String cntStr) {
  if (cntStr.indexOf(',') != -1) {
    int min = cntStr.substring(0, cntStr.indexOf(',')).toInt();
    int max = cntStr.substring(cntStr.indexOf(',') + 1).toInt();
    return random(min, max + 1);
  } else {
    return cntStr.toInt();
  }
}

Program* findProgramByName(String name) {
  for (auto& prog : programs) {
    if (prog.name == name) {
      return &prog;
    }
  }
  return nullptr;
}

String replacePlaceholders(String templateStr, const std::vector<String>& placeholders, const std::vector<String>& values) {
    // Ellenőrzi, hogy a két vector mérete megegyezik-e
    if (placeholders.size() != values.size()) {
        return templateStr;  // Visszatér az eredeti stringgel, ha a méretek nem egyeznek
    }

    for (size_t i = 0; i < placeholders.size(); i++) {
        templateStr.replace(placeholders[i], values[i]);
    }
    return templateStr;
}

void setup() {
  #ifdef SER
  Serial.begin(460800);
  delay(1000);
  Serial.print("Servo ");
    #endif

  delay(2000);
  ledcAttach(LED, 5000,8); // PWM csatorna 0
  ledcWrite(LED, 20);

  // Szervók inicializálása
  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(servoPins[i]);
    // Szervó parancssor létrehozása
    servoQueues[i] = xQueueCreate(5, sizeof(Command));
    // Szervó szeminárium létrehozása
    servoSemaphores[i] = xSemaphoreCreateBinary();
    // Szervó task létrehozása
    xTaskCreatePinnedToCore(taskServoControl, "Servo Task", 4096, (void*)i, 1, NULL, 1);
  }

  // Gomb bemenet beállítása
  pinMode(buttonPin, INPUT_PULLUP);

  // WiFi task létrehozása
  xTaskCreatePinnedToCore(taskWiFi, "WiFi Task", 4096, NULL, 1, NULL, 0);

  preferences.begin("catlaser", false); // Store settings in "catlaser" namespace
  loadPrograms();  // Load default or stored program

  // Szervók középre állítása
  centerServos();
  centerStartTime = millis();
  centering = true;
}
