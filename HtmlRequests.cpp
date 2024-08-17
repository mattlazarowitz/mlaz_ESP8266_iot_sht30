/*
MIT License

Copyright (c) 2024 Matthew Lazarowitz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>


#include "configItems.hpp"
#include "htmlRequests.hpp"

extern AsyncWebServer server;

bool configSaved = false;


String reportFields;
//The complete list of config fields to be used for the %CONFIG_FIELDS% template
String configFields;

configurationItems configItems; //melm rename this once all functional code is encapsulated into this class.

//
//    Webserver HTML template processor/callback
//
  
String processor(const String& var) {
  String retVal;
  Serial.print(F("str processor: "));
  Serial.println(var);


  if (var == "CONFIG_SAVED") {
    if (configSaved) {
      return "configuration saved";
    }
  }

//switch to this once configs are done
  if (var == "CONFIG_FIELDS") {
    return configFields;
  }
  if (var == "REPORT_FIELDS"){
    return reportFields;
  }
  //if (getItemValue(var, &item, retVal)) {
  if (configItems.getItemValue(var, retVal)) {
    return retVal;
  }
  return String();
}

void HandleConfigRequest(AsyncWebServerRequest *request) {
  Serial.println(F("request_handler"));

  // look through the config objects looking for the provided key
    //for (configItemData item : configItems) {
  configItems.saveResponseValues(request);
  request->redirect("/");
}


void HandleSaveRequest(AsyncWebServerRequest *request) {
  Serial.println("do save stuff here");
  configItems.dumpToJson(jsonConfig);
  if (configItems.isEmpty() || jsonConfig.isNull()) {
    eraseConfig(CONFIG_FILE);
  } else {
    configItems.dumpToJson(jsonConfig);
    saveConfigFile(CONFIG_FILE);
  }
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}

void HandleRebootRequest (AsyncWebServerRequest *request) {
  Serial.println(F("rebooting..."));
  request->send(200, "text/plain", "Rebooting...");
  ESP.restart();
}

void HandleClearRequest (AsyncWebServerRequest *request) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.print(F("Deleting config"));
  //TODO check return status
  //devConfig.clearConfig();
  jsonConfig.clear();
  configItems.clearValues();
  //eraseConfig(CONFIG_FILE);
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}


void notFound(AsyncWebServerRequest *request) {

  //Serial.print(request);
  request->send(404, "text/plain", "Not found");
}


//rename this as HTML startup. Instantiate the config objects here and build the HTML that needs to be output
void registerHtmlInterfaces()
{
  //String tempStr1 = new String;
  //String tempStr2 = new String;
  Serial.println(F("registerHtmlInterfaces"));
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    //request->send_P(200, "text/html", index_html, processor);
    request->send(LittleFS, "/index.htm", "text/html", false, processor);
  });
  server.on("/config", HTTP_POST, HandleConfigRequest);
  server.on("/save", HTTP_POST, HandleSaveRequest);
  server.on("/reset", HTTP_POST, HandleClearRequest);
  server.on("/reboot", HTTP_POST, HandleRebootRequest);
  server.onNotFound(notFound);

  //Init the config class
  configItems.LoadValues(jsonConfig);
  //build up our strings for the templates
  //they won't change so only do this once.
  configItems.buildInputFormEntries(configFields);
  configItems.buildReportEntries(reportFields);
  Serial.println(F("Input fields:"));
  Serial.println(configFields);
  Serial.println();
  Serial.println(F("Report fields:"));
  Serial.println(reportFields);

}