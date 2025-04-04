#include "WiFiMind.h"
#if defined(ESP8266) || defined(ESP32)
// #include "const_string.h"
// #include "index.h"

#ifdef ESP32
uint8_t WiFiMind::_lastconxresulttmp = WL_IDLE_STATUS;
#endif

// constructors
WiFiMind::WiFiMind()
{
}

WiFiMind::~WiFiMind()
{
    _end();
// remove event
// WiFi.onEvent(std::bind(&WiFiMind::WiFiEvent,this,_1,_2));
#ifdef ESP32
    WiFi.removeEvent(wm_event_id);
#endif
}

void WiFiMind::_begin()
{
    if (_hasBegun)
        return;
    _hasBegun = true;

#ifndef ESP32
    WiFi.persistent(false); // disable persistent so scannetworks and mode switching do not cause overwrites
#endif
}

void WiFiMind::_end()
{
    _hasBegun = false;
    if (_userpersistent)
        WiFi.persistent(true);
}

boolean WiFiMind::autoConnect(char const *apName, char const *apPassword)
{
    bool wifiIsSaved = true; // workaround until I can check esp32 wifiisinit and has nvs

#ifdef ESP32
    setupHostname(true);
    if (_hostname != "")
    {
        // disable wifi if already on
        if (WiFi.getMode() & WIFI_STA)
        {
            WiFi.mode(WIFI_OFF);
            int timeout = millis() + 1200;
            // async loop for mode change
            while (WiFi.getMode() != WIFI_OFF && millis() < timeout)
            {
                delay(0);
            }
        }
    }
#endif // ESP32

    if (wifiIsSaved)
    {
        _startconn = millis();
        _begin();
        if (!WiFi.enableSTA(true))
        {
            return false;
        }

        // WiFiSetCountry();

#ifdef ESP32
        if (esp32persistent)
            WiFi.persistent(false); // disable persistent for esp32 after esp_wifi_start or else saves wont work
#endif
        _usermode = WIFI_STA; // When using autoconnect , assume the user wants sta mode on permanently.
        WiFi_autoReconnect();
#ifdef ESP8266
        if (_hostname != "")
        {
            setupHostname(true);
        }
#endif
        bool connected = false;
        if (WiFi.status() == WL_CONNECTED)
        {
            connected = true;
        }

        if (connected || connectWifi(_defaultssid, _defaultpass) == WL_CONNECTED)
        {
            // connected
            _lastconxresult = WL_CONNECTED;
            return true; // connected success
        }
    }
    else
    {
    }

    // possibly skip the config portal
    if (!_enableConfigPortal)
    {
        return false; // not connected and not cp
    }

    // not connected start configportal
    bool res = startConfigPortal(apName, apPassword);
    return res;
}

boolean WiFiMind::configPortalHasTimeout()
{
    if (!configPortalActive)
        return false;
    uint16_t logintvl = 30000; // how often to emit timeing out counter logging

    // handle timeout portal client check
    if (_configPortalTimeout == 0 || (_apClientCheck && (WiFi_softap_num_stations() > 0)))
    {
        // debug num clients every 30s
        if (millis() - timer > logintvl)
        {
            timer = millis();
        }
        _configPortalStart = millis(); // kludge, bump configportal start time to skew timeouts
        return false;
    }

    // handle timeout webclient check
    if (_webClientCheck && (_webPortalAccessed > _configPortalStart) > 0)
        _configPortalStart = _webPortalAccessed;

    // handle timed out
    if (millis() > _configPortalStart + _configPortalTimeout)
    {
        return true; // timeout bail, else do debug logging
    }

    return false;
}

void WiFiMind::setupHTTPServer()
{

    server.reset(new WM_WebServer(_httpPort));
    server->enableCORS(true);
    // server->on(PATH_ROOT, HTTP_GET, std::bind(&WiFiMind::handleRoot, this));
    if (_webservercallback != NULL)
    {
        _webservercallback(); // @CALLBACK
    }
    server->on(PATH_WIFI, HTTP_GET, std::bind(&WiFiMind::handleWifi, this));
    server->on(PATH_WIFISAVE, HTTP_GET, std::bind(&WiFiMind::handleWifiSave, this));
    server->on(PATH_EXIT, HTTP_GET, std::bind(&WiFiMind::handleExit, this));
    server->on(PATH_RESTART, HTTP_GET, std::bind(&WiFiMind::handleRestart, this));
    server->on(PATH_ERASE, HTTP_GET, std::bind(&WiFiMind::handleErase, this));
    server->onNotFound(std::bind(&WiFiMind::handleNotFound, this));

    server->on(PATH_UPDATE, HTTP_POST, std::bind(&WiFiMind::handleUpdateDone, this), std::bind(&WiFiMind::handleUpdate, this));
    server->begin();
}

