#include <SoftwareSerial.h>  //이 헤더파일은 uart를 흉내낸거고 제대로 인터럽트 방식 uart 구현하고 그러려면 uart를 써야함.
#include <Wire.h>            //1회선을 가지고 통신하는 i2c dht이런거에 사용
#include <TimerOne.h>  //타이머1을 쓴이유는 rc서보모터같은게 타이머2를 써서 여기선 타이머 1을 씀


#define DEBUG  //시리얼로 디버깅할 때 조건부 컴파일로 보면 됨. 아래의 if def 이게 조건부 컴파일 임


#define ARR_CNT 5      // 아그먼트 그 블루투스에 문자열 5개 까지 가능하게하려고 이렇게 정의
#define CMD_SIZE 60    //데이터 read/write할 사이즈 정의하려고 디파인에 이렇게 함




char sendBuf[CMD_SIZE];                  //아두이노가 상대방에게 메세지 보낼 때 사용하려고
char recvId[10] = "KSH_LIN";             //getsensor라는 명령이 오면 누가 보냈는지 확인하려고
bool timerIsrFlag = false;               //인터럽트가 발생했는지 안했는지 확인하는 변수임
unsigned int secCount;



DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);  //lcd 초기화
SoftwareSerial BTSerial(10, 11);     // RX ==>BT:TXD, TX ==> BT:RX      BTSerial 우리가 정한 이름임 구조체 변수임.
//시리얼 모니터에서 9600bps, line ending 없음 설정 후 AT명령 --> OK 리턴
//AT+NAMEiotxx ==> OKname   : 이름 변경 iotxx
void setup() {
#ifdef DEBUG  //debug정의되있으면 baudrate 9600하고 글자 출력함   디버그할 때 이렇게 ifdef 안에 꺼 한다는 거임.
  Serial.begin(9600);
  Serial.println("setup() start!");
#endif
  lcd.init();  //lcd 초기화 하면 픽셀 옅어짐 초기화 안하면 픽셀 찐해서 글자 잘 안보임
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);  //첫번째 인자 x좌표 즉 열이고 두번째 인자 y좌표 행임, 세번째 인자는 넣는 글자임.
  lcdDisplay(0, 1, lcdLine2);
  //pinMode(BUTTON_PIN, INPUT_PULLUP);  //인풋할때는 풀업풀다운이 필수인데 avr안에 풀업 기능이 있음. mcu 안에 풀업저항있으니 스위치만 달아줘도 됨.
  pinMode(BUTTON_PIN, INPUT);        //우리는 풀다운으로 회로구성했으니 이렇게 한다.
  pinMode(LED_BUILTIN_PIN, OUTPUT);  //
  BTSerial.begin(9600);              // set the data rate for the SoftwareSerial port
  Timer1.initialize(1000000);        //us단위임 타이머1은 1초마다 인터럽트 호출을 하기 위한 시간 정의임
  Timer1.attachInterrupt(timerIsr);  // timerIsr to run every 1 seconds    위에서 정한 시간마다 호출한다는 의미임
  dht.begin();                       //dht를 통해 데이터 시간읽어온다는거임.
}

void loop() {
  if (BTSerial.available())  //수신데이터가 받아지면 즉 블루투스에서 데이터를 보내면 즉 데이터가 한번도 안오면 아래 이벤트 실행안된다는거임.
    bluetoothEvent();        //블루투스 데이터 들어오면 블루투스 이벤트 실행됨.

 

 

#ifdef DEBUG
  if (Serial.available())
    BTSerial.write(Serial.read());  //블루투스로 수신된 데이터를 보냄.
#endif
}
void bluetoothEvent() {
  int i = 0;
  char* pToken;
  char* pArray[ARR_CNT] = { 0 };
  char recvBuf[CMD_SIZE] = { 0 };
  int len = BTSerial.readBytesUntil('\n', recvBuf, sizeof(recvBuf) - 1);  //readByte 이거는 \n들어올때 까지 데이터 받는다는거고
  //마지막 인자때문에 \n안들어와도 사이즈에 맞춰서 들어오면 반환함. 그리고 \n이전까지 저장되는거임.

#ifdef DEBUG
  Serial.print("Recv : ");
  Serial.println(recvBuf);      //\n이전까지의 문자가 출력됨
#endif

  pToken = strtok(recvBuf, "[@]"); 
  //strtok는 문자열을 분리하는 함수로 두번째 인자가 [,@,] 이렇게 3개의 문자가 포함되어있으면 이걸 기준으로 분리함
  //그리고 [,@,]이 3개의 문자가 있으면 이 문자를 제거하고 \0인 널문자로 바꿔버림  즉 저걸 기준으로 나누고 널문자로 바뀐 부분 들을 기준으로
  //문자열이 나뉘어 지는데 그 문자열의 첫번째 값들의 주소를 반환함.
  while (pToken != NULL) {  //[BT]LED@on 이렇게 했을 때 pToken의 첫번째 반환 값은 [이 부분이 NULL로 바꼈으니 NULL이 리턴되고 그다음 번엔 B의 주소가 리턴됨 
    pArray[i] = pToken;       //while문에 의해 pToken에 주소가 들어가서 널문자 전까지 저장됨 pArray[i]의 경우 주소임 위에 보면 포인터 배열로 정의됨
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]"); //NULL을 쓰면 이전에 저장했던 recvBuf에서 계속 찾겠다는거임
  }
  //recvBuf : [XXX_BTM]LED@ON
  //pArray[0] = "XXX_BTM"   : 송신자 ID
  //pArray[1] = "LED"
  //pArray[2] = "ON"
  //pArray[3] = 0x0
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16) {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }
  if (!strcmp(pArray[1], "LED")) {              //같으면 0이 리턴되면서 내가 원하는게 들어왔는지 확인하는거임.
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_BUILTIN_PIN, HIGH);
    } else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_BUILTIN_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } else if (!strcmp(pArray[1], "GETSENSOR")) {
    if (pArray[2] == NULL) {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\n", pArray[0], cds, (int)temp, (int)humi);
      strcpy(recvId, pArray[0]);
      getSensorTime = 0;
    } else {
      getSensorTime = atoi(pArray[2]);
      return;
    }
  }

  else if (!strcmp(pArray[1], "MOTOR")) {
    motorPwm = atoi(pArray[2]);
    motorPwm = map(motorPwm, 0, 100, 0, 255);
    Serial.println(motorPwm);
    analogWrite(MOTOR_PIN, motorPwm);
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }


  else if (!strncmp(pArray[1], " New", 4))  // New Connected
  {
    return;
  }
  else if (!strncmp(pArray[1], " Alr", 4))  //Already logged
  {
    return;
  }



#ifdef DEBUG
  Serial.print("Send : ");
  Serial.print(sendBuf);
#endif
  BTSerial.write(sendBuf);
}
//stm은 선점형으로 하나의 프로세스가 cpu차지하고 있을 때 더 우선순위가 높은게 실행되면 거기로 넘겨주는 즉 인터럽트 실행중에
//더 중요한 인터럽트가 실행되면 교체되는데 avr은 그렇게 안되서 인터럽트가 너무 길면 교체가 늦어져 그만큼 딜레이가 생김




