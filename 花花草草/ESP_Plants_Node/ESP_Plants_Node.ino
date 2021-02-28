/*
  花花草草（ESP_Plants_Node） 土壤湿度 空气温湿度 v1.3
  平台地址：http://keai.icu/hhcc
  服务地址：http://keai.icu/hhcc/update
  项目地址：https://github.com/bilibilifmk/ESP_Plants_Node
  by：发明控
*/
#define FS_CONFIG
//激活文件系统模式配网
#include <wifi_link_tool.h>
//引入wifilinktool头文件
#include "dht11.h"
//引入dht11库
dht11 DHT11;
#define MOSPIN D1
#define DHT11PIN D2
String API = "http://keai.icu/hhcc/update";//默认请求地址
String KEY = "";
char MAC[12];
String soil_moisture = "50"; //土壤湿度
String air_humidity = "50"; //空气湿度
String air_temperature = "50"; //空气温度
int loadone = 0; //第一次启动
int loadpd = 0; //配置状态
unsigned long previousMillis = 0;
const long interval = 180000;//第一次启动服务时间
int calibration = 710; //设置校准 需要将土壤浸水溶液 测得实际传感器值 默认710
void setup() {
  Serial.begin(115200);
  Serial.println("");
  pinMode(MOSPIN, OUTPUT);
  digitalWrite(MOSPIN, HIGH);

  uint8_t MACs[6];
  WiFi.macAddress(MACs);
  for (int i = 0; i < sizeof(MACs); ++i) sprintf(MAC, "%s%02x", MAC, MACs[i]);
  rstb = 0;
  //重置io
  stateled = 2;
  //指示灯io
  Hostname = "花花草草_" + String(MAC[8]) + String(MAC[9]) + "_" + String(MAC[10]) + String(MAC[11]);
  //设备名称
  wxscan = true;
  //是否被小程序发现设备 开启意味该设备具有后台 true开启 false关闭

  webServer.on("/set", wfs);
  webServer.on("/getset", getset);
  webServer.on("/logo.png", logo);
  load();

  EEPROM.get(1000, loadone);
  //读取EEPROM
  delay(500);
  spifskey();
  spifsapi();
  Serial.println("配置API：" + API);
  Serial.println("配置KEY：" + KEY);


  if (link()) {

    if (KEY != "") get_API(80);
    //如果key不为空 向服务器发送请求
    if (loadone == 1) {
      loadone = 0;
      EEPROM.put(1000, loadone);
      EEPROM.commit();
      Serial.println("等待10秒进入休眠模式，如再此期间重启将视为第一次启动");
      for (int i = 0; i <= 10; i++)
      {
        digitalWrite(stateled, LOW);
        delay(500);
        digitalWrite(stateled, HIGH);
        delay(500);
      }
      if (WiFi.status() == WL_CONNECTED) digitalWrite(stateled, HIGH); else digitalWrite(stateled, LOW);
      loadone = 1;
      EEPROM.put(1000, loadone);
      EEPROM.commit();
      Serial.println("进入休眠1h");
      ESP.deepSleep(3600e6);
    } else {
      Serial.println("第一次启动 运行三分钟后进入休眠模式");
    }
  }
}


//logo加载
void logo()
{
  File file = SPIFFS.open("/logo.png", "r");
  webServer.streamFile(file, "aimage/apng");
  file.close();
}
//参数设置获取
void getset()
{
  webServer.send(200, "text/plain", "<p>MAC:" + get_mac() + "</p><p>KEY:" + KEY + "</p><p>API:" + API + "</p>");
}
//从文件系统读取参数
void spifskey()
{
  File file = SPIFFS.open("/KEY.txt", "r");
  if (file) {
    KEY = file.readString();
  }
  file.close();
}
void spifsapi()
{
  File file = SPIFFS.open("/API.txt", "r");
  if (file) {
    API = file.readString();
  }
  file.close();
}
//向文件系统写入参数
void wfs() {
  if (webServer.arg("key") != "") {
    String k = webServer.arg("key");
    SPIFFS.remove("/KEY.txt");
    File file = SPIFFS.open("/KEY.txt", "w");
    file.print(k);
    file.close();
  }
  if (webServer.arg("api") != "") {
    String k = webServer.arg("api");
    SPIFFS.remove("/API.txt");
    File file = SPIFFS.open("/API.txt", "w");
    file.print(k);
    file.close();
  }
  webServer.send(200, "text/plain", "ojbk");
  delay(1000);
  ESP.restart();
}
//请求函数
void get_API(int port)
{
 //简单对adc值进行平均数滤波
  int adc = 0;
  for (int i = 0; i < 10; i++)
  { 
    adc += analogRead(A0);
    delay(50);
  }
  adc = adc / 10;
  adc = map(adc, 0, 1023, 1023, 0);
  Serial.print("ADC实际获得值：");
  Serial.println(adc);
  if (adc > calibration) adc = calibration;
  soil_moisture = (String)map(adc, 0, calibration, 0, 100);

  int chk = DHT11.read(DHT11PIN);
  air_humidity = (String)DHT11.humidity; //空气湿度
  air_temperature = (String)DHT11.temperature; //空气温度
  Serial.print("土壤湿度：");
  Serial.println(soil_moisture);
  Serial.print("空气湿度：");
  Serial.println(air_humidity);
  Serial.print("空气温度：");
  Serial.println(air_temperature);
  String payload;
  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, API + "?api_key=" + KEY + "&mac=" + get_mac() + "&soil_moisture=" + soil_moisture + "&air_humidity=" + air_humidity + "&air_temperature=" + air_temperature), port) { // HTTP
    int httpCode = http.GET();
    payload = http.getString();
    Serial.println("请求返回");
    Serial.println(payload);
    http.end();
  }

  
  digitalWrite(MOSPIN, LOW);
  //对电阻及dht11断电
}







//获取设备mac
String get_mac()
{
  String macs;
  for (int i = 0; i < 12; i++) macs += MAC[i];
  return macs;
}



void loop() {
  pant();
  //如果联网成功 第一次启动 三分钟进入休眠一小时
  if (link() == 0 && loadpd == 0)loadpd = 1;
  if (loadpd == 0)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      Serial.println("准备休眠");
      loadone = 1;
      EEPROM.put(1000, loadone);
      delay(2);
      EEPROM.commit();
      delay(1000);
      Serial.println("进入休眠1h");
      ESP.deepSleep(3600e6);
    }
  }

}
