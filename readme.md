### 一个使用`arduino`软件开发的单片机程序
***
#### 说明
1. 使用`atmega8L`单片机（8MHz）。因为arduino官方没有atmega8L的bootloader，所以从网上找了一个，[点此链接](https://www.hackmeister.dk/2011/01/new-bootloader-file-for-atmega8l/)
1. 通信模块使用了`nrf24l01`和`esp8266`，nrf24l01使用了库[NRFLite](https://github.com/dparson55/NRFLite)
1. 此程序应用在了某县某年某考试的短跑项目上
#### 原理
使用38khz的红外线作为信号源，使用红外接收管检测信号中断（被遮挡）的状态，以此检测人体通过跑步终点，触发信号传给终端，终端显示跑步成绩


P.S. 对应的用户端app见另一个仓库`aUniappProject`
