#ifndef WiFiMind_h
#define WiFiMind_h

#if defined(ESP8266) || defined(ESP32)

#if defined(ESP8266)
#include <core_version.h>
extern "C"
{
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define WIFI_getChipId() ESP.getChipId()
#define WM_WIFIOPEN ENC_TYPE_NONE

#elif defined(ESP32)

#include <WiFi.h>
#include <esp_wifi.h>
#include <Update.h>

#define WIFI_getChipId() (uint32_t)ESP.getEfuseMac()
#define WM_WIFIOPEN WIFI_AUTH_OPEN

#include <WebServer.h>

#ifdef WM_ERASE_NVS
#include <nvs.h>
#include <nvs_flash.h>
#endif // WM_ERASE_NVS

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif // WM_MDNS

#ifdef WM_RTC
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif // CONFIG_IDF_TARGET_ESP32
#else  // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif // ESP_IDF_VERSION_MAJOR
#endif // WM_RTC

#endif // ESP8266 or ESP32

#include <DNSServer.h>
#include <memory>
#include "const_string.h"

class WiFiMind
{
public:
    WiFiMind();
    ~WiFiMind();

    // auto connect to saved wifi, or custom, and start config portal on failures
    boolean autoConnect(char const *apName, char const *apPassword = NULL);

    // manually start the config portal, autoconnect does this automatically on connect failure
    boolean startConfigPortal(char const *apName, char const *apPassword = NULL);

    // manually stop the config portal if started manually, stop immediatly if non blocking, flag abort if blocking
    bool stopConfigPortal();

    // manually start the web portal, autoconnect does this automatically on connect failure
    void startWebPortal();

    // manually stop the web portal if started manually
    void stopWebPortal();

    // Run webserver processing, if setConfigPortalBlocking(false)
    boolean process();
    int getRSSIasQuality(int RSSI);

    // erase wifi credentials
    void resetSettings();

    // sets timeout before AP,webserver loop ends and exits even if there has been no setup.
    // useful for devices that failed to connect at some point and got stuck in a webserver loop
    // in seconds setConfigPortalTimeout is a new name for setTimeout, ! not used if setConfigPortalBlocking
    void setConfigPortalTimeout(unsigned long seconds);
    void setTimeout(unsigned long seconds); // @deprecated, alias

    // sets timeout for which to attempt connecting, useful if you get a lot of failed connects
    void setConnectTimeout(unsigned long seconds);

    // sets number of retries for autoconnect, force retry after wait failure exit
    void setConnectRetries(uint8_t numRetries); // default 1

    // sets timeout for which to attempt connecting on saves, useful if there are bugs in esp waitforconnectloop
    void setSaveConnectTimeout(unsigned long seconds);

    // lets you disable automatically connecting after save from webportal
    void setSaveConnect(bool connect = true);

    // set min quality percentage to include in scan, defaults to 8% if not specified
    void setMinimumSignalQuality(int quality = 8);

    // sets a custom ip /gateway /subnet configuration
    void setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);

    // sets config for a static IP
    void setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);

    // sets config for a static IP with DNS
    void setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns);

    // setter for ESP wifi.persistent so we can remember it and restore user preference, as WIFi._persistent is protected
    void setRestorePersistent(boolean persistent);

    // if false, disable captive portal redirection
    void setCaptivePortalEnable(boolean enabled);

    // if false, timeout captive portal even if a STA client connected to softAP (false), suggest disabling if captiveportal is open
    void setAPClientCheck(boolean enabled);

    // if true, reset timeout when webclient connects (true), suggest disabling if captiveportal is open
    void setWebPortalClientCheck(boolean enabled);

    // if true, enable autoreconnecting
    void setWiFiAutoReconnect(boolean enabled);

    // set ap channel
    void setWiFiAPChannel(int32_t channel);

    // set a custom hostname, sets sta and ap dhcp client id for esp32, and sta for esp8266
    bool setHostname(const char *hostname);
    bool setHostname(String hostname);

    // set ap hidden
    void setWiFiAPHidden(bool hidden); // default false

    // clean connect, always disconnect before connecting
    void setCleanConnect(bool enable); // default false

    // set port of webserver, 80
    void setHttpPort(uint16_t port);

    // set the country code for wifi settings, CN
    void setCountry(String cc);

    // set allow exit config portal
    void setAllowExit(bool allow);

    // set basic auth
    void setAllowBasicAuth(bool allow);
    void setBasicAuth(String username, String password);

    // check if config portal is active (true)
    bool getConfigPortalActive();

    // check if web portal is active (true)
    bool getWebPortalActive();

    // get hostname helper
    String getWiFiHostname();

    std::unique_ptr<DNSServer> dnsServer;

#if defined(ESP32)
    using WM_WebServer = WebServer;
#else
    using WM_WebServer = ESP8266WebServer;
#endif

    std::unique_ptr<WM_WebServer> server;

