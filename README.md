# ESP_MQTT_DEVICE
基于ESP的MQTT客户端，通过JSON进行设备控制和数据上传

## 来源 ##
详情请参考[MQTT客户端](https://github.com/Shaopus/ESP_MQTT)，再此基础主要增加了JSON，可以进行设备控制和数据(温湿度)上传。

## 控制 ##
硬件上可以ESP的GPIO0作为按键对GPIO12进行控制，串口上查看数据；

- GPIO0配置成外部中断，下降沿触发
- GPIO12可作为LED，Relay。。。

软件上可以新建一个MQTT客户端，订阅ESP发布的主题(/Shao/<chip-ID>)，通过JSON进行推送控制以及温湿度的查看。

- [网页版MQTT客户端](http://m2m.demos.ibm.com/mqttclient/)
- 服务器地址：192.168.0.1，端口：8083([如何新建一个测试服务器](http://emqtt.com/))
- topic(/Shao/<chip-ID>)可通过192.168.0.164:18083查看
- JSON语句({"switch":"off"} or {"switch":"on})

## 编译 ##

使用./gen_misc.sh,编译步骤如下：

- STEP 1：1(new boot)
- STEP 2：1(user1.bin)
- STEP 3：2(40Mhz)
- STEP 4：2(DIO)
- STEP 5：6(4096KB(1024+1024))
