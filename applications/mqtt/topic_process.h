#ifndef __TOPIC_PROCESS_H_
#define __TOPIC_PROCESS_H_

typedef struct
{
    const char *start;
    int len;
}topic_item_t;

const char* topic_product_id_jetlinks(const char *topicName, int topicLen, topic_item_t *t);
const char* topic_device_id_jetlinks(const char *topicName, int topicLen, topic_item_t *t);
const char* topic_model_act_jetlinks(const char *topicName, int topicLen, topic_item_t *t);
const char* topic_operation_act_jetlinks(const char *topicName, int topicLen, topic_item_t *t);


#endif
