#ifndef WiFiMind_h
#define WiFiMind_h

#if defined(ESP8266) || defined(ESP32)

#ifdef ESP8266
#include <core_version.h>
extern "C"
{
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define WIFI_getChipId() ESP.getChipId()
#define WM_WIFIOPEN ENC_TYPE_NONE
#endif

#include <DNSServer.h>
#include <memory>
const char * const AUTH_MODE_NAMES[] PROGMEM
{
    "",
    "",
    "WPA_PSK",      // 2 ENC_TYPE_TKIP
    "",
    "WPA2_PSK",     // 4 ENC_TYPE_CCMP
    "WEP",          // 5 ENC_TYPE_WEP
    "",
    "OPEN",         //7 ENC_TYPE_NONE
    "WPA_WPA2_PSK", // 8 ENC_TYPE_AUTO
};

struct Config {
    char mqtt_host[32];
    char mqtt_user[32];
    char mqtt_id[32];
    char mqtt_pass[32];
    uint16_t mqtt_port, tele_interval, attr_interval;
};

class WiFiMind
{
public:
    WiFiMind(/* args */);
    ~WiFiMind();
    boolean         autoConnect(char const *apName, char const *apPassword = NULL);
    boolean         startConfigPortal(char const *apName, char const *apPassword = NULL);
    
    //manually stop the config portal if started manually, stop immediatly if non blocking, flag abort if blocking
    bool            stopConfigPortal();
    
    //manually start the web portal, autoconnect does this automatically on connect failure    
    void            startWebPortal();

    //manually stop the web portal if started manually
    void            stopWebPortal();
    
    // Run webserver processing, if setConfigPortalBlocking(false)
    boolean         process();

    // sets number of retries for autoconnect, force retry after wait failure exit
    void            setConnectRetries(uint8_t numRetries); // default 1

    //sets timeout before AP,webserver loop ends and exits even if there has been no setup.
    //useful for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout, ! not used if setConfigPortalBlocking
    void            setConfigPortalTimeout(unsigned long seconds);
    void            setTimeout(unsigned long seconds); // @deprecated, alias

    //sets timeout for which to attempt connecting, useful if you get a lot of failed connects
    void            setConnectTimeout(unsigned long seconds);

    //sets timeout for which to attempt connecting on saves, useful if there are bugs in esp waitforconnectloop
    void            setSaveConnectTimeout(unsigned long seconds);

    //if false, timeout captive portal even if a STA client connected to softAP (false), suggest disabling if captiveportal is open
    void            setAPClientCheck(boolean enabled);
    
    //if true, reset timeout when webclient connects (true), suggest disabling if captiveportal is open    
    void            setWebPortalClientCheck(boolean enabled);

    // erase wifi credentials
    void            resetSettings();

    // set allow exit web portal
    void            setWebPortalAllowExit(bool allow);

    // check if web portal is active (true)
    bool            getWebPortalActive();

    Config          getConfig();
    
    boolean         _preloadwifiscan        = false; // preload wifiscan if true
    unsigned int    _scancachetime          = 30000; // ms cache time for preload scans
    boolean         _asyncScan              = false; // perform wifi network scan async

    std::unique_ptr<DNSServer>          dnsServer;
    std::unique_ptr<ESP8266WebServer>   server;
private:
    #ifndef WL_STATION_WRONG_PASSWORD
    uint8_t WL_STATION_WRONG_PASSWORD       = 7; // @kludge define a WL status for wrong password
    #endif
    
    uint8_t         _lastconxresult         = WL_IDLE_STATUS; // store last result when doing connect operations
    unsigned long   _configPortalStart      = 0; // ms config portal start time (updated for timeouts)
    unsigned long   _webPortalAccessed      = 0; // ms last web access time
    unsigned long   _configPortalTimeout    = 0; // ms close config portal loop if set (depending on  _cp/webClientCheck options)
    unsigned long   _connectTimeout         = 0; // ms stop trying to connect to ap if set
    unsigned long   _saveTimeout            = 0; // ms stop trying to connect to ap on saves, in case bugs in esp waitforconnectresult
    int             _numNetworks            = 0; // init index for numnetworks wifiscans
    unsigned long   _lastscan               = 0; // ms for timing wifi scans
    unsigned long   _startscan              = 0; // ms for timing wifi scans
    unsigned long   _startconn              = 0; // ms for timing wifi connects
    int             _minimumQuality         = -1;    // filter wifiscan ap by this rssi
    
    bool            _allowExit              = true; // allow exit in nonblocking, else user exit/abort calls will be ignored including cptimeout
    bool            _connectonsave          = true; // connect to wifi when saving creds
    bool            _disableSTAConn         = true;  // disable sta when starting ap, if sta is not connected ( stability )
    bool            _disableSTA             = false;
    bool            _hasBegun               = false; // flag wm loaded,unloaded
    bool            _cleanConnect           = false; // disconnect before connect in connectwifi, increases stability on connects
    bool            WiFi_Mode(WiFiMode_t m,bool persistent);
    bool            WiFi_Disconnect();
    bool            WiFi_enableSTA(bool enable,bool persistent);
    bool            WiFi_hasAutoConnect();
    bool            WiFi_scanNetworks();
    bool            WiFi_scanNetworks(bool force,bool async);
    bool            WiFi_scanNetworks(unsigned int cachetime,bool async);
    bool            WiFi_scanNetworks(unsigned int cachetime);
    void            WiFi_scanComplete(int networksFound);
    
    uint8_t         _connectRetries         = 1; // number of sta connect retries, force reconnect, wait loop (connectimeout) does not always work and first disconnect bails
    uint16_t        _httpPort               = 80; // port for webserver
    void            setupConfigPortal();
    uint8_t         processConfigPortal();
    bool            shutdownConfigPortal();
    boolean         configPortalHasTimeout();
    uint8_t         connectWifi(String ssid, String pass, bool connect = true);
    uint8_t         waitForConnectResult();
    uint8_t         waitForConnectResult(uint32_t timeout);
    void            updateConxResult(uint8_t status);
    int             getRSSIasQuality(int RSSI);


    // flags
    boolean         connect                 = false;
    boolean         abort                   = false;
    boolean         reset                   = false;
    boolean         webPortalActive         = false;
    boolean         configPortalActive      = false;
    boolean         storeSTAmode            = true; // option store persistent STA mode in connectwifi 

    boolean         _autoforcerescan        = false;  // automatically force rescan if scan networks is 0, ignoring cache
    boolean         _enableConfigPortal     = true;  // FOR autoconnect - start config portal if autoconnect failed
    boolean         _disableConfigPortal    = true;  // FOR autoconnect - stop config portal if cp wifi save
    boolean         _wifiAutoReconnect      = true;
    boolean         _userpersistent         = true;  // users preffered persistence to restore
    boolean         _configPortalIsBlocking = true;  // configportal enters blocking loop 
    boolean         _apClientCheck          = false; // keep cp alive if ap have station
    boolean         _webClientCheck         = true;  // keep cp alive if web have client

    String          _apName                 = "no-net";
    String          _apPassword             = "";
    String          _hostname               = "";
    String          _ssid                   = ""; // var temp ssid
    String          _pass                   = ""; // var temp psk
    String          _defaultssid            = ""; // preload ssid
    String          _defaultpass            = ""; // preload pass

    String          _mqtthost               = "";
    String          _mqttport               = "";
    String          _mqttuser               = "";
    String          _mqttid                 = "";
    String          _mqttpassword           = "";
    String          _teleinterval           = "";
    String          _attrinterval           = "";
    
    void            _begin();
    void            _end();
    void            WiFi_autoReconnect();
    bool            startAP();
    void            setupDNSD();
    void            setupHTTPServer();
    bool            wifiConnectDefault();
    bool            wifiConnectNew(String ssid, String pass,bool connect = true);

    void            handleRequest();
    void            handleRoot();
    void            handleNotFound();
    void            handleWifi();
    void            handleWifiSave();
    void            handleConfigInfo();
    void            handleConfigSave();
    void            handleUpdate();
    void            handleUpdateDone();
    void            handleExit();
    void            handleRestart();
    void            handleErase();
    
    String          encryptionTypeStr(uint8_t authmode);
    String          getScanItemOut();
    String          WiFi_SSID(bool persistent = true) const;
    // String          WiFi_psk(bool persistent = true) const;
    Config          mind_config;
    void            loadConfiguration(const char *filename, Config &config);
    bool            saveConfiguration(const char *filename, Config &config);

public:
    // Notification OTA callbacks
    void onOTAStart(std::function<void()> cbOnStart)                { _cbOTAStart = cbOnStart; }
    void onOTAEnd(std::function<void()> cbOnEnd)                    { _cbOTAEnd = cbOnEnd; }
    void onOTAError(std::function<void(int)> cbOnError)             { _cbOTAError = cbOnError; }
    void onConfigPortal(std::function<void(bool)> cbConfigPortal)   { _cbConfigPortal = cbConfigPortal; }
    void onSavedConfig(std::function<void()> cbSavedConfig)         { _cbSavedConfig = cbSavedConfig; }

private:
    // OTA Callbacks
    std::function<void()>           _cbOTAStart;
    std::function<void()>           _cbOTAEnd;
    std::function<void(int)>        _cbOTAError;
    std::function<void(bool)>       _cbConfigPortal;
    std::function<void()>           _cbSavedConfig;
};

#endif
#endif
