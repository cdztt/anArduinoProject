/*
 * 路由盒子
 * mcu：mega8l
 * 外设：esp8266，nrf24l01
 */
#include <SPI.h>
#include <NRFLite.h>

//esp
//const String ATE0 = "ATE0\r\n";                                     //关闭AT指令的回显，只显示结果
//const String UART = "AT+UART_CUR=57600,8,1,0,0\r\n";                //esp的波特率
//const String CWMODE = "AT+CWMODE_CUR=1\r\n";                        //设为sta模式
//
//const String CWJAP = "AT+CWJAP_CUR=\"HY50M1\",\"11223344\"\r\n";/*手动修改*/       //连接到平板的热点
//
//const String CIPSTA = "AT+CIPSTA_CUR=\"192.168.43.2\"\r\n";         //设置本地ip，平板会连接这个ip
//const String CIPMUX = "AT+CIPMUX=1\r\n";                            //设为多连接模式，只有多连接才能创建server
//const String CIPSERVER = "AT+CIPSERVER=1,8080\r\n";                 //创建server，端口为8080
//const String CIPSTO = "AT+CIPSTO=0\r\n";                            //设置连接超时时间，为0表示永不超时

const String CIPSENDEX = "AT+CIPSENDEX=0,32\r\n";                   //发送消息，遇到\0或达到32个字节时结束

//以下时间单位：毫秒
const uint32_t SHORT_TIME = 20;             //这个时间关系到同时过线时的并发处理，数值越小并发性越好，5毫秒都可以
                                            //波特率影响到写at的速度，如果用115200的波特率，20毫秒就不够了，会出现字符的混乱
                                            //在57600的波特率下，20毫秒是可以的，以上所说的是通过wifi给client发消息的速度
                                            //在接收wifi数据方面，mega8l的频率是无法应对115200的，会出现字符的混乱
                                            //综合下来，选择57600，既有足够的发送速度，也能保证接收数据的正确
const uint32_t MIDDLE_TIME = 1000;
//const uint32_t LONG_TIME = 6000;

//指示灯
uint8_t LED_PIN = 8;

//rf24
const uint8_t CHANNEL = 113;/*手动修改*/     //rf的频道
const uint8_t RADIO_ID = 22;/*手动修改*/ //路由的rf24地址

const uint8_t CE_PIN = 9;
const uint8_t CSN_PIN = 10;

struct RadioPacket {                    //rf24的数据包
    uint8_t leafRadioId;                //节点地址
    uint32_t triggerTime;               //触发时间
};
//接收外设的消息
RadioPacket radioPacket;
//接收平板的消息
RadioPacket clientPacket;

NRFLite radio;

//数据缓存后再发送
const uint8_t MAX_LENGTH = 24;          //这里如果把长度设为32会出问题，单片机存储空间满了，使用F()腾出RAM空间
RadioPacket dataBuffer[MAX_LENGTH];
uint8_t dataLength = 0;

/*
 * 初始化
 */
void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);        //指示灯常亮
    //电源开启，mc和esp同时上电，mc需要等待esp准备好才能写入at
    //下面这个延迟是必需的
    delay(MIDDLE_TIME);
    
    //初始化esp
    Serial.begin(115200);           //esp的波特率默认为115200bps
    changeBaud();    
    Serial.begin(57600);
    initEsp();
    
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
    //接收rf24的消息
    while (radio.hasData()) {                      //有包时接收数据放入数组
        radio.readData(&radioPacket);

        if (dataLength < MAX_LENGTH) {
            dataBuffer[dataLength] = radioPacket;
            dataLength ++;
        }
    }
    if (dataLength > 0) {
        String msg = "";
        msg += dataBuffer[dataLength - 1].leafRadioId;
        msg += dataBuffer[dataLength - 1].triggerTime;
        msg += "\\0\r\n";                               //注意是\\0，并不是终止符\0
        sendToClient(msg);                              //发送消息给平板
        dataLength --;
    }
    //接收平板发送的消息
    if (Serial.available()) {
        int buf = 0;
        int mark = 0;           //检测冒号
        int target = 0;         //目标地址
        int action = 0;         //动作（指令）
        uint8_t index = 0;
        while (Serial.available()) {
            buf = Serial.read();
            delay(2);//必须要至少2毫秒的延迟（1毫秒都不行），才能进行index的相等性判断，否则变量没有及时更新导致相等性判断错误
            if (index == 10) {
                mark = buf;
            }
            else if (index == 11) {
                target = buf - 48;
            }
            else if (index == 12) {
                action = buf - 48;
            }
            index ++;
        }
        //当mark是冒号，target是数字1到8时，表示接收到平板传来的消息
        if (mark == 58 && target >= 1 && target <= 8) {
            uint8_t targetId = (uint8_t)target;
            clientPacket.leafRadioId = targetId;
            clientPacket.triggerTime = (uint32_t)action;
            
            if (RADIO_ID > 21) {
                sendPacket(RADIO_ID - 1, clientPacket);
            }
            else {
                sendPacket(targetId, clientPacket);
            }
        }
    }
}

/*
 * 给esp发送at指令
 */
void sendATcmd(String cmd, uint32_t waitTime) {
    Serial.print(cmd);
    uint32_t endTime = millis() + waitTime;
    while(millis() < endTime);
}
/*
 * 关闭回显，改变波特率
 */
void changeBaud() {
    sendATcmd(F("ATE0\r\n"), MIDDLE_TIME);
    sendATcmd(F("AT+UART_CUR=57600,8,1,0,0\r\n"), MIDDLE_TIME);
}
/*
 * esp初始化
 */
//void initEsp() {
//    sendATcmd(F("AT+CWMODE_CUR=1\r\n"), MIDDLE_TIME);
//    sendATcmd(F("AT+CWJAP_CUR=\"HY50M3\",\"11223344\"\r\n"), LONG_TIME);
//    sendATcmd(F("AT+CIPSTA_CUR=\"192.168.43.2\"\r\n"), MIDDLE_TIME);
//    sendATcmd(F("AT+CIPMUX=1\r\n"), MIDDLE_TIME);
//    sendATcmd(F("AT+CIPSERVER=1,8080\r\n"), MIDDLE_TIME);
//    sendATcmd(F("AT+CIPSTO=0\r\n"), MIDDLE_TIME);
//}
void initEsp() {
    sendATcmd(F("AT+CWMODE_CUR=2\r\n"), MIDDLE_TIME);//ap模式
//    sendATcmd(F("AT+RST\r\n"), MIDDLE_TIME);

    sendATcmd(F("AT+CWSAP_CUR=\"hy50m4\",\"hy12345678\",1,3,1\r\n"), MIDDLE_TIME);//最后3个数字：channel=1,encryption=3=WPA2_PSK,max conn=1
    
    sendATcmd(F("AT+CIPMUX=1\r\n"), MIDDLE_TIME);    
    sendATcmd(F("AT+CIPSERVER=1\r\n"), MIDDLE_TIME);//默认端口333
    sendATcmd(F("AT+CIPSTO=7200\r\n"), MIDDLE_TIME);//7200秒没动作自动断开
}
/*
 * esp发送消息给平板
 */
void sendToClient(String msg) {
    sendATcmd(CIPSENDEX, SHORT_TIME);
    sendATcmd(msg, SHORT_TIME);
}
/*
 * 发送消息，只发一次
 */
void sendPacket(uint8_t addr, RadioPacket packet) {
    radio.send(addr, &packet, sizeof(packet));
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