void WiFiMind::handleRequest()
{
    _webPortalAccessed = millis();
    
    if(!_auth) return;

    if (!server->authenticate("admin", "admin"))
    {
        return server->requestAuthentication(HTTPAuthMethod::BASIC_AUTH);
    }
}

// void WiFiMind::handleRoot()
// {
//     handleRequest();
//     server->sendHeader(F("Content-Encoding"), F("gzip"));
//     server->send(200, "text/html", (const char *)index_html_gz, index_html_gz_len);
// }

void WiFiMind::handleInfo()
{
    handleRequest();
    server->send(200, JSONTYPE, "{\"ha\":\"ha\"}");
}

void WiFiMind::handleWifi()
{
    handleRequest();
    WiFi_scanNetworks(server->hasArg(F("refresh")), false); // wifiscan, force if arg refresh
    server->send(200, JSONTYPE, getScanItemOut().c_str());
}

void WiFiMind::handleWifiSave()
{
    handleRequest();
    _ssid = server->arg(F(SSID_PARAM_WIFISAVE)).c_str();
    _pass = server->arg(F(PASS_PARAM_WIFISAVE)).c_str();
    server->send(200, JSONTYPE, F("{\"message\":\"OK\"}"));
    // if (webPortalActive)
    //     webPortalActive = false;
    connect = true; // signal ready to connect/reset process in processConfigPortal
}

void WiFiMind::handleUpdate()
{
    bool error = false;
    unsigned long _configPortalTimeoutSAV = _configPortalTimeout; // store cp timeout
    _configPortalTimeout = 0;                                     // disable timeout

    HTTPUpload &upload = server->upload();

    // UPLOAD START
    if (upload.status == UPLOAD_FILE_START)
    {
        uint32_t maxSketchSpace;
        if (_cbOTAStart)
        {
            _cbOTAStart();
        }
#ifdef ESP8266
        WiFiUDP::stopAll();
        maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#elif defined(ESP32)
        maxSketchSpace = UPDATE_SIZE_UNKNOWN;
#endif
        if (!Update.begin(maxSketchSpace))
        { // start with max available size
            error = true;
            Update.end();
        }
    }
    // UPLOAD WRITE
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            error = true;
        }
    }
    // UPLOAD FILE END
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
        {
            if (_cbOTAEnd)
            {
                _cbOTAEnd();
            }
        }
        else
        {
            error = true;
        }
    }
    // UPLOAD ABORT
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Update.end();
        error = true;
    }
    if (error)
    {
        if (_cbOTAError)
        {
            _cbOTAError(Update.getError());
        }
        _configPortalTimeout = _configPortalTimeoutSAV;
    }
    delay(0);
}

void WiFiMind::handleUpdateDone()
{
    handleRequest();
    if (Update.hasError())
    {
        server->send(400, JSONTYPE, F("{\"message\":\"Update failed!\"}"));
    }
    else
    {
        server->send(200, JSONTYPE, F("{\"message\":\"Update successful.\"}"));
    }
    delay(1000); // send page
    if (!Update.hasError())
    {
        ESP.restart();
    }
}

void WiFiMind::handleErase()
{
    handleRequest();
    bool ret;
#ifdef ESP8266
    ret = ESP.eraseConfig();
#elif defined(ESP32)
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(true);
    ret = WiFi.disconnect(true, true);
    delay(500);
    WiFi.persistent(false);
#endif
    if (ret)
    {
        server->send(200, JSONTYPE, F("{\"message\":\"Erase WiFi Success\"}"));
    }
    else
    {
        server->send(400, JSONTYPE, F("{\"message\":\"Erase WiFi Failed\"}"));
    }
    delay(2000);
    ESP.restart();
}

void WiFiMind::handleRestart()
{
    handleRequest();
    server->send(200, JSONTYPE, F("{\"message\":\"Restart device\"}"));
    delay(2000);
    ESP.restart();
}

void WiFiMind::handleExit()
{
    handleRequest();
    server->send(200, JSONTYPE, F("{\"message\":\"Exit Config Page\"}"));
    delay(2000);
    webPortalActive = false;
}

void WiFiMind::handleNotFound()
{
    handleRequest();
    server->send(404, JSONTYPE, F("{\"message\":\"Page not Found\"}"));
}

void WiFiMind::setupDNSD()
{
    dnsServer.reset(new DNSServer());
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DNS_PORT, F("*"), WiFi.softAPIP());
}

void WiFiMind::setupConfigPortal()
{
    setupHTTPServer();
    _lastscan = 0; // reset network scan cache
    if (_preloadwifiscan)
        WiFi_scanNetworks(true, true); // preload wifiscan , async
}

