/*
 * Author: Priyen Shah
 * Version: 1.0
 * This code controls the motor based on web request.
 * When the main server senses fully filled water tank, the server changes the status and makes web request to this device using the IP
 * This device serves as client and server and processes the service Switch() and again updates the status via the central server
*/

#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<ArduinoJson.h>
#include "acQuisor_WiFi_motor.h"
#include"FS.h"

int motorStatus=0;
float btryLvl=50.0;
float motorPower=2.0;

char* host = "172.22.25.3";
const int httpPort = 80;

String serverResponse, Cdate="2019-03-23";

ESP8266WebServer server(80);
String customerName="",motorName="";

String wifiSsid = "ap_comp_engg", wifiPass = "computer12345", apSsid = "water_acQuisor", apPass = "acquisor123";
acQuisorWiFiMotor acqWifi(wifiSsid, wifiPass, apSsid, apPass);

int relayOutput = D4;
int controlSwitch=D3;  

void Switch();
void switchPressed();
void devStat();
void virtualSwitch();
void handleEdit();
void handleSuccess();
void handleRoot();
bool loadConfig();
bool saveConfig();

void wait()
{
  digitalWrite(D4,1);
  delay(200);
  digitalWrite(D4,0);
  delay(200);
}
void setup() 
{
  Serial.begin(9600);
  Serial.println("Mounting FS...");

  if (!SPIFFS.begin()) 
  {
    Serial.println("Failed to mount file system");
    return;
  }
  pinMode(relayOutput, OUTPUT);  //motor output
  
  digitalWrite(relayOutput,1);
   
  Serial.print("Configuring access point...");
  
  WiFi.softAP(acqWifi.device_SSID.c_str(), acqWifi.device_PASS.c_str());
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  //attachInterrupt(digitalPinToInterrupt(controlSwitch), switchPressed, RISING);
  
  server.on("/switch",virtualSwitch);
  server.on("/", handleRoot);
  server.on("/edit", handleEdit); //Associate the handler function to the path
  server.on("/success", handleSuccess);
  server.on("/deviceStatus",devStat);
  server.begin();

  Serial.println("Server listening");
  
  if(!(loadConfig()))
    Serial.println("\n Flash memory didnt load");
  else
    Serial.println("\n Flash data imported successfully");
  
  Serial.println("\n Recovered data: ");
  Serial.println(acqWifi.customerWifiSsid);
  Serial.println(acqWifi.customerWifiPass);
  Serial.println(customerName);
  Serial.println(motorName);
  if(acqWifi.customerWifiSsid!="default"&&acqWifi.customerWifiSsid!="")
    acqWifi.Wconnect();
    
}

