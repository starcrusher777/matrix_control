#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <LedControl.h>

const char* ssid = "";
const char* password = "";

// define ur pins
const int DIN_PIN = D4;
const int CS_PIN = D0;
const int CLK_PIN = D5;

const int NUM_MATRICES = 4;

LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);

ESP8266WebServer server(80);

//rgb led and 2 leds control(unnecessary)

uint8_t LED1pin = D7;
bool LED1status = LOW;

uint8_t LED2pin = D6;
bool LED2status = LOW;

uint8_t Rpin = D1;
uint8_t Gpin = D2;
uint8_t Bpin = D3;


void setup() 
{
  Serial.begin(115200);
  delay(100);
  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);
  pinMode(Rpin, OUTPUT);
  pinMode(Gpin, OUTPUT);
  pinMode(Bpin, OUTPUT);
  

  Serial.println("Connecting to ");
  Serial.println(ssid);

  
  WiFi.begin(ssid, password);

  
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  delay(500);

  for (int i = 0; i < NUM_MATRICES; i++)
  {
    lc.shutdown(i, true);   // Отключение матрицы
    lc.shutdown(i, false);
    lc.setIntensity(i, 4);
    lc.clearDisplay(i);
  }

  server.on("/", handle_OnConnect);
  server.on("/color", handle_ColorChange); //this too
  server.on("/root", handleRoot);
  server.on("/updateMatrix", HTTP_POST, handleUpdateMatrix);
  server.on("/led1on", handle_led1on); //and this
  server.on("/led1off", handle_led1off); //and this
  server.on("/led2on", handle_led2on); //and this
  server.on("/led2off", handle_led2off); //and this
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() 
{
  server.handleClient();
  
  if(LED1status)
    digitalWrite(LED1pin, HIGH);
  else
    digitalWrite(LED1pin, LOW);
  
  if(LED2status)
    digitalWrite(LED2pin, HIGH);
  else
    digitalWrite(LED2pin, LOW);
}



void handle_OnConnect() 
{
  Serial.print("GPIO7 Status: ");
  if(LED1status)
    Serial.print("ON");
  else
    Serial.print("OFF");

  Serial.print(" | GPIO6 Status: ");
  if(LED2status)
    Serial.print("ON");
  else
    Serial.print("OFF");

  Serial.println("");
  server.send(200, "text/html", SendHTML(LED1status,LED2status)); 
}

void handle_led1on() 
{
  LED1status = HIGH;
  Serial.println("GPIO7 Status: ON");
  server.send(200, "text/html", SendHTML(true,LED2status)); 
}

void handle_led1off() 
{
  LED1status = LOW;
  Serial.println("GPIO7 Status: OFF");
  server.send(200, "text/html", SendHTML(false,LED2status)); 
}

