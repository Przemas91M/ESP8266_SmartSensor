#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>

#define SEALEVELPRESSURE_HPA (1013.25)

//Dane serwera w przypadku braku połączenia
const char *ssid_ap = "SmartSensor";
const char *password_ap = "87654321";
bool serverOn = false;
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

//do przechowywania danych pobranych z EEPROMU
String ssid;          //0-49
String password;      //50-99
String mqtt_ip;       //100-149
String mqtt_user;     //150-199
String mqtt_password; //200-249
String temp_topic;    //250-349
String hum_topic;     //350-449
String press_topic;   //450-549

//Obiekt serwera
ESP8266WebServer server(80);
//Obiekt klienta WiFi
WiFiClient esp;
//Obiekt klienta mqtt
PubSubClient clientMqtt(esp);
//Obiekt czujnika
Adafruit_BME280 bme280;
//zmienna wskazująca obecność czujnika
bool sensorPresent = false;
//zmienne przechowujące odczyt z czujnika
float temperature, humidity, pressure;
//zmienne potrzebne do funkcji millis
unsigned long now, lastread;
/*
Pobierz dane do łączenia się z eepromu |/
Po nieudanej próbie połączenia uruchom serwer (sygnalizacja diodą?) |/
Na stronce odpal możliwość wpisania danych do połączenia  |/
Zapisz dane do eepromu |/
Po restarcie i prawidłowym połączeniu wyślij dane do home assistanta
deep sleep
*/
//strona domowa
String HOMEPAGE = R"=====(
<!DOCTYPE html>
<html>
 <head>
   <meta charset="utf-8" />
   <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
   <meta name="viewport" content="width=device-width, initial-scale=1.0">
   <style>
     button {
     font-size: 20px;
     margin: 10px; }
   </style>
  </head>
<body style="background-color:powderblue;">
<center>
  <div style="background-color:orange;border:3px solid black;color:black">
<h1> Smart Sensor</h1>
  <br>
    <div style="background-color:orange;border:3px solid black;color:black">
    &sensor&
      <br>
  </div>
  <a href="config"><button id=config>Konfiguruj połączenie sieciowe</button></a>
  <div style="clear : both">
  </div>
  <br>
  <a href="exit"><button id=exit>Wyjdź</button></a>
  <br><br>
  <div style="background-color:orange;border:3px solid black;color:black">
    Po wyjściu urządzenie zostanie zresetowane<br>i podejmie próbę połączenia z zapisaną siecią.
  </div>
  </div>
<center>
</body>
</html>
)=====";

//strona na której wyświetają się dostępne sieci i użytkownik wybiera sieć, z którą chce się połączyć
String CONFIGPAGE = R"=====(
<!DOCTYPE html>
<html>
 <head>
   <meta charset="utf-8" />
   <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
   <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
<body style ="background-color:powderblue">
  <center>
<div style="background-color:orange;border:3px solid black;color:black">
<h1> Smart Sensor</h1>
  <div style="background-color:orange;border:3px solid black;color:black">
    Dostępne sieci WiFi:<br>
    &sieci&
  </div>
    <form method = "post" action = "/connectWifi">
        SSID:
    <p>
    <input type="text" id="ssid" name="ssid" value="">
    </p>
      Hasło:
      <p>
    <input type ="password" id="password" name="password" value="">
      </p><p>
    <button type="submit">Połącz</button>  
    </form>
    <a href="config" ><button id=config>Wyszukaj ponownie sieci</button></a>
  </p>
  </div>
  </center>
</body>
</html>
)=====";

//strona wyświetlana, gdy użytkownik chce się połączyć z siecią
//w miejscu &conn& wyświetlany jest komunikat o powodzeniu bądź niepowodzeniu połączenia z wybraną siecią
String CONNECTWIFIPAGE = R"=====(
<!DOCTYPE html>
<html>
 <head>
   <meta charset="utf-8" />
   <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
   <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
<body style="background-color:powderblue">
  <center>
    <div style="background-color:orange;border:3px solid black;color:black">
<h1> Smart Sensor</h1>
      <br>
    <a href="config" ><button id=config>Wyszukaj ponownie sieci</button></a>
  </p><br>
  <div style="background-color:orange;border:3px solid white;color:black">
    &conn&
  </div>
    <br>
    <p>
    <a href="mqttsettings"><button id=mqttsettings>Dalej</button></a>      
    </p>
    <br>
  <p>
  <a href="/"><button id=homepage>Wróć</button></a>
  </p>
    </div>
  </center>