bool WiFiMind::startAP()
{
    bool ret = true;

#ifdef ESP8266
    // @bug workaround for bug #4372 https://github.com/esp8266/Arduino/issues/4372
    if (!WiFi.enableAP(true))
    {
        return false;
    }
    delay(500); // workaround delay
#endif

    // setup optional soft AP static ip config
    if (_ap_static_ip)
    {
        if (!WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn))
        {
        }
    }

    int32_t channel = 0;
    if (_channelSync)
        channel = WiFi.channel();
    else
        channel = _apChannel;

    if (channel > 0)
    {
    }

    if (_apPassword != "")
    {
        if (channel > 0)
        {
            ret = WiFi.softAP(_apName.c_str(), _apPassword.c_str(), channel, _apHidden);
        }
        else
        {
            ret = WiFi.softAP(_apName.c_str(), _apPassword.c_str(), 1, _apHidden); // password option
        }
    }
    else
    {
        if (channel > 0)
        {
            ret = WiFi.softAP(_apName.c_str(), "", channel, _apHidden);
        }
        else
        {
            ret = WiFi.softAP(_apName.c_str(), "", 1, _apHidden);
        }
    }

    delay(500); // slight delay to make sure we get an AP IP

// set ap hostname
#ifdef ESP32
    if (ret && _hostname != "")
    {
        bool res = WiFi.softAPsetHostname(_hostname.c_str());
    }
#endif
    return ret;
}

bool WiFiMind::setupHostname(bool restart)
{
    if (_hostname == "")
    {
        return false;
    }
    else
    {
    }
    bool res = true;
#ifdef ESP8266
    res = WiFi.hostname(_hostname.c_str());
#ifdef WM_MDNS
    if (MDNS.begin(_hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }
#endif // WM_MDNS
#elif defined(ESP32)

    res = WiFi.setHostname(_hostname.c_str());
#ifdef WM_MDNS
    if (MDNS.begin(_hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }
#endif // WM_MDNS
#endif // ESP8266 ESP32

    if (restart && (WiFi.status() == WL_CONNECTED))
    {
        // WiFi.reconnect(); // This does not reset dhcp
        WiFi_Disconnect();
        delay(200); // do not remove, need a delay for disconnect to change status()
    }
    return res;
}

boolean WiFiMind::startConfigPortal(char const *apName, char const *apPassword)
{
    _begin();
    if (configPortalActive)
    {
        return false;
    }

    _apName = apName;
    _apPassword = apPassword;

    if (!validApPassword())
        return false;

    if (_disableSTA || (!WiFi.isConnected() && _disableSTAConn))
    {
        // #ifdef WM_DISCONWORKAROUND
        //         WiFi.mode(WIFI_AP_STA);
        // #endif
        WiFi_Disconnect();
        WiFi_enableSTA(false, false);
    }
    else
    {
    }

    configPortalActive = true;
    bool result = connect = abort = false;
    uint8_t state;

    _configPortalStart = millis();

    startAP();
    // WiFiSetCountry();

    if (_apcallback != NULL)
    {
        _apcallback(this);
    }

    // init configportal
    setupConfigPortal();

    setupDNSD();

    if (!_configPortalIsBlocking)
    {
        return result; // skip blocking loop
    }

    // enter blocking loop, waiting for config

    while (1)
    {

        // if timed out or abort, break
        if (configPortalHasTimeout() || abort)
        {
            shutdownConfigPortal();
            result = abort ? portalAbortResult : portalTimeoutResult; // false, false
            break;
        }

        state = processConfigPortal();

        if (state != WL_IDLE_STATUS)
        {
            result = (state == WL_CONNECTED); // true if connected
            break;
        }

        if (!configPortalActive)
            break;

        yield(); // watchdog
    }
    return result;
}

boolean WiFiMind::process()
{
// process mdns, esp32 not required
#if defined(WM_MDNS) && defined(ESP8266)
    MDNS.update();
#endif

    if (webPortalActive || (configPortalActive && !_configPortalIsBlocking))
    {
        // if timed out or abort, break
        if (_allowExit && (configPortalHasTimeout() || abort))
        {
            webPortalActive = false;
            shutdownConfigPortal();
            // if (_configportaltimeoutcallback != NULL) {
            //   _configportaltimeoutcallback();  // @CALLBACK
            // }
            return false;
        }

        uint8_t state = processConfigPortal(); // state is WL_IDLE or WL_CONNECTED/FAILED
        return state == WL_CONNECTED;
    }
    return false;
}

uint8_t WiFiMind::processConfigPortal()
{
    if (configPortalActive)
    {
        // DNS handler
        dnsServer->processNextRequest();
    }

    // HTTP handler
    server->handleClient();

    // Waiting for save...
    if (connect)
    {
        connect = false;
        if (_enableCaptivePortal)
            delay(_cpclosedelay); // keeps the captiveportal from closing to fast.

        // skip wifi if no ssid
        if (_ssid == "")
        {
        }
        else
        {
            // attempt sta connection to submitted _ssid, _pass
            uint8_t res = connectWifi(_ssid, _pass, _connectonsave) == WL_CONNECTED;
            if (res || (!_connectonsave))
            {
                //   if ( _savewificallback != NULL) {
                //     _savewificallback(); // @CALLBACK
                //   }
                if (!_connectonsave)
                    return WL_IDLE_STATUS;
                if (_disableConfigPortal)
                    shutdownConfigPortal();
                return WL_CONNECTED; // CONNECT SUCCESS
            }
        }

        if (_configPortalIsBlocking)
        {
            // clear save strings
            _ssid = "";
            _pass = "";
            // if connect fails, turn sta off to stabilize AP
            WiFi_Disconnect();
            WiFi_enableSTA(false, false);
        }
        else
        {
        }
    }
    return WL_IDLE_STATUS;
}

void WiFiMind::startWebPortal()
{
    if (configPortalActive || webPortalActive)
        return;
    connect = abort = false;
    setupConfigPortal();
    webPortalActive = true;
}

void WiFiMind::stopWebPortal()
{
    if (!configPortalActive && !webPortalActive)
        return;
    webPortalActive = false;
    shutdownConfigPortal();
}

void WiFiMind::stopCaptivePortal()
{
    _enableCaptivePortal = false;
}

bool WiFiMind::stopConfigPortal()
{
    if (_configPortalIsBlocking)
    {
        abort = true;
        return true;
    }
    return shutdownConfigPortal();
}

void WiFiMind::resetSettings()
{
    WiFi_enableSTA(true, true); // must be sta to disconnect erase
    delay(500);                 // ensure sta is enabled
    // if (_resetcallback != NULL)
    // {
    //     _resetcallback(); // @CALLBACK
    // }

#ifdef ESP32
    WiFi.disconnect(true, true);
#else
    WiFi.persistent(true);
    WiFi.disconnect(true);
    WiFi.persistent(false);
#endif
}

void WiFiMind::setTimeout(unsigned long seconds)
{
    setConfigPortalTimeout(seconds);
}

void WiFiMind::setConfigPortalTimeout(unsigned long seconds)
{
    _configPortalTimeout = seconds * 1000;
}

void WiFiMind::setConnectTimeout(unsigned long seconds)
{
    _connectTimeout = seconds * 1000;
}

void WiFiMind::setConnectRetries(uint8_t numRetries)
{
    _connectRetries = constrain(numRetries, 1, 10);
}

void WiFiMind::setCleanConnect(bool enable)
{
    _cleanConnect = enable;
}

void WiFiMind::setSaveConnectTimeout(unsigned long seconds)
{
    _saveTimeout = seconds * 1000;
}

void WiFiMind::setSaveConnect(bool connect)
{
    _connectonsave = connect;
}

void WiFiMind::setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
    _ap_static_ip = ip;
    _ap_static_gw = gw;
    _ap_static_sn = sn;
}

void WiFiMind::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn)
{
    _sta_static_ip = ip;
    _sta_static_gw = gw;
    _sta_static_sn = sn;
}

