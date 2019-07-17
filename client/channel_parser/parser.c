#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_common.h"
#include "parser_utils.h"
#include "pub_utils.h"
#include "mosquitto_internal.h"




int mid_sent = 0;
int status = STATUS_CONNECTING;
bool process_messages = true;
int msg_count = 0;
int last_mid = 0;

int main(int argc, char *argv[]) {
  mosq_config_t cfg;
  struct mosquitto *mosq = NULL;
  //struct mosquitto *mosq = NULL;
  mosq_retcode_t ret;

  // Initialize `mosq` and `cfg`
  parser_config_init(&mosq, &cfg);

  // Set callback functions
  if (cfg.general_config->debug) {
    mosquitto_log_callback_set(mosq, log_callback_parser_func);
    mosquitto_subscribe_callback_set(mosq, subscribe_callback_parser_func);
  }
  mosquitto_connect_v5_callback_set(mosq, connect_callback_parser_func);
  mosquitto_disconnect_v5_callback_set(mosq, disconnect_callback_parser_func);
  mosquitto_publish_v5_callback_set(mosq, publish_callback_parser_func);
  mosquitto_message_v5_callback_set(mosq, message_callback_parser_func);

  // Set the configures and message for testing
  ret = gossip_channel_setting(&cfg, HOST, TOPIC, TOPIC_RES);
  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
    goto done;
  }

  // Set the message that is going to be sent. This function could be used in the function `parser_loop`
  // We just put it here for demostration.
  ret = gossip_message_set(&cfg, MESSAGE);
  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
    goto done;
  }

  mosq->userdata = &cfg;

  // Start listening subscribing topics, once we received a message from the listening topics, we can send corresponding
  // message.
  // if we need to take the above task forever, just put it in a infinit loop.
  ret = parser_loop(mosq, &cfg);
  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
  }
  return ret;

done:
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  mosq_config_cleanup(&cfg);
  pub_shared_cleanup();
  return EXIT_FAILURE;
}
