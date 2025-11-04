#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <String.h>
#include <TridentTD_LineNotify.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mySoftwareSerial(13, 12); // TX (D7), RX (D6)
DFRobotDFPlayerMini myDFPlayer;

Servo myservo;
#define sensor D5
#define buzzer D2
#define Relay  D1
int x, i, k, val;

String POSTURL = "POSTURL";
String GETURL  = "GETURL";
String POSTSTT = "POSTSTT";

// ประกาศตัวแปรและ Object HTTP
WiFiClient client;
HTTPClient http;
int httpCode;
int modesend; // 1 = Telegram | 0 = LINE
int Mode_Sound; // 1 = เช้า, กลางวัน, เย็น, ก่อนนอน | 0 = ก่อนอาหาร, หลังอาหาร // ไม่ใช้
int mode_setting; // 1 = ดังเป็นเสียงลำโพง | 2 = ดังเป็นเสียงพูดอย่างเดียว | 3 = ดังเป็นเสียงลำโพง&เสียงพูด
int UserID;
String time_get, Fullname, LINE_TOKEN, bf_time, lun_time, dn_time, bb_time;
String bf_stt, lun_stt, dn_stt, bb_stt;
int bf_medic[4], lun_medic[4], dn_medic[4], bb_medic[4], medic_send[4];
int meal;
int Delay;
int Count_medicine;
String status;
// #define LINE_TOKEN "xxxxx" // ใส่ LINE token ที่ได้จาก LINE Notify
//Telegram
#define BOT_TOKEN "7509420222:AAH9hUDidj4h3Z1W6RULIX5dX0vtENjIMas"
#define CHAT_ID "-1002266490516"
const char* host = "api.telegram.org";
WiFiClientSecure client_teltegram;

String lastStatus = "offline";

void setup() {
  Serial.begin(115200);
  mySoftwareSerial.begin(9600);
  pinMode(sensor, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(buzzer, HIGH);
  digitalWrite(Relay, HIGH);
  myservo.attach(D4);

  Serial.println("Initializing DFPlayer Mini...");
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println("DFPlayer Mini initialization failed!");
    Serial.println("Check connections and SD card.");
    while (true);
  }
  Serial.println("DFPlayer Mini initialized successfully!");
  myDFPlayer.volume(20);
  connectWiFi();

  http.begin(client, GETURL);
  httpCode = http.GET();
  String payload = http.getString();
  Serial.print("HTTP_GET Response Code: "); Serial.println(httpCode);
  delay(5000);
  while (httpCode != 200) {
    Serial.print(".");
    condition_GET();
    delay(500);
  }
  Serial.print("Connect Code: "); Serial.println(httpCode);
  sendTelegramMessage("เครื่องจ่ายยาอัตโนมัติ พร้อมใช้งาน");
  //Serial.print("payload: "); Serial.println(payload);
  Serial.println("----------SETUP CONTROL.INO READY--------------");
  delay(250);
}

void loop() {
  digitalWrite(buzzer, HIGH);
  digitalWrite(Relay, HIGH);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("!WiFi");
    connectWiFi();
    delay(250);
  }
  while (UserID <= 0 || httpCode != 200) {
    Serial.println("!UserID");
    condition_GET();
    delay(250);
  }

  //Serial.println(UserID);
  String currentStatus = (WiFi.status() == WL_CONNECTED) ? "online" : "offline";
  sendStatus(currentStatus);
  lastStatus = currentStatus;

  // Test Serial.println(value) Here for sure
  condition_GET();
  condition_CHECK();
  delay(1000 * 5);
}