void WiFiMind::setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn, IPAddress dns)
{
    setSTAStaticIPConfig(ip, gw, sn);
    _sta_static_dns = dns;
}

void WiFiMind::setMinimumSignalQuality(int quality)
{
    _minimumQuality = quality;
}

void WiFiMind::setRestorePersistent(boolean persistent)
{
    _userpersistent = persistent;
}

void WiFiMind::setCaptivePortalEnable(boolean enabled)
{
    _enableCaptivePortal = enabled;
}

void WiFiMind::setAPClientCheck(boolean enabled)
{
    _apClientCheck = enabled;
}

void WiFiMind::setWebPortalClientCheck(boolean enabled)
{
    _webClientCheck = enabled;
}

void WiFiMind::setWiFiAutoReconnect(boolean enabled)
{
    _wifiAutoReconnect = enabled;
}

void WiFiMind::setWiFiAPChannel(int32_t channel)
{
    _apChannel = channel;
}

bool WiFiMind::setHostname(const char *hostname)
{
    _hostname = String(hostname);
    return true;
}

bool WiFiMind::setHostname(String hostname)
{
    _hostname = hostname;
    return true;
}

void WiFiMind::setWiFiAPHidden(bool hidden)
{
    _apHidden = hidden;
}

void WiFiMind::setHttpPort(uint16_t port)
{
    _httpPort = port;
}

bool WiFiMind::getConfigPortalActive()
{
    return configPortalActive;
}

void WiFiMind::setCountry(String cc)
{
    _wificountry = cc;
}

