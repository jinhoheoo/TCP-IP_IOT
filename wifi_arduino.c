#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

#define AP_SSID "SEMICON_2.4G"
#define AP_PASS "a1234567890"
#define SERVER_NAME "10.10.52.208"
#define SERVER_PORT 5000
#define LOGID "HJH_WIFI"
#define PASSWD "PASSWD"

#define LED_LAMP_PIN 3  //Relay On/Off
#define DHTPIN 4
#define SERVO_PIN 5
#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define LED_BUILTIN_PIN 13
#define FLAME A1
#define SOUND A0

#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11

bool timerIsrFlag = false;
boolean ledOn = false;  // LED의 현재 상태 (on/off)


//char sendId[10] = "KSH_ARD";
char sendBuf[CMD_SIZE];
char lcdLine1[17] = "IOT BY LMW, HJH";
char lcdLine2[17] = "WiFi Connecting!";


unsigned int secCount;
unsigned int myservoTime = 0;

char getSensorId[10];
int sensorTime;
float temp = 0.0;
float humi = 0.0;
bool updatTimeFlag = false;
int soundflag = 0;
int flameflag = 0;
typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;
DATETIME dateTime = { 0, 0, 0, 12, 0, 0 };
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

void setup() {
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);

  pinMode(LED_LAMP_PIN, OUTPUT);     // LED 핀을 출력으로 설정
  pinMode(LED_BUILTIN_PIN, OUTPUT);  //D13
  pinMode(FLAME, INPUT);
  pinMode(SOUND, INPUT);

#ifdef DEBUG
  Serial.begin(115200);  //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr);  // 1000ms period
  MsTimer2::start();

  myservo.attach(SERVO_PIN);
  myservo.write(0);
  myservoTime = secCount;
  dht.begin();
}

void loop() {

  if (client.available()) {
    socketEvent();
  }


  if (timerIsrFlag)  //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    if (!(secCount % 3))  //5초에 한번씩 실행
    {


      humi = dht.readHumidity();
      temp = dht.readTemperature();
#ifdef DEBUG

      Serial.print(" Humidity: ");
      Serial.print(humi);
      Serial.print(" Temperature: ");
      Serial.println(temp);

 

#endif
      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect();
      }

    }

    flameflag = analogRead(FLAME);  //500이상이면 불안남 500이하 불남
    soundflag = analogRead(SOUND);  //50 이하이면 소음없음 50이상이면 소음심함
if ((flameflag > 500) && (soundflag < 50)) {
        sprintf(lcdLine2, "Tem:%dC Hum:%d%%", (int)temp, (int)humi);  // lcdline2에 저 내용 저장하는거임.
        lcdDisplay(0, 1, lcdLine2);
#ifdef DEBUG  //이런식으로 내가 원하는거 제어할 때마다 이렇게 프린트되게 ifdef 이렇게 해줘야함 모터나 다른거도 전부
        Serial.println(lcdLine2);
#endif
      } else if ((flameflag > 500) && (soundflag > 50)) {

        sprintf(lcdLine2, "Be quite!!!!!");  // lcdline2에 저 내용 저장하는거임.
        lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG  //이런식으로 내가 원하는거 제어할 때마다 이렇게 프린트되게 ifdef 이렇게 해줘야함 모터나 다른거도 전부
        Serial.println(lcdLine2);
#endif

      } else {
        sprintf(lcdLine2, "WARNING!!!!!");  // lcdline2에 저 내용 저장하는거임.
        lcdDisplay(0, 1, lcdLine2);
#ifdef DEBUG  //이런식으로 내가 원하는거 제어할 때마다 이렇게 프린트되게 ifdef 이렇게 해줘야함 모터나 다른거도 전부
        Serial.println(lcdLine2);
#endif
      }


#ifdef DEBUG

    Serial.print(" FLAME: ");
    Serial.print(FLAME);
    Serial.print(" SOUND: ");
    Serial.println(SOUND);

