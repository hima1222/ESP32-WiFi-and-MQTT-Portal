#pragma once
#include <Arduino.h>

class NetworkCatalog
{
public:
    static const char* defaultSSID();
    static const char* defaultPassword();

    static bool isAllowedSSID(const String& ssid);
};