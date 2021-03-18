/*
 * 路由盒子
 * mcu：mega8l
 * 外设：esp8266，nrf24l01
 */
#include <SPI.h>
#include <NRFLite.h>

//esp
const String CWMODE = "AT+CWMODE_DEF=1\r\n";                        //设为sta模式
const String CWJAP = "AT+CWJAP_DEF=\"A12\",\"11223344\"\r\n";       //连接到平板的热点
const String CIPSTA = "AT+CIPSTA_DEF=\"192.168.43.2\"\r\n";         //设置本地ip，平板会连接这个ip
const String CIPMUX = "AT+CIPMUX=1\r\n";                            //设为多连接模式，只有多连接才能创建server
const String CIPSERVER = "AT+CIPSERVER=1,8080\r\n";                 //创建server，端口为8080
const String CIPSTO = "AT+CIPSTO=0\r\n";                            //设置连接超时时间，为0表示永不超时

const String CIPSENDEX = "AT+CIPSENDEX=0,32\r\n";                   //发送消息，遇到\0或达到32个字节时结束

const uint32_t SHORT_TIME = 250;        //单位：毫秒
const uint32_t MIDDLE_TIME = 1000;
const uint32_t LONG_TIME = 6000;

uint8_t LED_PIN = 8;                    //指示灯

//rf24
const uint8_t RADIO_ID = 23;             //路由的rf24地址
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
String dataBuffer[MAX_LENGTH];
uint8_t dataLength = 0;


void setup() {
    //esp
    Serial.begin(115200);               //esp的波特率默认为115200bps
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);        //指示灯常亮

    //电源开启，mc和esp同时上电，mc需要等待esp准备好才能写入at
    //下面这个延迟是必需的
    delay(MIDDLE_TIME);
    
    initEsp();      //初始化esp
    
    //初始化rf24
    if (!radio.init(RADIO_ID, CE_PIN, CSN_PIN)) {
        while (1);
    }
}

void loop() {
    if (radio.hasData()) {                      //有包时接收数据放入数组
        radio.readData(&radioPacket);
        
        if (dataLength < MAX_LENGTH) {                    
            String msg = "";
            msg += radioPacket.leafRadioId;
            msg += "L";
            msg += radioPacket.triggerTime;
            msg += "\\0\r\n";                       //注意是\\0，并不是终止符\0
            dataBuffer[dataLength] = msg;
            dataLength ++;
        }        
    }
    else {                                      //没有时发送数组里的数据
        if (dataLength > 0) {
            sendToClient(dataBuffer[dataLength - 1]);
            dataLength --;
        }
    }
}

//给esp发送at指令
void sendATcmd(String cmd, uint32_t waitTime) {
    Serial.print(cmd);
    uint32_t endTime = millis() + waitTime;
    while(millis() < endTime);
}
//esp初始化
void initEsp() {
    sendATcmd(CWMODE, MIDDLE_TIME);
    sendATcmd(CWJAP, LONG_TIME);
    sendATcmd(CIPSTA, MIDDLE_TIME);
    sendATcmd(CIPMUX, MIDDLE_TIME);
    sendATcmd(CIPSERVER, MIDDLE_TIME);
    sendATcmd(CIPSTO, MIDDLE_TIME);
}
//esp发送消息给平板
void sendToClient(String msg) {
    sendATcmd(CIPSENDEX, SHORT_TIME);
    sendATcmd(msg, SHORT_TIME);
}
