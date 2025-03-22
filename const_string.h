#define CONNECT_TIMEOUT 10
#define HOSTNAME "MIND Vibration"
#define PASSWORD "mindprojects"

// MQTT CONFIG
#define DEVICE_TELEMETRY_TOPIC "v1/devices/me/telemetry"
#define DEVICE_ATTRIBUTES_TOPIC "v1/devices/me/attributes"
#define DEVICE_ATTRIBUTES_TOPIC_REQUEST "v1/devices/me/attributes/request/"
#define DEVICE_ATTRIBUTES_TOPIC_RESPONSE "v1/devices/me/attributes/response/"
#define DEVICE_RPC_TOPIC_REQUEST "v1/devices/me/rpc/request/"
#define DEVICE_RPC_TOPIC_RESPONSE "v1/devices/me/rpc/response/"

// DEVICE JSON CONFIG 
#define WRITE_FILE "w"
#define READ_FILE "r"
#define CONFIG_FILE_PATH "/config.json"

// HTTP CONFIG
#define PATH_ROOT "/"
#define PATH_EXIT "/exit"
#define PATH_RESTART "/restart"
#define PATH_ERASE "/erase"

/**
 * WiFi
 */
// Save
#define PATH_WIFISAVE "/wifisave"
#define SSID_PARAM_WIFISAVE "ssid"
#define PASS_PARAM_WIFISAVE "pass"
// Load
#define PATH_WIFI "/wifi"

/**
 * Config
 */
// Save
#define PATH_CONFIGSAVE "/configsave"
#define HOST_PARAM_CONFIGSAVE "mqtt_host"
#define PORT_PARAM_CONFIGSAVE "mqtt_port"
#define USER_PARAM_CONFIGSAVE "mqtt_user"
#define ID_PARAM_CONFIGSAVE "mqtt_id"
#define PASS_PARAM_CONFIGSAVE "mqtt_pass"
#define TELEINT_PARAM_CONFIGSAVE "tele_interval"
#define ATTRINT_PARAM_CONFIGSAVE "attr_interval"
// Load
#define PATH_CONFIGINFO "/configinfo"

/**
 * Update Firmware
 */
#define PATH_UPDATE "/update"
