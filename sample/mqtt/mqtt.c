
#include "aliyun_iot_platform_memory.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_mqtt_client.h"
#include "aliyun_iot_auth.h"
#include "aliyun_iot_device.h"
#include "aliyun_iot_shadow.h"

//用户需要根据设备信息完善以下宏定义中的四元组内容
#define PRODUCT_KEY         "4eViBFJ2QGH"
#define DEVICE_NAME         "device_xikan_3"
#define DEVICE_ID           "xItJzEDpi6Tv184UIkH7"
#define DEVICE_SECRET       "TR1gdwnRVN0nIRttfeEqmp1v3nUYWHDi"


//以下三个TOPIC的宏定义不需要用户修改，可以直接使用
//IOT HUB为设备建立三个TOPIC：update用于设备发布消息，error用于设备发布错误，get用于订阅消息
#define TOPIC_UPDATE         "/4eViBFJ2QGH/device_xikan_3/update"
#define TOPIC_ERROR          "/4eViBFJ2QGH/device_xikan_3/update/error"
#define TOPIC_GET            "/4eViBFJ2QGH/device_xikan_3/get"

#define MSG_LEN_MAX         (2048)

/**********************************************************************
 *            接收消息的回调函数
 *
 * 说明：当其它设备的消息到达时，此函数将被执行。
 * 注意：此回调函数中用户在做业务处理时不要使用耗时操作，否则会阻塞接收通道
 **********************************************************************/
static void messageArrived(MessageData *md)
{
    //接收消息缓存
    char msg[MSG_LEN_MAX] = {0};

    MQTTMessage *message = md->message;
    if(message->payloadlen > MSG_LEN_MAX - 1)
    {
        ALIOT_LOG_DEBUG("process part of receive message");
        message->payloadlen = MSG_LEN_MAX - 1;
    }

    //复制接收消息到本地缓存
    memcpy(msg,message->payload,message->payloadlen);

    //to-do此处可以增加用户自己业务逻辑，例如：开关灯等操作

    //打印接收消息
    ALIOT_LOG_DEBUG("Message : %s", msg);
}

static void publishComplete(void* context, unsigned int msgId)
{
    ALIOT_LOG_DEBUG("publish message is arrived,id = %d",msgId);
}

static void subAckTimeout(SUBSCRIBE_INFO_S *subInfo)
{
    ALIOT_LOG_DEBUG("msgId = %d,sub ack is timeout",subInfo->msgId);
}


int mqtt_client(unsigned char *msg_buf,unsigned char *msg_readbuf)
{
    int rc = 0, ch = 0, msg_len, cnt;
    MQTTClient_t client;
    aliot_mqtt_param_t initParams;
    char msg_pub[128];

    aliyun_iot_device_init();

    if (0 != aliyun_iot_set_device_info(PRODUCT_KEY, DEVICE_NAME, DEVICE_ID, DEVICE_SECRET))
    {
        ALIOT_LOG_DEBUG("run aliyun_iot_set_device_info() error!");
        return -1;
    }

    if (0 != aliyun_iot_auth(aliyun_iot_get_device_info(), aliyun_iot_get_user_info()))
    {
        ALIOT_LOG_DEBUG("run aliyun_iot_auth() error!");
        return -1;
    }

    memset(&client, 0x0, sizeof(client));
    memset(&initParams, 0x0, sizeof(initParams));

    initParams.mqttCommandTimeout_ms = 2000;
    initParams.pReadBuf = msg_readbuf;
    initParams.readBufSize = MSG_LEN_MAX;
    initParams.pWriteBuf = msg_buf;
    initParams.writeBufSize = MSG_LEN_MAX;
    initParams.disconnectHandler = NULL;
    initParams.disconnectHandlerData = (void*) &client;
    initParams.deliveryCompleteFun = publishComplete;
    initParams.subAckTimeOutFun = subAckTimeout;

    initParams.cleansession      = 0;
    initParams.MQTTVersion       = 4;
    initParams.keepAliveInterval = 180;
    initParams.willFlag          = 0;

    rc = aliyun_iot_mqtt_init(&client, &initParams, aliyun_iot_get_user_info());
    if (0 != rc)
    {
        ALIOT_LOG_DEBUG("aliyun_iot_mqtt_init failed ret = %d", rc);
        return rc;
    }

    rc = aliyun_iot_mqtt_connect(&client);
    if (0 != rc)
    {
        aliyun_iot_mqtt_release(&client);
        ALIOT_LOG_DEBUG("ali_iot_mqtt_connect failed ret = %d", rc);
        return rc;
    }

    rc = aliyun_iot_mqtt_subscribe(&client, TOPIC_GET, QOS0, messageArrived);
    if (0 != rc)
    {
        aliyun_iot_mqtt_release(&client);
        ALIOT_LOG_DEBUG("ali_iot_mqtt_subscribe failed ret = %d", rc);
        return rc;
    }


    MQTTMessage message;
    memset(&message, 0x0, sizeof(message));

    strcpy(msg_pub, "message: hello! start!");

    message.qos        = QOS1;
    message.retained   = FALSE_IOT;
    message.dup        = FALSE_IOT;
    message.payload    = (void *)msg_pub;
    message.payloadlen = strlen(msg_pub);
    message.id         = 0;

    rc = aliyun_iot_mqtt_publish(&client, TOPIC_UPDATE, &message);
    if (SUCCESS_RETURN != rc)
    {
        aliyun_iot_mqtt_release(&client);
        ALIOT_LOG_DEBUG("aliyun_iot_mqtt_publish failed ret = %d", rc);
        return rc;
    }

    cnt = 0;
    do
    {
        ++cnt;
        msg_len = snprintf(msg_pub, sizeof(msg_pub), "message: hello, %d!", cnt);
        if (msg_len < 0) {
            ALIOT_LOG_DEBUG("Error occur! Exit program");
            break;
        }

        message.payload = (void *)msg_pub;
        message.payloadlen = msg_len;

        aliyun_iot_mqtt_yield(&client, 500);
        //rc = aliyun_iot_mqtt_publish(&client, TOPIC_UPDATE, &message);
    } while (ch != 'Q' && ch != 'q');

    aliyun_iot_mqtt_release(&client);
    aliyun_iot_pthread_taskdelay(200);
    return 0;
}



int main()
{
    unsigned char *msg_buf = (unsigned char *)aliyun_iot_memory_malloc(MSG_LEN_MAX);
    unsigned char *msg_readbuf = (unsigned char *)aliyun_iot_memory_malloc(MSG_LEN_MAX);

    mqtt_client(msg_buf, msg_readbuf);

    aliyun_iot_memory_free(msg_buf);
    aliyun_iot_memory_free(msg_readbuf);

    ALIOT_LOG_DEBUG("out of demo!");

    return 0;
}
