#include "NetworkCatalog.h"

namespace
{
    constexpr const char* WIFI_NETWORKS[] =
    {
        "Hima",
        "Himaa",
        "Dialog 4G 101",
        "Office_WiFi",
        "Factory_WiFi",
        "Lab_WiFi"
    };

    constexpr size_t NETWORK_COUNT =
        sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

    constexpr const char* DEFAULT_SSID = "Himaa";
    constexpr const char* DEFAULT_PASSWORD = "Hima_2002";
}

const char* NetworkCatalog::defaultSSID()
{
    return DEFAULT_SSID;
}

const char* NetworkCatalog::defaultPassword()
{
    return DEFAULT_PASSWORD;
}

bool NetworkCatalog::isAllowedSSID(const String& ssid)
{
    for (size_t i = 0; i < NETWORK_COUNT; i++)
    {
        if (ssid == WIFI_NETWORKS[i])
            return true;
    }
    return false;
}