void loop() 
{
  server.handleClient();
  while(acqWifi.customerWifiSsid=="default"||acqWifi.customerWifiSsid=="")
  {
    wait();
    server.handleClient();
  }
  
  if(digitalRead(D3)==1)
  {
    switchPressed();
  }
  
  if(motorStatus)
  {
    digitalWrite(relayOutput, 0);
  }
  else
  {
    digitalWrite(relayOutput, 1);
  }

  if (WiFi.status() == WL_CONNECTED)
  {   
    Serial.print("\nThis device's IP: ");
    Serial.println(acqWifi.deviceIP);
    Serial.print("\nConnecting to host @ ");
    Serial.print(host);
    Serial.println("\n");

    WiFiClient client;
    if (!client.connect(host, httpPort))
    {
      Serial.print("\nHost connection failed");
      digitalWrite(D4,1); //No connection indicator #D4 is active low pin#
    }
    else
    {
      acqWifi.generateURL(customerName, motorName, Cdate, btryLvl, motorStatus);
      Serial.println(acqWifi.url);
      client.print(String("GET ") + acqWifi.url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  
      unsigned long timeout = millis();
      while (client.available() == 0) 
      {
        if (millis() - timeout > 5000) 
        {
          Serial.println(">>> Client Timeout: ");
          client.stop();
          return;
        }
      }
     // cycleCount++;
      serverResponse="x";
      while (client.available()) {
        serverResponse = client.readStringUntil('\r');
      }
      
       int index = serverResponse.indexOf('_');
       String cmd = serverResponse.substring(1,index);
       Cdate = serverResponse.substring(index+1);
       Serial.print("\nMotor data update status: ");
       Serial.println(cmd);
       Serial.print("\nToday is: ");
       Serial.println(Cdate);

    }
   //----------------------------------------------------------//
    //Serial.print("\nNumber of successful cycles: ");
    //Serial.println(cycleCount);
    Serial.println("\n\nEnd of Cycle.");
  }

  else
    Serial.print(".");
  delay(200);
}
void switchPressed()
{
  motorStatus != motorStatus;
}

void virtualSwitch()
{
  String htmlCode;
  String req = String(server.arg("stat"));
  if(req=="on")
  {
    motorStatus=1;
    htmlCode = "on";
  }
  else if(req=="off")
  {
    motorStatus=0;
    htmlCode = "off";
  }
  server.send(200, "text/html", htmlCode); 
}

void handleEdit()
{
  String htmlCode = "<center><h1>Setup your Motor controller here</h1><br><br><form method='get' action='success'><table><tr><td>Default customer name:</td><td><input type='text' name='customerName' placeholder='username_in_website' value='priyen'/></td></tr><tr><td>Motor Name:</td><td><input type='text' name='motorName' placeholder='Ex:department_name'/></td></tr><tr><td> WIFI_SSID: </td><td><input type='text' name='customerWifiSsid' placeholder='Name of hotspot to connect' value='ap_comp_engg'/></td></tr><tr><td>Password: </td><td><input type='password' name='customerWifiPass' placeholder='Password of hotspot' value='computer12345'/></td></tr></table><br><input type='submit' value='update details'/></form></center>";
  server.send(200, "text/html", htmlCode);
}

void devStat()
{
  String req = String(server.arg("stat"));
  if(req=="on")
    motorStatus=1;
   else if(req=="off")
    motorStatus=0;
    
  String htmlCode = "<center><h2>Device / Sensor Status and Parameters</h2><table><tr><th>IP address: </th><td>";  
  htmlCode += acqWifi.deviceIP;
  htmlCode += "</td></tr><tr><th>Valve Status: </th><td>";
  if(digitalRead(relayOutput)==0)
    htmlCode += "ON" ;
  else
    htmlCode += "OFF";

  htmlCode += "</td></tr><tr><th>Customer name: </th><td>";
  htmlCode += customerName;
  htmlCode += "</td></tr><tr><th>Motor Name: </th><td>";
  htmlCode += motorName;
  htmlCode += "</td></tr><tr><th>Connected to: </th><td>";
  htmlCode += acqWifi.customerWifiSsid;
  htmlCode += "</td></tr><tr><th>Battery Level: </th><td>";
  htmlCode += btryLvl;
  htmlCode += "</td></tr></table><br><br><form method='get' action='#'><input type='radio' name='stat' value='on'> Turn ON motor <br><br><input type='radio' name='stat' value='off' checked> Turn OFF motor <br><br><input type='submit' name='status' value='Take Action'></button></form><br><br><b>Contact us at: 1505051@ritindia.edu</b></center>";
  server.send(200, "text/html", htmlCode);
}

void handleSuccess()
{
  
  motorName = String(server.arg("motorName"));
  acqWifi.customerWifiSsid = String(server.arg("customerWifiSsid"));
  acqWifi.customerWifiPass = String(server.arg("customerWifiPass"));
  customerName = String(server.arg("customerName"));
  
  if(!(saveConfig()))
    Serial.println("\n Data not saved to Flash");
  else
    Serial.println("\n Data saved on Flash");

  acqWifi.Wconnect();

  String message = "<br><br><center><h1>The Tank details have been successfully updated.</h1><br><br> The Water acQuisor Sensor is connected to: ";
  message += acqWifi.customerWifiSsid;
  message += "<br><br>The IP address of tank is: ";
  message += acqWifi.deviceIP;
  message += "<br><br>The tank is named as: ";
  message += motorName;
  message += "<br><br><li><a href='http://172.22.25.3/water/'>Visit website for management and control</a></li><br><li><b>Contact us at: 1505051@ritindia.edu</b></li>";
  message += "</center>";
  delay(500);
  server.send(200, "text/html", message);
}
void handleRoot()
{
  String message2 = "<center><h1> Welcome to Water acQuisor </h1><br><br><li><a href = '/edit'> Setup the Water acQuisor </a></li><br><li><a href = '/deviceStatus'> View device status and parameters</a></li><br><br> Thank you for buying Water acQuisor.<br><br><b>Contact us at 1505051@ritindia.edu </b></center>";

  server.send(200, "text/html", message2);
}
bool loadConfig()
{
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }
  
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  String customerWifiSsidr = json["Wssid"];
  String customerWifiPassr = json["Wpass"];
  String customerNamer = json["cname"];
  String motorNamer = json["mname"];

  acqWifi.customerWifiSsid = customerWifiSsidr;
  acqWifi.customerWifiPass = customerWifiPassr;
  customerName = customerNamer;
  motorName = motorNamer;
  
  return true;
}
  
bool saveConfig()
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["Wssid"] = acqWifi.customerWifiSsid;
  json["Wpass"] = acqWifi.customerWifiPass;
  json["cname"] = customerName;
  json["mname"] = motorName;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}
