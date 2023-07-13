#include "html_ui.h"

#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"

//https://techtutorialsx.com/2018/08/24/esp32-web-server-serving-html-from-file-system/
//https://randomnerdtutorials.com/esp32-async-web-server-espasyncwebserver-library/

// Create AsyncWebServer object on port 80
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static const char * (*processWSFunc)(JsonVariant &) = nullptr;
static StaticJsonDocument<2048> responseDoc0;
static StaticJsonDocument<2048> responseDoc1;

class SysModWeb:public Module {

public:
  bool modelUpdated = false;
  static bool clientsChanged;

  SysModWeb() :Module("Web") {};

  //setup wifi an async webserver
  void setup() {
    Module::setup();
    print->print("%s %s\n", __PRETTY_FUNCTION__, name);

    print->print("%s %s %s\n", __PRETTY_FUNCTION__, name, success?"success":"failed");
  }

  void loop() {
    // Module::loop();

    //currently not used as each variable is send individually
    if (modelUpdated) {
      sendDataWs(nullptr, false); //send new data, all clients, no def
      modelUpdated = false;
    }
  }

  void connected() {
      ws.onEvent(wsEvent);
      server.addHandler(&ws);

      server.begin();

      print->print("%s server (re)started\n", name);
  }

  //WebSocket connection to 'ws://192.168.8.152/ws' failed: The operation couldn’t be completed. Protocol error
  //WS error 192.168.8.126 9 (2)
  //WS event data 0.0.0.0 (1) 0 0 0=0? 34 0
  //WS pong 0.0.0.0 9 (1)
  //wsEvent deserializeJson failed with code EmptyInput

  // https://github.com/me-no-dev/ESPAsyncWebServer/blob/master/examples/ESP_AsyncFSBrowser/ESP_AsyncFSBrowser.ino