void WiFiMind::setAllowExit(bool allow)
{
    _allowExit = allow;
}

void WiFiMind::setAllowBasicAuth(bool allow)
{
    _auth = allow;
}

void WiFiMind::setBasicAuth(String username, String password)
{
    _authUsername = String(username);
    _authPassword = String(password);
}

String WiFiMind::encryptionTypeStr(uint8_t authmode)
{
    return AUTH_MODE_NAMES[authmode];
}

String WiFiMind::getScanItemOut()
{
    String page = "[";

    if (!_numNetworks)
        WiFi_scanNetworks(); // scan in case this gets called before any scans

    int n = _numNetworks;
    if (n == 0)
    {
        // page += FPSTR(S_nonetworks); // @token nonetworks
        // page += F("<br/><br/>");
    }
    else
    {
        // sort networks
        int indices[n];
        for (int i = 0; i < n; i++)
        {
            indices[i] = i;
        }

        // RSSI SORT
        for (int i = 0; i < n; i++)
        {
            for (int j = i + 1; j < n; j++)
            {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
                {
                    std::swap(indices[i], indices[j]);
                }
            }
        }

        // remove duplicates ( must be RSSI sorted )
        String cssid;
        for (int i = 0; i < n; i++)
        {
            if (indices[i] == -1)
                continue;
            cssid = WiFi.SSID(indices[i]);
            for (int j = i + 1; j < n; j++)
            {
                if (cssid == WiFi.SSID(indices[j]))
                {
                    indices[j] = -1; // set dup aps to index -1
                }
            }
        }

        // display networks in page
        for (int i = 0; i < n; i++)
        {
            if (indices[i] == -1)
                continue; // skip dups

            int rssiperc = getRSSIasQuality(WiFi.RSSI(indices[i]));
            uint8_t enc_type = WiFi.encryptionType(indices[i]);

            if (_minimumQuality == -1 || _minimumQuality < rssiperc)
            {
                if (WiFi.SSID(indices[i]) == "")
                {
                    // Serial.println(WiFi.BSSIDstr(indices[i]));
                    continue; // No idea why I am seeing these, lets just skip them for now
                }

                /**
                 * {
                 *      "ssid": WiFi.SSID(indices[i]),
                 *      "quality": rssiperc,
                 *      "encrytion": encryptionTypeStr(enc_type)
                 * }
                 * */
                String item = "{\"ssid\":\"";
                item += WiFi.SSID(indices[i]);
                item += "\",\"quality\":\"";
                item += (String)rssiperc;
                item += "\",\"encrytion\":\"";
                item += encryptionTypeStr(enc_type);
                item += "\"}";

                page += item;
                if (i < (n - 1))
                {
                    page += ",";
                }
                delay(0);
            }
        }
        page += "]";
    }

    return page;
}

int WiFiMind::getRSSIasQuality(int RSSI)
{
    int quality = 0;

    if (RSSI <= -100)
    {
        quality = 0;
    }
    else if (RSSI >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (RSSI + 100);
    }
    return quality;
}

bool WiFiMind::shutdownConfigPortal()
{
    if (webPortalActive)
        return false;

    if (configPortalActive)
    {
        // DNS handler
        dnsServer->processNextRequest();
    }

    // HTTP handler
    server->handleClient();

    // @todo what is the proper way to shutdown and free the server up
    // debug - many open issues aobut port not clearing for use with other servers
    server->stop();
    server.reset();

    WiFi.scanDelete(); // free wifi scan results

    if (!configPortalActive)
        return false;

    dnsServer->stop(); //  free heap ?
    dnsServer.reset();

    bool ret = false;
    ret = WiFi.softAPdisconnect(false);

    delay(1000);
    WiFi_Mode(_usermode, false);
    if (WiFi.status() == WL_IDLE_STATUS)
    {
        WiFi.reconnect();
    }
    configPortalActive = false;
    _end();
    return ret;
}

uint8_t WiFiMind::connectWifi(String ssid, String pass, bool connect)
{
    uint8_t retry = 1;
    uint8_t connRes = (uint8_t)WL_NO_SSID_AVAIL;

    setSTAConfig();
    if (_cleanConnect)
        WiFi_Disconnect(); // disconnect before begin, in case anything is hung, this causes a 2 seconds delay for connect
    while (retry <= _connectRetries && (connRes != WL_CONNECTED))
    {
        if (_connectRetries > 1)
        {
        }
        if (ssid != "")
        {
            wifiConnectNew(ssid, pass, connect);
            if (_saveTimeout > 0)
            {
                connRes = waitForConnectResult(_saveTimeout); // use default save timeout for saves to prevent bugs in esp->waitforconnectresult loop
            }
            else
            {
                connRes = waitForConnectResult();
            }
        }
        else
        {
            // connect using saved ssid if there is one
            if (WiFi_hasAutoConnect())
            {
                wifiConnectDefault();
                connRes = waitForConnectResult();
            }
            else
            {
            }
        }
        retry++;
    }

#ifdef NO_EXTRA_4K_HEAP
    if (_tryWPS && connRes != WL_CONNECTED && pass == "")
    {
        startWPS();
        connRes = waitForConnectResult();
    }
#endif
    if (connRes != WL_SCAN_COMPLETED)
    {
        updateConxResult(connRes);
    }
    return connRes;
}

