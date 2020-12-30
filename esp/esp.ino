#include <WebSocketsServer.h>   // https://github.com/Links2004/arduinoWebSockets

WebSocketsServer webSocket = WebSocketsServer(42069);

#include <vector>
std::vector<int> sockets;

#include <LuaWrapper.h> // download https://github.com/Sasszem/ESP8266-Arduino-Lua and add as library (Read the readme there)

#include <WiFi.h>
#include <HTTPClient.h>
// rename wifi_password_example.h to wifi_password.h and fill in the stuff
#include "wifi_password.h"

int ledPin = 2; // Arduino standard is GPIO13 but lolin nodeMCU is 2 http://www.esp8266.com/viewtopic.php?f=26&t=13410#p61332

#include <FastLED.h>
#define NUM_STRIPS 4
#define NUM_LEDS_PER_STRIP 44

CRGB leds[NUM_STRIPS][NUM_LEDS_PER_STRIP];

LuaWrapper lua;

String script = "";

#include "pastebin_api_keys.h"

#include <map>
std::map<String, String> scriptNameToID;

void setupWifi()
{
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.setHostname("led-lampies-esp32");
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED)
    {
        digitalWrite(ledPin, LOW);
        delay(100);
        digitalWrite(ledPin, HIGH);
        Serial.print(".");
        delay(100);
    }
    Serial.println("");
    Serial.println("WiFi connected");
}

void log(const char *str)
{
    Serial.println(str);

    for (int socket : sockets)
        webSocket.sendTXT(socket, str);
}

void lua_setColor(lua_State *lua_state)
{
    unsigned int stripIndex = luaL_checkinteger(lua_state, 1);
    unsigned int ledIndex = luaL_checkinteger(lua_state, 2);
    unsigned char r = luaL_checkinteger(lua_state, 3);
    unsigned char g = luaL_checkinteger(lua_state, 4);
    unsigned char b = luaL_checkinteger(lua_state, 5);

    if (stripIndex >= NUM_STRIPS || ledIndex >= NUM_LEDS_PER_STRIP)
        return;

    leds[stripIndex][ledIndex] = CRGB(r, g, b);
}

void lua_setColorMultiple(lua_State *lua_state)
{
    unsigned int stripIndex = luaL_checkinteger(lua_state, 1);
    unsigned int ledIndex = luaL_checkinteger(lua_state, 2);
    unsigned int nrOfLeds = luaL_checkinteger(lua_state, 3);
    unsigned char r = luaL_checkinteger(lua_state, 4);
    unsigned char g = luaL_checkinteger(lua_state, 5);
    unsigned char b = luaL_checkinteger(lua_state, 6);

    for (int i = ledIndex; i < ledIndex + nrOfLeds; i++)
    {
        if (stripIndex >= NUM_STRIPS || i >= NUM_LEDS_PER_STRIP)
            return;

        leds[stripIndex][i] = CRGB(r, g, b);
    }
}

lua_State *_lua_state = NULL;

void lua_show(lua_State *lua_state)
{
    _lua_state  = lua_state;
    FastLED.show();
}

void setupLua()
{
    lua.Lua_register("set_color", (const lua_CFunction) &lua_setColor);
    lua.Lua_register("set_color_multiple", (const lua_CFunction) &lua_setColorMultiple);
    lua.Lua_register("show", (const lua_CFunction) &lua_show);
    

    String hack = "show()";
    lua.Lua_dostring(&hack); // hacky way to obtain _lua_state. Maybe I should use another Lua library because this wrapper sucks.
}

String downloadScript(String &id)
{
    HTTPClient http;
    http.begin("https://pastebin.com/raw/" + id);
    int responseCode = http.GET();
    String result = "";
    if (responseCode != 200)
        log(("downloadScript(): Got HTTP code " + String(responseCode)).c_str());
    else
        result = http.getString();
    http.end();
    return result;
}