private:
    // ip configs @todo struct ?
    IPAddress _ap_static_ip;
    IPAddress _ap_static_gw;
    IPAddress _ap_static_sn;
    IPAddress _sta_static_ip;
    IPAddress _sta_static_gw;
    IPAddress _sta_static_sn;
    IPAddress _sta_static_dns;

    unsigned long _configPortalStart = 0;     // ms config portal start time (updated for timeouts)
    unsigned long _webPortalAccessed = 0;     // ms last web access time
    uint8_t _lastconxresult = WL_IDLE_STATUS; // store last result when doing connect operations
    int _numNetworks = 0;                     // init index for numnetworks wifiscans
    unsigned long _lastscan = 0;              // ms for timing wifi scans
    unsigned long _startscan = 0;             // ms for timing wifi scans
    unsigned long _startconn = 0;             // ms for timing wifi connects

    // options flags
    unsigned long _configPortalTimeout = 0; // ms close config portal loop if set (depending on  _cp/webClientCheck options)
    unsigned long _connectTimeout = 0;      // ms stop trying to connect to ap if set
    unsigned long _saveTimeout = 0;         // ms stop trying to connect to ap on saves, in case bugs in esp waitforconnectresult

    // defaults
    const byte DNS_PORT = 53;
    String _apName = "no-net";
    String _apPassword = "";
    String _ssid = "";        // var temp ssid
    String _pass = "";        // var temp psk
    String _defaultssid = ""; // preload ssid
    String _defaultpass = ""; // preload pass

    WiFiMode_t _usermode = WIFI_STA; // Default user mode
    // String _wifissidprefix = FPSTR(S_ssidpre); // auto apname prefix prefix+chipid
    int _cpclosedelay = 2000;    // delay before wifisave, prevents captive portal from closing to fast.
    bool _cleanConnect = false;  // disconnect before connect in connectwifi, increases stability on connects
    bool _connectonsave = true;  // connect to wifi when saving creds
    bool _disableSTA = false;    // disable sta when starting ap, always
    bool _disableSTAConn = true; // disable sta when starting ap, if sta is not connected ( stability )
    bool _channelSync = false;   // use same wifi sta channel when starting ap
    int32_t _apChannel = 0;      // default channel to use for ap, 0 for auto
    bool _apHidden = false;      // store softap hidden value
    uint16_t _httpPort = 80;     // port for webserver
    uint8_t _connectRetries = 1; // number of sta connect retries, force reconnect, wait loop (connectimeout) does not always work and first disconnect bails
    bool _allowExit = true;      // allow exit in nonblocking, else user exit/abort calls will be ignored including cptimeout
    bool _auth = false;          // basic auth
    String _authUsername = "admin";
    String _authPassword = "admin";

#ifdef ESP32
    wifi_event_id_t wm_event_id = 0;
    static uint8_t _lastconxresulttmp; // tmp var for esp32 callback
#endif

#ifndef WL_STATION_WRONG_PASSWORD
    uint8_t WL_STATION_WRONG_PASSWORD = 7; // @kludge define a WL status for wrong password
#endif

    // parameter options
    int _minimumQuality = -1;               // filter wifiscan ap by this rssi
    boolean _removeDuplicateAPs = true;     // remove dup aps from wifiscan
    boolean _showPassword = false;          // show or hide saved password on wifi form, might be a security issue!
    boolean _configPortalIsBlocking = true; // configportal enters blocking loop
    boolean _enableCaptivePortal = true;    // enable captive portal redirection
    boolean _userpersistent = true;         // users preffered persistence to restore
    boolean _wifiAutoReconnect = true;      // there is no platform getter for this, we must assume its true and make it so
    boolean _apClientCheck = false;         // keep cp alive if ap have station
    boolean _webClientCheck = true;         // keep cp alive if web have client
    boolean _scanDispOptions = false;       // show percentage in scans not icons
    boolean _enableConfigPortal = true;     // FOR autoconnect - start config portal if autoconnect failed
    boolean _disableConfigPortal = true;    // FOR autoconnect - stop config portal if cp wifi save
    String _hostname = "";                  // hostname for esp8266 for dhcp, and or MDNS

public:
    boolean _preloadwifiscan = false;    // preload wifiscan if true
    unsigned int _scancachetime = 30000; // ms cache time for preload scans
    boolean _asyncScan = false;          // perform wifi network scan async

private:
    boolean _autoforcerescan = false; // automatically force rescan if scan networks is 0, ignoring cache

    boolean _disableIpFields = false; // modify function of setShow_X_Fields(false), forces ip fields off instead of default show if set, eg. _staShowStaticFields=-1

    String _wificountry = ""; // country code, @todo define in strings lang

    // wrapper functions for handling setting and unsetting persistent for now.
    bool esp32persistent = false;
    bool _hasBegun = false; // flag wm loaded,unloaded
    void _begin();
    void _end();

    void setupConfigPortal();
    bool shutdownConfigPortal();
    bool setupHostname(bool restart);

