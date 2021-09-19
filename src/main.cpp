#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <CH9328Keyboard.h>

#define SWSP_TX D1
#define SWSP_RX D2
SoftwareSerial swSerial;

WiFiManager wm;
ESP8266WebServer server(80);

#define CH9328_RST D8
const int KEY_DELAY = 24; //ms delay between key presses

bool ledState = HIGH;

Ticker wifiStatusLED ([]() {
  if (WiFi.status() != WL_CONNECTED) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}, 100, 0, MILLIS);

void type(String text) {
  for (unsigned int i = 0; i < text.length(); i++) {
    Keyboard.press(text[i]); delay(KEY_DELAY);
    Keyboard.releaseAll(); delay(KEY_DELAY);
  }
}

const String html = "\
<html>\
  <body>\
    <h1>Keyboard</h1>\
    <form>\
      <input type=\"text\" id=\"input\" name=\"input\">\
      <input type=\"submit\">\
    </form>\
  <script type=\"text/javascript\">\
    document.forms[0].onsubmit = async(e) => {\
      e.preventDefault();\
      const params = new URLSearchParams([...new FormData(e.target).entries()]);\
      fetch(\"/\", { method: \"POST\", body: params });\
      const response = await new Response(params).text();\
      console.log(response);\
    }\
  </script>\
  </body>\
</html>";


void handleRequest() {
  if (server.method() != HTTP_POST) {
    server.send(200, "text/html", html);
  } else {
    String input = server.arg("input");
    swSerial.print("Input: "); swSerial.println(input);
    type(input);
    server.send(200);
  } 
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(13, INPUT_PULLUP);
  swSerial.begin(9600, SWSERIAL_8N1, SWSP_RX, SWSP_TX, false);
  WiFi.mode(WIFI_STA);

  wm.setConfigPortalBlocking(false);
  wm.setMinimumSignalQuality(50);
  wm.setRemoveDuplicateAPs(false);
  wm.setTimeout(10);
  wm.setDarkMode(true);
  wm.setSaveConfigCallback([]() {
    swSerial.println("Saving WiFi config! Restarting device...");
    ESP.restart();
  });
  wm.setBreakAfterConfig(true);

  wifiStatusLED.start();

  if (wm.autoConnect("Keyboard")) {
    swSerial.println("Connected! IP address: " + WiFi.localIP().toString());
    if (MDNS.begin("keyboard")) {
      MDNS.addService("http", "tcp", 80);
      swSerial.println("MDNS responder started: http://keyboard.local/");
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      wifiStatusLED.stop();
      swSerial.println("MDNS responder failed!");
      digitalWrite(LED_BUILTIN, HIGH);
    }
    Keyboard.begin(&Serial, CH9328_RST, 9600);

    server.on("/", handleRequest);
    server.begin();
  }
}

void loop() {
  wm.process();
  MDNS.update();
  wifiStatusLED.update();
  server.handleClient();
}
