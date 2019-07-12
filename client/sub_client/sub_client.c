#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "client_common.h"
#include "sub_utils.h"

struct mosq_config cfg;
bool process_messages = true;
int msg_count = 0;
struct mosquitto *mosq = NULL;
int last_mid = 0;

int main(int argc, char *argv[]) {
  mosq_retcode_t ret = MOSQ_ERR_SUCCESS;
  struct sigaction sigact;

  init_mosq_config(&cfg, client_sub);
  mosquitto_lib_init();

  // set the configures and message for testing
  cfg.general_config->host = strdup(HOST);
  if (cfg_add_topic(&cfg, client_sub, TOPIC)) {
    return EXIT_FAILURE;
  }

  if (cfg.sub_config->no_retain && cfg.sub_config->retained_only) {
    fprintf(stderr, "\nError: Combining '-R' and '--retained-only' makes no sense.\n");
    goto cleanup;
  }

  if (generate_client_id(&cfg)) {
    goto cleanup;
  }

  mosq = mosquitto_new(cfg.general_config->id, cfg.general_config->clean_session, &cfg);
  if (!mosq) {
    switch (errno) {
      case ENOMEM:
        if (!cfg.general_config->quiet) fprintf(stderr, "Error: Out of memory.\n");
        break;
      case EINVAL:
        if (!cfg.general_config->quiet) fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
        break;
    }
    goto cleanup;
  }

  if (mosq_opts_set(mosq, &cfg)) {
    goto cleanup;
  }

  if (cfg.general_config->debug) {
    mosquitto_log_callback_set(mosq, log_callback_sub_func);
    mosquitto_subscribe_callback_set(mosq, subscribe_callback_sub_func);
  }
  mosquitto_connect_v5_callback_set(mosq, connect_callback_sub_func);
  mosquitto_message_v5_callback_set(mosq, message_callback_sub_func);

  ret = mosq_client_connect(mosq, &cfg);
  if (ret) {
    goto cleanup;
  }

  sigact.sa_handler = signal_handler_func;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;

  if (sigaction(SIGALRM, &sigact, NULL) == -1) {
    perror("sigaction");
    goto cleanup;
  }

  if (cfg.sub_config->timeout) {
    alarm(cfg.sub_config->timeout);
  }

  ret = mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();

  if (cfg.sub_config->msg_count > 0 && ret == MOSQ_ERR_NO_CONN) {
    ret = MOSQ_ERR_SUCCESS;
  }
  mosq_config_cleanup(&cfg);

  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
  }

cleanup:
  mosquitto_lib_cleanup();
  mosq_config_cleanup(&cfg);
  return ret;
}
