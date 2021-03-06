/*
 * 黑灯测试程序
 * mcu：mega8l
 * 外设：nrf24l01，红外接收头（特定的型号，主要参数是gap时长要短）
 */
#include <SPI.h>
#include <NRFLite.h>

//RF24
const uint8_t RADIO_ID = 1;                 //路数
const uint8_t TO_RADIO_ID = 0;              //目标路由的地址
const uint8_t CE_PIN = 9;
const uint8_t CSN_PIN = 10;

struct RadioPacket {
    uint8_t fromRadioId;
    uint32_t triggerTime;
};

NRFLite radio;
RadioPacket radioPacket;

//黑灯
const uint8_t LEP_PIN = 8;              //指示灯
const uint8_t IR_RECV_PIN = 7;          //黑灯的信号引脚，digital输入
bool hasRestarted = false;              //触发状态标志，遮挡黑灯之后，需要接收一定时长的pwm来重新开始，避免一个长gap导致连续触发多次
uint8_t restartKey = 0;                 //测到3个pwm表示重新开始
uint32_t restartKeyTime;                //用于监测重新开始
uint32_t gapStartTime;                  //gap开始的时间，用来判断是否遮挡黑灯
uint32_t PWMStartTime;                  //PWM开始的时间
uint32_t PWMElapsedTime;                //PWM的持续时间

void setup() {
//    Serial.begin(115200);
    pinMode(IR_RECV_PIN, INPUT);        //黑灯的out使用digitalRead引脚
    
    pinMode(LEP_PIN, OUTPUT);           //开机指示灯闪烁3次后熄灭
    blinkLed(LEP_PIN, 500, 300, 3);     //被载波照射时亮，遮挡时灭

    //初始化RF24
    if (!radio.init(RADIO_ID, CE_PIN, CSN_PIN)) {
        while (1);
    }
    radioPacket.fromRadioId = RADIO_ID;
}

void loop() {
    detectTrigger();
    detectRestart();
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
            if (micros() - gapStartTime > 8000 && hasRestarted) {//载波自己的gap时长为26us * 260 = 6760us，高电平时间大于8000us则表示遮挡
                digitalWrite(LEP_PIN, LOW);         //遮挡时指示灯灭
//                Serial.println("trigger");
                sendPacket();
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
            if (restartKey == 3 && micros() - restartKeyTime < 18000) {             //在18000us内测到3个pwm表示重新开始，进一步排除环境干扰
                hasRestarted = true;                                                // 300+300+40个周期 = 16842us < 18000us
                restartKey = 0;
                digitalWrite(LEP_PIN, HIGH);        //指示灯亮
            }
        }
    }
}

/*
 * 发送消息
 */
void sendPacket() {
    radioPacket.triggerTime = millis();
    radio.send(TO_RADIO_ID, &radioPacket, sizeof(radioPacket));
}

//指示灯闪烁
void blinkLed(uint8_t led, uint16_t onTime, uint16_t offTime, uint8_t times) {
    for (uint8_t i = times; i > 0; i --) {
        digitalWrite(led, HIGH);
        delay(onTime);
        digitalWrite(led, LOW);
        delay(offTime);
    }
}