void removeScript(String &id)
{
    HTTPClient http;
    http.begin("https://pastebin.com/api/api_post.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String formData =
        String("api_dev_key=") + pastebinDevKey +
        "&api_user_key=" + pastebinUserKey +
        "&api_paste_key=" + id +
        "&api_option=delete";

    int responseCode = http.POST(formData);
    if (responseCode != 200)
        log(("removeScript(): responseCode is " + String(responseCode)).c_str());
    http.end();
}

String uploadScript(String &name, String &script)
{
    HTTPClient http;
    http.begin("https://pastebin.com/api/api_post.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String formData =
        String("api_dev_key=") + pastebinDevKey +
        "&api_user_key=" + pastebinUserKey +
        "&api_paste_name=" + name +
        "&api_paste_code=" + script +
        "&api_option=paste&api_paste_format=lua&api_paste_expire_date=N&api_paste_private=0";

    String id = "didnt-upload";

    int responseCode = http.POST(formData);
    if (responseCode != 200)
        log(("removeScript(): responseCode is " + String(responseCode)).c_str());
    else
    {
        id = http.getString();
        id = id.substring(id.lastIndexOf('/') + 1);
    }
    http.end();
    return id;
}

void webSocketEvent(uint8_t socket, WStype_t type, uint8_t *payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            for (int i = 0; i < sockets.size(); i++) if (sockets[i] == socket)
            {
                sockets[i] = sockets.back();
                sockets.pop_back();
                break;
            }
            Serial.printf("[%u] Disconnected!\n", socket);
            break;
        case WStype_CONNECTED: 
        {
            sockets.push_back(socket);
            IPAddress ip = webSocket.remoteIP(socket);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", socket, ip[0], ip[1], ip[2], ip[3], payload);
            // send message to client
            webSocket.sendTXT(socket, "Welkom bij de API van de lampies in me kast.\nBeetje lief zijn want er zit amper beveiliging op.");
            break;
        }
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", socket, payload);

            String txt = (const char *) payload;

            if (txt == "memory")
                log(String("Free heap: " + String(ESP.getFreeHeap())).c_str());
            else if (txt == "scripts")
                for (auto &s : scriptNameToID)
                    log(("script:" + s.first + ",id:" + s.second).c_str());
            else if (txt.startsWith("setscript "))
            {
                String name = txt.substring(10);
                if (scriptNameToID.count(name))
                {
                    log(("Switching to script " + name).c_str());
                    script = downloadScript(scriptNameToID[name]);
                }
                else
                    log(("Error: no script named '" + name + "' found!").c_str());
            }
            else if (txt.startsWith("editscript "))
            {
                String name = txt.substring(11, txt.indexOf('\n'));
                script = txt.substring(txt.indexOf('\n') + 1);
                if (scriptNameToID.count(name))
                {
                    log(("Removing old version of " + name).c_str());
                    removeScript(scriptNameToID[name]);
                }
                log(("Uploading  " + name).c_str());
                scriptNameToID[name] = uploadScript(name, script);
                log(("script:" + name + ",id:" + scriptNameToID[name]).c_str());
            }
            break;
    }

}

void getScriptNamesAndIds()
{
    HTTPClient http;
    http.begin("https://pastebin.com/api/api_post.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String formData =
        String("api_dev_key=") + pastebinDevKey +
        "&api_user_key=" + pastebinUserKey +
        "&api_option=list&api_results_limit=64";

    int responseCode = http.POST(formData);
    if (responseCode != 200)
    {
        log(("getScriptNamesAndIds(): Got HTTP code " + String(responseCode)).c_str());
    }
    else
    {
        String response = http.getString();
        String id = "", name = "";
        while (true)
        {
            int newlineIndex = response.indexOf('\n');
            if (newlineIndex == -1)
                break;
            String line = response.substring(0, newlineIndex);
            line.trim();
            response = response.substring(newlineIndex + 1);
            if (line.startsWith("<paste_key>"))
                id = line.substring(11, line.lastIndexOf("</"));
            if (line.startsWith("<paste_title>"))
                name = line.substring(13, line.lastIndexOf("</"));
            if (line == "</paste>")
                scriptNameToID[name] = id;
        }
        scriptNameToID[name] = id; // dunno why this is needed.
    }
    http.end();
}

void setup()
{
                                                                // label next to pin:
    FastLED.addLeds<NEOPIXEL, 16>(leds[0], NUM_LEDS_PER_STRIP); // RX2
    FastLED.addLeds<NEOPIXEL, 17>(leds[1], NUM_LEDS_PER_STRIP); // TX2
    FastLED.addLeds<NEOPIXEL, 18>(leds[2], NUM_LEDS_PER_STRIP); // D18 (unreadable)
    FastLED.addLeds<NEOPIXEL, 19>(leds[3], NUM_LEDS_PER_STRIP); // D19

    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++)
    {
        // todo remove this hardcoded bullshit:
        leds[0][i] = (i / 11) % 2 == 0 ? CRGB::Green : CRGB::Red;
        leds[1][i] = (i / 11) % 2 == 0 ? CRGB::Red : CRGB::Blue;
        leds[2][i] = (i / 11) % 2 == 0 ? CRGB::Blue : CRGB::Green;
        leds[3][i] = (i / 11) % 2 == 0 ? CRGB::Green : CRGB::Red;
    }

    FastLED.show();
    
    Serial.begin(115200);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    setupWifi();
    setupLua();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    getScriptNamesAndIds();
}

void loop()
{
    webSocket.loop();

    String result = lua.Lua_dostring(&script);
    if (result.length() > 0)
    {
        log(result.c_str());
        delay(100);
    }
}
