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
#include <ESPAsyncWebServer.h> //needed by configItems.hpp
#include <LittleFS.h>

#include "configItems.hpp"



JsonDocument jsonConfig;

//
// loadConfigFile
// Attempt to read the provided filename and load the JSON data into jsonConfig
//
bool loadConfigFile(String configFileLoc)
{
  Serial.println(F("Loading configuration"));
  if (configFileLoc.length() == 0) {
    Serial.println("devConfigData.loadConfigData: No config file set");
    return false; 
  }
  File configFile = LittleFS.open(configFileLoc, "r");
  if (!configFile) {
    Serial.println(F("loadConfigData: failed to read file"));
    return false;
  }
  size_t size = configFile.size();
  //is this part even necessary?
  if (size > 4096) { //using 4K min alloc size for littleFS. Actual size should be smaller
    Serial.println(F("Data file size is too large"));
    return false;
  }
  auto error = deserializeJson(jsonConfig, configFile);
  //need to see if I need to copy the data fromt eh file out before closing. If so, I'll need to break out the steps more.
  configFile.close();
  if (error) {
    Serial.println(F("Failed to parse config file"));
    Serial.println(error.f_str());
    return false;
  }
  return true;
}

//
// saveConfigFile
// Attempt to save jsonConfig to the provided filename to flash.
//
bool saveConfigFile(String configFileLoc) {
  Serial.println(F("saveConfigFile"));
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.remove(configFileLoc)) {
    Serial.println(F("File deleted"));
  }

  // Open file for writing
  File file = LittleFS.open(configFileLoc, "w");
  if (!file) {
    Serial.println(F("Failed to open file for writing"));
    return false;
  }

  // Serialize JSON to file
  if (serializeJson(jsonConfig, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  Serial.println(F("Config saved"));
  // Close the file
  file.close();
  return true;
}

//
// saveConfigFile
// Attempt to erase the provided configuration file. Used to reset the device.
//
bool eraseConfig (String configFileLoc) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.print(F("eraseConfig: Deleting config"));
  //TODO:make sure config gets saved or config file is deleted.
  LittleFS.remove(configFileLoc);
  delay(500);
  return true;
}