</body>
</html>
)=====";
//strona konfiguracji MQTT
String MQTTSETPAGE = R"=====(
  <!DOCTYPE html>
<html>
 <head>
   <meta charset="utf-8" />
   <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
   <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
<body style ="background-color:powderblue">
  <center>
<div style="background-color:orange;border:3px solid black;color:black">
<h1> Smart Sensor</h1>
  <div style="background-color:orange;border:3px solid black;color:black">
Proszę uzupełnić poniższe pola<br>
  w celu zakończenia konfiguracji urządzenia.</div>
    <form method = "post" action = "/connectMQTT">
        IP serwera MQTT:
    <p>
    <input type="text" id="IP" name="IP" value="&IP">
    </p>
      Nazwa użytkownika:
      <p>
    <input type ="text" id="username" name="username" value="&user">
      </p>
      Hasło:
      <p>
    <input type ="password" id="password" name="password" value="">
      </p>
      Topic temperatury:
      <p>
    <input type ="text" id="temp" name="temp" value="&temp">
      </p>
      Topic wilgotności powietrza:
      <p>
    <input type ="text" id="hum" name="hum" value="&hum">
      </p>
      Topic ciśnienia powietrza:
      <p>
    <input type ="text" id="pressure" name="pressure" value="&press">
      </p>
      <p>
    <button type="submit">Zapisz</button>  
    </form>
    <a href="/" ><button id=homepage>Wyjdź</button></a>
  </p>
  </div>
  </center>
</body>
</html>
)=====";

String SAVEPAGE = R"=====(
<!DOCTYPE html>
<html>
 <head>
   <meta charset="utf-8" />
   <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
   <meta name="viewport" content="width=device-width, initial-scale=1.0">
  </head>
<body style="background-color:powderblue">
  <center>
    <div style="background-color:orange;border:3px solid black;color:black">
<h1> Smart Sensor</h1>
      <br>
  </p><br>
  <div style="background-color:orange;border:3px solid white;color:black">
    &conn&
  </div>
    <br>
    <p>
      <a href="mqttsettings"><button id=mqttsettings>Wróć</button></a>
    </p>
  <p>
    <a href="/"><button id=homepage>Zakończ</button></a>    
  </p>
    </div>
  </center>
</body>
</html>
)=====";

