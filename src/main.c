/***** Includes *****/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "cJSON.h"

#include "curl.h"

/***** Defines *****/

#define AWS_TOPIC_NAME "hatchtrack/data/put"

#define INFLUXDB_URL ("https://db.hatchtrack.com:8086/write?db=peep0")
#define INFLUXDB_USER_PASS ("writer:YGAPARxJ0DOPm2mC")
#define INFLUXDB_DATA_STR_MAX_LEN (512)

/***** Local Data *****/

static volatile bool _is_running = true;
static AWS_IoT_Client client;

pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;

/***** Local Functions *****/

void
sigint_callback(int dummy)
{
  _is_running = false;
}

static bool
_https_post_to_db(char * msg)
{
  CURL *curl = NULL;
  CURLcode res = CURLE_OK;
  bool r = true;

  curl = curl_easy_init();
  if (NULL == curl) {
    r = false;
  }

  if (r) {
    curl_easy_setopt(curl, CURLOPT_URL, INFLUXDB_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, INFLUXDB_USER_PASS);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg);

    // perform the request, res will get the return code 
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      r = false;
      fprintf(
        stderr,
        "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
    }
  }

  if (curl) {
    curl_easy_cleanup(curl);
  }

  return r;
}

static char *
_parse_json_and_format(char * js)
{
  cJSON * node = NULL;
  cJSON * cjson = NULL;
  char * out = NULL;
  char * peep_uuid = NULL;
  char * hatch_uuid = NULL;
  float temperature = 0.0;
  float humidity = 0.0;
  uint64_t unix_time = 0;
  bool r = true;

  cjson = cJSON_Parse(js);
  if (NULL == cjson) {
    printf("failed to parse JSON message!\n");
    r = false;
  }

  if (r) {
    node = cJSON_GetObjectItemCaseSensitive(cjson, "peepUUID");
    if (cJSON_IsString(node) && (node->valuestring != NULL)) {
      peep_uuid = node->valuestring;
    }
    else {
      r = false;
    }
  }

  if (r) {
    node = cJSON_GetObjectItemCaseSensitive(cjson, "hatchUUID");
    if (cJSON_IsString(node) && (node->valuestring != NULL)) {
      hatch_uuid = node->valuestring;
    }
    else {
      r = false;
    }
  }

  if (r) {
    node = cJSON_GetObjectItemCaseSensitive(cjson, "temperature");
    if (cJSON_IsNumber(node)) {
      temperature = node->valuedouble;
    }
    else {
      r = false;
    }
  }

  if (r) {
    node = cJSON_GetObjectItemCaseSensitive(cjson, "humidity");
    if (cJSON_IsNumber(node)) {
      humidity = node->valuedouble;
    }
    else {
      r = false;
    }
  }

  if (r) {
    node = cJSON_GetObjectItemCaseSensitive(cjson, "unixTime");
    if (cJSON_IsNumber(node)) {
      unix_time = (uint32_t) node->valuedouble;
      // convert unix time in seconds to nano seconds for InfluxDB.
      unix_time *= 1000000000;
    }
    else {
      r = false;
    }
  }

  if (r) {
    out = malloc(INFLUXDB_DATA_STR_MAX_LEN);
    sprintf(
      out,
      "peep,"
      "peep_uuid=%s,"
      "hatch_uuid=%s "
      "temperature=%.2f,"
      "humidity=%.2f "
      "%" PRIu64 "",
      peep_uuid,
      hatch_uuid,
      temperature,
      humidity,
      unix_time);
  }

  return out;
}

static void
_subscribe_callback(AWS_IoT_Client * p_client, char *topic_name,
  uint16_t topic_name_len, IoT_Publish_Message_Params * params, void * p_data)
{
  (void) topic_name_len;
  (void) p_client;
  (void) p_data;
  char * db_str = NULL;

  db_str = _parse_json_and_format((char *) params->payload);
  if (db_str) {
    _https_post_to_db(db_str);
    free(db_str);
  }
}

