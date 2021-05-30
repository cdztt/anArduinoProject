/*
 * 黑灯测试程序，防犯规
 * mcu：mega8l
 * 外设：nrf24l01，红外接收头（特定的型号，主要参数是gap时长要短）
 */
#include <SPI.h>
#include <NRFLite.h>

//RF24
const uint8_t CHANNEL = 113;/*手动修改*/         //rf的频道
const uint8_t RADIO_ID = 0;/*手动修改*/          //路数

const uint8_t CE_PIN = 9;
const uint8_t CSN_PIN = 10;
const uint8_t TO_RADIO_ID = 22;                 //直接发给路由

struct RadioPacket {
    uint8_t leafRadioId;
    uint32_t triggerTime;
};
RadioPacket radioPacket;
//RadioPacket clientPacket;

NRFLite radio;

//黑灯
const uint8_t LED_PIN = 8;//指示灯
const uint8_t IR_RECV_PIN = 7;          //黑灯的信号引脚，digital输入
bool hasRestarted = false;              //触发状态标志，遮挡黑灯之后，需要接收一定时长的pwm来重新开始，避免一个长gap导致连续触发多次
uint8_t restartKey = 0;                 //测到3个pwm表示重新开始
uint32_t restartKeyTime;                //用于监测重新开始
uint32_t gapStartTime;                  //gap开始的时间，用来判断是否遮挡黑灯
uint32_t PWMStartTime;                  //PWM开始的时间
uint32_t PWMElapsedTime;                //PWM的持续时间

uint32_t lastSendTime = 0;              //上一次发送的时间，用来控制发送频率，因为一rf24跟不上触发的频率，二在业务逻辑上也不需要连续发送
const uint32_t SEND_INTERVAL = 1000;    //单位毫秒，连续两次发送之间的间隔

const uint8_t SEND_TIMES = 5;           //递归发送的次数，否则会一直递归下去

/*
 * 初始化
 */
void setup() {
//    Serial.begin(57600);
    pinMode(IR_RECV_PIN, INPUT);        //黑灯的out使用digitalRead引脚
        
    pinMode(LED_PIN, OUTPUT);           //开机指示灯闪烁
    blinkLed(LED_PIN, 300, 300, 3);

    //初始化RF24
    if (!radio.init(RADIO_ID, CE_PIN, CSN_PIN, NRFLite::BITRATE2MBPS, CHANNEL)) {
        while (1) {
            blinkLed(LED_PIN, 200, 200, 1);     //如果rf24初始化失败则快速闪烁
        }
    }
    //初始化radioPacket
    radioPacket.leafRadioId = RADIO_ID;
    radioPacket.triggerTime = 10;           //初始触发时间为10
    
    //随机数种子
    //uint32_t seed = millis();
    //randomSeed(seed);
}

/*
 * 主循环
 */
void loop() {
    detectTrigger();        //监测触发
    detectRestart();        //监测复位
    
    //detectRecvMsg();
}

/*
 * 监测触发
 * 载波的gap（为了让黑灯恢复，如果一直发送pwm则黑灯会无效）时长近似为7000us
 * 如果超过这个时长，则认为黑灯被遮挡
 */
void detectTrigger() {
    if (digitalRead(IR_RECV_PIN)) {         //高电平表示gap
        gapStartTime = micros();
        while (digitalRead(IR_RECV_PIN)) {
            if (micros() - gapStartTime > 6800 && hasRestarted) {//载波自己的gap时长为26us * 220 = 5786us，高电平时间大于6800us则表示遮挡
                uint32_t triggerTime = millis();
                digitalWrite(LED_PIN, HIGH);         //遮挡时指示灯亮

                if (triggerTime - lastSendTime > SEND_INTERVAL) {       //大于间隔才发送
                    sendPacketRecursive(TO_RADIO_ID, radioPacket, SEND_TIMES);
                    lastSendTime = triggerTime;                         //重新赋值lastSendTime
                }
                hasRestarted = false;
            }
        }
    }
}

/*
 * 监测重新开始
 * 在38khz的pwm期间，黑灯一直输出低电平
 */
void detectRestart() {
    if (!digitalRead(IR_RECV_PIN)) {        //低电平表示pwm
        PWMStartTime = micros();
        if (restartKey == 0) {
            restartKeyTime = PWMStartTime;
        }
        while (!digitalRead(IR_RECV_PIN));    //循环等待电平变化
        
        PWMElapsedTime = micros() - PWMStartTime;
        if (PWMElapsedTime > 900 && PWMElapsedTime < 1200 && !hasRestarted) {       //载波的pwm时长为26us * 40 = 1040us
            restartKey ++;                                                          //限制pwm的持续时间在900到1200us之间，以排除环境干扰
            if (restartKey == 3 && micros() - restartKeyTime < 17000) {             //在17000us内测到3个pwm表示重新开始，进一步排除环境干扰
                hasRestarted = true;                                                // 260+260+40个周期 = 14728us < 17000us
                restartKey = 0;
                digitalWrite(LED_PIN, LOW);        //指示灯灭
            }
        }
    }
}
/*
 * 如果收到来自平板的消息
 */
//void detectRecvMsg() {
//    if (radio.hasData()) {
//        radio.readData(&clientPacket);
//        if (clientPacket.triggerTime == 1) {
//            radioPacket.triggerTime = millis();//如果指令是1则发送当前时间，否则发送10或触发时间
//        }
//        sendPacketOnce(TO_RADIO_ID, radioPacket);
//    }
//}
/*
 * 发送消息，递归发送，times为递归的次数
 */
//辅助函数
void sendPacketRecursive(uint8_t addr, RadioPacket packet, uint8_t times) {
    if (times > 0) {
        uint8_t flag = sendPacketOnce(addr, packet);
        times --;
        if (!flag) {
            //uint32_t endTime = millis() + random(200);//N毫秒的随机延迟
            //while(millis() < endTime);
            sendPacketRecursive(addr, packet, times);
        }
    }
}
//主函数，第一次和第二次发送不需要随机延迟，后续发送使用辅助函数
//void sendPacket(uint8_t addr, RadioPacket packet, uint8_t times) {
//    uint8_t flag = sendPacketOnce(addr, packet);
//    if (!flag) {
//        uint8_t flag = sendPacketOnce(addr, packet);
//        if (!flag) {
//            sendPacketRecursive(addr, packet, times);
//        }
//    }    
//}
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