void handle_led2on() 
{
  LED2status = HIGH;
  Serial.println("GPIO6 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,true)); 
}

void handle_led2off() 
{
  LED2status = LOW;
  Serial.println("GPIO6 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,false)); 
}

void handle_ColorChange() {
  String color = server.arg("color");
  long number = strtol(color.substring(1).c_str(), NULL, 16);
  
  byte r = (number >> 16) & 0xFF;
  byte g = (number >> 8) & 0xFF;
  byte b = number & 0xFF;

  analogWrite(Rpin, r);
  analogWrite(Gpin, g);
  analogWrite(Bpin, b);
  
  server.send(200, "text/html", SendHTML(LED1status, LED2status));
}

void handleRoot() {
  server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
      <title>LED Matrix Editor</title>
      <style>
        html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
        body {margin-top: 50px;}
        table {margin: 0 auto;}
        td {width: 20px; height: 20px; border: 1px solid #000;}
        .active {background-color: #000;}
        .button {display: block; width: 100px; background-color: #1abc9c; border: none; color: white; padding: 10px; text-decoration: none; font-size: 16px; margin: 20px auto; cursor: pointer; border-radius: 4px;}
      </style>
    </head>
    <body>
      <h1>LED Matrix Editor</h1>
      <table id="matrix">
        <!-- The table cells will be added here by JavaScript -->
      </table>
      <button class="button" onclick="sendMatrix()">Send to LED Matrix</button>

      <script>
        const rows = 8;
        const cols = 8 * NUM_MATRICES;
        const table = document.getElementById('matrix');

        // Create the grid
        for (let i = 0; i < rows; i++) {
          const tr = document.createElement('tr');
          for (let j = 0; j < cols; j++) {
            const td = document.createElement('td');
            td.addEventListener('click', () => td.classList.toggle('active'));
            tr.appendChild(td);
          }
          table.appendChild(tr);
        }

        function sendMatrix() {
          const matrix = [];
          const cells = table.getElementsByTagName('td');
          for (let i = 0; i < cells.length; i++) {
            matrix.push(cells[i].classList.contains('active') ? 1 : 0);
          }
          fetch('/updateMatrix', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
            },
            body: JSON.stringify({matrix}),
          }).then(response => {
            if (response.ok) {
              alert('Matrix updated successfully!');
            } else {
              alert('Failed to update matrix.');
            }
          });
        }
      </script>
    </body>
    </html>
  )rawliteral");
}

void handleUpdateMatrix() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  DynamicJsonDocument doc(1024);
  String body = server.arg("plain");
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  JsonArray matrix = doc["matrix"].as<JsonArray>();

  for (int matIndex = 0; matIndex < NUM_MATRICES; matIndex++) {
        lc.clearDisplay(NUM_MATRICES - 1 - matIndex); // Reverse the order of matrices

        // Calculate offset for each matrix
        int matrixOffset = matIndex * 8;

        // Loop through each row of the matrix
        for (int row = 0; row < 8; row++) {
            byte rowData = 0;
            // Loop through each column of the matrix
            for (int col = 0; col < 8; col++) {
                int index = row * 32 + matrixOffset + col;
                if (matrix[index] == 1) {
                    rowData |= (1 << (7 - col)); // Reverse the column order within the row
                }
            }
            lc.setRow(NUM_MATRICES - 1 - matIndex, row, rowData); // Set rows for reversed matrix order
        }
    }

  server.send(200, "application/json", "{\"status\":\"success\"}");
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

String SendHTML(uint8_t led1stat, uint8_t led2stat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>LED Control</title>\n";
  ptr += "<style>\n";
  ptr += "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body { margin-top: 50px; }\n";
  ptr += "h1 { color: #444444; margin: 50px auto 30px; }\n";
  ptr += "h3 { color: #444444; margin-bottom: 50px; }\n";
  ptr += ".button { display: block; width: 80px; background-color: #1abc9c; border: none; color: white; padding: 13px 30px; text-decoration: none; font-size: 25px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px; }\n";
  ptr += ".button-on { background-color: #1abc9c; }\n";
  ptr += ".button-on:active { background-color: #16a085; }\n";
  ptr += ".button-off { background-color: #34495e; }\n";
  ptr += ".button-off:active { background-color: #2c3e50; }\n";
  ptr += "p { font-size: 14px; color: #888; margin-bottom: 10px; }\n";
  ptr += "table { margin: 0 auto; border-collapse: collapse; }\n";
  ptr += "td { width: 20px; height: 20px; border: 1px solid #000; cursor: pointer; }\n";
  ptr += ".active { background-color: #000; }\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP8266 Web Server</h1>\n";
  ptr += "<h3>Using Station(STA) Mode</h3>\n";
  
  if (led1stat)
    ptr += "<p>LED1 Status: ON</p><a class=\"button button-off\" href=\"/led1off\">OFF</a>\n";
  else
    ptr += "<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";

  if (led2stat)
    ptr += "<p>LED2 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";
  else
    ptr += "<p>LED2 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";

  ptr += "<h3>RGB LED Control</h3>\n";
  ptr += "<form action=\"/color\" method=\"get\">\n";
  ptr += "<input type=\"color\" name=\"color\" value=\"#ff0000\">\n";
  ptr += "<input type=\"submit\" value=\"Set Color\">\n";
  ptr += "</form>\n";
  
  ptr += "<h3>LED Matrix Editor</h3>\n";
  ptr += "<table id=\"matrix\">\n";
  for (int i = 0; i < 8; i++) {
    ptr += "<tr>\n";
    for (int j = 0; j < 32; j++) {
      ptr += "<td onclick=\"togglePixel(this)\"></td>\n";
    }
    ptr += "</tr>\n";
  }
  ptr += "</table>\n";
  ptr += "<button class=\"button\" onclick=\"sendMatrix()\">Send to LED Matrix</button>\n";

  ptr += "<script>\n";
  ptr += "function togglePixel(cell) { cell.classList.toggle('active'); }\n";
  ptr += "function sendMatrix() {\n";
  ptr += "  let matrix = [];\n";
  ptr += "  document.querySelectorAll('#matrix td').forEach(cell => { matrix.push(cell.classList.contains('active') ? 1 : 0); });\n";
  ptr += "  fetch('/updateMatrix', {\n";
  ptr += "    method: 'POST',\n";
  ptr += "    headers: { 'Content-Type': 'application/json' },\n";
  ptr += "    body: JSON.stringify({ matrix })\n";
  ptr += "  }).then(response => response.json()).then(data => { alert('Matrix updated!'); });\n";
  ptr += "}\n";
  ptr += "</script>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