void
disconnect_callback(AWS_IoT_Client * p_client, void *data)
{
  IoT_Error_t rc = FAILURE;
  (void) data;

  if(NULL == p_client) {
    return;
  }

  if(aws_iot_is_autoreconnect_enabled(p_client)) {
    printf("Auto Reconnect is enabled, reconnecting attempt will start now\n");
  }
  else {
    printf("Auto Reconnect not enabled, starting manual reconnect...\n");
    rc = aws_iot_mqtt_attempt_reconnect(p_client);
    if(NETWORK_RECONNECTED == rc) {
      printf("manual reconnect successful\n");
    }
    else {
      fprintf(stderr, "manual reconnect failed (%d)\n", rc);
    }
  }
}

bool
_aws_connect(char * root_ca_file, char * client_cert_file,
  char * client_private_key_file)
{
  IoT_Client_Init_Params mqtt_param = iotClientInitParamsDefault;
  IoT_Client_Connect_Params connect_param = iotClientConnectParamsDefault;
  IoT_Error_t rc = FAILURE;
  bool r = true;

  if (r) {
    mqtt_param.enableAutoReconnect = false; // We enable this later below
    mqtt_param.pHostURL = AWS_IOT_MQTT_HOST;
    mqtt_param.port = AWS_IOT_MQTT_PORT;
    mqtt_param.pRootCALocation = root_ca_file;
    mqtt_param.pDeviceCertLocation = client_cert_file;
    mqtt_param.pDevicePrivateKeyLocation = client_private_key_file;
    mqtt_param.mqttCommandTimeout_ms = 20000;
    mqtt_param.tlsHandshakeTimeout_ms = 5000;
    mqtt_param.isSSLHostnameVerify = true;
    mqtt_param.disconnectHandler = disconnect_callback;
    mqtt_param.disconnectHandlerData = NULL;

    rc = aws_iot_mqtt_init(&client, &mqtt_param);
    if(SUCCESS != rc) {
      fprintf(stderr, "aws_iot_mqtt_init failed (%d)\n", rc);
      r = false;
    }
  }

  if (r) {
    connect_param.keepAliveIntervalInSec = 600;
    connect_param.isCleanSession = true;
    connect_param.MQTTVersion = MQTT_3_1_1;
    connect_param.pClientID = AWS_IOT_MQTT_CLIENT_ID;
    connect_param.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
    connect_param.isWillMsgPresent = false;

    printf("connecting...\n");
    rc = aws_iot_mqtt_connect(&client, &connect_param);
    if(SUCCESS != rc) {
      fprintf(stderr, "failed to connect to %s:%d (%d)\n",
        mqtt_param.pHostURL,
        mqtt_param.port,
        rc);
      r = false;
    }
    else {
      printf("connected to %s:%d\n", mqtt_param.pHostURL, mqtt_param.port);
    }
  }

  if (r) {
    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of
     * Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) {
      fprintf(stderr, "unable to set Auto Reconnect to true (%d)", rc);
      r = false;
    }
  }

  return r;
}

static bool
_aws_listen(char * topic)
{
  IoT_Error_t rc = FAILURE;
  bool r = true;

  if (r) {
    rc = aws_iot_mqtt_subscribe(
      &client,
      topic,
      strlen(topic),
      QOS0,
      _subscribe_callback,
      NULL);
  }

  if(SUCCESS != rc) {
    printf("Error(%d): failed to subscribe\n", rc);
    r = false;
  }

  printf("subscribed to MQTT topic %s\n", topic);

  while ((r) && (_is_running)) {
    aws_iot_mqtt_yield(&client, 5000);
  }

  return r;
}

/***** Global Functions *****/

int
main(int argc, char **argv)
{
  bool r = true;

  if (4 != argc) {
    printf("%s [ROOT CA] [CLIENT_CERT] [CLIENT PRIVATE KEY]\n", argv[0]);
    r = false;
  }

  // catch ctrl+c
  signal(SIGINT, sigint_callback);
  curl_global_init(CURL_GLOBAL_DEFAULT);

  if (r) {
    printf("loading certificates...\n%s\n%s\n%s\n", argv[1], argv[2], argv[3]);
    r = _aws_connect(argv[1], argv[2], argv[3]);
  }

  if (r) {
    r = _aws_listen(AWS_TOPIC_NAME);
  }

  curl_global_cleanup();

  return (r) ? 0 : 1;
}
