/*
 * 红外白灯发射，38khz载波
 * mcu：mega8L
 */
const uint8_t PWM_IR_PIN = 10;              //使用OC1B引脚
const uint8_t LED_PIN = 8;                  //指示灯
const uint32_t FREQUENCY = 38000;           //载波频率，单位hz
const uint16_t TOP = F_CPU / FREQUENCY / 2; //16位相频修正产生38k载波的top值，F_CPU为8M
const uint16_t OCR1B_CARRIER_ON = TOP / 2;  //载波占空比50%
const uint16_t OCR1B_CARRIER_OFF = 0;       //占空比0%，相当于红外发射灯关闭
uint16_t count = 0;                         //发生溢出中断的次数，用于在信号之间产生间隔，有间隔的信号才能让黑灯正常工作
bool pwmOn = false;                         //载波状态，初始时关闭
const uint16_t ON_CYCLE = 40;               //开启载波的时间为40个周期
const uint16_t OFF_CYCLE = 260;             //关闭载波的时间为260个周期（对于特定型号的黑灯），以产生间隔让黑灯恢复，间隔时间为260 / 38000 = 6.9毫秒

/*
 * T1计时器溢出中断ISR
 */
ISR(TIMER1_OVF_vect) {
    count ++;
    if (count == ON_CYCLE && pwmOn) {         
        OCR1B = OCR1B_CARRIER_OFF;
        pwmOn = false;
        count = 0;
    }
    else if (count == OFF_CYCLE && !pwmOn) {   
        OCR1B = OCR1B_CARRIER_ON;
        pwmOn = true;
        count = 0;
    }
}

void setup() {
    pinMode(PWM_IR_PIN, OUTPUT);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);            //指示灯

    TCCR1A = bit(WGM10) | bit(COM1B1);      //16位相频修正，TOP值为OCR1A
    TCCR1B = bit(WGM13) | bit(CS10);        //无预分频
    TIMSK = bit(TOIE1);                     //开启溢出中断
    
    OCR1A = TOP;                            //OCR1A等于TOP，产生38khz
    OCR1B = OCR1B_CARRIER_OFF;              //初始时红外发射灯关闭
}

void loop() {
    
}
