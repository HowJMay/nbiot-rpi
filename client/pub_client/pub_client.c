#include <errno.h>
#include <string.h>
#include "client_common.h"
#include "pub_utils.h"

int main(int argc, char *argv[]) {
  struct mosquitto *mosq = NULL;
  mosq_config_t cfg;
  mosq_retcode_t ret;

  init_mosq_config(&cfg, client_pub);
  mosquitto_lib_init();

  // set the configures and message for testing
  cfg.general_config->host = strdup(HOST);
  if (cfg_add_topic(&cfg, client_pub, TOPIC)) {
    return EXIT_FAILURE;
  }
  cfg.pub_config->message = strdup(MESSAGE);
  cfg.pub_config->msglen = strlen(cfg.pub_config->message);
  cfg.pub_config->pub_mode = MSGMODE_CMD;

  if (generate_client_id(&cfg)) {
    goto cleanup;
  }

  init_check_error(&cfg, client_pub);

  mosq = mosquitto_new(cfg.general_config->id, true, NULL);
  if (!mosq) {
    switch (errno) {
      case ENOMEM:
        fprintf(stderr, "Error: Out of memory.\n");
        break;
      case EINVAL:
        fprintf(stderr, "Error: Invalid id.\n");
        break;
    }
    goto cleanup;
  }

  if (mosq_opts_set(mosq, &cfg)) {
    goto cleanup;
  }
  mosquitto_user_data_set(mosq, &cfg);
  if (cfg.general_config->debug) {
    mosquitto_log_callback_set(mosq, log_callback_pub_func);
  }
  mosquitto_connect_v5_callback_set(mosq, connect_callback_pub_func);
  mosquitto_disconnect_v5_callback_set(mosq, disconnect_callback_pub_func);
  mosquitto_publish_v5_callback_set(mosq, publish_callback_pub_func);

  ret = mosq_client_connect(mosq, &cfg);
  if (ret) {
    goto cleanup;
  }

  ret = publish_loop(mosq, &cfg);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  mosq_config_cleanup(&cfg);

  if (ret) {
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(ret));
  }
  return ret;

cleanup:
  mosquitto_lib_cleanup();
  mosq_config_cleanup(&cfg);
  return EXIT_FAILURE;
}