#endif

    if (myservo.attached() && myservoTime + 2 == secCount) {
      myservo.detach();
    }
    if (sensorTime != 0 && !(secCount % sensorTime)) {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d\r\n", getSensorId, (int)temp, (int)humi);
      /*      char tempStr[5];
            char humiStr[5];
            dtostrf(humi, 4, 1, humiStr);  //50.0 4:전체자리수,1:소수이하 자리수
            dtostrf(temp, 4, 1, tempStr);  //25.1
            sprintf(sendBuf,"[%s]SENSOR@%d@%s@%s\r\n",getSensorId,tempStr,humiStr);
      */
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
    }
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec);
    lcdDisplay(0, 0, lcdLine1);
    if (updatTimeFlag) {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }
  }
}
void socketEvent() {
  int i = 0;
  char *pToken;
  char *pArray[ARR_CNT] = { 0 };
  char recvBuf[CMD_SIZE] = { 0 };
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) {
    pArray[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //[KSH_ARD]LED@ON : pArray[0] = "KSH_ARD", pArray[1] = "LED", pArray[2] = "ON"
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16) {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }
  if (!strcmp(pArray[0], "HJH_BT") && !strcmp(pArray[1], "LED"))
    return;

  if (!strncmp(pArray[1], " New", 4))  // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif


    updatTimeFlag = true;
    return;
  } else if (!strncmp(pArray[1], " Alr", 4))  //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return;
  } else if (!strcmp(pArray[1], "LED")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_BUILTIN_PIN, HIGH);
    } else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_BUILTIN_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } else if (!strcmp(pArray[1], "LAMP")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_LAMP_PIN, HIGH);
    } else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_LAMP_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } else if (!strcmp(pArray[1], "GETSTATE")) {
    //  strcpy(sendId, pArray[0]);
    if (!strcmp(pArray[2], "LED")) {
      sprintf(sendBuf, "[%s]LED@%s\n", pArray[0], digitalRead(LED_BUILTIN_PIN) ? "ON" : "OFF");
    }
  } else if (!strcmp(pArray[1], "SERVO")) {
    myservo.attach(SERVO_PIN);
    myservoTime = secCount;
    if (!strcmp(pArray[2], "ON"))
      myservo.write(180);
    else
      myservo.write(0);
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);

  } else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
    if (pArray[2] != NULL) {
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      sensorTime = 0;
      sprintf(sendBuf, "[%s]%s@%d@%d\n", pArray[0], pArray[1], (int)temp, (int)humi);
    }
  } else if (!strcmp(pArray[0], "GETTIME")) {  //GETTIME
    dateTime.year = (pArray[1][0] - 0x30) * 10 + pArray[1][1] - 0x30;
    dateTime.month = (pArray[1][3] - 0x30) * 10 + pArray[1][4] - 0x30;
    dateTime.day = (pArray[1][6] - 0x30) * 10 + pArray[1][7] - 0x30;
    dateTime.hour = (pArray[1][9] - 0x30) * 10 + pArray[1][10] - 0x30;
    dateTime.min = (pArray[1][12] - 0x30) * 10 + pArray[1][13] - 0x30;
    dateTime.sec = (pArray[1][15] - 0x30) * 10 + pArray[1][16] - 0x30;
#ifdef DEBUG
    sprintf(sendBuf, "\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r", dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec);
    Serial.println(sendBuf);
#endif
    return;
  } else
    return;

  client.write(sendBuf, strlen(sendBuf));
  client.flush();

  // client.write("[HJH_BT]LED@ON\n", strlen([HJH_BT]LED@ON\n));
  // client.flush();

  // client.write("[HJH_SQL]SET@LAMP@1\n", strlen([HJH_BT]LED@ON\n));
  // client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}
void timerIsr() {
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}
void clock_calc(DATETIME *dateTime) {
  int ret = 0;
  dateTime->sec++;  // increment second

  if (dateTime->sec >= 60)  // if second = 60, second = 0
  {
    dateTime->sec = 0;
    dateTime->min++;

    if (dateTime->min >= 60)  // if minute = 60, minute = 0
    {
      dateTime->min = 0;
      dateTime->hour++;  // increment hour
      if (dateTime->hour == 24) {
        dateTime->hour = 0;
        updatTimeFlag = true;
      }
    }
  }
}
void lcdDisplay(int x, int y, char *str) {
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}

void wifi_Setup() {
  wifiSerial.begin(19200);
  wifi_Init();
  server_Connect();
}
void wifi_Init() {
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    } else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
  sprintf(lcdLine1, "ID:%s", LOGID);
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}
int server_Connect() {
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("[" LOGID ":" PASSWD "]");
  } else {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}
void printWifiStatus() {
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