#ifdef NO_EXTRA_4K_HEAP
    boolean _tryWPS = false; // try WPS on save failure, unsupported
    void startWPS();
#endif

    bool startAP();
    void setupDNSD();
    void setupHTTPServer();

    uint8_t connectWifi(String ssid, String pass, bool connect = true);
    bool setSTAConfig();
    bool wifiConnectDefault();
    bool wifiConnectNew(String ssid, String pass, bool connect = true);

    uint8_t waitForConnectResult();
    uint8_t waitForConnectResult(uint32_t timeout);
    void updateConxResult(uint8_t status);

    // webserver handlers
public:
    void handleNotFound();
    void handleRequest();

private:
    // void handleRoot();
    void handleWifi();
    void handleWifiSave();
    void handleInfo();

    void handleRestart();
    void handleExit();
    void handleErase();

    boolean configPortalHasTimeout();
    uint8_t processConfigPortal();
    void stopCaptivePortal();

    // OTA Update handler
    void handleUpdate();
    void handleUpdateDone();

    // wifi platform abstractions
    // bool WiFi_Mode(WiFiMode_t m);
    bool WiFi_Mode(WiFiMode_t m, bool persistent);
    bool WiFi_Disconnect();
    // bool WiFi_enableSTA(bool enable);
    bool WiFi_enableSTA(bool enable, bool persistent);
    bool WiFi_eraseConfig();
    uint8_t WiFi_softap_num_stations();
    bool WiFi_hasAutoConnect();
    void WiFi_autoReconnect();
    String WiFi_SSID(bool persistent = true) const;
    String WiFi_psk(bool persistent = true) const;
    bool WiFi_scanNetworks();
    bool WiFi_scanNetworks(bool force, bool async);
    bool WiFi_scanNetworks(unsigned int cachetime, bool async);
    bool WiFi_scanNetworks(unsigned int cachetime);
    void WiFi_scanComplete(int networksFound);
    bool WiFiSetCountry();

#ifdef ESP32

// check for arduino or system event system, handle esp32 arduino v2 and IDF
#if defined(ESP_ARDUINO_VERSION) && defined(ESP_ARDUINO_VERSION_VAL)

#define WM_ARDUINOVERCHECK ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
#define WM_ARDUINOVERCHECK_204 ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(2, 0, 5)

#ifdef WM_ARDUINOVERCHECK
#define WM_ARDUINOEVENTS
#else
#define WM_NOSOFTAPSSID
#define WM_NOCOUNTRY
#endif

#ifdef WM_ARDUINOVERCHECK_204
#define WM_DISCONWORKAROUND
#endif

#else
#define WM_NOCOUNTRY
#endif // ESP_ARDUINO_VERSION

#ifdef WM_NOCOUNTRY
#warning "ESP32 set country unavailable"
#endif // WM_NOCOUNTRY

#ifdef WM_ARDUINOEVENTS
    void WiFiEvent(WiFiEvent_t event, arduino_event_info_t info);
#else
    void WiFiEvent(WiFiEvent_t event, system_event_info_t info);
#endif // WM_ARDUINOEVENTS
#endif // ESP32

    // output helpers
    String getScanItemOut();
    String encryptionTypeStr(uint8_t authmode);
    boolean validApPassword();
    String toStringIp(IPAddress ip);

    // flags
    boolean connect = false;
    boolean abort = false;
    boolean reset = false;
    boolean configPortalActive = false;

    // these are state flags for portal mode, we are either in webportal mode(STA) or configportal mode(AP)
    // these are mutually exclusive as STA+AP mode is not supported due to channel restrictions and stability
    // if we decide to support this, these checks will need to be replaced with something client aware to check if client origin is ap or web
    // These state checks are critical and used for internal function checks
    boolean webPortalActive = false;
    boolean portalTimeoutResult = false;

    boolean portalAbortResult = false;
    boolean storeSTAmode = true; // option store persistent STA mode in connectwifi
    int timer = 0;               // timer for debug throttle for numclients, and portal timeout messages

    // Notification OTA callbacks
public:
    void setAPCallback(std::function<void(WiFiMind *)> func) { _apcallback = func; }
    void setWebServerCallback(std::function<void()> func) { _webservercallback = func; }
    void onOTAStart(std::function<void()> cbOnStart) { _cbOTAStart = cbOnStart; }
    void onOTAEnd(std::function<void()> cbOnEnd) { _cbOTAEnd = cbOnEnd; }
    void onOTAError(std::function<void(int)> cbOnError) { _cbOTAError = cbOnError; }

private:
    // SET CALLBACKS
    // called after AP mode and config portal has started
    std::function<void(WiFiMind *)> _apcallback;

    // called after webserver has started
    std::function<void()> _webservercallback;

    std::function<void()> _cbOTAStart;
    std::function<void()> _cbOTAEnd;
    std::function<void(int)> _cbOTAError;
};
#endif // ESP
#endif // WiFiMind_h
