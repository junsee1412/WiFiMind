#include "WiFiMind.h"
#if defined(ESP8266) || defined(ESP32)
#include "const_string.h"
#include "index.h"

void WiFiMind::_begin()
{
    if (_hasBegun)
        return;
    _hasBegun = true;
}

void WiFiMind::_end()
{
    _hasBegun = false;
    if (_userpersistent)
        WiFi.persistent(true);
}

char *WiFiMind::getMqttHost()
{
    return mind_config.mqtt_host;
}

uint16_t WiFiMind::getMqttPort()
{
    return mind_config.mqtt_port;
}

String WiFiMind::getMqttToken()
{
    return String(mind_config.mqtt_token);
}

String WiFiMind::getMqttID()
{
    return String(mind_config.mqtt_id);
}

String WiFiMind::getMqttPassword()
{
    return String(mind_config.mqtt_pass);
}

uint16_t WiFiMind::getTelemetryInterval()
{
    return mind_config.tele_interval;
}

uint16_t WiFiMind::getAttributeInterval()
{
    return mind_config.attr_interval;
}

void WiFiMind::updateConxResult(uint8_t status)
{
    // hack in wrong password detection
    _lastconxresult = status;
    if (_lastconxresult == WL_CONNECT_FAILED)
    {
        if (wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
        {
            _lastconxresult = WL_STATION_WRONG_PASSWORD;
        }
    }
}

void WiFiMind::setTimeout(unsigned long seconds)
{
    setConfigPortalTimeout(seconds);
}

void WiFiMind::setConnectRetries(uint8_t numRetries)
{
    _connectRetries = constrain(numRetries, 1, 10);
}

void WiFiMind::setConfigPortalTimeout(unsigned long seconds)
{
    _configPortalTimeout = seconds * 1000;
}

void WiFiMind::setConnectTimeout(unsigned long seconds)
{
    _connectTimeout = seconds * 1000;
}

void WiFiMind::setSaveConnectTimeout(unsigned long seconds)
{
    _saveTimeout = seconds * 1000;
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
        if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
        {
            return status;
        }
        delay(100);
    }
    return status;
}

