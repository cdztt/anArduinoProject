/*
 * 中继
 * mcu：mega8l
 * 外设：nrf24l01
 */
#include <SPI.h>
#include <NRFLite.h>

uint8_t LED_PIN = 8;//指示灯

//rf24
//中继地址依次为21、22、23……最后一个含esp01模块
const uint8_t CHANNEL = 73;/*手动修改*/     //rf的频道
const uint8_t RADIO_ID = 21;/*手动修改*/    //本机地址

const uint8_t CE_PIN = 9;
const uint8_t CSN_PIN = 10;

struct RadioPacket {                    //rf24的数据包
    uint8_t leafRadioId;                //节点地址
    uint32_t triggerTime;               //触发时间
};
RadioPacket radioPacket;

NRFLite radio;

//数据缓存后再发送
const uint8_t MAX_LENGTH = 64;
RadioPacket dataBuffer[MAX_LENGTH];
uint8_t dataLength = 0;

/*
 * 初始化
 */
void setup() {
//    Serial.begin(57600);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);        //指示灯常亮
    //delay(100);
    
    //初始化rf24
    if (!radio.init(RADIO_ID, CE_PIN, CSN_PIN, NRFLite::BITRATE2MBPS, CHANNEL)) {
        while (1) {
            blinkLed(LED_PIN, 200, 200, 1);     //如果rf24初始化失败则快速闪烁
        }
    }
}

/*
 * 主循环
 */
void loop() {    
    while (radio.hasData()) {                   //有包时接收数据放入数组
        radio.readData(&radioPacket);

        if (dataLength < MAX_LENGTH) {
            dataBuffer[dataLength] = radioPacket;
            dataLength ++;
        }
    }    
    if (dataLength > 0) {                       //没有时发送数组里的数据
        RadioPacket packet = dataBuffer[dataLength - 1];
        uint8_t flag = 0;
        
        //如果是平板的消息，平板的消息triggerTime < 10        
        if (packet.triggerTime < 10) {
            if (RADIO_ID > 21) {
                flag = sendPacketOnce(RADIO_ID - 1, packet);
            }
            else {
                flag = sendPacketOnce(packet.leafRadioId, packet);
            }
//            Serial.println("to Sensor");
        }
        //如果是外设的消息
        else {
            flag = sendPacketOnce(RADIO_ID + 1, packet);
//            Serial.println("to App");
        }
        //发送成功后
        if (flag) {
            dataLength --;
        }        
    }
}

/*
 * 发送消息，只发一次
 */
uint8_t sendPacketOnce(uint8_t addr, RadioPacket packet) {
    return radio.send(addr, &packet, sizeof(packet));
}
/*
 * 指示灯闪烁
 */
void blinkLed(uint8_t led, uint16_t onTime, uint16_t offTime, uint8_t times) {
    for (uint8_t i = times; i > 0; i --) {
        digitalWrite(led, LOW);
        delay(offTime);
        digitalWrite(led, HIGH);
        delay(onTime);
    }
}