bool WiFiMind::wifiConnectNew(String ssid, String pass, bool connect)
{
    bool ret = false;
    WiFi_enableSTA(true, storeSTAmode); // storeSTAmode will also toggle STA on in default opmode (persistent) if true (default)
    WiFi.persistent(true);
    ret = WiFi.begin(ssid.c_str(), pass.c_str(), 0, NULL, connect);
    WiFi.persistent(false);
    return ret;
}

// String WiFiMind::toStringIp(IPAddress ip)
// {
//     String res = "";
//     for (int i = 0; i < 3; i++)
//     {
//         res += String((ip >> (8 * i)) & 0xFF) + ".";
//     }
//     res += String(((ip >> 8 * 3)) & 0xFF);
//     return res;
// }

boolean WiFiMind::validApPassword()
{
    // check that ap password is valid, return false
    if (_apPassword == NULL)
        _apPassword = "";
    if (_apPassword != "")
    {
        if (_apPassword.length() < 8 || _apPassword.length() > 63)
        {
            _apPassword = "";
            return false; // @todo FATAL or fallback to empty , currently fatal, fail secure.
        }
    }
    return true;
}

bool WiFiMind::wifiConnectDefault()
{
    bool ret = false;

    ret = WiFi_enableSTA(true, storeSTAmode);
    delay(500); // THIS DELAY ?
    ret = WiFi.begin();
    return ret;
}

bool WiFiMind::setSTAConfig()
{
    bool ret = true;
    if (_sta_static_ip)
    {
        if (_sta_static_dns)
        {
            ret = WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn, _sta_static_dns);
        }
        else
        {
            ret = WiFi.config(_sta_static_ip, _sta_static_gw, _sta_static_sn);
        }
    }
    else
    {
    }
    return ret;
}

void WiFiMind::updateConxResult(uint8_t status)
{
    _lastconxresult = status;
#ifdef ESP8266
    if (_lastconxresult == WL_CONNECT_FAILED)
    {
        if (wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
        {
            _lastconxresult = WL_STATION_WRONG_PASSWORD;
        }
    }
#elif defined(ESP32)
    if (_lastconxresult == WL_CONNECT_FAILED || _lastconxresult == WL_DISCONNECTED)
    {
        if (_lastconxresulttmp != WL_IDLE_STATUS)
        {
            _lastconxresult = _lastconxresulttmp;
        }
    }
#endif
}

uint8_t WiFiMind::waitForConnectResult()
{
    return waitForConnectResult(_connectTimeout);
}

uint8_t WiFiMind::waitForConnectResult(uint32_t timeout)
{
    if (timeout == 0)
    {
        return WiFi.waitForConnectResult();
    }

    unsigned long timeoutmillis = millis() + timeout;
    uint8_t status = WiFi.status();

    while (millis() < timeoutmillis)
    {
        status = WiFi.status();
        // @todo detect additional states, connect happens, then dhcp then get ip, there is some delay here, make sure not to timeout if waiting on IP
        if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
        {
            return status;
        }
        delay(100);
    }
    return status;
}

// toggle STA without persistent
bool WiFiMind::WiFi_enableSTA(bool enable, bool persistent)
{
#ifdef ESP8266
    WiFiMode_t newMode;
    WiFiMode_t currentMode = WiFi.getMode();
    bool isEnabled = (currentMode & WIFI_STA) != 0;
    if (enable)
        newMode = (WiFiMode_t)(currentMode | WIFI_STA);
    else
        newMode = (WiFiMode_t)(currentMode & (~WIFI_STA));

    if ((isEnabled != enable) || persistent)
    {
        if (enable)
        {
            return WiFi_Mode(newMode, persistent);
        }
        else
        {
            return WiFi_Mode(newMode, persistent);
        }
    }
    else
    {
        return true;
    }
#elif defined(ESP32)
    bool ret;
    if (persistent && esp32persistent)
        WiFi.persistent(true);
    ret = WiFi.enableSTA(enable); // @todo handle persistent when it is implemented in platform
    if (persistent && esp32persistent)
        WiFi.persistent(false);
    return ret;
#endif
}

bool WiFiMind::WiFiSetCountry()
{
    if (_wificountry == "")
        return false; // skip not set
    bool ret = true;
#ifdef ESP32
    esp_err_t err = ESP_OK;
    if (WiFi.getMode() == WIFI_MODE_NULL)
    {
    } // exception if wifi not init!
    else
    {
#ifndef WM_NOCOUNTRY
        err = esp_wifi_set_country_code(_wificountry.c_str(), true);
#else
        err = true;
#endif
    }
    ret = err == ESP_OK;

#elif defined(ESP8266) && !defined(WM_NOCOUNTRY)
    // if(WiFi.getMode() == WIFI_OFF); // exception if wifi not init!
    if (_wificountry == "US")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_US);
    else if (_wificountry == "JP")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_JP);
    else if (_wificountry == "CN")
        ret = wifi_set_country((wifi_country_t *)&WM_COUNTRY_CN);
