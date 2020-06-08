#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "topic_process.h"
#include "logger.h"

static const char* _topic_get_item(const char *topicName, int index, const char delimiter, topic_item_t *t)
{
    int i;
    const char *p = topicName;
    char *head, *tail;
    for (i=0; i<=index; i++)
    {
        head = strchr(p, delimiter);
        if (NULL == head)
        {
            dbg(IOT_WARNING, "no '/' found in topic, the topicName is %s", topicName);
            return NULL;
        }
        else if (i == index)
        {
            tail = strchr(head+1, delimiter);
            if (NULL == tail)
            {
                t->start = head+1;
                t->len = strlen(t->start);
            }
            else
            {
                tail -= 1;
                if (head == tail) // "//"
                {
                    dbg(IOT_WARNING, "continue // in topic");
                    return NULL;
                }
                else
                {
                    t->start = head+1;
                    t->len = tail - head;
                }
            }
            break;
        }
        else
        {
            head += 1;
        }
        p = head;
    }
    return head;
}

const char* topic_product_id_jetlinks(const char *topicName, int topicLen, topic_item_t *t)
{   
    const char delimiter = '/';
    
    if (topicLen == 1)
    {
        dbg(IOT_WARNING, "only 1 char in topic");
        return NULL;
    }
    
    return _topic_get_item(topicName, 0, delimiter, t);
}

const char* topic_device_id_jetlinks(const char *topicName, int topicLen, topic_item_t *t)
{   
    const char delimiter = '/';
    
    if (topicLen == 1)
    {
        dbg(IOT_WARNING, "only 1 char in topic");
        return NULL;
    }
    
    return _topic_get_item(topicName, 1, delimiter, t);
}

/**
 * 
 * @brief 获取productId和deviceId之后的具体操作，具体操作有properties\function\event
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
const char* topic_model_act_jetlinks(const char *topicName, int topicLen, topic_item_t *t)
{   
    const char delimiter = '/';
    
    if (topicLen == 1)
    {
        dbg(IOT_WARNING, "only 1 char in topic");
        return NULL;
    }
    
    return _topic_get_item(topicName, 2, delimiter, t);
}

/**
 * 
 * @brief 获取properties\function\event后面的具体动作
 * 
 * @param[in]   @n 
 * @param[out]  @n 
 * @return 
 */
const char* topic_operation_act_jetlinks(const char *topicName, int topicLen, topic_item_t *t)
{   
    const char delimiter = '/';
    
    if (topicLen == 1)
    {
        dbg(IOT_WARNING, "only 1 char in topic");
        return NULL;
    }
    
    return _topic_get_item(topicName, 1, delimiter, t);
}




