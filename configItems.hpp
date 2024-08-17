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
#ifndef CONFIG_ITEMS_H_
#define CONFIG_ITEMS_H_


#define CONFIG_FILE "/config.json"

extern JsonDocument jsonConfig;

struct configItemData {
  const String displayName;
  const String key;
  const boolean protect_pw;
  const int maxLength;
  String value;
};

bool loadConfigFile(String configFileLoc);
bool saveConfigFile(String configFileLoc);
bool eraseConfig(String configFileLoc);


//class configurationItems: encapsulation of the config items.
// To modify or add etries to be stored in teh config file, 
// see the vector in the provate data at the end of the class.
//
// TODO: consider adding wrapper methods for the [] operator and size() method.
//   Right now these aren't really needed but it might be a nice to have in the future.
//
class configurationItems {

public:
//
// LoadValues
// Take values from JSON configuration data and load it to the value field
// of the config item with a matching key.
// Currenty, no error is generated if a key is defined in the config data 
// but is not in the JSON data.
//
// Note for the future is that this should indicate an issue with the config 
// and recovery action should be to erase the file then bring the device back 
// up as unconfigured.
// 
  void LoadValues(JsonDocument &jsonConfig) {
    Serial.println(F("Load config values"));
    //load up the JSON file and prepare to save it.
    for (int i = 0; i < std::size(configItems); i++) {
      Serial.print(F("loading [\""));
      Serial.print(configItems[i].key);
      Serial.print(F("\"]:[\""));
      if (jsonConfig.containsKey(configItems[i].key)) {
        configItems[i].value = static_cast<String>(jsonConfig[configItems[i].key]);
        if (configItems[i].protect_pw) {
          Serial.print(F("<protected PW>"));
        } else {
          Serial.print(configItems[i].value);
        }

      } else {
        Serial.print(F("<not found>"));
      }
      Serial.println(F("\"]"));
    }
    return;
  }

  //
  // buildInputFormItem
  // HTML input is handled via a form with multiple fields.
  // This adds a field for the form section of the HTML as a table row.
  //
  // Parameter: &buffer - A reference to the buffer where the complete
  //  set of HTML table rows should be stored for the caller to use.
  //
  // 1) Check if the field is for a protected item like a password.
  // 2) If not, build a row where the input type is text.
  // 3) If it is, build a row where the input type is a password.
  // 4) Concatinate the generated string to the buffer provided by the caller.
  //
  // TODO: consider changing to void return. It doesn't look like the Arduino string class
  // throws exceptions. One might happen from memory allocation which needs to be investigated.
  // 
  // Note: Sprint might be syntactically cleaner but the need to make a best 
  // guess at a max buffer size makes counters that with design issues.
  // Support std::format in Arduino isn't great so the best compromise 
  // seems to be string concatination via the addition operator.
  //
  //
  bool buildInputFormEntries(String &buffer) {
    for (int i = 0; i < configItems.size(); i++) {
      //<tr><td>{prettyName} <td><input type="text" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
      // This could probably be done more 'cleanly' in terms of syntax with sprint_f but the manual buffer managment
      // takes away from that.
      // So while String concatination is syntactically messier, it seems to be the cleaner design.
      if (!configItems[i].protect_pw) {
        buffer = buffer + "<tr><td>" + configItems[i].displayName + " <td><input type=\"text\" name=\"" + configItems[i].key + 
          "\" maxlength=\"" + String(configItems[i].maxLength) + "\" placeholder=\"%" + configItems[i].key + "%\"><br>\n";
      } else {
        //<tr><td>{prettyName} <td><input type="password" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
        buffer = buffer + "<tr><td>" + configItems[i].displayName + " <td><input type=\"password\" name=\"" + configItems[i].key + 
          "\" maxlength=\"" + String(configItems[i].maxLength) + "\" placeholder=\"%" + configItems[i].key + "%\"><br>\n";
      }
    }
    return true;
  }