//funkcja szukająca sieci
String searchWifi()
{
  String p = CONFIGPAGE;
  int n = WiFi.scanNetworks();
  //jeżeli urządzenie znajdzie sieci WiFi
  if (n > 0)
  {
    String replacement;
    for (int i = 0; i < n; i++)
    {
      replacement += WiFi.SSID(i) + "<br>";
    }
    p.replace("&sieci&", replacement);
  }
  //gdy nie znaleziono żadnej sieci
  else if (n == 0)
  {
    p.replace("&sieci&", "");
  }
  return p;
}
//funkcja do łączenia się z siecią
bool connectToSSID(const char *SSID, const char *PASSWORD)
{
  uint8_t counter = 0;
  //bool output = false;
  //bool connected = false;
  //Serial.print("Connecting to: ");
  //Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED && counter < 70)
  {
    //output = !output;
    counter++;
    //digitalWrite(LED_BUILTIN, output);
    delay(100);
  }
  if (counter < 70)
  {
    //Serial.println("Connection sucessfull");
    return true;
  }
  //Serial.println("Cant connect!");
  return false;
}
//połączenie z MQTT
bool connectToMqtt()
{
  uint8_t counter = 0;
  clientMqtt.setServer(mqtt_ip.c_str(), 1883);
  String client_id = "ESP8266-";
  client_id += String(WiFi.macAddress());
  while (!clientMqtt.connected() && counter < 5)
  {
    //Serial.print("Connecting to MQTT broker ...");
    if (clientMqtt.connect(client_id.c_str(), mqtt_user.c_str(), mqtt_password.c_str()))
    {
      //Serial.println("OK");
      return true;
    }
    else
    {
      //Serial.print("[Error] Not connected: ");
      //Serial.print(clientMqtt.state());
      //Serial.println(" Wait 1 second before retry.");
      counter++;
      delay(1000);
    }
  }
  return false;
}
//czyszczenie pamięci EEPROM
void clearEEPROM()
{
  EEPROM.begin(600);
  for (int i = 0; i < 600; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.end();
  //Serial.println("EEPROM clear completed!");
}
//odczyt danych z EEPROM
void readEEPROM()
{
  EEPROM.begin(600);
  //odczyt danych sieciowych 0 - 49
  for (int i = 0; i < 50; i++)
  {
    ssid += char(EEPROM.read(i));
    password += char(EEPROM.read(50 + i));
    mqtt_ip += char(EEPROM.read(100 + i));
    mqtt_user += char(EEPROM.read(150 + i));
    mqtt_password += char(EEPROM.read(200 + i));
  }
  //odczyt topiców 0 - 99
  for (int i = 0; i < 100; i++)
  {
    temp_topic += char(EEPROM.read(250 + i));
    hum_topic += char(EEPROM.read(350 + i));
    press_topic += char(EEPROM.read(450 + i));
  }
  EEPROM.end();
  //Serial.println("Read from EEPROM: ");
  //Serial.println(ssid);
  //Serial.println(password);
  //Serial.println(mqtt_ip);
  //Serial.println(mqtt_user);
  //Serial.println(mqtt_password);
  //Serial.println(temp_topic);
  //Serial.println(hum_topic);
  //Serial.println(press_topic);
}
//zapis danych sieci do EEPROM
void writeEEPROM(String _ssid, String _pass, String _mqttIp, String _mqttUsr, String _mqttPass,
                 String _tempTopic, String _humTopic, String _pressTopic)
{
  clearEEPROM();
  EEPROM.begin(600);
  //zapis ssid sieci Wifi
  for (int i = 0; i < _ssid.length(); i++)
  {
    EEPROM.write(i, _ssid[i]);
  }
  //zapis hasła do Wifi
  for (int i = 0; i < _pass.length(); i++)
  {
    EEPROM.write(i + 50, _pass[i]);
  }
  //zapis IP serwera mqtt
  for (int i = 0; i < _mqttIp.length(); i++)
  {
    EEPROM.write(i + 100, _mqttIp[i]);
  }
  //zapis nazwy użytkownika mqtt
  for (int i = 0; i < _mqttUsr.length(); i++)
  {
    EEPROM.write(i + 150, _mqttUsr[i]);
  }
  //zapis hasła do serwera mqtt
  for (int i = 0; i < _mqttPass.length(); i++)
  {
    EEPROM.write(i + 200, _mqttPass[i]);
  }
  //zapis topicu temperatury
  for (int i = 0; i < _tempTopic.length(); i++)
  {
    EEPROM.write(i + 250, _tempTopic[i]);
  }
  //zapis topicu wilgotności
  for (int i = 0; i < _humTopic.length(); i++)
  {
    EEPROM.write(i + 350, _humTopic[i]);
  }
  //zapis topicu ciśnenia
  for (int i = 0; i < _pressTopic.length(); i++)
  {
    EEPROM.write(i + 450, _pressTopic[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  //Serial.println("Data saved to EEPROM");
}
//strona domowa
void handleRoot()
{
  String p = HOMEPAGE;
  if (sensorPresent)
  {
    p.replace("&sensor&", "Czujnik działa prawidłowo.");
  }
  else
  {
    p.replace("&sensor&", "Brak połączenia z czujnikiem!<br>Sprawdź proszę, czy wszystko jest dobrze podłączone.");
  }
  server.send(200, "text/html", p);
}
//strona połączenia z wybraną siecią - sprawdzam połączenie z WiFI, następnie proszę o wpisanie danych mqtt
void handleConnectWiFi()
{
  String p = CONNECTWIFIPAGE;
  ssid = server.arg("ssid");
  password = server.arg("password");
  //Serial.print("SSID: ");
  //Serial.println(ssid);
  //Serial.print("Password: ");
  //Serial.println(password);
  /*if (connectToSSID(ssid.c_str(), password.c_str()))
  {
    p.replace("&conn&", "Nawiązano połączenie z siecią bezprzewodową<br>Następnym krokiem jest konfiguracja parametrów MQTT.");
    //writeEEPROM(ssid, password, mqtt_ip, mqtt_user, mqtt_password, temp_topic, hum_topic, press_topic);
  }
  else
  {
    p.replace("&conn&", "Błąd połączenia z wybraną siecią!");
  }*/
  p.replace("&conn&", "Konfiguracja sieci zakończona");
  server.send(200, "text/html", p);
}
//strona konfiguracji i wyświetlania znalezionych sieci
void handleConfig()
{
  String page = searchWifi();
  server.send(200, "text/html", page);
}
//strona połączenia z mqtt
void handleConnectMQTT()
{
  String p = SAVEPAGE;
  mqtt_ip = server.arg("IP");
  mqtt_user = server.arg("username");
  mqtt_password = server.arg("password");
  temp_topic = server.arg("temp");
  hum_topic = server.arg("hum");
  press_topic = server.arg("pressure");
  //Serial.println("MQTT settings:");
  //Serial.println(mqtt_ip);
  //Serial.println(mqtt_user);
  //Serial.println(mqtt_password);
  //Serial.println(temp_topic);
  //Serial.println(hum_topic);
  //Serial.println(press_topic);
  p.replace("&conn&", "Konfiguracja zakończona");
  writeEEPROM(ssid, password, mqtt_ip, mqtt_user, mqtt_password, temp_topic, hum_topic, press_topic);
  server.send(200, "text/html", p);
}
//strona konfiguracji mqtt
void handleMQTTConfig()
{
  String p = MQTTSETPAGE;
  //wstawiam zapisane dane, jeżeli takie mam
  //Serial.println(mqtt_ip.c_str());
  //Serial.println(mqtt_user.c_str());
  //Serial.println(temp_topic.c_str());
  p.replace("&IP", mqtt_ip.c_str());
  p.replace("&user", mqtt_user.c_str());
  p.replace("&temp", temp_topic.c_str());
  p.replace("&hum", hum_topic.c_str());
  p.replace("&press", press_topic.c_str());
  server.send(200, "text/html", p);
}
//wyjście z konfiguratora
void handleExit()
{
  ESP.restart();
}

void setup()
{
  uint8_t counter = 0;
  //Serial.begin(115200);
  //zmiana funkcjonowania GPIO 1 i 3
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);
  pinMode(1, OUTPUT);
  pinMode(3, OUTPUT);
  digitalWrite(1, HIGH);
  //uruchomienie komunikacji i2c
  Wire.begin(0, 2);
  delay(2000);
  //wyszukiwanie czujnika pod adresem 76
  if (bme280.begin(0x76))
  {
    //Serial.println("Połączono z czujnikiem!");
    sensorPresent = true;
  }
  readEEPROM();
  //połączenie z Wifi nastąpi tylko wtedy, gdy czujnik działa
  //w przeciwnym wypadku uruchamiany jest portal konfiguracyjny
  if (sensorPresent) //dorzucić obsługę przycisku konfiguracyjnego
  {
    //Serial.println("Connecting to WiFi.");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED && counter < 70)
    {
      counter++;
      delay(500);
      if(digitalRead(1))
      {
        digitalWrite(1, LOW);
      }
      else
      {
        digitalWrite(1, HIGH);
      }
    }
  }
  //jeżeli nie ma połączenia z Wifi, bądź nie znaleziono czujnika, uruchamia AP
  if (WiFi.status() != WL_CONNECTED)
  {
    ////Serial.println("Starting AP");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ssid_ap, password_ap);
    server.on("/", handleRoot);
    server.on("/connectWifi", handleConnectWiFi);
    server.on("/config", handleConfig);
    server.on("/mqttsettings", handleMQTTConfig);
    server.on("/connectMQTT", handleConnectMQTT);
    server.on("/exit", handleExit);
    server.begin();
    serverOn = true;
    //Serial.println("Server started!");
  }
  else
  {
    //Serial.println("Connected!");
    connectToMqtt();
  }
}

void loop()
{
  digitalWrite(1, HIGH);
  while (serverOn)
  {
    server.handleClient();
    digitalWrite(1, LOW);
  }
  now = millis();
  if (now - lastread > 60000) //przesyłaj odczyt co minutę (docelowo 10 minut)
  {

    digitalWrite(1, LOW);
    temperature = bme280.readTemperature();
    humidity = bme280.readHumidity();
    pressure = (bme280.readPressure() / 100.0F);
    clientMqtt.publish(temp_topic.c_str(), String(temperature, 1).c_str(), true);
    clientMqtt.publish(hum_topic.c_str(), String(humidity, 1).c_str(), true);
    clientMqtt.publish(press_topic.c_str(), String(pressure, 1).c_str(), true);
    //Serial.println("Data sent!");
    lastread = now;
  }
  clientMqtt.loop();
}
