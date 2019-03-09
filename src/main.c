/***** Includes *****/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
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

/***** Defines *****/

#define AWS_TOPIC_NAME "hatchtrack/data/put"

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

void
subscribe_callback(AWS_IoT_Client * p_client, char *topic_name,
  uint16_t topic_name_len, IoT_Publish_Message_Params * params, void * p_data)
{
  (void) topic_name_len;
  (void) p_client;
  (void) p_data;

  FILE * fp = NULL;
  static char buf[1024];
  char * temperature_file = NULL;
  char * humidity_file = NULL;
  double temperature = 0.0;
  double humidity = 0.0;
  bool r = true;

  pthread_mutex_lock(&_lock);

  // NOTE: Assumes payload is properly formatted JSON message
  // TODO: Check to ensure it is valid JSON?
  strncpy(buf, params->payload, params->payloadLen);
  buf[params->payloadLen] = '\0';

  printf("Subscribe callback\n");
  printf("%s\n", buf);

  pthread_mutex_unlock(&_lock);
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
    printf("Auto Reconnect is enabled, Reconnecting attempt will start now\n");
  }
  else {
    printf("Auto Reconnect not enabled. Starting manual reconnect...\n");
    rc = aws_iot_mqtt_attempt_reconnect(p_client);
    if(NETWORK_RECONNECTED == rc) {
      printf("Manual Reconnect Successful\n");
    } else {
      printf("Manual Reconnect Failed - %d\n", rc);
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
      printf("Error(%d): aws_iot_mqtt_init failed\n", rc);
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

    printf("Connecting...\n");
    rc = aws_iot_mqtt_connect(&client, &connect_param);
    if(SUCCESS != rc) {
      printf("Error(%d): connecting to %s:%d\n",
        rc,
        mqtt_param.pHostURL,
        mqtt_param.port);
      r = false;
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
      IOT_ERROR("Error(%d): Unable to set Auto Reconnect to true", rc);
      r = false;
    }
  }

  return r;
}

bool
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
      subscribe_callback,
      NULL);
  }

  if(SUCCESS != rc) {
    printf("Error(%d): failed to subscribe\n", rc);
    r = false;
  }

  printf("listening to %s\n", topic);

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

  if (r) {
    printf("%s\n%s\n%s\n", argv[1], argv[2], argv[3]);
    r = _aws_connect(argv[1], argv[2], argv[3]);
  }

  if (r) {
    r = _aws_listen(AWS_TOPIC_NAME);
  }

  return (r) ? 0 : 1;
}
