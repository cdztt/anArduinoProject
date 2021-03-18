/*
 * 中继
 * mcu：mega8l
 * 外设：nrf24l01
 */
#include <SPI.h>
#include <NRFLite.h>

uint8_t LED_PIN = 8;                    //指示灯

//rf24
//中继地址依次为21、22、23……最后一个含esp01模块
const uint8_t RADIO_ID = 22;            //本机地址
const uint8_t TO_RADIO_ID = 23;         //目标地址
const uint8_t CE_PIN = 9;
const uint8_t CSN_PIN = 10;

struct RadioPacket {                    //rf24的数据包
    uint8_t leafRadioId;                //节点地址
    uint32_t triggerTime;               //触发时间
};
RadioPacket radioPacket;

NRFLite radio;

//数据缓存后再发送
const uint8_t MAX_LENGTH = 16;
RadioPacket dataBuffer[MAX_LENGTH];
uint8_t dataLength = 0;


void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);        //指示灯常亮
    //初始化rf24
    if (!radio.init(RADIO_ID, CE_PIN, CSN_PIN)) {
        while (1);
    }
}

void loop() {
    if (radio.hasData()) {                      //有包时接收数据放入数组
        radio.readData(&radioPacket);
        
        if (dataLength < MAX_LENGTH) {
            dataBuffer[dataLength] = radioPacket;
            dataLength ++;
        }        
    }
    else {                                      //没有时发送数组里的数据
        if (dataLength > 0) {
            radio.send(TO_RADIO_ID, &dataBuffer[dataLength - 1], sizeof(radioPacket));     //中继不做任何处理，透传
            dataLength --;
        }
    }
}
