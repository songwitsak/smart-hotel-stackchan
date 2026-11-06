#include <M5Core2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>
// M5Stack Core2 demo: simulated check-in and lights; no mains GPIO control.
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* HOTEL_API = "";
const char* ROOM_ID = "101";
bool checkedIn=false, lightOn=false, awake=false;
float tiltDeg=0;
uint32_t tiltStart=0,lastWake=0,lastDraw=0;
String statusText="Ready";
bool sendRequest(const String& path,const String& payload){
  if(WiFi.status()!=WL_CONNECTED || strlen(HOTEL_API)==0)return false;
  HTTPClient http;
  http.setTimeout(3000);
  if(!http.begin(String(HOTEL_API)+path))return false;
  http.addHeader("Content-Type","application/json");
  int code=http.POST(payload);
  http.end();
  return code>=200 && code<300;
}
void drawUI(){
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(CYAN,BLACK);
  M5.Lcd.setCursor(12,12);
  M5.Lcd.println("SMART HOTEL ROOM 101");
  M5.Lcd.setTextColor(WHITE,BLACK);
  M5.Lcd.setCursor(12,48);
  M5.Lcd.printf("Tilt: %.1f deg\n",tiltDeg);
  M5.Lcd.printf("Assistant: %s\n",awake?"AWAKE":"IDLE");
  M5.Lcd.printf("Check-in: %s\n",checkedIn?"YES":"NO");
  M5.Lcd.printf("Light: %s\n",lightOn?"ON":"OFF");
  M5.Lcd.setTextColor(YELLOW,BLACK);
  M5.Lcd.setCursor(12,162);
  M5.Lcd.println(statusText.substring(0,24));
  M5.Lcd.setTextColor(GREEN,BLACK);
  M5.Lcd.setCursor(8,211);
  M5.Lcd.print("A:CHECKIN  B:LIGHT  C:CLEAN");
}
void setup(){
  M5.begin(); M5.IMU.Init(); Serial.begin(115200);
  if(strlen(WIFI_SSID)>0){
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    uint32_t start=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-start<5000)delay(100);
  }
  drawUI();
}
void loop(){
  M5.update();
  float ax,ay,az;
  M5.IMU.getAccelData(&ax,&ay,&az);
  tiltDeg=atan2f(ax,sqrtf(ay*ay+az*az))*180.0f/PI;
  bool tilted=fabsf(tiltDeg)>=25.0f;
  if(tilted){
    if(!tiltStart)tiltStart=millis();
    if(!awake && millis()-tiltStart>=400 && millis()-lastWake>2000){
      awake=true;lastWake=millis();statusText="Hello! Welcome!";
      M5.Spk.DingDong();
    }
  } else {
    tiltStart=0;
    if(awake && millis()-lastWake>2500)awake=false;
  }
  if(M5.BtnA.wasPressed()){
    checkedIn=!checkedIn;
    statusText=checkedIn?"Guest checked in":"Guest checked out";
  }
  if(M5.BtnB.wasPressed()){
    if(!checkedIn)statusText="Check in first";
    else{lightOn=!lightOn;statusText=lightOn?"Light ON (demo)":"Light OFF (demo)";}
  }
  if(M5.BtnC.wasPressed()){
    if(!checkedIn)statusText="Check in first";
    else{
      bool ok=sendRequest("/api/housekeeping",String("{\"roomId\":\"")+ROOM_ID+"\",\"type\":\"cleaning\"}");
      statusText=ok?"Request submitted":"Request NOT sent";
    }
  }
  if(millis()-lastDraw>150){lastDraw=millis();drawUI();}
  delay(15);
}
