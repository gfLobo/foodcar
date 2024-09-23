#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  
#include <ArduinoJson.h> 

// Definindo os pinos para controle
const int pins[] = {D0, D1, D2, D3}; 

// Servidor HTTP e WebSocket
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);



void manageJoystick(float x, float y) {
  
  // Controle de movimento
  if (x < -0.5) {
    // Virar à esquerda
    digitalWrite(pins[0], HIGH);
    digitalWrite(pins[1], LOW);
    digitalWrite(pins[2], LOW);
    digitalWrite(pins[3], HIGH);

  } else if (x > 0.5) {
    // Virar à direita
    digitalWrite(pins[0], LOW);
    digitalWrite(pins[1], HIGH);
    digitalWrite(pins[2], HIGH);
    digitalWrite(pins[3], LOW);

  } else {
    // Movimento para a frente ou para trás
    if (y < -0.5) {
      // Marcha à ré
      digitalWrite(pins[0], LOW);
      digitalWrite(pins[1], LOW);
      digitalWrite(pins[2], HIGH);
      digitalWrite(pins[3], HIGH);
    } else if (y > 0.5) {
      // Avançar
      digitalWrite(pins[0], HIGH);
      digitalWrite(pins[1], HIGH);
      digitalWrite(pins[2], LOW);
      digitalWrite(pins[3], LOW);
    } else {
      // Parar
      digitalWrite(pins[0], LOW);
      digitalWrite(pins[1], LOW);
      digitalWrite(pins[2], LOW);
      digitalWrite(pins[3], LOW);
    }
  }
}




// Função para obter o estado de todos os pinos
String getPinStates() {
    String states = "{";

    for (int i = 0; i < 4; i++) {
        states += "\"pin" + String(i) + "\":" + String(digitalRead(pins[i])) + (i < 3 ? "," : "");
    }

    // Inclui o estado de marcha à ré
    states += "}";
    
    return states;
}

// Função para enviar o estado atual dos pinos para os clientes WebSocket
void sendPinStates(uint8_t client_num) {
    String states = getPinStates();
    webSocket.sendTXT(client_num, states);  // Envia o estado para o cliente específico
}

// Função de callback para eventos WebSocket
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_DISCONNECTED) {
        Serial.printf("Cliente [%u] desconectado!\n", num);
    } else if (type == WStype_CONNECTED) {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("Cliente [%u] conectado: %s\n", num, ip.toString().c_str());
        sendPinStates(num);
    } else if (type == WStype_TEXT) {
        String message = String((char*)payload);
        Serial.printf("Comando recebido: %s\n", message.c_str());

        StaticJsonDocument<256> jsonBuffer;

        // Decodifique o JSON
        DeserializationError error = deserializeJson(jsonBuffer, payload);

        // Verifique se houve um erro de decodificação
        if (error) {
          Serial.print("Error decoding JSON: ");
          Serial.println(error.c_str());
          return;
        }

        // Acesse os valores do JSON
        float x = jsonBuffer["x"];
        float y = jsonBuffer["y"];
        manageJoystick(x, y);
        // Envia o estado dos pinos para o cliente após a atualização
        String states = getPinStates();
        webSocket.sendTXT(num, states);
    }
}

// Inicializando o WebSocket
void initWebSocket() {
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
}

// Configura o servidor HTTP para servir uma página HTML simples
void setupServer() {
    server.on("/", HTTP_GET, []() {
        String html = R"rawliteral(
          <!DOCTYPE html>
          <html lang="pt-BR">
          <head>
              <meta charset="UTF-8">
              <meta name="viewport" content="width=device-width, initial-scale=1.0">
              <title>Estado dos Motores</title>
              <style>
                  body {
                      font-family: Arial, sans-serif;
                      display: flex;
                      justify-content: center;
                      align-items: center;
                      height: 100vh;
                      background-color: #f4f4f4;
                  }
                  .container {
                      display: grid;
                      grid-template-columns: 1fr 1fr;
                      gap: 20px;
                  }
                  .motor {
                      width: 150px;
                      height: 150px;
                      display: flex;
                      justify-content: center;
                      align-items: center;
                      font-size: 20px;
                      color: white;
                      border-radius: 10px;
                  }

              </style>
          </head>
          <body>
              <div class="container">
                  <div id="motor0" class="motor">Motor 0</div>
                  <div id="motor1" class="motor">Motor 1</div>
                  <div id="motor2" class="motor">Motor 2</div>
                  <div id="motor3" class="motor">Motor 3</div>
              </div>

          <script>
          // Cria uma conexão WebSocket
          const socket = new WebSocket(`ws://${window.location.hostname}:81/`);

          // Função para lidar com mensagens recebidas do WebSocket
          socket.onmessage = function(event) {
              const data = JSON.parse(event.data);

              // Atualiza o estado dos motores
              for (let i = 0; i < 4; i++) {
                  let motorElement = document.getElementById('motor' + i);
                  if (data['pin' + i] == 1) {                      
                      motorElement.style.background = 'green';  // Motores em movimento
                  } else {
                      motorElement.style.background = 'gray';  // Motores parados
                  }
              }
          };

          // Função de tratamento de erros
          socket.onerror = function(error) {
              console.error('WebSocket Error: ', error);
          };
          </script>

          </body>
          </html>
        )rawliteral";
        server.send(200, "text/html", html);
    });



    // Rota para obter o estado dos pinos
    server.on("/pin_states", HTTP_GET, []() {
        String states = getPinStates();
        server.send(200, "application/json", states);
    });

    server.begin();
}

void setup() {
    // Inicializa comunicação serial
    Serial.begin(115200);

    // Configura os pinos como saída
    for (int i = 0; i < 4; i++) {
        pinMode(pins[i], OUTPUT);
    }
    pinMode(LED_BUILTIN, OUTPUT);

    // Inicializa o WiFiManager para configuração Wi-Fi
    WiFiManager wifiManager;

    // Tenta conectar-se ao Wi-Fi existente ou cria um ponto de acesso para configurar
    if (!wifiManager.autoConnect("NodeMCU-WebSocket")) {
        Serial.println("Falha na conexão, reiniciando...");
        ESP.restart();
    }
    Serial.println("Conectado ao Wi-Fi!");

    // Exibe o IP do NodeMCU
    Serial.println(WiFi.localIP());

    // Inicializa o servidor WebSocket e HTTP
    initWebSocket();
    setupServer();
}

void loop() {
    // Mantém o servidor WebSocket ativo
    webSocket.loop();
    
    // Mantém o servidor HTTP ativo
    server.handleClient();
    
    // Envia periodicamente o estado dos pinos para todos os clientes WebSocket conectados
    static unsigned long lastUpdateTime = 0;
    unsigned long currentTime = millis();
    
    String states = getPinStates();
    webSocket.broadcastTXT(states);
}

