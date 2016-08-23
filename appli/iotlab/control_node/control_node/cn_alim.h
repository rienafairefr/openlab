#ifndef IOTLAB_ALIM_H_
#define IOTLAB_ALIM_H_

/** Start the alim library */
void cn_alim_start();

/** Add pre stop and post start commands */
void cn_alim_config(void (*pre_stop_cmd)(), void (*post_start_cmd)());


#endif /* IOTLAB_ALIM_H_ */
