#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_manager.h"

// Forward declaration for display refresh callback
typedef void (*RefreshDisplayCallback)();
typedef float (*ReadBatteryVoltageCallback)();
typedef int (*ReadBatteryPercentCallback)();
typedef void (*PlayToneCallback)(uint16_t freq, uint16_t duration);

class WebServerManager {
public:
    WebServerManager();
    void begin(RefreshDisplayCallback refreshCb, 
               ReadBatteryVoltageCallback vbatCb, 
               ReadBatteryPercentCallback battPctCb,
               PlayToneCallback playToneCb);
    void handleClient();
    void broadcastLog(const String& message);

private:
    WebServer server;
    WebSocketsServer webSocket;
    File uploadFile;

    RefreshDisplayCallback onRefreshDisplay;
    ReadBatteryVoltageCallback onReadVbat;
    ReadBatteryPercentCallback onReadBattPct;
    PlayToneCallback onPlayTone;

    void setupRoutes();
    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
    void executeReplCommand(uint8_t clientNum, const String& command);
    
    // HTTP Handlers
    void handleRoot();
    void handleGetConfig();
    void handlePostConfig();
    void handleListFiles();
    void handleFileUpload();
    void handleDeleteFile();
    void handleNotFound();
};

extern WebServerManager webServerManager;

#endif // WEB_SERVER_MANAGER_H
