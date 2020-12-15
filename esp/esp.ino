#include <LuaWrapper.h> // download https://github.com/fdu/ESP8266-Arduino-Lua and add as library (Read the readme there)

//Original source code : http://enrique.latorres.org/2017/10/17/testing-lolin-nodemcu-v3-esp8266/
//Download LoLin NodeMCU V3 ESP8266 Board for Arduino IDE (json) : http://arduino.esp8266.com/stable/package_esp8266com_index.json
#include <ESP8266WiFi.h>

// rename wifi_password_example.h to wifi_password.h and fill in the stuff
#include "wifi_password.h"

int ledPin = 2; // Arduino standard is GPIO13 but lolin nodeMCU is 2 http://www.esp8266.com/viewtopic.php?f=26&t=13410#p61332

#include <FastLED.h>
#define NUM_STRIPS 4
#define NUM_LEDS_PER_STRIP 44

CRGB leds[NUM_STRIPS][NUM_LEDS_PER_STRIP];

LuaWrapper lua;

void setupWifi()
{
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.hostname("led-lampies");
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
}

void lua_setColor(lua_State *lua_state)
{
    unsigned int stripIndex = luaL_checkinteger(lua_state, 1);
    unsigned int ledIndex = luaL_checkinteger(lua_state, 2);
    unsigned char r = luaL_checkinteger(lua_state, 3);
    unsigned char g = luaL_checkinteger(lua_state, 4);
    unsigned char b = luaL_checkinteger(lua_state, 5);

    if (stripIndex >= NUM_STRIPS)
    {
        log("ERROR IN lua_setColor: stripIndex out of range");
        return;
    }
    if (ledIndex >= NUM_LEDS_PER_STRIP)
    {
        log("ERROR IN lua_setColor: ledIndex out of range");
        return;
    }
    leds[stripIndex][ledIndex] = CRGB(r, g, b);
}

void lua_show(lua_State *lua_state)
{
    FastLED.show();
}

void setupLua()
{
    lua.Lua_register("set_color", (const lua_CFunction) &lua_setColor);
    lua.Lua_register("show", (const lua_CFunction) &lua_show);
    
}

void setup()
{
    FastLED.addLeds<NEOPIXEL, 5>(leds[0], NUM_LEDS_PER_STRIP);
    FastLED.addLeds<NEOPIXEL, 4>(leds[1], NUM_LEDS_PER_STRIP);
    FastLED.addLeds<NEOPIXEL, 0>(leds[2], NUM_LEDS_PER_STRIP);
    FastLED.addLeds<NEOPIXEL, 2>(leds[3], NUM_LEDS_PER_STRIP);

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

    String script = String("print(\"haha\")");
    Serial.println(lua.Lua_dostring(&script));
}

void loop()
{
    String script = "";
    char c = 0;
    Serial.println();
    while (true) {
        if (!Serial.available())
            continue;
        
        c = Serial.read();
        Serial.write(c);
        script += c;
        if (c == '\n') break;
    }
    if(script.length() > 0) {
        Serial.println();
        log(lua.Lua_dostring(&script));
    }
}