  //
  // buildReportEntries
  // Configured items are reported to the user in a table (to improve formatting).
  // This function builds a row for the table where each row is a separate configuration item.
  //
  // Parameter: &buffer - A reference to the buffer where the complete
  //  set of HTML table rows should be stored for the caller to use.
  //
  // TODO: consider changing to void return. It doesn't look like the Arduino string class
  // throws exceptions. One might happen from memory allocation which needs to be investigated.
  //
  bool buildReportEntries(String &buffer) {
    for (int i = 0; i < configItems.size(); i++) {
      //<tr><td>{prettyName} <td>%{key}%<br>
      //sprint needs a pre-defined buffer. The issue is with string inputs, it's hard to figure out a good
      //compromse.
      //So while String concatination is syntactically messier, it seems to be the cleaner design.
      buffer = buffer + "<tr><td>" + configItems[i].displayName + " <td>%" + configItems[i].key + "%<br>\n";
    }
    return true;
  }

//
// saveResponseValues
// Take the response data from the HTML server, extract the values from the response, 
// and store it according the appropriate key.
//
// Parameter: request A pointer to the request data provided by the HTML server.
//    Since the form data  *should* have been build by this class, the response 
//    data should also match the config data.
//
// TODO: Error checking on the response data. A mismatch should be considered to 
//    indicate a possible attack and the data ignored.
//
  void saveResponseValues (AsyncWebServerRequest *request) {
    for (int i = 0; i < configItems.size(); i++) {
        //TODO: Figure out what to do with an item that isn't handled.
        //If that happens, the implication is either a communications issue or an attack on the interface.
      if (request->hasParam(configItems[i].key, true)) {
        Serial.println(configItems[i].key);
        if (request->getParam(configItems[i].key, true)->value().length() < configItems[i].maxLength &&
            request->getParam(configItems[i].key, true)->value().length() > 0 ){ 
          //only update if a value is below the max length and 
          //if data was actually sent. Clearing data is the function of the clear button.
          configItems[i].value = request->getParam(configItems[i].key, true)->value();
          continue;
        }
      }
    }
    configEmpty = false;
  }

  //
  // getItemValue
  // Provide a value string for both the report section and the placeholder in the input section.
  // Protected items must have a dummy string provided rather than the actual value.
  //
  // 1) Check the template variable provided by the webserver against the provided configuration item.
  // 2) If the item is a match, check if the item is protected.
  // 3) If it is not protected, set a reference to the stored value string.
  // 4) If it is protected, check if there is a stored value.
  // 5) If it is not emply, set a reference to a dummy string.
  // 6) Return true if the reference was updated, false if the refernece isn't 'valid'.
  //
  // Parameter: keyVar - the string for the template variabe from the HTML data to be replaced.
  // Parameter: valueString - The refernce to provide the text to use in place of the template variable.
  //
  // Returns True if valueString has been updated with approriate data.
  //         False if the variable was not claimed.
  //
  bool getItemValue(String keyVar, String &valueString) {
    static const String dummyString = "********";
    Serial.print(F("looking for "));
    Serial.println(keyVar);
    for (int i = 0; i < configItems.size(); i++) {
      if (keyVar == configItems[i].key) {
        Serial.print(F("key claimed, value: "));
        //now see if we return the value string or a dummy
        if (!configItems[i].protect_pw) {
          Serial.println(configItems[i].value);
          valueString = configItems[i].value;
          return true;
        } else {
          if (configItems[i].value.length() > 0) {
            Serial.println(F("returning dummy string"));
            valueString = dummyString;
          }
          return true;
        }
      }
    }
    return false;
  }

//
// clearValues
// Trivial method to remove all value data from each of the config items
//
  void clearValues(){
    for (int i = 0; i < configItems.size(); i++) {
      configItems[i].value.remove(0,configItems[i].value.length());
    }
    configEmpty = true;
  }

//
// dumpToJson
// Copy all keys and their values to the provided json document so 
// it can be saved to the filesystem
//
// TODO: consider making this function require a blank json document 
// to make sure no garbage make it into the config file
//
  bool dumpToJson (JsonDocument &jsonConfig) {
    Serial.println(F("valuesToJson"));
    for (int i = 0; i < configItems.size(); i++) {
      jsonConfig[configItems[i].key] = configItems[i].value;
    }
    return true;
  }

  bool isEmpty() {
    return configEmpty;
  }


//
// Configuration data. Placed at the very end of this file in an effort 
// to make it easier to find and update for the project's needs.
private:
  bool configEmpty = true; //prevents saving of an empty config
  
  std::vector<configItemData> configItems = 
  {
    {
      "Device host name",
      "hostname",
      false,
      32
    },
      { "WiFi SSID",
        "ssid",
        false,
        32 },
    {
      "WiFi Password",
        "WiFiPw",
        true,
        64
    },
    {
      "MQTT server IP",
        "MqttIp",
        false,
        64
    },
    {
      "MQTT username",
        "MqttUser",
        false,
        64
    }, 
    {
      "MQTT password",
        "MqttPw",
        true,
        64
    },   
    {
      "MQTT temperature topic",
        "MqttTempTopic",
        false,
        128
    },
    {
      "MQTT Humidity topic",
        "MqttHumTopic",
        false,
        128
    }
  };
};


#endif