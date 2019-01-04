//引用相关库文件
#include <Microduino_Key.h>
#include <U8glib.h>
#include <SD.h>
#include <Microduino_AudioPro.h>

//定义引脚
#define micPin A0//MIC引脚
#define LED 8//LED引脚
#define keyPin 4//碰撞开关引脚

//创建对象
AudioPro_FilePlayer musicPlayer=AudioPro_FilePlayer(SD);//创建音乐播放器对象,并定义在引脚
DigitalKey keyDigital=DigitalKey(keyPin);//创建按键对象,并定义在引脚4
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);//创建OLED对象,并定义在引脚12

//字体大小定义
#define setFont_L u8g.setFont(u8g_font_timB18)
#define setFont_M u8g.setFont(u8g_font_timB14)
#define setFont_S u8g.setFont(u8g_font_timB10)//u8glib.h官网可查看所有可用的字体支持

//定义OLED属性
#define INTERVAL_OLED 200 //定义刷新率
double db = 0.0; //显示在 OLED 上的当前分贝数并初始化
double recodeDB = 0.0; //显示在OLED上的最高的分贝数(纪录值)

//定义MIC属性
#define voice 30 //噪声阈值

//定义音乐播放对象属性
uint8_t musicNum=1;
uint8_t fileNum=0;

//应用millis函数可获取机器运行的时间长度，单位ms。系统最长的记录时间为9小时22分，如果超出时间将从0开始。
//可以逻辑为,当if(millis()-OLEDShowTime>INTERVAL_OLED) OLEDShowTime=millis()
//则立即重置计时器
//if (OLEDShowTime > millis()) OLEDShowTime = millis();
//可能会出现在到达最长记录时间之后,OLEDShowTimes是一个很大的数,而millis()返回的是一个很小的值,所以要重置
unsigned long OLEDShowTime = millis();

//控制变量
uint8_t sign=0;


void setup(){
    Serial.begin(115200);//设置波特率
    Serial.println(F("Arduino init successful"));
    pinMode(SD_PIN_SEL,HIGH);//设置7号引脚为高电平,给SD卡上电
    digitalWrite(SD_PIN_SEL,HIGH);//维持高电平
    delay(500);//等待SD卡上电,初始化完成
    modulesCheck();//模块检查
    pinMode(micPin, INPUT);//定义声音传感器 mic 所接引脚 micPin(A0)为输入模式
    pinMode(LED, OUTPUT);//定义LED灯引脚,输出模式
    musicPlayer.useInterrupt(VS1053_PIN_DREQ);//定义音频播放控制方法,采用中断方法暂停曲目播放
    loadSongs();
}

void loop(){
    db=getDB();
    analysisDB(db);
    updateOLED();
    if(sign=1){
        key_status();
    }
    if(sign=2){
      if(db>voice){
        if(musicPlayer.paused()){
          musicPlayer.pausePlaying(false); 
        }
        sign=1;
      }
    }
}


//检测模块是否正常的方法
void modulesCheck(){
    while(!musicPlayer.begin()){
        Serial.println(F("music player init failed"));
        delay(80);
    }
    Serial.println(F("music player init successful"));
    while(SD.begin(SD_PIN_SEL)){
        Serial.println(F("SD module init failed"));
        delay(80);
    }
    Serial.println(F("SD module init successful"));
}


//音乐播放器乐曲读取
void loadSongs(){
    musicPlayer.setVolume(50);
    fileNum=musicPlayer.getMusicNum();
    Serial.print(F("Total"));
    Serial.print(fileNum);
    Serial.println(F("songs"));
    for(uint8_t a=0;a<fileNum;a++){//循环取出曲目名称
      Serial.print(F("\tFile["));
      Serial.print(a);
      Serial.print(F("]:"));
      Serial.println(musicPlayer.getMusicName(a));
    }

}
//通过麦克风电压获取分贝数
double getDB(){
    double voice_data = analogRead(micPin);//读取接在 micPin(A0)管脚上声音传感器数值(0~1023 之间)
    // voice_data = map(voice_data, 0, 1023, 0, 5);//将读取数值 0~1023 转换为电压 0~5V。 (通过这种方式可以快速将数值做转换,但是数值只有 0,1,2,3,4 不够精确)
    voice_data = voice_data / 204.6; //将读取数值 0~1023 转换为电压 0~5V (这种方法直接做除法 1023/5=204.6)
    double db = (20. * log(10)) * (voice_data / 1.0);//将电压转换为分贝:20lg(电压值)
    if (db > recodeDB) {//recodeDB 存放最大分贝值,如果数值破纪录则覆盖原记录
        recodeDB = db;
    }
    return db;//返回当前分贝值
}
//分析麦克风电压执行其他操作
void analysisDB(double db){
    if(db>voice){
        digitalWrite(LED,HIGH);
        if(musicPlayer.stopped()){
            playNum(musicNum-1);
            sign=1;
            key_status();
        }
    }else{
        digitalWrite(LED,LOW);
    }
}

//播放乐曲
void playNum(uint8_t num){
    if(num>fileNum-1){
        return;
    }
    if(!musicPlayer.stopped()){
        musicPlayer.stopPlaying();
    }
    musicPlayer.flushCancel(both);
    String song_name=musicPlayer.getMusicName(num);//根据代码,当获取歌曲名之后,会播放被获取歌名的歌曲
    Serial.print(F("Playing:"));
    if(!musicPlayer.playMP3(song_name)){
        Serial.println(F("ERROR"));
    }else{
        Serial.print(F("File:"));
        Serial.println(song_name);
    }
}

//监听碰撞开关
void key_status() {
  switch (keyDigital.readEvent()) {
    case SHORT_PRESS:
      if (musicPlayer.stopped()) {
        Serial.println(F("Playing"));
      }else if(!musicPlayer.paused()){
          Serial.println(F("Paused"));
          musicPlayer.pausePlaying(true);
          sign=2;
      }
      else{
        Serial.println(F("Resumed"));
        musicPlayer.pausePlaying(false);
        sign=1;
      }
      break;
    case LONG_PRESS:
      musicNum++;
      if (musicNum > fileNum) {
        musicNum = 1;
      }
      playNum(musicNum - 1);
      break;
  }
  delay(50);
}
 
//OLED刷新/显示函数
void updateOLED() {//刷新 OLED
  if (OLEDShowTime > millis()) OLEDShowTime = millis();//通常 OLEDShowTime 都应<定时器millis(),防止出错做对比
  if (millis() - OLEDShowTime > INTERVAL_OLED) {//OLED 每 INTERVAL_OLED(200)毫秒刷新一次
    OLEDShow(); //调用显示库
    OLEDShowTime = millis();//记录当前定时器 millis()时间。重新开始计算时间
  }
}
void OLEDShow() {//OLED 显示函数
  u8g.firstPage();
  do {
    setFont_S;//以小字体,在(x=0,y=20)坐标显示“record:”以及最大分贝值
    u8g.setPrintPos(0, 20);
    u8g.print("record:");
    u8g.print(recodeDB);
    setFont_M;//以中字体,在(x=0,y=50)坐标显示“your: ”以及当前分贝值
    u8g.setPrintPos(0, 50);
    u8g.print("your:");
    u8g.print(db);
    setFont_L;//以大字体,在(x=90,y=40)坐标显示“dB”单位字样
    u8g.setPrintPos(90, 40);
    u8g.print("dB");
  } while ( u8g.nextPage() );
}
//当音乐暂停后,定义控制变量为2,重新开始监听mic,并监听key_status,第一次的30db当做一个开启监听的条件