#endif
    return ret;
}

// set mode ignores WiFi.persistent
bool WiFiMind::WiFi_Mode(WiFiMode_t m, bool persistent)
{
    bool ret;
#ifdef ESP8266
    if ((wifi_get_opmode() == (uint8)m) && !persistent)
    {
        return true;
    }
    ETS_UART_INTR_DISABLE();
    if (persistent)
        ret = wifi_set_opmode(m);
    else
        ret = wifi_set_opmode_current(m);
    ETS_UART_INTR_ENABLE();
    return ret;
#elif defined(ESP32)
    if (persistent && esp32persistent)
        WiFi.persistent(true);
    ret = WiFi.mode(m); // @todo persistent check persistant mode, was eventually added to esp lib, but have to add version checking probably
    if (persistent && esp32persistent)
        WiFi.persistent(false);
    return ret;
#endif
}

bool WiFiMind::getWebPortalActive()
{
    return webPortalActive;
}

String WiFiMind::getWiFiHostname()
{
#ifdef ESP32
    return (String)WiFi.getHostname();
#else
    return (String)WiFi.hostname();
#endif
}

bool WiFiMind::WiFi_eraseConfig()
{
#ifdef ESP8266
#ifndef WM_FIXERASECONFIG
    return ESP.eraseConfig();
#else
    const size_t cfgSize = 0x4000;
    size_t cfgAddr = ESP.getFlashChipSize() - cfgSize;

    for (size_t offset = 0; offset < cfgSize; offset += SPI_FLASH_SEC_SIZE)
    {
        if (!ESP.flashEraseSector((cfgAddr + offset) / SPI_FLASH_SEC_SIZE))
        {
            return false;
        }
    }
    return true;
#endif
#elif defined(ESP32)
    bool ret;
    WiFi.mode(WIFI_AP_STA); // cannot erase if not in STA mode !
    WiFi.persistent(true);
    ret = WiFi.disconnect(true, true); // disconnect(bool wifioff, bool eraseap)
    delay(500);
    WiFi.persistent(false);
    return ret;
#endif
}

uint8_t WiFiMind::WiFi_softap_num_stations()
{
#ifdef ESP8266
    return wifi_softap_get_station_num();
#elif defined(ESP32)
    return WiFi.softAPgetStationNum();
#endif
}

bool WiFiMind::WiFi_hasAutoConnect()
{
    return WiFi_SSID(true) != "";
}

String WiFiMind::WiFi_SSID(bool persistent) const
{

#ifdef ESP8266
    struct station_config conf;
    if (persistent)
        wifi_station_get_config_default(&conf);
    else
        wifi_station_get_config(&conf);

    char tmp[33]; // ssid can be up to 32chars, => plus null term
    memcpy(tmp, conf.ssid, sizeof(conf.ssid));
    tmp[32] = 0; // nullterm in case of 32 char ssid
    return String(reinterpret_cast<char *>(tmp));

#elif defined(ESP32)
    // bool res = WiFi.wifiLowLevelInit(true); // @todo fix for S3, not found
    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (persistent)
    {
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        return String(reinterpret_cast<const char *>(conf.sta.ssid));
    }
    else
    {
        if (WiFiGenericClass::getMode() == WIFI_MODE_NULL)
        {
            return String();
        }
        wifi_ap_record_t info;
        if (!esp_wifi_sta_get_ap_info(&info))
        {
            return String(reinterpret_cast<char *>(info.ssid));
        }
        return String();
    }
#endif
}

String WiFiMind::WiFi_psk(bool persistent) const
{
#ifdef ESP8266
    struct station_config conf;

    if (persistent)
        wifi_station_get_config_default(&conf);
    else
        wifi_station_get_config(&conf);

    char tmp[65]; // psk is 64 bytes hex => plus null term
    memcpy(tmp, conf.password, sizeof(conf.password));
    tmp[64] = 0; // null term in case of 64 byte psk
    return String(reinterpret_cast<char *>(tmp));

#elif defined(ESP32)
    // only if wifi is init
    if (WiFiGenericClass::getMode() == WIFI_MODE_NULL)
    {
        return String();
    }
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<char *>(conf.sta.password));
#endif
}

