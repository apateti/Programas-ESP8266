#define DEBUG_MODE
#define Sen_DHT
//#define Sen_Agua
//#define Rele_isLed
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
//#include "MqttConect.h"
#include <WifiConect.h>
#include <EepromConect.h>
#include <PubSubClient.h>
#ifdef Sen_DHT
  #include "DHT.h"
#endif
#ifdef Sen_Agua
  const int analogInPin = A0;
  int sensorValue = 0; // El valor leído del pin
  unsigned long previousMillisAgua = 0;
#endif

//Se definen el Hardware a Utilizar
#define ledStatus     2           //D4
//#define LedStatus   12          //D6
#define Rele1         13          //D7
#define RELAY_ON      1
#define RELAY_OFF     0
//#define Rele2       14          //D5
#define Time_100      1
#define BUTTON        0           //D3
//#define SWITCH2     5           //D1
#define LED_OFF       1
#define LED_ON        0
#define Time_4000_uSeg  40
uint16_t  brightnessStatus = 0;
#define brightOFF 1023
#define brightON 0
//Se definen variables de Configuracion
bool  Time_100mS_L;
bool  Time_100mS_B;
bool  Prim_V = true;
bool  flagConect = false;
bool  flagProg = false;
bool  statusSw = false;
uint8_t Count_Botton = 0;
uint8_t Cont_LedStatus = 0;
const char* ssid_AP = "Home_IoT_";
String ssid_ap;
String deviceId;
String ID_ESP;
const char* paswr_ap = "0123456789";
const long interval = 100;
const long intervalSen = 30000;
const long intervalRec = 60000;
unsigned long previousMillisLed = 0;
unsigned long previousMillisButton = 0;
unsigned long previousMillisMQTT = 0;
unsigned long previousMillisReconect = 0;
//Se devinen variables de las Clases
WiFiClient ESP8266Client;
ESP8266WebServer server(80);
PubSubClient client_MQTT(ESP8266Client);
//Definidas por Usuario
WifiConect wifiESP(ledStatus);
EepromConect eepromESP;
#ifdef Sen_DHT
  const int DHTPin = 2;
  #define DHTTYPE DHT11   // DHT 11
  DHT dht(DHTPin, DHTTYPE);
  String temp, hume;
#endif
////////////////////////////////////////////////////////////////////////////////
///Variables MQTT
////////////////////////////////////////////////////////////////////////////////
const char* Topic_LUZ = "LUZ";               //Topico; ChipID/Ubicacion/LUZ        Para Recibir Condicion LUZ
const char* Topic_ToggleLuz = "Toggle";      //Topico; ChipID/Ubicacion/Toggle     Para Recibir Condicion Toggle LUZ
const char* Topic_StaLuz = "Est_LUZ";        //Topico; ChipID/Ubicacion/Est_LUZ    Para Enviar Estatuz LUZ
#ifdef Sen_DHT
  const char* Topic_Tem = "Temp";              //Topico; ChipID/Ubicacion/Temp       Para Enviar Temperatura
  const char* Topic_Hum = "Hume";              //Topico; ChipID/Ubicacion/Hum        Para Enviar Humedad
  String topicTem, topicHum;
#endif
#ifdef Sen_Agua
  const char* Topic_SensAgua = "Agua";
  const char* Topic_AlarAgua = "AlarAgua";
  String topicAgua, topicAlarAgua;  
  #define umbralAlarmaAgua 150
#endif
String topicLuz, topicToggle, topicStatLuz;
//Broker
String Broker_MQTT, Topic_MQTT; //Direccion y Topic del Servidor MQTT