  static void wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if (!ws.count()) {
      print->print("wsEvent no clients\n");
      return;
    }
    if(type == WS_EVT_CONNECT) {
      printClient("WS client connected", client);
      sendDataWs(client, true); //send definition to client
      clientsChanged = true;
    } else if(type == WS_EVT_DISCONNECT) {
      printClient("WS Client disconnected", client);
      clientsChanged = true;
    } else if(type == WS_EVT_DATA){
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      printClient("WS event data", client);
      print->print("  info %d %d %d=%d? %d %d\n", info->final, info->index, info->len, len, info->opcode, data[0]);
      if (info->final && info->index == 0 && info->len == len){
        // the whole message is in a single frame and we got all of its data (max. 1450 bytes)
        if (info->opcode == WS_TEXT)
        {
          if (len > 0 && len < 10 && data[0] == 'p') {
            // application layer ping/pong heartbeat.
            // client-side socket layer ping packets are unresponded (investigate)
            printClient("WS client pong", client);
            client->text(F("pong"));
            return;
          }

          if (processWSFunc) { //processJson defined
            DeserializationError error = deserializeJson(strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1, data, len); //data to responseDoc
            JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
            print->print("response wsevent core %d %s\n", xPortGetCoreID(), pcTaskGetTaskName(NULL));

            if (error || responseVariant.isNull()) {
              print->print("wsEvent deserializeJson failed with code %s %s\n", error.c_str(), data);
              client->text(F("{\"success\":true}")); // we have to send something back otherwise WS connection closes
            } else {
              const char * error = processWSFunc(responseVariant); //processJson, adds to responsedoc

              if (responseVariant.size()) {
                print->printJson("WS_EVT_DATA send response", responseVariant);

                //uiFun only send to requesting client
                if (responseVariant["uiFun"].isNull())
                  sendDataWs(responseVariant);
                else
                  sendDataWs(client, responseVariant);
              }
              else {
                print->print("WS_EVT_DATA no responseDoc\n");
                client->text(F("{\"success\":true}")); // we have to send something back otherwise WS connection closes
              }
            }
          }
          else {
            print->print("WS_EVT_DATA no processWSFunc\n");
            client->text(F("{\"success\":true}")); // we have to send something back otherwise WS connection closes
          }
         }
      } else {
        //message is comprised of multiple frames or the frame is split into multiple packets
        //if(info->index == 0){
          //if (!wsFrameBuffer && len < 4096) wsFrameBuffer = new uint8_t[4096];
        //}

        //if (wsFrameBuffer && len < 4096 && info->index + info->)
        //{

        //}

        if((info->index + len) == info->len){
          if(info->final){
            if(info->message_opcode == WS_TEXT) {
              client->text(F("{\"error\":9}")); //we do not handle split packets right now
            }
          }
        }
        print->print("WS multipart message.\n");
      }
    } else if (type == WS_EVT_ERROR){
      //error was received from the other end
      printClient("WS error", client);
      Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);

    } else if (type == WS_EVT_PONG){
      //pong message was received (in response to a ping request maybe)
      printClient("WS pong", client);
      Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
    }
  }

  static void printClient(const char * text, AsyncWebSocketClient * client) {
    print->print("%s client: %d %s (%d)\n", text, client?client->id():-1, client?client->remoteIP().toString().c_str():"No", ws.count());
  }

  //send json to client or all clients
  static void sendDataWs(AsyncWebSocketClient * client = nullptr, JsonVariant json = JsonVariant()) {
    ws.cleanupClients();

    if (ws.count()) {
      size_t len = measureJson(json);
      AsyncWebSocketMessageBuffer *wsBuf = ws.makeBuffer(len); // will not allocate correct memory sometimes on ESP8266

      if (wsBuf) {
        wsBuf->lock();
        serializeJson(json, (char *)wsBuf->get(), len);
        if (client) {
          if (!client->queueIsFull() && client->status() == WS_CONNECTED) 
            client->text(wsBuf);
          else 
            print->print("sendDataWs client %s full or not connected\n", client->remoteIP().toString().c_str());
          // DEBUG_PRINTLN(F("to a single client."));
        } else {
          for (auto client:ws.getClients()) {
            if (!client->queueIsFull() && client->status() == WS_CONNECTED) 
              client->text(wsBuf);
            else 
              print->print("sendDataWs client %s full or not connected\n", client->remoteIP().toString().c_str());
          }
          // DEBUG_PRINTLN(F("to multiple clients."));
        }
        wsBuf->unlock();
        ws._cleanBuffers();
      }
      else {
        print->print("sendDataWs WS buffer allocation failed\n");
        ws.closeAll(1013); //code 1013 = temporary overload, try again later
        ws.cleanupClients(0); //disconnect all clients to release memory
        ws._cleanBuffers();
      }

    }
  }

  //specific json data send to all clients
  static void sendDataWs(JsonVariant json) {
    sendDataWs(nullptr, json);
  }

  //complete document send to client or all clients
  static void sendDataWs(AsyncWebSocketClient * client = nullptr, bool inclDef = false) {
    model[0]["incldef"] = inclDef;
    sendDataWs(client, model);
  }

  //add an url to the webserver to listen to
  bool addURL(const char * uri, const char * path, const char * contentType) {
    // File f = files->open(path, "r");
    // if (!f) {
    //   print->print("addURL error opening file %s", path);
    //   return false;
    // } else {
      // print->print("addURL File %s size %d\n", path, f.size());

    server.on(uri, HTTP_GET, [uri, path, contentType](AsyncWebServerRequest *request) {
      print->print("Webserver: client request %s %s %s", uri, path, contentType);

      if (strcmp(path, "/index.htm") == 0) {
        AsyncWebServerResponse *response;
        response = request->beginResponse_P(200, "text/html", PAGE_index, PAGE_index_L);

        response->addHeader(FPSTR("Content-Encoding"),"gzip");
        // setStaticContentCacheHeaders(response);
        request->send(response);
      } 
      // temporary removed!!!
      // else {
      //   request->send(LittleFS, path, contentType);
      // }

      print->print("!\n");
    });
    // }
    // f.close();
    return true;
  }

  //not used at the moment
  bool processURL(const char * uri, void (*func)(AsyncWebServerRequest *)) {
    server.on(uri, HTTP_GET, [uri, func](AsyncWebServerRequest *request) {
      func(request);
    });
    return true;
  }

  //processJsonUrl handles requests send in javascript using fetch and from a browser or curl
  //try this !!!: curl -X POST "http://192.168.121.196/json" -d '{"Pin2":false}' -H "Content-Type: application/json"
  //curl -X POST "http://4.3.2.1/json" -d '{"Pin2":false}' -H "Content-Type: application/json"
  //curl -X POST "http://4.3.2.1/json" -d '{"bri":20}' -H "Content-Type: application/json"
  //curl -X POST "http://192.168.8.152/json" -d '{"fx":2}' -H "Content-Type: application/json"
  //curl -X POST "http://192.168.8.152/json" -d '{"nrOfLeds":2000}' -H "Content-Type: application/json"

  bool setupJsonHandlers(const char * uri, const char * (*processFunc)(JsonVariant &)) {
    processWSFunc = processFunc; //for WebSocket requests

    //URL handler
    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/json", [processFunc](AsyncWebServerRequest *request, JsonVariant &json) {
      JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
      (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).clear();
      
      print->printJson("AsyncCallbackJsonWebHandler", json);
      const char * pErr = processFunc(json); //processJson
      if (responseVariant.size()) {
        char resStr[200]; 
        serializeJson(responseVariant, resStr, 200);
        print->print("processJsonUrl response %s\n", resStr);
        request->send(200, "text/plain", resStr);
      }
      else
        request->send(200, "text/plain", "OKOK");

    });
    server.addHandler(handler);
    return true;
  }

  void addResponse(JsonObject object, const char * key, const char * value) {
    const char * id = object["id"];
    JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
    if (responseVariant[id].isNull()) responseVariant.createNestedObject(id);
    responseVariant[id][key] = value;
  }

  void addResponseV(JsonObject object, const char * key, const char * format, ...) {
    const char * id = object["id"];
    JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
    if (responseVariant[id].isNull()) responseVariant.createNestedObject(id);
    va_list args;
    va_start(args, format);

    // size_t len = vprintf(format, args);
    char value[100];
    vsnprintf(value, sizeof(value), format, args);

    va_end(args);

    responseVariant[id][key] = value;
  }

  void addResponseInt(JsonObject object, const char * key, int value) { //temporary, use overloading
    const char * id = object["id"];
    JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
    if (responseVariant[id].isNull()) responseVariant.createNestedObject(id);
    responseVariant[id][key] = value;
  }
  void addResponseBool(JsonObject object, const char * key, bool value) { //temporary, use overloading
    const char * id = object["id"];
    JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
    if (responseVariant[id].isNull()) responseVariant.createNestedObject(id);
    responseVariant[id][key] = value;
  }
  JsonArray addResponseArray(JsonObject object, const char * key) {
    const char * id = object["id"];
    JsonVariant responseVariant = (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0?responseDoc0:responseDoc1).as<JsonVariant>();
    if (responseVariant[id].isNull()) responseVariant.createNestedObject(id);
    return responseVariant[id].createNestedArray(key);
  }

};

static SysModWeb *web;

bool SysModWeb::clientsChanged = false;
