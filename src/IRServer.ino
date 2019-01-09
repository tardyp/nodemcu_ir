#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);
extern char *ssid;
extern char *password;

const int led = 13;
const int START_PULSE = 30600;
const int START_PAUSE = 51100;
const int START_PULSE2 = 3356;
const int START_PAUSE2 = 1723;

const int PULSE_LEN = 430;
const int PAUSE_HIGH = 1247;
const int PAUSE_LOW = 430;

const int freq = 36000 * 2;
const int char_len = 1000000 / (freq / 10);
int remain_delay = 0;

void pulse(int delay_us)
{
  delay_us -= remain_delay; /* was written in previous pause */
  while (delay_us > char_len)
  {
    Serial1.write(0x55);
    delay_us -= char_len;
  }
  remain_delay = delay_us;
  Serial1.write(0x55 >> (((remain_delay * 8) / char_len) & 0xe));
}
void pause(int delay_us)
{
  delay_us -= remain_delay; /* was written in previous pulse */
  while (delay_us > char_len)
  {
    Serial1.write(0x00);
    delay_us -= char_len;
  }
  remain_delay = delay_us;
  Serial1.write(0x55 << (((remain_delay * 8) / char_len) & 0xe));
}
unsigned char buf[] = {
    0x1, 0x10, 0x30, 0x40, 0xBF, 0x1, 0xFE, 0x11, 0x12, 0x1, 0x3, 0x20,
    0x0, 0x2, 0x0, 0x3, 0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1,
    0x0, 0x0, 0};
int len = sizeof(buf);

void sendHitachiCode(int temperature, int ventilation, int mode, int powerful, int eco, int on)
{

  unsigned char c = 0xc2;
  buf[11] = temperature << 1;
  buf[13] = ventilation + 1;
  buf[10] = mode;
  buf[25] = eco ? 0x02 : (powerful ? 0x20 : 0);
  buf[17] = on ? 0x80 : 0;

  for (int i = 0; i < 27; i++)
    c = (c + buf[i]) & 0xff;

  c = 0xff ^ (c - 1);
  buf[27] = c;

  Serial1.begin(freq);
  pulse(START_PULSE);
  pause(START_PAUSE);
  pulse(START_PULSE2);
  pause(START_PAUSE2);
  for (int i = 0; i < len; i++)
  {
    unsigned char byte = buf[i];
    for (int bit = 0; bit < 8; bit++)
    {
      int bit_value = (byte >> bit) & 1;
      pulse(PULSE_LEN);
      if (bit_value)
        pause(PAUSE_HIGH);
      else
        pause(PAUSE_LOW);
    }
  }
  pulse(PULSE_LEN);
  Serial1.flush();
  Serial1.end();
  pinMode(2, OUTPUT);
  digitalWrite(2, 0);

  server.send(200, "text/plain", String("written 28 bytes ") + on);
}

void handleIr()
{
  int temperature = 24, ventilation = 2, mode = 3, powerful = 0, eco = 0, on = 1;
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "temp")
    {
      temperature = server.arg(i).toInt();
      if (temperature < 15 || temperature > 30)
      {
        server.send(400, "text/plain", "bad temperature");
        return;
      }
    }
    if (server.argName(i) == "vent")
    {
      ventilation = server.arg(i).toInt();
      if (ventilation < 0 || ventilation > 3)
      {
        server.send(400, "text/plain", "bad ventilation");
        return;
      }
    }
    if (server.argName(i) == "mode")
    {
      String a = server.arg(i);
      if (a == "HOT")
        mode = 3;
      else if (a == "COLD")
        mode = 4;
      else if (a == "HUM")
        mode = 5;
      else
      {
        server.send(400, "text/plain", "bad mode");
        return;
      }
    }
    if (server.argName(i) == "pow")
      powerful = !!server.arg(i).toInt();
    if (server.argName(i) == "eco")
      eco = !!server.arg(i).toInt();
    if (server.argName(i) == "on")
      on = !!server.arg(i).toInt();
  }
  sendHitachiCode(temperature, ventilation, mode, powerful, eco, on);
  digitalWrite(led, 0);
}

void handleRoot()
{
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(led, 0);
}

void handleNotFound()
{
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void)
{
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  Serial1.begin(38000 * 2);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266"))
  {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/ir", handleIr);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void)
{
  server.handleClient();
}