void setup() {
  // Se configuran los Perifericos
  #ifdef DEBUG_MODE
    Serial.begin(115200);
  #endif
  pinMode(BUTTON, INPUT);
  pinMode(Rele1, OUTPUT);
  pinMode(ledStatus, OUTPUT);
  digitalWrite(Rele1, RELAY_OFF);
  eepromESP.eepromInit();
  Param_ESP();
  conectarAP();
  #ifdef Sen_DHT
    dht.begin();
  #endif
  #ifdef Sen_Agua
    pinMode(analogInPin, INPUT);
  #endif
  parametrosMqtt();
  Init_MQTT();
  brightnessStatus = eepromESP.readBrighLED();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
  Function_Botton();
  Function_Led();
  #ifdef Sen_DHT
    Funcion_Sensor_DHT();
  #endif
  #ifdef Sen_Agua
    Funcion_Sensor_Agua();
  #endif
  if (client_MQTT.connected()){
    client_MQTT.loop();
  }
  Funcion_ReconectMQTT();
}
//*******************************************************************************
//Rutina Function_Botton():
// Rutina monitores el Estado del Botton
//*******************************************************************************
void Function_Botton(){
  if (!digitalRead(BUTTON)){
    if(Prim_V)
    {
      delay(20);
      if (!digitalRead(BUTTON))
      {
        Prim_V = false;
        Count_Botton = 0;
        digitalWrite(Rele1, !digitalRead(Rele1));
        #ifdef Rele_isLed
          String statusR = digitalRead(Rele1) ? "OFF" : "ON";
        #else
          String statusR = digitalRead(Rele1) ? "ON" : "OFF";
        #endif
        if (client_MQTT.connected()){
          client_MQTT.publish(topicStatLuz.c_str(), statusR.c_str());
        }
        #ifdef DEBUG_MODE
          Serial.println("Pulsador Botton");
        #endif
      }
    }
  }
  else{
    Count_Botton = 0;
    Prim_V = true;
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisButton >= interval){
    previousMillisButton = currentMillis;
    Count_Botton++;
    Time_100mS_B = false;
    if(Count_Botton > Time_4000_uSeg){
      Count_Botton = 0;
      flagProg = !flagProg;
      if(flagProg){
        #ifndef DEBUG_MODE
          Serial.println("Modo Programación");
        #endif
        //digitalWrite(ledStatus, !digitalRead(ledStatus));
        analogWrite(ledStatus, brightON);
        Mode_Prog_AP();
      }else{
        #ifdef DEBUG_MODE
          Serial.println("\nRESET de Fabrica");
        #endif
        eepromESP.erraseALL();
        delay(500);
        ESP.restart();
      }
    }
  }
}////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para indicar por medio del Led de Status el estado del Equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Function_Led(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisLed < interval){
    return;
  }
  if(!digitalRead(BUTTON)){
    return;
  }
  pinMode(ledStatus, OUTPUT);
  Time_100mS_L = false;
  previousMillisLed = currentMillis;
  if(WiFi.status() != WL_CONNECTED){
    flagConect = false;
  }else{
    flagConect = true;
  }
  if(flagProg){
    if(Cont_LedStatus == 1 || Cont_LedStatus == 4 || Cont_LedStatus == 7){
      analogWrite(ledStatus, brightnessStatus); //digitalWrite(LedStatus,HIGH);
    }else{
      analogWrite(ledStatus, brightOFF); //digitalWrite(LedStatus,LOW);
    }
  }else{
    if(flagConect){
      if(Cont_LedStatus == 1){
        analogWrite(ledStatus, brightnessStatus); //digitalWrite(LedStatus,HIGH);
      }else{
        analogWrite(ledStatus, brightOFF); //digitalWrite(LedStatus,LOW);
      }
    }else{
      if(Cont_LedStatus == 1 || Cont_LedStatus == 5){
        analogWrite(ledStatus, brightnessStatus); //digitalWrite(LedStatus,HIGH);
      }else{
        analogWrite(ledStatus, brightOFF); //digitalWrite(LedStatus,LOW);
      }
    }
  }
  Cont_LedStatus++;
  if(Cont_LedStatus > 19){
    Cont_LedStatus = 0; 
    #ifdef DEBUG_MODE
      Serial.println("Funcion LED: 2 Seg");
    #endif
  }
    
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Funcion que configura el equipo en modo Acces Point
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void  Mode_Prog_AP(void)
{
    Param_ESP();
    delay(500);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(400);
    wifiESP.resetSettingsSTA();
    #ifdef DEBUG_MODE
      Serial.println("SSID como Acces Point: "+ssid_ap);
      Serial.println("Pasword como Acces Point: "+String(paswr_ap));
    #endif
    WiFi.softAP(ssid_ap.c_str(), paswr_ap);
    int Intentos = 0;
    conectarServer();
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Parametros del ESP
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Param_ESP(void)
{
  ID_ESP = String(ESP.getChipId(), HEX);
  ID_ESP.toUpperCase();
  deviceId = ID_ESP.c_str();
  ssid_ap = ssid_AP;
  ssid_ap += ID_ESP;

//    Serial.print("El Chip ID  es: ");
//    Serial.println(ID_ESP.c_str());
//    Serial.println("");
//    Serial.print("El reset fue por: ");
//    Serial.println(ESP.getResetReason());
//    Serial.print("La Version del Core es: ");
//    Serial.println(ESP.getCoreVersion());
//    Serial.print("La Version del SDK es: ");
//    Serial.println(ESP.getSdkVersion());
//    Serial.print("La Frecuencia del CPU es: ");
//    Serial.println(ESP.getCpuFreqMHz());
//    Serial.print("El Tamaño del Programa es: ");
//    Serial.println(ESP.getSketchSize());
//    Serial.print("El actual Scketch es: ");
//    Serial.println(ESP.getSketchMD5());
//    Serial.print("El ID de la FLASH es: ");
//    Serial.println(ESP.getFlashChipId());
//    Serial.print("La Frecuencia de la FLASH es: ");
//    Serial.println(ESP.getFlashChipSpeed());
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Conectarse al Acces Point que esta almacenada en al EEPROM
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void conectarAP(void){
  String ssidR, passwR;
  IPAddress ipFijas[5];
  ssidR = eepromESP.readSSID();
  passwR = eepromESP.readPASS();
  #ifdef DEBUG_MODE
    Serial.println("");
    Serial.print("SSID de la EEPROM: "); Serial.println(ssidR);
    Serial.print("PASSW de la EEPROM: "); Serial.println(passwR);
  #endif
  ipFijas[0] = eepromESP.readIP();
  ipFijas[1] = eepromESP.readGATEWAY();
  ipFijas[2] = eepromESP.readSUBNET();
  ipFijas[3] = eepromESP.readdns1();
  ipFijas[4] = eepromESP.readdns2();
  if(ipFijas[0][0] == 0xFF){
    ipFijas[0][0] = 0; ipFijas[0][1] = 0; ipFijas[0][2] = 0; ipFijas[0][3] = 0;
  }
  #ifdef DEBUG_MODE
    Serial.println("");
    Serial.print("IP EEPROM: "); Serial.println(ipFijas[0]);
    Serial.print("GATEWAY EEPROM: "); Serial.println(ipFijas[1]);
    Serial.print("SUBNET EEPROM: "); Serial.println(ipFijas[2]);
    Serial.print("DNS1 EEPROM: "); Serial.println(ipFijas[3]);
    Serial.print("DNS2 EEPROM: "); Serial.println(ipFijas[4]);
  #endif
  if(ssidR != ""){
      if(wifiESP.conectAP(ssidR, passwR, ipFijas[0], ipFijas[1], ipFijas[2])){
        conectarServer();
        #ifdef DEBUG_MODE
          Serial.println("");
          Serial.println("Conectado!!!");
          Serial.print("IP address: "); Serial.println(WiFi.localIP());
          Serial.print("IP: "); Serial.println(ipFijas[0]);
          Serial.print("GATEWAY: "); Serial.println(ipFijas[1]);
          Serial.print("SUBNET: "); Serial.println(ipFijas[2]);
          Serial.print("DNS1: "); Serial.println(ipFijas[3]);
          Serial.print("DNS2: "); Serial.println(ipFijas[4]);
        #endif
      }else
        #ifdef DEBUG_MODE
          Serial.println("NOOOOO Conectado!!!");
        #endif
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Configurar el Server, Rutina que ejecutara el recibir el http
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void conectarServer(void){
  //server.on("/cmdo", cmdoJson);
  server.on(F("/cmdoJson"), HTTP_POST, verCmdoJson);
  server.onNotFound(handleNotFound);
  server.begin();
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Determinar el Comando Recibido en el Json
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void verCmdoJson(void){
  //StaticJsonDocument<1000> jsonBuffer;
  DynamicJsonDocument jsonBuffer(8196);
  DeserializationError error = deserializeJson(jsonBuffer, server.arg("plain"));
  String pay = server.arg("plain");   //Colocando la palabra "plain" guarda todo lo Recibido
  #ifdef DEBUG_MODE
    Serial.println("Payload Json Rx: ");
    Serial.println(pay);
  #endif
  if(error){
    #ifdef DEBUG_MODE
      Serial.println("Error al Rx Json");
    #endif
    String SJson = "{\"error\":1,\"data\":{\"deviceid\":\""+deviceId+"\"}}";
    #ifdef DEBUG_MODE
      Serial.print("Error al Rx Json: ");
      Serial.println(SJson);
    #endif
    server.send(200, F("application/json"), SJson);
    return;
  }
  String comando = jsonBuffer["cmdo"];
  if(comando == "status1")
    status1Json();
  else if(comando == "reset")
    resetJson();
  else if(comando == "softReset")
    softResetJson();
  else if(comando == "brightLedStatus"){
    uint8_t brightLed = jsonBuffer["data"]["Bright"];
    brightLedStatus(brightLed);
  }
  else if(comando == "readBrightLedStatus"){
    readBrightLedStatus();
  }
  else if(comando == "toggle1")
    toggle1Json();
  else if(comando == "luzOn1")
    luzOn1Json();
  else if(comando == "luzOff1")
    luzOff1Json();
  else if(comando == "accPoint")
    accPointJson();
  else if(comando == "infoWifi")
    infoWiFi();
  #ifdef Sen_DHT
    else if(comando == "tempHume")
      tempHume();
  #endif
  else if(comando == "conectAP"){
    String ssidJSON = jsonBuffer["data"]["ssid"];
    if ( ssidJSON.length() > 1 ){
      conectAPJson(jsonBuffer["data"]["ssid"], jsonBuffer["data"]["pass"]);
    }else{
      errorJson();
    }
  }
  else if(comando == "infoMqtt"){
    infoMQTT();
  }
  else if(comando == "cmdBroker"){
    String brokerJSON = jsonBuffer["data"]["broker"];
    if ( brokerJSON.length() > 1 )
      saveBroker(brokerJSON);
    else
      errorJson();
  }
  else if(comando == "infoAll"){
    infoAll();
  }
  else if(comando == "topic"){
    String topicJSON = jsonBuffer["data"]["topic"];
    if ( topicJSON.length() > 1 )
      saveTopic(topicJSON);
    else
      errorJson();
  }
  else if(comando == "erraseEE"){
    erraseEEPROM();
  }
  else
    errorJson();
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar el Status del equipo en Json
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool status1Json(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  #ifdef Rele_isLed
    String statusR = digitalRead(Rele1) ? "OFF" : "ON";
  #else
    String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  #endif
  obj["switch"] = statusR;
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar hacer Reset del Equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool resetJson(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  IPAddress ipFijas[5];
  String ssidR, passwR;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
  ssidR = eepromESP.readSSID();
  passwR = eepromESP.readPASS();
  #ifdef DEBUG_MODE
    Serial.println("");
    Serial.print("SSID de la EEPROM: "); Serial.println(ssidR);
    Serial.print("PASSW de la EEPROM: "); Serial.println(passwR);
  #endif
  ipFijas[0] = eepromESP.readIP();
  ipFijas[1] = eepromESP.readGATEWAY();
  ipFijas[2] = eepromESP.readSUBNET();
  ipFijas[3] = eepromESP.readdns1();
  ipFijas[4] = eepromESP.readdns2();
  if(ipFijas[0][0] == 0xFF){
    ipFijas[0][0] = 0; ipFijas[0][1] = 0; ipFijas[0][2] = 0; ipFijas[0][3] = 0;
  }
  if(ssidR != ""){
      //noInterrupts();
      if(wifiESP.conectAP(ssidR, passwR, ipFijas[0], ipFijas[1], ipFijas[2])){
        conectarServer();
        #ifdef DEBUG_MODE
          Serial.println("");
          Serial.println("Conectado despues de JSON RESET!!!");
          Serial.print("IP address: "); Serial.println(WiFi.localIP());
          Serial.print("IP: "); Serial.println(ipFijas[0]);
          Serial.print("GATEWAY: "); Serial.println(ipFijas[1]);
          Serial.print("SUBNET: "); Serial.println(ipFijas[2]);
          Serial.print("DNS1: "); Serial.println(ipFijas[3]);
          Serial.print("DNS2: "); Serial.println(ipFijas[4]);
        #endif
        flagProg = false;
      }else
        #ifdef DEBUG_MODE
          Serial.println("NOOOOO Conectado!!!");
        #endif
      //interrupts();
  }
  //conectarAP();  
  conectarServer();
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar hacer Reset por Software del Equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void softResetJson(void){
  ESP.restart();
}
void brightLedStatus(uint8_t brightLed){
  switch (brightLed){
    case 1:
      brightnessStatus = 970;
      break;
    case 2:
      brightnessStatus = 850;
      break;
    case 3:
      brightnessStatus = 700;
      break;
    case 4:
      brightnessStatus = 600;
      break;
    case 5:
      brightnessStatus = 500;
      break;
    case 6:
      brightnessStatus = 400;
      break;
    case 7:
      brightnessStatus = 300;
      break;
    case 8:
      brightnessStatus = 200;
      break;
    case 9:
      brightnessStatus = 100;
      break;
    case 10:
      brightnessStatus = 0;
      break;
    default:
       brightnessStatus = 0;
      break;
  }
  eepromESP.writeBrighLED(brightnessStatus);
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
}
void readBrightLedStatus(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  uint8_t brightLed;
  switch (brightnessStatus){
    case 970:
      brightLed = 1;
      break;
    case 850:
      brightLed = 2;
      break;
    case 700:
      brightLed = 3;
      break;
    case 600:
      brightLed = 4;
      break;
    case 500:
      brightLed = 5;
      break;
    case 400:
      brightLed = 6;
      break;
    case 300:
      brightLed = 7;
      break;
    case 200:
      brightLed = 8;
      break;
    case 100:
      brightLed = 9;
      break;
    case 0:
      brightLed = 10;
      break;
    default:
       brightLed = 10;
      break;
  }
  JsonObject obj = jsonBuffer.createNestedObject("data");
  obj["Bright"] = brightLed;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para toggle del LED
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool toggle1Json(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  digitalWrite(Rele1, !digitalRead(Rele1));
  ///digitalWrite(LedStatus, digitalRead(Rele1));
  #ifdef Rele_isLed
    String statusR = digitalRead(Rele1) ? "OFF" : "ON";
  #else
    String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  #endif
  obj["switch"] = statusR;
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
  //String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  if (client_MQTT.connected()){
    #ifdef DEBUG_MODE
      Serial.print("Funcion toggle1Json + MQTT Conectado");
    #endif
    client_MQTT.publish(topicStatLuz.c_str(), statusR.c_str());
  }else{
    #ifdef DEBUG_MODE
      Serial.print("Funcion toggle1Json + MQTT NO Conectado");
    #endif
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para ON del LED
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool luzOn1Json(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  digitalWrite(Rele1, LED_ON);
  #ifdef Rele_isLed
    String statusR = digitalRead(Rele1) ? "OFF" : "ON";
  #else
    String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  #endif
  //digitalWrite(LedStatus, HIGH);
  obj["switch"] = statusR;
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
  //String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  if (client_MQTT.connected()){
    #ifdef DEBUG_MODE
      Serial.print("Funcion luzOn1Json + MQTT Conectado");
    #endif
    client_MQTT.publish(topicStatLuz.c_str(), statusR.c_str());
  }else{
    #ifdef DEBUG_MODE
      Serial.print("Funcion luzOn1Json + MQTT NO Conectado");
    #endif
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para OFF del LED
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool luzOff1Json(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  digitalWrite(Rele1, LED_OFF);
  //digitalWrite(LedStatus, LOW);
  #ifdef Rele_isLed
    String statusR = digitalRead(Rele1) ? "OFF" : "ON";
  #else
    String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  #endif
  obj["switch"] = statusR;
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
  //String statusR = digitalRead(Rele1) ? "ON" : "OFF";
  if (client_MQTT.connected()){
    #ifdef DEBUG_MODE
      Serial.print("Funcion luzOff1Json + MQTT Conectado");
    #endif
    client_MQTT.publish(topicStatLuz.c_str(), statusR.c_str());
  }else{
    #ifdef DEBUG_MODE
      Serial.print("Funcion luzOff1Json + MQTT NO Conectado");
    #endif
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar los credenciales del el equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool infoWiFi(void){
  StaticJsonDocument<500> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  int rssidAP = WiFi.RSSI();
  int qualit;
  if (rssidAP <= -100)
    qualit = 0;
  else if (rssidAP >= -50)
    qualit = 100;
  else
  {
    qualit = 2 * (rssidAP + 100);
  }
   obj["ssid"] = WiFi.SSID();
   obj["passw"] = WiFi.psk();
   obj["rssi"] = qualit;
   obj["ip"] = WiFi.localIP().toString();
   obj["gateway"] = WiFi.gatewayIP().toString();
   obj["subnetMask"] = WiFi.subnetMask().toString();
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar la lista de Routers (AccesPoint) que ve el equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool accPointJson(void){
  String SJson;
  StaticJsonDocument<1000> jsonBuffer;
  JsonObject s1 = jsonBuffer.createNestedObject();
  JsonObject json = jsonBuffer.createNestedObject();
  json["error"] = 0;
  json["deviceid"] = deviceId;
  JsonArray datosS = json.createNestedArray("wifi");
  String aux;
  int qualit;
  int n = WiFi.scanNetworks();
  if(n!=0){
    #ifdef DEBUG_MODE
      Serial.println("Redes Encontradas: ");
    #endif
    for(int i = 0; i < n; ++i)
    {
      int Senal_RSSI = WiFi.RSSI(i);
      if (Senal_RSSI <= -100)
        qualit = 0;
      else if (Senal_RSSI >= -50)
        qualit = 100;
      else
      {
        qualit = 2 * (Senal_RSSI + 100);
      }
      s1["SSID"]= WiFi.SSID(i);
      s1["QoS"]= qualit;
      s1["Encryption"]= WiFi.encryptionType(i);
      datosS.add(s1);
    }
    serializeJson(json, SJson);
    server.send(200, F("application/json"), SJson);
  }else{
    #ifdef DEBUG_MODE
      Serial.println("Cero redes Encontradas");
    #endif
    s1["SSID"]= "Sin Redes Encontradas";
    s1["QoS"]= 0;
    s1["Encryption"]= 0;
    datosS.add(s1);
    serializeJson(json, SJson);
    server.send(200, F("application/json"), SJson);
  }
  #ifdef DEBUG_MODE
    Serial.println("JSON Enviado: ");
    Serial.println(SJson);
  #endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para conectarse el equipo al Router seleccionado
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool conectAPJson(String ssidRx, String passwRx){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  IPAddress ipFijas[5];
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
  #ifdef DEBUG_MODE
    Serial.println("SSID: "+ssidRx);
    Serial.println("Pass: "+passwRx);
  #endif
  ipFijas[0][0] = 0; ipFijas[0][1] = 0; ipFijas[0][2] = 0; ipFijas[0][3] = 0;
  if(ssidRx.length() > 5){
    //eepromESP.erraseALL();
    int Intentos = 0;
    WiFi.persistent(true);
    WiFi.config(ipFijas[0], ipFijas[0], ipFijas[0]);
    WiFi.begin("", "");
    while (WiFi.status() != WL_CONNECTED && Intentos < 5){
      delay(100);
      Intentos++;
    }
    ESP.eraseConfig();
    WiFi.persistent(false);
    delay(500);
    WiFi.begin(ssidRx.c_str(), passwRx.c_str());
    Intentos = 0;
    #ifdef DEBUG_MODE
      Serial.println("");
      Serial.print("Intentos: ");
    #endif
    while (WiFi.status() != WL_CONNECTED && Intentos < 25) {
    //delay(500);
    digitalWrite(ledStatus, LOW);
    delay(100);
    digitalWrite(ledStatus, HIGH);
    delay(500);
    Intentos++;
    #ifdef DEBUG_MODE
      Serial.print("*");
    #endif
  }
    //interrupts();
  }
  if(WiFi.status() != WL_CONNECTED){
    jsonBuffer["error"] = 9;
  }else{
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    JsonObject obj = jsonBuffer.createNestedObject("data");
    obj["dirIP"] = ipStr;
    eepromESP.writeSSID(ssidRx);
    eepromESP.writePASS(passwRx);  
    eepromESP.writeIP(WiFi.localIP());
    eepromESP.writeGATEWAY(WiFi.gatewayIP());
    eepromESP.writeSUBNET(WiFi.subnetMask());
    eepromESP.writeDNS1(WiFi.dnsIP());
    //
    ipFijas[0] = WiFi.localIP();
    ipFijas[1] = WiFi.gatewayIP();
    ipFijas[2] = WiFi.subnetMask();
    ipFijas[3] = WiFi.dnsIP();
    #ifdef DEBUG_MODE
      Serial.println("");
      Serial.println("Conectado despues de JSON RESET!!!");
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
      Serial.print("IP: "); Serial.println(ipFijas[0]);
      Serial.print("GATEWAY: "); Serial.println(ipFijas[1]);
      Serial.print("SUBNET: "); Serial.println(ipFijas[2]);
      Serial.print("DNS1: "); Serial.println(ipFijas[3]);
      Serial.print("DNS2: "); Serial.println(ipFijas[4]);
    #endif
    #ifdef DEBUG_MODE
      Serial.println("*****************************************");
      Serial.println("Parametros por WiFi.printDiag(Serial)");
      WiFi.printDiag(Serial);
      Serial.println("*****************************************");
    #endif
    
  }
  //serializeJson(jsonBuffer, json);
  //server.send(200, F("application/json"), json);
  //Init_MQTT();
}
#ifdef Sen_DHT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////     Rutina para enviar Json la informacion de los Sensores  ///////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void tempHume(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  JsonObject obj = jsonBuffer.createNestedObject("data");
  readSensor();
  obj["Temperatura"] = temp;
  obj["Humedad"] = hume;
  serializeJson(jsonBuffer, json);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), json);
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para borrar toda la EEPROM - Equipo de Fabrica
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void erraseEEPROM(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  eepromESP.erraseALL();
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 1;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar eror por no recibir Comando Valido
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleNotFound(){
  DynamicJsonDocument doc(512);
  doc["code"] = 404;
  //Serial.print(F("Stream..."));
  String buf;
  serializeJson(doc, buf);
  //sendRequest(400, buf);
  server.send(400, F("application/json"), buf);
  //Serial.print(F("done."));
  #ifdef DEBUG_MODE
    Serial.print("Error de Request del cliente");
  #endif
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar eror por no recibir Json Valido
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool errorJson(void){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 1;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
  
}
#ifdef Sen_DHT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////     Rutina para Publicar MQTT la informacion de los Sensores  ///////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void readSensor(void)
{
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float f = dht.readTemperature(true);

    // Check if any reads failed and exit early (to try again).
    //if (isnan(h) || isnan(t) || isnan(f)) {
    if (isnan(h) || isnan(t)) {
      Serial.println("Fallo la Lectura del Sensor DHT...!");    
      return;
    }
    Serial.print("Temp: ");
    Serial.println(t);
    Serial.print("Hum: ");
    Serial.println(h);
    static char temperatureTemp[7];
    dtostrf(t, 6, 1, temperatureTemp);
    static char humidityTemp[7];
    dtostrf(h, 6, 1, humidityTemp);
    temp = temperatureTemp;
    hume = humidityTemp;
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////     Rutina para Ajustar los Parametros MQTT    ///////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void parametrosMqtt(void){
  String ChipID = String(ESP.getChipId());
  Topic_MQTT = eepromESP.readTopic();
  Broker_MQTT = eepromESP.readBroker();
  topicLuz = ChipID+"/"+Topic_MQTT+"/"+Topic_LUZ;
  topicToggle = ChipID+"/"+Topic_MQTT+"/"+Topic_ToggleLuz;
  topicStatLuz = ChipID+"/"+Topic_MQTT+"/"+Topic_StaLuz;
  #ifdef Sen_DHT
    topicTem = ChipID+"/"+Topic_MQTT+"/"+Topic_Tem;
    topicHum = ChipID+"/"+Topic_MQTT+"/"+Topic_Hum;
  #endif
  #ifdef Sen_Agua
    topicAgua = ChipID+"/"+Topic_MQTT+"/"+Topic_SensAgua;
    topicAlarAgua = ChipID+"/"+Topic_MQTT+"/"+Topic_AlarAgua;
  #endif
  #ifdef DEBUG_MODE
    Serial.println("\nParametros MQTT:");
    Serial.print("Broker MQTT:"); Serial.println(Broker_MQTT);
    Serial.print("Topic MQTT:"); Serial.println(Topic_MQTT);
    Serial.print("topicLuz MQTT:"); Serial.println(topicLuz);
    Serial.print("topicToggle MQTT:"); Serial.println(topicToggle);
    Serial.print("topicStatLuz MQTT:"); Serial.println(topicStatLuz);
    #ifdef Sen_DHT
      Serial.print("topicTem MQTT:"); Serial.println(topicTem);
      Serial.print("topicHum MQTT:"); Serial.println(topicHum);
    #endif
    #ifdef Sen_Agua
      Serial.print("topicAgua MQTT:"); Serial.println(topicAgua);
      Serial.print("topicAlarAgua MQTT:"); Serial.println(topicAlarAgua);
    #endif
  #endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Inicializar las Variables del Servidor y Topico del Servidor MQTT
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Init_MQTT(void)
{
  String EE_Mqtt_Broker;
  EE_Mqtt_Broker = "";
  EE_Mqtt_Broker = eepromESP.readBroker();
  if(EE_Mqtt_Broker == ""){
    return;
  }
  client_MQTT.setServer(Broker_MQTT.c_str(), 1883);
  client_MQTT.setCallback(callback);
  reconnect();
  #ifdef DEBUG_MODE
    Serial.print("\nConectado al Broker: ");
    Serial.println(Broker_MQTT); 
  #endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para recibir los payload MQTT
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void callback(String topic, byte* message, unsigned int length) 
{
  #ifdef DEBUG_MODE
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.println("");
    Serial.print(". Message: ");
  #endif
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  //Serial.print("Mensaje del TOPIC: ");
  #ifdef DEBUG_MODE
    Serial.println(messageTemp);
  #endif
  String Topic_VER_LUZ = topicLuz;
  if(topic==Topic_VER_LUZ){
        String Pub_Topic;
        Pub_Topic = topicStatLuz;
      if(messageTemp == "1"){
        digitalWrite(Rele1,RELAY_ON);
        client_MQTT.publish(Pub_Topic.c_str(), "ON");
      }
      else if(messageTemp == "0"){
        digitalWrite(Rele1,RELAY_OFF);
        client_MQTT.publish(Pub_Topic.c_str(), "OFF");
      }
  }else if(topic==topicToggle){
    if(messageTemp == "1"){
      digitalWrite(Rele1, !digitalRead(Rele1));
      //String statusLuz = digitalRead(Rele1) ? "ON" : "OFF";
      #ifdef Rele_isLed
        String statusR = digitalRead(Rele1) ? "OFF" : "ON";
      #else
        String statusR = digitalRead(Rele1) ? "ON" : "OFF";
      #endif
      client_MQTT.publish(topicStatLuz.c_str(), statusR.c_str());
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Reconectarse al Servidor MQTT
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void reconnect(void) {
  if (!client_MQTT.connected()) {
    String Pub_Topic_StaLuz, Pub_Topic_LUZ;
    Pub_Topic_LUZ = topicLuz;
      Serial.println("Topicos a Publicar: ");
      Serial.println(Pub_Topic_LUZ);
    if(topicLuz == ""){
      Serial.println("Funcion reconect: Sin Topic");
      return;
    }
    String clienteMQTT = "ESP8266Client_";
    clienteMQTT += deviceId;
    if (client_MQTT.connect(clienteMQTT.c_str())) {
      Serial.println("reconnect client_MQTT.connect(\"ESP8266Client\")");
      //client_MQTT.subscribe(Pub_Topic_StaLuz.c_str());
      client_MQTT.subscribe(Pub_Topic_LUZ.c_str());
      client_MQTT.subscribe(topicToggle.c_str());
      //client_MQTT.subscribe(topicStatLuz.c_str());
     }else{
      Serial.println("NOOO.. reconnect client_MQTT.connect(\"ESP8266Client\")");
     }
  }else{
    Serial.println("Conectado a MQTT");
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar informacion del Equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void infoMQTT(void){
  StaticJsonDocument<550> jsonBuffer;
  String Sjson;
  parametrosMqtt();
  //JsonObject json = jsonBuffer.createNestedObject();
  JsonObject json = jsonBuffer.createNestedObject();
  json["error"] = 0;
  json["deviceid"] = deviceId;
  JsonObject s1 = jsonBuffer.createNestedObject();
  JsonObject obj = json.createNestedObject("data");
  obj["Broker"] = Broker_MQTT;
  obj["TopicGeneral"] = Topic_MQTT;
  JsonArray datosS = json.createNestedArray("mqtt");
  
  s1["tipoTopic"] = "Topic Luz";
  s1["topicIs"] = topicLuz;
  datosS.add(s1);
  s1["tipoTopic"] = "Topic Toggle Luz";
  s1["topicIs"] = topicToggle;
  datosS.add(s1);
  s1["tipoTopic"] = "Topic Estatus Luz";
  s1["topicIs"] = topicStatLuz;
  datosS.add(s1);
  #ifdef Sen_DHT
    s1["tipoTopic"] = "Topic Temperatura";
    s1["topicIs"] = topicTem;
    datosS.add(s1);
    s1["tipoTopic"] = "Topic Humedad";
    s1["topicIs"] = topicHum;
    datosS.add(s1);
   #endif
  #ifdef Sen_Agua
    s1["tipoTopic"] = "Topic Agua";
    s1["topicIs"] = topicAgua;
    datosS.add(s1);
    s1["tipoTopic"] = "Topic Alarma Agua";
    s1["topicIs"] = topicAlarAgua;
    datosS.add(s1);
  #endif


  serializeJson(json, Sjson);
  #ifdef DEBUG_MODE
    Serial.print("cmdo Status, se Tx: ");
    Serial.println(json);
  #endif
  server.send(200, F("application/json"), Sjson);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para Almacenar la direccion del Broker
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool saveBroker(String brokerRx){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
  #ifdef DEBUG_MODE
    Serial.println("Broker: "+brokerRx);
  #endif
  eepromESP.writeBroker(brokerRx);
  Init_MQTT();
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para almacenar el Topic
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool saveTopic(String topicRx){
  StaticJsonDocument<200> jsonBuffer;
  String json;
  //JsonObject json = jsonBuffer.createNestedObject();
  jsonBuffer["error"] = 0;
  jsonBuffer["deviceid"] = deviceId;
  serializeJson(jsonBuffer, json);
  server.send(200, F("application/json"), json);
  #ifdef DEBUG_MODE
    Serial.println("Topic: "+topicRx);
  #endif
  eepromESP.writeTopic(topicRx);
}
///////////////////////////////////////////////////////////////////////////////////////////////////77/////////////////////////
////////////// Rutina para enviar informacion del Equipo
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void infoAll(void){
  StaticJsonDocument<1000> jsonBuffer;
  String Sjson;
  parametrosMqtt();
  //JsonObject json = jsonBuffer.createNestedObject();
  JsonObject json = jsonBuffer.createNestedObject();
  json["error"] = 0;
  json["deviceid"] = deviceId;
  JsonObject s1 = jsonBuffer.createNestedObject();
  JsonObject objW = json.createNestedObject("dataW");
  int rssidAP = WiFi.RSSI();
  int qualit;
  if (rssidAP <= -100)
    qualit = 0;
  else if (rssidAP >= -50)
    qualit = 100;
  else
  {
    qualit = 2 * (rssidAP + 100);
  }
  objW["ssid"] = WiFi.SSID();
  objW["passw"] = WiFi.psk();
  objW["rssi"] = qualit;
  objW["ip"] = WiFi.localIP().toString();
  objW["gateway"] = WiFi.gatewayIP().toString();
  objW["subnetMask"] = WiFi.subnetMask().toString();
  JsonObject obj = json.createNestedObject("dataM");
  obj["Broker"] = Broker_MQTT;
  obj["TopicGeneral"] = Topic_MQTT;
  JsonArray datosS = json.createNestedArray("mqtt");
  s1["tipoTopic"] = "Topic Luz";
  s1["topicIs"] = topicLuz;
  datosS.add(s1);
  s1["tipoTopic"] = "Topic Toggle Luz";
  s1["topicIs"] = topicToggle;
  datosS.add(s1);
  s1["tipoTopic"] = "Topic Estatus Luz";
  s1["topicIs"] = topicStatLuz;
  datosS.add(s1);
  #ifdef Sen_DHT
    s1["tipoTopic"] = "Topic Temperatura";
    s1["topicIs"] = topicTem;
    datosS.add(s1);
    s1["tipoTopic"] = "Topic Humedad";
    s1["topicIs"] = topicHum;
    datosS.add(s1);
   #endif
  #ifdef Sen_Agua
    s1["tipoTopic"] = "Topic Agua";
    s1["topicIs"] = topicAgua;
    datosS.add(s1);
    s1["tipoTopic"] = "Topic Alarma Agua";
    s1["topicIs"] = topicAlarAgua;
    datosS.add(s1);
  #endif


  serializeJson(json, Sjson);
  #ifdef DEBUG_MODE
    Serial.print("cmdo infoAll, se Tx: ");
    Serial.println(Sjson);
  #endif
  server.send(200, F("application/json"), Sjson);
}
#ifdef Sen_DHT
void Funcion_Sensor_DHT(void){
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillisMQTT >= intervalSen){
    previousMillisMQTT = currentMillis;
    readSensor();
    if (client_MQTT.connected()){
      #ifdef DEBUG_MODE
        Serial.println("Publica Temp/Hum MQTT: ");
        Serial.print("Temp: "); Serial.println(temp.c_str());
        Serial.print("Hum: "); Serial.println(hume.c_str());
      #endif
      client_MQTT.publish(topicTem.c_str(), temp.c_str());
      client_MQTT.publish(topicHum.c_str(), hume.c_str());
    }else{
      #ifdef DEBUG_MODE
        Serial.println("MQTT NO Conectado");
      #endif
    }
  }
    
}
#endif
#ifdef Sen_Agua
void Funcion_Sensor_Agua(void){
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillisAgua >= intervalSen){
    previousMillisAgua = currentMillis;
    int senAgua = readSensorAgua();
    static char stringAgua[5];
    dtostrf(senAgua, 4, 0, stringAgua);
    String aguaString;
    aguaString = stringAgua;
    if (client_MQTT.connected()){
      #ifdef DEBUG_MODE
        Serial.println("Publica Sensor agua MQTT: ");
        Serial.print("Sensor Agua: "); Serial.println(aguaString.c_str());
        //Serial.print("Hum: "); Serial.println(hume.c_str());
      #endif
      client_MQTT.publish(topicAgua.c_str(), aguaString.c_str());
      if(senAgua < umbralAlarmaAgua){
        client_MQTT.publish(topicAlarAgua.c_str(),"Alarma de Agua");
      }else{
        client_MQTT.publish(topicAlarAgua.c_str(),"Sin Alarma de Agua");
      }
    }else{
      #ifdef DEBUG_MODE
        Serial.println("MQTT NO Conectado");
      #endif
    }
  }
}
int readSensorAgua(void){
  int sensorValue = analogRead(analogInPin);
  return sensorValue;
}
#endif
void Funcion_ReconectMQTT(void){
  unsigned long currentMillis = millis();
  //previousMillisReconect = currentMillis;
  if(currentMillis - previousMillisReconect > intervalRec){
    previousMillisReconect = currentMillis;
    #ifdef DEBUG_MODE
      Serial.println("Ver si Reconecta MQTT");
    #endif
    reconnect();
  }
}