void sendStatus(String status_post) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(client, POSTSTT);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "status=" + status_post + "&UserID=" + String(UserID);
    int httpResponseCode = http.POST(postData);
    // ดีบักข้อมูล
    //Serial.println("POST Data: " + postData);
    //Serial.println("HTTP Response code: " + String(httpResponseCode));
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void connectWiFi() {
  WiFiManager wifiManager;
  
  if (!wifiManager.autoConnect("ESP8266_AP_Controller")) {  // ชื่อ AP "ESP8266_AP"
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();  // รีบูตใหม่ถ้าเชื่อมต่อไม่สำเร็จ
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Servo rotate counter-clockwise full-speed
void rotateservo() {
  myservo.writeMicroseconds(1400);
}

// Servo rotation stopped
void stopservo() {
  myservo.writeMicroseconds(1500);
}

// Condition_GET
void condition_GET() {
  http.begin(client, GETURL);
  httpCode = http.GET();
  String payload = http.getString();
  //Serial.print("HTTP_GET Response Code: "); Serial.println(httpCode);
  //Serial.print("payload : ");               Serial.println(payload);

  // GET UserID from INPUT Website
  if (UserID <= 0) { 
  int index = payload.indexOf("UserID=");
    String userIDString = payload.substring(index + 7);
    UserID = userIDString.toInt();
  }

  // GET RealTime from PHP
  int t = payload.indexOf("time=");
    String tt = payload.substring(t + 5);
    int substr_tt = tt.indexOf("UserID=");
    tt = tt.substring(0, substr_tt);
  time_get = tt;

  // GET Fullname = firstname + lastname
  int n = payload.indexOf("Fullname=");
    String nn = payload.substring(n + 9);
    int substr_nn = nn.indexOf("|");
    nn = nn.substring(0, substr_nn);
  Fullname = nn;

  int index1 = payload.indexOf("Delay=");
    String userIDString1 = payload.substring(index1 + 6);
    Delay = userIDString1.toInt();

  // GET TokenLine
  if (LINE_TOKEN == NULL || LINE_TOKEN.length() == 0) { // ตรวจสอบว่าค่าว่างหรือ NULL
    int m = payload.indexOf("Token=");
    String mm = payload.substring(m + 6);
    int substr_mm = mm.indexOf("|");
    mm = mm.substring(0, substr_mm);
    LINE_TOKEN = mm; // ตั้งค่าใหม่ให้กับ LINE_TOKEN
    //Serial.println(LINE.getVersion());
    LINE.setToken(LINE_TOKEN);
    LINE.notify("เครื่องจ่ายยาอัตโนมัติ พร้อมใช้งาน");
  }

  // GET bf_time
  int bf = payload.indexOf("bf_time=");
    String bf_timeString = payload.substring(bf + 8);
    int substr_bf = bf_timeString.indexOf("bf_medic");
    bf_timeString = bf_timeString.substring(0, substr_bf);
    bf_time = bf_timeString;
  int bf1 = payload.indexOf("bf_medic1=");
    String bf1_String = payload.substring(bf1 + 10);
    bf_medic[0] = bf1_String.toInt();
  int bf2 = payload.indexOf("bf_medic2=");
    String bf2_String = payload.substring(bf2 + 10);
    bf_medic[1] = bf2_String.toInt();
  int bf3 = payload.indexOf("bf_medic3=");
    String bf3_String = payload.substring(bf3 + 10);
    bf_medic[2] = bf3_String.toInt();
  int bf4 = payload.indexOf("bf_medic4=");
    String bf4_String = payload.substring(bf4 + 10);
    bf_medic[3] = bf4_String.toInt();
  int s_bf = payload.indexOf("stt_alert=");
    String s_bf_substr = payload.substring(s_bf + 10);
    int after_s_bf = s_bf_substr.indexOf("lun_time=");
    s_bf_substr = s_bf_substr.substring(0, after_s_bf);
    bf_stt = s_bf_substr;

  // GET lun_time
  int lun = payload.indexOf("lun_time=");
    String lun_timeString = payload.substring(lun + 9);
    int substr_lun = lun_timeString.indexOf("lun_medic");
    lun_timeString = lun_timeString.substring(0, substr_lun);
    lun_time = lun_timeString;
  int lun1 = payload.indexOf("lun_medic1=");
    String lun1_String = payload.substring(lun1 + 11);
    lun_medic[0] = lun1_String.toInt();
  int lun2 = payload.indexOf("lun_medic2=");
    String lun2_String = payload.substring(lun2 + 11);
    lun_medic[1] = lun2_String.toInt();
  int lun3 = payload.indexOf("lun_medic3=");
    String lun3_String = payload.substring(lun3 + 11);
    lun_medic[2] = lun3_String.toInt();
  int lun4 = payload.indexOf("lun_medic4=");
    String lun4_String = payload.substring(lun4 + 11);
    lun_medic[3] = lun4_String.toInt();
  int s_lun = payload.indexOf("stt_alert=");
    String s_lun_substr = payload.substring(s_lun + 10);
    int after_s_lun = s_lun_substr.indexOf("dn_time");
    s_lun_substr = s_lun_substr.substring(0, after_s_lun);
    lun_stt = s_lun_substr;

  // GET dn_time
  int dn = payload.indexOf("dn_time=");
    String dn_timeString = payload.substring(dn + 8);
    int substr_dn = dn_timeString.indexOf("dn_medic");
    dn_timeString = dn_timeString.substring(0, substr_dn);
    dn_time = dn_timeString;
  int dn1 = payload.indexOf("dn_medic1=");
    String dn1_String = payload.substring(dn1 + 10);
    dn_medic[0] = dn1_String.toInt();
  int dn2 = payload.indexOf("dn_medic2=");
    String dn2_String = payload.substring(dn2 + 10);
    dn_medic[1] = dn2_String.toInt();
  int dn3 = payload.indexOf("dn_medic3=");
    String dn3_String = payload.substring(dn3 + 10);
    dn_medic[2] = dn3_String.toInt();
  int dn4 = payload.indexOf("dn_medic4=");
    String dn4_String = payload.substring(dn4 + 10);
    dn_medic[3] = dn4_String.toInt();
  int s_dn = payload.indexOf("stt_alert=");
    String s_dn_substr = payload.substring(s_dn + 10);
    dn_stt = s_dn_substr;

  // GET bb_time by UserID
  int bb = payload.indexOf("bb_time=");
    String bb_timeString = payload.substring(bb + 8);
    int substr_bb = bb_timeString.indexOf("bb_medic");
    bb_timeString = bb_timeString.substring(0, substr_bb);
    bb_time = bb_timeString;
  int bb1 = payload.indexOf("bb_medic1=");
    String bb1_String = payload.substring(bb1 + 10);
    bb_medic[0] = bb1_String.toInt();
  int bb2 = payload.indexOf("bb_medic2=");
    String bb2_String = payload.substring(bb2 + 10);
    bb_medic[1] = bb2_String.toInt();
  int bb3 = payload.indexOf("bb_medic3=");
    String bb3_String = payload.substring(bb3 + 10);
    bb_medic[2] = bb3_String.toInt();
  int bb4 = payload.indexOf("bb_medic4=");
    String bb4_String = payload.substring(bb4 + 10);
    bb_medic[3] = bb4_String.toInt();
  int s_bb = payload.indexOf("stt_alert=");
    String s_bb_substr = payload.substring(s_bb + 10);
    int after_s_bb = s_bb_substr.indexOf("lun_time=");
    s_bb_substr = s_bb_substr.substring(0, after_s_bb);
    bb_stt = s_bb_substr;

  int cots = payload.indexOf("Count_medicine=");
  String stcots = payload.substring(cots + 15);
    int cotsIndex = stcots.indexOf("|");
    stcots = stcots.substring(0, cotsIndex);
  Count_medicine = stcots.toInt();

  int cots1 = payload.indexOf("Mode_Sound=");
  String stcots1 = payload.substring(cots1 + 11);
    int cotsIndex1 = stcots1.indexOf("|");
    stcots1 = stcots1.substring(0, cotsIndex1);
  Mode_Sound = stcots1.toInt();

  int cots2 = payload.indexOf("modesend=");
  String stcots2 = payload.substring(cots2 + 9);
    int cotsIndex2 = stcots2.indexOf("|");
    stcots2 = stcots2.substring(0, cotsIndex2);
  modesend = stcots2.toInt();

  int cots3 = payload.indexOf("mode_setting=");
  String stcots3 = payload.substring(cots3 + 13);
    int cotsIndex3 = stcots3.indexOf("|");
    stcots3 = stcots3.substring(0, cotsIndex3);
  mode_setting = stcots3.toInt();
}

// Condition_CHECK data before POST *
void condition_CHECK() {
  if (time_get == bf_time && x == 0 && httpCode == 200) {
      meal = 1;
      Serial.println(time_get);
      Serial.println(bf_time);
      medic_send[0] = bf_medic[0];
      medic_send[1] = bf_medic[1];
      medic_send[2] = bf_medic[2];
      medic_send[3] = bf_medic[3];
      if (mode_setting == 2 || mode_setting == 3) {
        myDFPlayer.play(5); //ถึงเวลากำหนดจ่ายยา
        Serial.println("ถึงเวลากำหนดจ่ายยา");
        delay(1000 * 5);
      }
      condition_CHECK_send();
  } else if (time_get == lun_time && x == 0 && httpCode == 200) {
      meal = 2;
      Serial.println(time_get);
      Serial.println(lun_time);
      medic_send[0] = lun_medic[0];
      medic_send[1] = lun_medic[1];
      medic_send[2] = lun_medic[2];
      medic_send[3] = lun_medic[3];
      if (mode_setting == 2 || mode_setting == 3) {
        myDFPlayer.play(5); //ถึงเวลากำหนดจ่ายยา
        Serial.println("ถึงเวลากำหนดจ่ายยา");
        delay(1000 * 5);
      }
      condition_CHECK_send();
  } else if (time_get == dn_time && x == 0 && httpCode == 200) {
      meal = 3;
      Serial.println(time_get);
      Serial.println(dn_time);
      medic_send[0] = dn_medic[0];
      medic_send[1] = dn_medic[1];
      medic_send[2] = dn_medic[2];
      medic_send[3] = dn_medic[3];
      if (mode_setting == 2 || mode_setting == 3) {
        myDFPlayer.play(5); //ถึงเวลากำหนดจ่ายยา
        Serial.println("ถึงเวลากำหนดจ่ายยา");
        delay(1000 * 5);
      }
      condition_CHECK_send();
  } else if (time_get == bb_time && x == 0 && httpCode == 200) {
      meal = 4;
      Serial.println(time_get);
      Serial.println(bb_time);
      medic_send[0] = bb_medic[0];
      medic_send[1] = bb_medic[1];
      medic_send[2] = bb_medic[2];
      medic_send[3] = bb_medic[3];
      if (mode_setting == 2 || mode_setting == 3) {
        myDFPlayer.play(5); //ถึงเวลากำหนดจ่ายยา
        Serial.println("ถึงเวลากำหนดจ่ายยา");
        delay(1000 * 5);
      }
      condition_CHECK_send();
  }
}

// Condition in condition_CHECK
void condition_CHECK_send() {
  Serial.println("ถึงเวลาที่กำหนดจ่ายยาแล้ว!");
  if(modesend == 0){
    LINE.notify("ถึงเวลาที่กำหนดจ่ายยาแล้ว!");
  }else if(modesend == 1){
    sendTelegramMessage("ถึงเวลาที่กำหนดจ่ายยาแล้ว!");
  }
  rotateservo();
  delay(275); //lol
  stopservo();
  delay(500);
  if (mode_setting == 1 || mode_setting == 3) {
    digitalWrite(buzzer, LOW);
    digitalWrite(Relay, LOW);
    delay(750);
    digitalWrite(buzzer, HIGH);
    digitalWrite(Relay, HIGH);
    delay(750);
  } else if (mode_setting == 2){
    digitalWrite(Relay, LOW);
    delay(750);
    digitalWrite(Relay, HIGH);
    delay(750);
  }
  x = 1;
  Count_medicine = Count_medicine-1;
  condition_grap();
}

// Condition grap after time_get == time_meal
void condition_grap() {
  while (x == 1) {
    k++;
    val = digitalRead(sensor);
    delay(1000); // 1-second delay for timing
    
    if (x == 1 && val == 1) {
      // Medicine successfully taken
      digitalWrite(Relay, LOW);
      delay(150);
      digitalWrite(Relay, HIGH);
      delay(150);
      digitalWrite(Relay, LOW);
      delay(150);
      digitalWrite(Relay, HIGH);
      if (mode_setting == 2 || mode_setting == 3) {
        myDFPlayer.play(2); // ขอบคุณค่ะ
        Serial.println("ขอบคุณค่ะ");
        delay(1000 * 5);
        myDFPlayer.play(3); // การรับประทานยาให้ตรง...
        Serial.println("การรับประทานยาให้ตรง...");
        delay(1000 * 10);
      } 
      Serial.println("Take medicine success!");
      status = "success";
      if(modesend == 0){
        LINE.notify("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยทันเวลาที่กำหนด!");
        LINE.notifyPicture("https://cdn-icons-png.flaticon.com/512/4436/4436481.png");
        LINE.notify("\nขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
      }else if(modesend == 1){
        sendTelegramMessage("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยทันเวลาที่กำหนด!");
        sendTelegramPhoto("https://cdn-icons-png.flaticon.com/512/4436/4436481.png");
        sendTelegramMessage("ขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
      }

      delay(1000);
      condition_POST();
      k = 0; // Reset timer
    } 
    else if (k == 60) { // 1 minute passed with no action
      Serial.println("Reminder: Take your medicine!");
      if(modesend == 0){
        LINE.notify("กรุณารับประทานยาตามเวลาที่กำหนด!");
      }else if(modesend == 1){
        sendTelegramMessage("กรุณารับประทานยาตามเวลาที่กำหนด!");
      }
      // Repeated buzzer and relay activation every second until medicine is taken or 2 minutes pass
      while (k <= 70 && digitalRead(sensor) == 0) { // Keep notifying until 2 minutes or medicine is taken
        if (mode_setting == 1) {
          digitalWrite(buzzer, LOW);
          digitalWrite(Relay, LOW);
          delay(500);
          digitalWrite(buzzer, HIGH);
          digitalWrite(Relay, HIGH);
          delay(500);
          digitalWrite(buzzer, LOW);
          digitalWrite(Relay, LOW);
          delay(500);
          digitalWrite(buzzer, HIGH);
          digitalWrite(Relay, HIGH);
          delay(500);
        } else if (mode_setting == 2) {
          myDFPlayer.play(6); // ยังไม่ได้รับยาค่ะ กรุณารับยา
          delay(1000 * 5);
          digitalWrite(Relay, LOW);
          delay(500);
          digitalWrite(Relay, HIGH);
          delay(500);
        } else if (mode_setting == 3) {
          myDFPlayer.play(6); // ยังไม่ได้รับยาค่ะ กรุณารับยา
          delay(1000 * 5);
          digitalWrite(buzzer, LOW);
          digitalWrite(Relay, LOW);
          delay(250);
          digitalWrite(buzzer, HIGH);
          digitalWrite(Relay, HIGH);
          delay(250);
          digitalWrite(buzzer, LOW);
          digitalWrite(Relay, LOW);
          delay(250);
          digitalWrite(buzzer, HIGH);
          digitalWrite(Relay, HIGH);
          delay(250);
        }
        k++; // Increment time counter
      }
      // Check if medicine is taken after reminders
      if (x == 1 && val == 1) {
        digitalWrite(Relay, LOW);
        delay(150);
        digitalWrite(Relay, HIGH);
        delay(150);
        digitalWrite(Relay, LOW);
        delay(150);
        digitalWrite(Relay, HIGH);
        if (mode_setting == 2 || mode_setting == 3) {
          myDFPlayer.play(2); // ขอบคุณค่ะ
          Serial.println("ขอบคุณค่ะ");
          delay(1000 * 5);
          myDFPlayer.play(3); // การรับประทานยาให้ตรง...
          Serial.println("การรับประทานยาให้ตรง...");
          delay(1000 * 10);
        } 
        Serial.println("Take medicine success after reminder!");
        status = "success";
        if(modesend == 0){
          LINE.notify("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยทันเวลาที่กำหนดหลังการแจ้งเตือน!");
          LINE.notifyPicture("https://cdn-icons-png.flaticon.com/512/4436/4436481.png");
          LINE.notify("\nขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
        } else if(modesend == 1){
          sendTelegramMessage("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยทันเวลาที่กำหนดหลังการแจ้งเตือน!");
          sendTelegramPhoto("https://cdn-icons-png.flaticon.com/512/4436/4436481.png");
          sendTelegramMessage("ขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
        }
        delay(1000);
        condition_POST();
        k = 0; // Reset timer
      }
    } else if (k >= 70) { // 2 minutes passed (failed)
      digitalWrite(buzzer, HIGH);
      digitalWrite(Relay, HIGH);
      Serial.println("Take medicine failed!");
      status = "failed";
      if(modesend == 0){
        LINE.notify("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยไม่สำเร็จ!");
        LINE.notifyPicture("https://www.shareicon.net/data/256x256/2015/09/15/101562_incorrect_512x512.png");
        LINE.notify("\nขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
      }else if(modesend == 1){
        sendTelegramMessage("เครื่องที่ " + Fullname + " " + UserID + " จ่ายยาให้ผู้ป่วยไม่สำเร็จ!");
        sendTelegramPhoto("https://www.shareicon.net/data/256x256/2015/09/15/101562_incorrect_512x512.png");
        sendTelegramMessage("ขณะนี้จำนวนหลอดยาภายในเครื่องจ่ายยาอยู่ที่ " + String(Count_medicine) + " หลอด กรุณาตรวจสอบความถูกต้อง และดำเนินการเติมหลอดยาตามความเหมาะสมการใช้งานของท่าน");
      }
      delay(1000);
      condition_POST();
      k = 0; // Reset timer
    }
  }

  // Ensure buzzer and relay return to default states after loop
  digitalWrite(buzzer, HIGH);
  digitalWrite(Relay, HIGH);
}

// Condition_POST after condition_CHECK
void condition_POST() {
  String postData = "UserID=" + String(UserID) + "&meal=" + String(meal) + 
  "&medic_send1=" + String(medic_send[0]) + "&medic_send2=" + String(medic_send[1]) + 
  "&medic_send3=" + String(medic_send[2]) + "&medic_send4=" + String(medic_send[3]) + 
  "&status=" + String(status) + "&Count_medicine=" + String(Count_medicine);
  WiFiClient client;
  HTTPClient http;
  http.begin(client, POSTURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // ***Send data
  int httpCode = http.POST(postData);
  String payload = http.getString();
  Serial.print("HTTP_POST Response Code: "); Serial.println(httpCode);
  Serial.print("POSTURL : "); Serial.println(POSTURL); 
  Serial.print("Data: ");     Serial.println(postData);
  //Serial.print("payload : "); Serial.println(payload);
  delay(1000 * 55);
  condition_GET();
  x = 0;
  delay(150);
  loop();
}

void sendTelegramMessage(String message) {
  client_teltegram.setInsecure(); 
  if (client_teltegram.connect(host, 443)) {  // เชื่อมต่อกับ Telegram API ผ่าน HTTPS
    String url = "/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + message;
    client_teltegram.print(String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" + 
    "Connection: close\r\n\r\n");
    delay(500);  // ให้เวลาสำหรับการส่งคำขอ
    //Serial.println("Telegram");
  } else {
    Serial.println("Telegram Connection failed!");
  }
  client_teltegram.stop();  // ปิดการเชื่อมต่อ
}

void sendTelegramPhoto(String photoUrl) {
  client_teltegram.setInsecure(); 
  if (client_teltegram.connect(host, 443)) {  // เชื่อมต่อกับ Telegram API ผ่าน HTTPS
    String url = "/bot" + String(BOT_TOKEN) + "/sendPhoto?chat_id=" + CHAT_ID + "&photo=" + photoUrl;
    client_teltegram.print(String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" + 
    "Connection: close\r\n\r\n");
    delay(500);  // ให้เวลาสำหรับการส่งคำขอ
    //Serial.println("Telegram Photo Sent!");
  } else {
    Serial.println("Telegram Connection failed!");
  }
  client_teltegram.stop();  // ปิดการเชื่อมต่อ
}
