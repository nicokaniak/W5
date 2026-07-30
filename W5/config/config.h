#ifndef CONFIG_H
#define CONFIG_H

struct WifiCreds {
  const char *ssid;
  const char *password;
};

// Add your networks here. The code will try them in order.
static const WifiCreds WIFI_NETWORKS[] = {{"TN-CV8441", "6ShrynreacBo"},
                                          {"MashWIFI_6Ghz", "SPV01Udupi"},
                                          {"MASHWIFI", "SPV01Udupi"}};

static const int WIFI_NETWORK_COUNT =
    sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// Copenhagen coordinates (example)
// Find your coordinates: https://open-meteo.com
#define LATITUDE "55.6761"
#define LONGITUDE "12.5683"

#endif