bool WiFiMind::WiFi_Disconnect()
{
    if ((WiFi.getMode() & WIFI_STA) != 0)
    {
        bool ret;
        ETS_UART_INTR_DISABLE(); // @todo possibly not needed
        ret = wifi_station_disconnect();
        ETS_UART_INTR_ENABLE();
        return ret;
    }
    return false;
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

bool WiFiMind::wifiConnectDefault()
{
    bool ret = false;
    ret = WiFi_enableSTA(true, storeSTAmode);
    delay(500); // THIS DELAY ?
    ret = WiFi.begin();
    return ret;
}

void WiFiMind::WiFi_autoReconnect()
{
    WiFi.setAutoReconnect(_wifiAutoReconnect);
}

uint8_t WiFiMind::connectWifi(String ssid, String pass, bool connect)
{
    uint8_t retry = 1;
    uint8_t connRes = (uint8_t)WL_NO_SSID_AVAIL;

    if (_cleanConnect)
        WiFi_Disconnect();

    while (retry <= _connectRetries && (connRes != WL_CONNECTED))
    {
        if (ssid != "")
        {
            wifiConnectNew(ssid, pass, connect);
            if (_saveTimeout > 0)
            {
                connRes = waitForConnectResult(_saveTimeout);
            }
            else
            {
                connRes = waitForConnectResult();
            }
        }
        else
        {
            if (WiFi_hasAutoConnect())
            {
                wifiConnectDefault();
                connRes = waitForConnectResult();
            }
        }
        retry++;
    }

    if (connRes != WL_SCAN_COMPLETED)
    {
        updateConxResult(connRes);
    }

    return connRes;
}

bool WiFiMind::WiFi_hasAutoConnect()
{
    return WiFi_SSID(true) != "";
}

String WiFiMind::WiFi_SSID(bool persistent) const
{
    struct station_config conf;
    if (persistent)
        wifi_station_get_config_default(&conf);
    else
        wifi_station_get_config(&conf);

    char tmp[33]; // ssid can be up to 32chars, => plus null term
    memcpy(tmp, conf.ssid, sizeof(conf.ssid));
    tmp[32] = 0; // nullterm in case of 32 char ssid
    return String(reinterpret_cast<char *>(tmp));
}

boolean WiFiMind::autoConnect(char const *apName, char const *apPassword)
{
    bool wifiIsSaved = true;
    if (wifiIsSaved)
    {
        _startconn = millis();
        _begin();

        if (!WiFi.enableSTA(true))
        {
            return false;
        }
    }

    WiFi_autoReconnect();
    // WiFi.hostname(HOSTNAME);

    bool connected = false;
    if (WiFi.status() == WL_CONNECTED)
    {
        connected = true;
    }

    if (connected || connectWifi(_defaultssid, _defaultpass) == WL_CONNECTED)
    {
        _lastconxresult = WL_CONNECTED;
        return true;
    }
    bool res = startConfigPortal(apName, apPassword);
    return res;
}

bool WiFiMind::WiFi_enableSTA(bool enable, bool persistent)
{
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
}

bool WiFiMind::WiFi_Mode(WiFiMode_t m, bool persistent)
{
    bool ret;
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
}

WiFiMind::WiFiMind()
{
    EEPROM.begin(512);
    EEPROM.get(0, mind_config);
    EEPROM.end();
}

WiFiMind::~WiFiMind()
{
    _end();
}

// CONFIG PORTAL
bool WiFiMind::startAP()
{
    bool ret = true;

    if (!WiFi.enableAP(true))
    {
        return false;
    }
    delay(500); // workaround delay

    ret = WiFi.softAP(_apName.c_str(), _apPassword.c_str(), 1, false);

    delay(500); // slight delay to make sure we get an AP IP
    return ret;
}

void WiFiMind::setAPClientCheck(boolean enabled)
{
    _apClientCheck = enabled;
}

void WiFiMind::setWebPortalClientCheck(boolean enabled)
{
    _webClientCheck = enabled;
}

boolean WiFiMind::startConfigPortal(char const *apName, char const *apPassword)
{
    _begin();

    if (configPortalActive)
    {
        return false;
    }

    _apName = apName; // @todo check valid apname ?
    _apPassword = apPassword;

    if (_apName == "")
        _apName = HOSTNAME;

    if (_disableSTA || (!WiFi.isConnected() && _disableSTAConn))
    {
        WiFi_Disconnect();
        WiFi_enableSTA(false, false);
    }

    // init configportal globals to known states
    configPortalActive = true;
    bool result = connect = abort = false; // loop flags, connect true success, abort true break
    uint8_t state;

    _configPortalStart = millis();

    startAP();
    setupConfigPortal();
    setupDNSD();

    while (1)
    {
        if (configPortalHasTimeout() || abort)
        {
            shutdownConfigPortal();
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
        yield();
    }

    return result;
}

boolean WiFiMind::process()
{
    if (webPortalActive || (configPortalActive && !_configPortalIsBlocking))
    {
        // if timed out or abort, break
        if (_allowExit && (configPortalHasTimeout() || abort))
        {
            webPortalActive = false;
            shutdownConfigPortal();
            return false;
        }
        uint8_t state = processConfigPortal(); // state is WL_IDLE or WL_CONNECTED/FAILED
        return state == WL_CONNECTED;
    }
    return false;
}

uint8_t WiFiMind::processConfigPortal()
{
    server->handleClient();
    if (connect)
    {
        connect = false;
        if (_ssid != "")
        {
            // attempt sta connection to submitted _ssid, _pass
            uint8_t res = connectWifi(_ssid, _pass, _connectonsave) == WL_CONNECTED;
            if (res || (!_connectonsave))
            {
                if (!_connectonsave)
                    return WL_IDLE_STATUS;
                if (_disableConfigPortal)
                    shutdownConfigPortal();
                return WL_CONNECTED; // CONNECT SUCCESS
            }
        }
    }
    return WL_IDLE_STATUS;
}

bool WiFiMind::getWebPortalActive()
{
    return webPortalActive;
}

bool WiFiMind::shutdownConfigPortal()
{

    server->handleClient();
    server->stop();
    server.reset();
    WiFi.scanDelete();
    bool ret = false;
    ret = WiFi.softAPdisconnect(false);
    delay(1000);
    WiFi_Mode(WIFI_STA, false);
    if (WiFi.status() == WL_IDLE_STATUS)
    {
        WiFi.reconnect(); // restart wifi since we disconnected it in startconfigportal
    }
    configPortalActive = false;
    _end();
    return ret;
}

void WiFiMind::setupConfigPortal()
{
    setupHTTPServer();
    // _lastscan = 0; // reset network scan cache
    // if (_preloadwifiscan)
    //     WiFi_scanNetworks(true, true); // preload wifiscan , async
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

boolean WiFiMind::configPortalHasTimeout()
{
    if (!webPortalActive)
        return false;

    // handle timeout portal client check
    if (_configPortalTimeout == 0 || (_apClientCheck && (wifi_softap_get_station_num() > 0)))
    {
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
    WiFi.persistent(true);
    WiFi.disconnect(true);
    WiFi.persistent(false);
}

void WiFiMind::setupDNSD()
{
    dnsServer.reset(new DNSServer());

    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, F("*"), WiFi.softAPIP());
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
#ifndef WM_NOASYNC                             // no async available < 2.4.0
            using namespace std::placeholders; // for `_1`
            WiFi.scanNetworksAsync(std::bind(&WiFiMind::WiFi_scanComplete, this, _1));
#else
            res = WiFi.scanNetworks();
#endif
            return false;
        }
        else
        {
            res = WiFi.scanNetworks();
        }
        if (res == WIFI_SCAN_FAILED)
        {
        }
        else if (res == WIFI_SCAN_RUNNING)
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
    return false;
}

void WiFiMind::WiFi_scanComplete(int networksFound)
{
    _lastscan = millis();
    _numNetworks = networksFound;
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

void WiFiMind::setupHTTPServer()
{
    server.reset(new ESP8266WebServer(_httpPort));
    server->enableCORS(true);
    server->on(PATH_WIFI, HTTP_GET, std::bind(&WiFiMind::handleWifi, this));
    server->on(PATH_WIFISAVE, HTTP_GET, std::bind(&WiFiMind::handleWifiSave, this));
    server->on(PATH_CONFIGINFO, HTTP_GET, std::bind(&WiFiMind::handleConfigInfo, this));
    server->on(PATH_CONFIGSAVE, HTTP_GET, std::bind(&WiFiMind::handleConfigSave, this));
    server->on(PATH_EXIT, HTTP_GET, std::bind(&WiFiMind::handleExit, this));
    server->on(PATH_RESTART, HTTP_GET, std::bind(&WiFiMind::handleRestart, this));
    server->on(PATH_ROOT, HTTP_GET, std::bind(&WiFiMind::handleRoot, this));
    server->on(PATH_ERASE, HTTP_GET, std::bind(&WiFiMind::handleErase, this));
    server->onNotFound(std::bind(&WiFiMind::handleNotFound, this));

    server->on(PATH_UPDATE, HTTP_POST, std::bind(&WiFiMind::handleUpdateDone, this), std::bind(&WiFiMind::handleUpdate, this));
    server->begin();
}

void WiFiMind::handleRequest()
{
    _webPortalAccessed = millis();
}

void WiFiMind::handleRestart()
{
    handleRequest();
    server->send(200, "application/json", F("{\"message\":\"Restart device\"}"));
    delay(2000);
    ESP.restart();
}

void WiFiMind::handleErase()
{
    handleRequest();
    if (ESP.eraseConfig())
    {
        server->send(200, "application/json", F("{\"message\":\"Erase WiFi Success\"}"));
    }
    else
    {
        server->send(400, "application/json", F("{\"message\":\"Erase WiFi Failed\"}"));
    }
    delay(2000);
    ESP.restart();
}

/**
 * HTTP 404
 */
void WiFiMind::handleNotFound()
{
    handleRequest();
    server->send(404, "application/json", F("{\"message\":\"Page not Found\"}"));
}

/**
 * HTTP Exit
 */
void WiFiMind::handleExit()
{
    handleRequest();
    server->send(200, "application/json", F("{\"message\":\"Exit Config Page\"}"));
    delay(2000);
    webPortalActive = false;
}

/**
 * HTTP ROOT
 **/
void WiFiMind::handleRoot()
{
    handleRequest();
    // sprintf_P(message, "{\"message\":\"Nothing here\"}");
    // server->send(200, "application/json", message);
    server->sendHeader(F("Content-Encoding"), F("gzip"));
    server->send(200, "text/html", (const char *)index_html_gz, index_html_gz_len);
}

/**
 * HTTP ROOT
 **/
void WiFiMind::handleConfigInfo()
{
    char message[256];
    handleRequest();
    sprintf(
        message,
        "{\"%s\":\"%s\",\"%s\":%u,\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":%u,\"%s\":%u}",
        HOST_PARAM_CONFIGSAVE, mind_config.mqtt_host,
        PORT_PARAM_CONFIGSAVE, mind_config.mqtt_port,
        TOKEN_PARAM_CONFIGSAVE, mind_config.mqtt_token,
        ID_PARAM_CONFIGSAVE, mind_config.mqtt_id,
        PASS_PARAM_CONFIGSAVE, mind_config.mqtt_pass,
        TELEINT_PARAM_CONFIGSAVE, mind_config.tele_interval,
        ATTRINT_PARAM_CONFIGSAVE, mind_config.attr_interval);
    server->send(200, "application/json", message);
}

/**
 * HTTP SAVE CONFIG
 **/
void WiFiMind::handleConfigSave()
{
    handleRequest();
    _mqtthost = server->arg(F(HOST_PARAM_CONFIGSAVE)).c_str();
    _mqttport = server->arg(F(PORT_PARAM_CONFIGSAVE)).c_str();
    _mqtttoken = server->arg(F(TOKEN_PARAM_CONFIGSAVE)).c_str();
    _mqttid = server->arg(F(ID_PARAM_CONFIGSAVE)).c_str();
    _mqttpassword = server->arg(F(PASS_PARAM_CONFIGSAVE)).c_str();
    _teleinterval = server->arg(F(TELEINT_PARAM_CONFIGSAVE)).c_str();
    _attrinterval = server->arg(F(ATTRINT_PARAM_CONFIGSAVE)).c_str();

    if (_mqtthost == "" || _mqttport == "" || _mqtttoken == "" ||
        _mqttid == "" || _teleinterval == "" || _attrinterval == "")
    {
        server->send(400, "application/json", F("{\"message\":\"SOME FIELDS ARE EMPTY\"}"));
        return;
    }

    strncpy(mind_config.mqtt_host, _mqtthost.c_str(), 32);
    strncpy(mind_config.mqtt_token, _mqtttoken.c_str(), 32);
    strncpy(mind_config.mqtt_id, _mqttid.c_str(), 32);
    strncpy(mind_config.mqtt_pass, _mqttpassword.c_str(), 32);

    mind_config.mqtt_port = (uint16_t)_mqttport.toInt();
    mind_config.tele_interval = (uint16_t)_teleinterval.toInt();
    mind_config.attr_interval = (uint16_t)_attrinterval.toInt();

    EEPROM.begin(512);
    EEPROM.put(0, mind_config);
    if (EEPROM.commit())
    {
        server->send(200, "application/json", F("{\"message\":\"Settings saved\"}"));
    }
    else
    {
        server->send(400, "application/json", F("{\"message\":\"Save error\"}"));
    }
    EEPROM.end();
}

/**
 * HTTP save form and redirect to WLAN config page again
 */
void WiFiMind::handleWifi()
{
    handleRequest();
    WiFi_scanNetworks(server->hasArg(F("refresh")), false); // wifiscan, force if arg refresh
    server->send(200, "application/json", getScanItemOut().c_str());
}

void WiFiMind::handleWifiSave()
{
    handleRequest();
    _ssid = server->arg(F(SSID_PARAM_WIFISAVE)).c_str();
    _pass = server->arg(F(PASS_PARAM_WIFISAVE)).c_str();
    server->send(200, "application/json", F("{\"message\":\"OK\"}"));
    if (webPortalActive)
        webPortalActive = false;
    connect = true; // signal ready to connect/reset process in processConfigPortal
}

/**
 * HTTP update firmware
 */
void WiFiMind::handleUpdate()
{
    bool error = false;
    unsigned long _configPortalTimeoutSAV = _configPortalTimeout; // store cp timeout
    _configPortalTimeout = 0;                                     // disable timeout

    HTTPUpload &upload = server->upload();

    // UPLOAD START
    if (upload.status == UPLOAD_FILE_START)
    {
        if (_cbOTAStart)
        {
            _cbOTAStart();
        }
        uint32_t maxSketchSpace;
        WiFiUDP::stopAll();
        maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

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
        server->send(400, "application/json", F("{\"message\":\"Update failed!\"}"));
    }
    else
    {
        server->send(200, "application/json", F("{\"message\":\"Update successful.\"}"));
    }
    delay(1000); // send page
    if (!Update.hasError())
    {
        ESP.restart();
    }
}

#endif