void WiFiMind::WiFi_scanComplete(int networksFound)
{
    _lastscan = millis();
    _numNetworks = networksFound;
}

bool WiFiMind::WiFi_scanNetworks()
{
    return WiFi_scanNetworks(false, false);
}

bool WiFiMind::WiFi_scanNetworks(unsigned int cachetime, bool async)
{
    return WiFi_scanNetworks(millis() - _lastscan > cachetime, async);
}
bool WiFiMind::WiFi_scanNetworks(unsigned int cachetime)
{
    return WiFi_scanNetworks(millis() - _lastscan > cachetime, false);
}
bool WiFiMind::WiFi_scanNetworks(bool force, bool async)
{
#ifdef WM_DEBUG_LEVEL
// DEBUG_WM(WM_DEBUG_DEV,"scanNetworks async:",async == true);
// DEBUG_WM(WM_DEBUG_DEV,_numNetworks,(millis()-_lastscan ));
// DEBUG_WM(WM_DEBUG_DEV,"scanNetworks force:",force == true);
#endif

    // if 0 networks, rescan @note this was a kludge, now disabling to test real cause ( maybe wifi not init etc)
    // enable only if preload failed?
    if (_numNetworks == 0 && _autoforcerescan)
    {
        force = true;
    }

    // if scan is empty or stale (last scantime > _scancachetime), this avoids fast reloading wifi page and constant scan delayed page loads appearing to freeze.
    if (!_lastscan || (_lastscan > 0 && (millis() - _lastscan > _scancachetime)))
    {
        force = true;
    }

    if (force)
    {
        int8_t res;
        _startscan = millis();
        if (async && _asyncScan)
        {
#ifdef ESP8266
#ifndef WM_NOASYNC                             // no async available < 2.4.0
            using namespace std::placeholders; // for `_1`
            WiFi.scanNetworksAsync(std::bind(&WiFiMind::WiFi_scanComplete, this, _1));
#else
            res = WiFi.scanNetworks();
#endif
#else
            res = WiFi.scanNetworks(true);
#endif
            return false;
        }
        else
        {
            res = WiFi.scanNetworks();
        }
        if (res == WIFI_SCAN_RUNNING)
        {
            while (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
            {
                delay(100);
            }
            _numNetworks = WiFi.scanComplete();
        }
        else if (res >= 0)
            _numNetworks = res;
        _lastscan = millis();
        return true;
    }
    else
    {
    }
    return false;
}

#ifdef ESP32
#ifdef WM_ARDUINOEVENTS
void WiFiMind::WiFiEvent(WiFiEvent_t event, arduino_event_info_t info)
{
#else
void WiFiMind::WiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
#define wifi_sta_disconnected disconnected
#define ARDUINO_EVENT_WIFI_STA_DISCONNECTED SYSTEM_EVENT_STA_DISCONNECTED
#define ARDUINO_EVENT_WIFI_SCAN_DONE SYSTEM_EVENT_SCAN_DONE
#endif
    if (!_hasBegun)
    {
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
        if (info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_EXPIRE || info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_FAIL)
        {
            _lastconxresulttmp = 7; // hack in wrong password internally, sdk emit WIFI_REASON_AUTH_EXPIRE on some routers on auth_fail
        }
        else
            _lastconxresulttmp = WiFi.status();
#ifdef esp32autoreconnect
        WiFi.reconnect();
#endif
    }
    else if (event == ARDUINO_EVENT_WIFI_SCAN_DONE && _asyncScan)
    {
        uint16_t scans = WiFi.scanComplete();
        WiFi_scanComplete(scans);
    }
}
#endif

void WiFiMind::WiFi_autoReconnect()
{
#ifdef ESP8266
    WiFi.setAutoReconnect(_wifiAutoReconnect);
#elif defined(ESP32)
    using namespace std::placeholders;
    if (wm_event_id == 0)
        wm_event_id = WiFi.onEvent(std::bind(&WiFiMind::WiFiEvent, this, _1, _2));
#endif
}

bool WiFiMind::WiFi_Disconnect()
{
#ifdef ESP8266
    if ((WiFi.getMode() & WIFI_STA) != 0)
    {
        bool ret;
        ETS_UART_INTR_DISABLE(); // @todo possibly not needed
        ret = wifi_station_disconnect();
        ETS_UART_INTR_ENABLE();
        return ret;
    }
#elif defined(ESP32)
    return WiFi.disconnect(); // not persistent atm
#endif
    return false;
}

#endif // defined(ESP8266) || defined(ESP32)
