////////////////////////////////////////////////////////////////////////
//
// MSX Gaming Cartridge Control program
// Copyright 2023 @V9938
//	
//	23/01/20 V1.0		1st version
//	23/02/01 V1.1		CMD No0-29を実装
//	23/02/03 V1.2		CMD No30を実装
//	23/02/13 V1.3		CMD No31-32を実装
//	23/02/14 V1.4		CMD No32-35を実装。
//						割り込み周りの変数ハンドリング処理を変更。
//
////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include "aled.h"  // Indoor Corgi アドレサブルRGB LED制御ライブラリを使用

//#define SERIALPRINT_DEBUG
//#define SERIALPRINT_ERROR

//出荷時にLEDデモを有効にするか
#define LEDDEMO_DEF_ENABLE

//時間計測用のCMD IN/OUT有効化
#define CMDINOUT_ENABLE

#define setLEDpos(x) aled.pos = 5.0f + (float) x * 10.0f
#define decodeCMD(i) 0xff & (((unsigned char)(CmdBufferD[i] >> 2)) | (CmdBufferB[i] << 6))

#define errorLedOn		digitalWrite(16,HIGH)
#define errorLedOff		digitalWrite(16,LOW)

#ifdef CMDINOUT_ENABLE
#define cmdLedIn		digitalWrite(15,HIGH)
#define cmdLedOut		digitalWrite(15,LOW)
#else
#define cmdLedIn 
#define cmdLedOut 
#endif


#define CMDBUFFER_SIZE 128*2

#define EEROM_HEAD 0x5A
#define BANKSIZE 0x80

// デモ切り替え時間 10秒
#define DEMOCHG_COUNT 625

//ERROR LED点灯時間 2秒
#define ERRORLED_COUNT 125

volatile unsigned char CmdBufferB[CMDBUFFER_SIZE+2];
volatile unsigned char CmdBufferD[CMDBUFFER_SIZE+2];
volatile unsigned char CmdLength;
volatile uint16_t MsCount;
volatile uint16_t Ms2Count;


volatile bool CmdEnable;
unsigned char CmdHead;
unsigned char CurrentCmdHead;

unsigned char CmdOption;
unsigned char CmdStatus;
unsigned char Cmd;

uint16_t TimeOutMs;
uint16_t ErrorMS;
bool timeOutEnable;

uint16_t ChangeMs;
uint16_t ErrorMs;
uint16_t DemoChangeTime;
unsigned char DemoStartBank;
unsigned char DemoEndBank;

bool DebugOut;
bool LedDemoAll;

uint16_t DemoPattern;
uint16_t DemoCountMs;

bool ChangeEnable;
bool ChangeHueMode;
bool ChangeSatMode;
bool ChangeValMode;
bool ChangePosMode;

bool ResetCmdLength;
bool ResetMsCount;

float ChangeHueMode_sign;
float  ChangeSatMode_sign;
float ChangeValMode_sign;
float ChangePosMode_sign;

//一次保存用ワーク
float pushHue,pushSat,pushVal,pushPos;
uint8_t pushPattern;

// Time Out = 16x6=96ms
#define CMD_TIMEOUT     6

#define WAIT_STARTCMD   0
#define WAIT_CMD        1
#define WAIT_CMDOPTION  2


// COMMAND TABLE

#define MAX_CMD 36

#define cl_debugen  0  // (2Byte) Debug Message
#define cl_off      1  // (1Byte) LED OFF/DEMO STOP  No1
#define cl_draw     2  // (1Byte) LED draw       No2
#define cl_pos      3  // (2Byte) LED position setting No3

#define cl_pat      4  // (2Byte) LED pattern setting  No4
#define cl_eepinit  5  // (1Byte) EEPROM 初期化        No5
#define cl_demo     6  // (4Byte) LED DEMO       No6
#define cl_real     7  // (9Byte) No7 未使用　倍精度テスト用

#define cl_c_rgb    8  // (4Byte) RGB Coler code   No8
#define cl_c_hue    9  // (9Byte) HSV Coler code hue   No9
#define cl_c_sat    10  // (9Byte) HSV Coler code sat   No10
#define cl_c_val    11  // (9Byte) HSV Coler code val   No11

#define cl_p_hue    12  // (9Byte) Gradation Pattern hue step value   No12
#define cl_p_rep    13  // (2Byte) LED pattern repeat value     No13
#define cl_a_enable 14  // (1Byte) LED Animation enable No14
#define cl_a_dis    15  // (1Byte) LED Animation disable No15

#define cl_a_hue    16  // (9Byte) HSV Animation hue    No16
#define cl_a_sat    17  // (9Byte) HSV Animation sat    No17
#define cl_a_val    18  // (9Byte) HSV Animation val    No18
#define cl_a_pos    19  // (9Byte) HSV Animation pos    No19

#define cl_a_modeh  20  // (2Byte) HSV AnimationMode hue   No20
#define cl_a_modes  21  // (2Byte) HSV AnimationMode sat   No21
#define cl_a_modev  22  // (2Byte) HSV AnimationMode val   No22
#define cl_a_modep  23  // (2Byte) HSV AnimationMode pos   No23

#define cl_p_sat	24	// (9Byte) Gradation Pattern sat step value   No24
#define cl_p_val	25	// (9Byte) Gradation Pattern val step value   No25
#define cl_p_aval	26	// (9Byte) Gradation Pattern all val value    No26
#define cl_ee_set   27  // (2Byte) EEPROM Write

#define cl_ee_get   28  // (2Byte) EEPROM Read 
#define cl_ee_cnf   29  // (1Byte) EEPROM Config set
#define cl_a_pause  30  // (2Byte) AnimationMode pause
#define cl_p_rgb    31  // (5Byte) Color Parameter set (RGB format)

#define cl_p_hsv    32  // (6Byte) Color Parameter set (HSV format)
#define cl_null     33  // (1Byte) NULL CMD (Start CMDと共通なのでNULLにしている） No33
#define cl_p_rgball 34  // (31Byte) ALL Color Parameter set & Draw(RGB) No34
#define cl_p_hsvall 35  // (41Byte)  ALL Color Parameter set & Draw(HSV) No35

//Command長テーブル
const unsigned char OptLength[MAX_CMD] = {	0x01,0x00,0x00,0x01, 0x01,0x00,0x03,0x08, 0x03,0x08,0x08,0x08, 
0x08,0x01,0x00,0x00, 0x08,0x08,0x08,0x08, 0x01,0x01,0x01,0x01, 0x08,0x08,0x08,0x01, 0x01,0x00,0x01,0x04, 0x05,0x00,0x1E,0x28};

// アドレサブルRGB制御用のALEDクラスを使う
//  デフォルトは全チャネルに同じデータを送信
//  LED数は使うデバイスに合わせる
ALED aled(CH1, 10,0,0,0,0);

void rgb2hsv(uint8_t red,uint8_t green,uint8_t blue){
	uint8_t max_rgb,min_rgb;

	max_rgb = red;
	if (max_rgb < green) max_rgb = green;
	if (max_rgb < blue) max_rgb = blue; // RGBの中の最大値

	min_rgb = red;
	if (min_rgb > green) min_rgb = green;
	if (min_rgb > blue) min_rgb = blue;

	aled.color.val = ((float)max_rgb/255.0f)*100.0f;

	if (max_rgb != 0.0)  aled.color.sat = ((float)(max_rgb-min_rgb)/(float)max_rgb)*100.0f;
	else aled.color.sat = 0.0f;

	if ((red == green) && (red == blue))aled.color.hue = 0.0f;
	else {
		if (max_rgb == 0)           aled.color.hue = 0.0f;
		else if (max_rgb == red)    aled.color.hue = ((float)(green - blue) / (float)(max_rgb - min_rgb)) * 60.0f;
		else if (max_rgb == green)  aled.color.hue = (2.0f + (float)(blue - red) / (float)(max_rgb - min_rgb)) * 60.0f;
		else                        aled.color.hue = (4.0f + (float)(red - green) / (float)(max_rgb - min_rgb)) * 60.0f;
	}

}
void clearCmd(){
	if (CmdLength == CmdHead)  {

//		noInterrupts();
//		CmdHead = 0;
//		CmdLength = 0;    // CMDバッファーリセット
//		interrupts();

		CmdHead = 0;
		ResetCmdLength = true;		//Lengthリセット要求
	}
	timeOutEnable = false;
	CmdStatus = WAIT_STARTCMD ;
}
void setup() {
// 使うピンを出力に設定
	pinMode(16, OUTPUT);  // Debug port
	pinMode(15, OUTPUT);  // Debug port
	pinMode(14, OUTPUT);  // チャネル1 Pin14, PC0
	pinMode(2, INPUT);   // DAT0
	pinMode(3, INPUT);   // DAT1
	pinMode(4, INPUT);   // DAT2
	pinMode(5, INPUT);   // DAT3
	pinMode(6, INPUT);   // DAT4
	pinMode(7, INPUT);   // DAT5
	pinMode(8, INPUT);   // DAT6
	pinMode(9, INPUT);   // DAT7

	digitalWrite(16,LOW);
	digitalWrite(15,LOW);


//割り込み設定
	EICRA |= (3 << ISC00);    // Trigger on pos edge
	EIMSK |= (1 << INT0);     // Enable external interrupt INT0 = pin3

	TCCR2A = 0;           
	TCCR2B = 0;          
	TCCR2A |= (1 << WGM21);   						//CTC mode
	TCCR2B |= (1 << CS22)|(1 << CS21|(1 << CS20));  //分周1024
	OCR2A   = 250-1;         						//250カウント=16ms
	TIMSK2 |= (1 << OCIE2A);						//TIMER2割り込み有効

	CmdLength = 0;    // CMDバッファーの長さ
	MsCount = 0;      // 1msカウンター（CMD TimeOut管理用）
	CmdHead = 0;
	CmdStatus = WAIT_STARTCMD ;
	timeOutEnable =false;
	ResetCmdLength = true;
	ResetMsCount = true;
	
//attachInterrupt(digitalPinToInterrupt(2),setcmd_func,RISING);

//	MsTimer2::set(16, ms_counter); // 16msカウント
//	MsTimer2::start();




//	sei();                    // Enable global interrupts




// initialize serial and wait for port to open:
	Serial.begin(115200);
//  while (!Serial) {
//    ; // wait for serial port to connect. Needed for native USB port only
//  }
	Serial.println("MSX Gaming LED System");
	Serial.println("firmware rev 1.4(02/14/2023) @v9938");

//LED消灯
	aled.reset(true);

// Temp Buffer初期化
	pushHue = 360.0f;
	pushSat = 100.0f;
	pushVal = 0.0f;
	pushPos = 0.0f;

	noInterrupts();

// アニメーションパターン系初期値
	ChangeMs = 0; 			//更新カウンター
	DemoCountMs = 0;
	DemoPattern = 0;

	setInitParameter();				//初期値設定


	eeprom_get();			//EEPROMの設定値で上書き


	interrupts();
// DEMOモードが有効の場合は値を更新
	if (ChangeEnable) {
		Serial.println("Demo Mode ON");
		setDemoValue(DemoStartBank);
		aled.draw();
	}


	#ifdef SERIALPRINT_ERROR
	if (DebugOut)	Serial.println("User Debug mode ON");
	#endif
	#ifdef SERIALPRINT_DEBUG
	Serial.println("All print mode ON");
	#endif

	Serial.println();


}

void __INT0_vect(){};		//エディタの都合のDummy関数
ISR(INT0_vect)//pin3
{
	if (ResetCmdLength) {
		CmdLength = 0;
		ResetCmdLength = false;
	}
	
	CmdBufferB[CmdLength] = PINB;
	CmdBufferD[CmdLength] = PIND;
	CmdLength++;
}

void __TIMER2_COMPA_vect(){};		//エディタの都合のDummy関数
ISR (TIMER2_COMPA_vect)
//void ms_counter(){
{
	interrupts();     //INT0割り込みは最優先なので有効にしておく
	if (ResetMsCount) {
		MsCount = 0;
		ResetMsCount = false;
	}else{
		MsCount++;
	}
	Ms2Count++;
}
uint16_t marge16bit(uint16_t pos) {
	uint16_t upper,lower;
	uint16_t value;

	upper = 0x7f & ((uint16_t)(decodeCMD(pos+1))>>1);
	lower = 0x7f & ((uint16_t)(decodeCMD(pos+0))>>1);
	value = (upper << 7) | lower;

	return value;

}
float fpow(uint16_t n)
{
	uint16_t i;
	float ret = 1.0;

	for (i = 0; i < n; i++) {
		ret *= 10;
	}

	return ret;
}

//2進化10進数から10進数
float BCDtoDEC(unsigned char* Pointer){
	char bu[10];
	uint8_t b,c,d,e,f,g;
	float dec;
	int8_t s;

	dec = 0.0f;
	b = (*(Pointer+1))>>4;
	c = (*(Pointer+1))&0x0f;
	d = (*(Pointer+2))>>4;
	e = (*(Pointer+2))&0x0f;
	f = (*(Pointer+3))>>4;
	g = (*(Pointer+3))&0x0f;

	dec = (float)b * 100000.0f + (float)c * 10000.0f + (float)d * 1000.0f + (float)e *100.0f + (float)f *10.0f +(float)g;
	#ifdef SERIALPRINT_DEBUG
	Serial.print("\nBCD :  ");
	Serial.println(dec,6);
	#endif
	dec = pow(10.0f,((float)(*(Pointer) & 0x7f)-70.0f))*dec;
	if ((*(Pointer) & 0x80) == 0x80) dec = dec * -1.0f; 
	return dec;
}
float getFloat(){
	unsigned char bcd[4];
	float num;

	bcd[0]=(unsigned char) marge16bit(CmdOption+0);
	bcd[1]=(unsigned char) marge16bit(CmdOption+2);
	bcd[2]=(unsigned char) marge16bit(CmdOption+4);
	bcd[3]=(unsigned char) marge16bit(CmdOption+6);
	num =BCDtoDEC(bcd);

	#ifdef SERIALPRINT_DEBUG
	Serial.print("\ngetFloat :  ");
	Serial.println(num,6);
	#endif

	return num;
}

float getFloat360(){
	float temp;

	temp = getFloat();
//丸め誤差対策
	if (temp >= 360.0f) temp = 360.0f;
	if (temp <= -360.0f) temp = -360.0f;
	return temp;
}
float getFloat100(){
	float temp;

	temp = getFloat();
//丸め誤差対策
	if (temp >= 100.0f) temp = 100.0f;
	if (temp <= -100.0f) temp = -100.0f;
	return temp;
}
void eeprom_setData(int Address){
	int eeAddress = Address;   // 書き込むデータの最初のアドレス


	EEPROM.put(eeAddress, aled.pos);
	eeAddress += sizeof(aled.pos);

	EEPROM.put(eeAddress, aled.repeat);
	eeAddress += sizeof(aled.repeat);

	EEPROM.put(eeAddress, aled.color.hue);
	eeAddress += sizeof(aled.color.hue);

	EEPROM.put(eeAddress, aled.color.sat );
	eeAddress += sizeof(aled.color.sat );

	EEPROM.put(eeAddress, aled.color.val);
	eeAddress += sizeof(aled.color.val);

	EEPROM.put(eeAddress, aled.colorPattern.hue);
	eeAddress += sizeof(aled.colorPattern.hue);

	EEPROM.put(eeAddress, aled.colorPattern.sat );
	eeAddress += sizeof(aled.colorPattern.sat );

	EEPROM.put(eeAddress, aled.colorPattern.val);
	eeAddress += sizeof(aled.colorPattern.val);

	EEPROM.put(eeAddress, ChangeHueMode);
	eeAddress += sizeof(ChangeHueMode);

	EEPROM.put(eeAddress, ChangeSatMode);
	eeAddress += sizeof(ChangeSatMode);

	EEPROM.put(eeAddress, ChangeValMode);
	eeAddress += sizeof(ChangeValMode);

	EEPROM.put(eeAddress, ChangePosMode);
	eeAddress += sizeof(ChangePosMode);


	EEPROM.put(eeAddress, aled.colorChange.hue);
	eeAddress += sizeof(aled.colorChange.hue);

	EEPROM.put(eeAddress, aled.colorChange.sat);
	eeAddress += sizeof(aled.colorChange.sat);

	EEPROM.put(eeAddress, aled.colorChange.val);
	eeAddress += sizeof(aled.colorChange.val);

	EEPROM.put(eeAddress, aled.posChange);
	eeAddress += sizeof(aled.posChange);

	EEPROM.put(eeAddress, aled.pattern);
	eeAddress += sizeof(aled.pattern);

}
void eeprom_setCommon(){
	int eeAddress = 0x10;

	eeAddress += sizeof(DebugOut);

	EEPROM.put(eeAddress, ChangeEnable);
	eeAddress += sizeof(ChangeEnable);
	EEPROM.put(eeAddress, LedDemoAll);
	eeAddress += sizeof(LedDemoAll);
	EEPROM.put(eeAddress, DemoChangeTime);
	eeAddress += sizeof(DemoChangeTime);
	EEPROM.put(eeAddress, DemoStartBank);
	eeAddress += sizeof(DemoStartBank);
	EEPROM.put(eeAddress, DemoEndBank);
	eeAddress += sizeof(DemoEndBank);
	EEPROM.put(eeAddress, DebugOut);
	eeAddress += sizeof(DebugOut);


}
void eeprom_init(){
	int eeAddress = 0x10;

	Serial.println("EEPROM INIT...");

	EEPROM.write(0,EEROM_HEAD);		//HEADER

	EEPROM.put(eeAddress, DebugOut);
	eeAddress += sizeof(DebugOut);

	EEPROM.put(eeAddress, ChangeEnable);
	eeAddress += sizeof(ChangeEnable);
	EEPROM.put(eeAddress, LedDemoAll);
	eeAddress += sizeof(LedDemoAll);
	EEPROM.put(eeAddress, DemoChangeTime);
	eeAddress += sizeof(DemoChangeTime);
	EEPROM.put(eeAddress, DemoStartBank);
	eeAddress += sizeof(DemoStartBank);
	EEPROM.put(eeAddress, DemoEndBank);
	eeAddress += sizeof(DemoEndBank);



	eeprom_setData(BANKSIZE+BANKSIZE*0);	//BANK0
	eeprom_setData(BANKSIZE+BANKSIZE*1);	//BANK1
	eeprom_setData(BANKSIZE+BANKSIZE*2);	//BANK2
	eeprom_setData(BANKSIZE+BANKSIZE*3);	//BANK3
	eeprom_setData(BANKSIZE+BANKSIZE*4);	//BANK4

}

void eeprom_getData(int Address){
	int eeAddress = Address;   // 書き込むデータの最初のアドレス

	EEPROM.get(eeAddress, aled.pos);
	eeAddress += sizeof(aled.pos);

	EEPROM.get(eeAddress, aled.repeat);
	eeAddress += sizeof(aled.repeat);

	EEPROM.get(eeAddress, aled.color.hue);
	eeAddress += sizeof(aled.color.hue);

	EEPROM.get(eeAddress, aled.color.sat );
	eeAddress += sizeof(aled.color.sat );

	EEPROM.get(eeAddress, aled.color.val);
	eeAddress += sizeof(aled.color.val);

	EEPROM.get(eeAddress, aled.colorPattern.hue);
	eeAddress += sizeof(aled.colorPattern.hue);

	EEPROM.get(eeAddress, aled.colorPattern.sat );
	eeAddress += sizeof(aled.colorPattern.sat );

	EEPROM.get(eeAddress, aled.colorPattern.val);
	eeAddress += sizeof(aled.colorPattern.val);


	EEPROM.get(eeAddress, ChangeHueMode);
	eeAddress += sizeof(ChangeHueMode);

	EEPROM.get(eeAddress, ChangeSatMode);
	eeAddress += sizeof(ChangeSatMode);

	EEPROM.get(eeAddress, ChangeValMode);
	eeAddress += sizeof(ChangeValMode);

	EEPROM.get(eeAddress, ChangePosMode);
	eeAddress += sizeof(ChangePosMode);


	EEPROM.get(eeAddress, aled.colorChange.hue);
	eeAddress += sizeof(aled.colorChange.hue);

	EEPROM.get(eeAddress, aled.colorChange.sat);
	eeAddress += sizeof(aled.colorChange.sat);

	EEPROM.get(eeAddress, aled.colorChange.val);
	eeAddress += sizeof(aled.colorChange.val);

	EEPROM.get(eeAddress, aled.posChange);
	eeAddress += sizeof(aled.posChange);

	EEPROM.get(eeAddress, aled.pattern);
	eeAddress += sizeof(aled.pattern);



}

void eeprom_get(){

	int eeAddress = 0x10;

//Header Check
	if (EEPROM.read(0) != EEROM_HEAD) eeprom_init();

	EEPROM.get(eeAddress, DebugOut);
	eeAddress += sizeof(DebugOut);

	EEPROM.get(eeAddress, ChangeEnable);
	eeAddress += sizeof(ChangeEnable);
	EEPROM.get(eeAddress, LedDemoAll);
	eeAddress += sizeof(LedDemoAll);
	EEPROM.get(eeAddress, DemoChangeTime);
	eeAddress += sizeof(DemoChangeTime);
	EEPROM.get(eeAddress, DemoStartBank);
	eeAddress += sizeof(DemoStartBank);
	EEPROM.get(eeAddress, DemoEndBank);
	eeAddress += sizeof(DemoEndBank);

//電源ON時の設定値をロード
	eeprom_getData(BANKSIZE+BANKSIZE*DemoStartBank);			//BANK0 Dataは56Byte
}

void setInitParaAnimation(){
	ChangeEnable = false;		//アニメーションOFF
	LedDemoAll = false;


//アニメーションパターンの設定
//true = 往復 / false=片道
	ChangeHueMode	= false;
	ChangeSatMode	= false;
	ChangeValMode	= false;
	ChangePosMode	= false;

//往復モード時の方向フラグ disable時に戻すこと
	ChangeHueMode_sign	= 1.0f;
	ChangeSatMode_sign	= 1.0f;
	ChangeValMode_sign	= 1.0f;
	ChangePosMode_sign	= 1.0f;

	aled.colorChange.hue = 1.0f;
	aled.colorChange.sat = 0.0f;
	aled.colorChange.val = 0.01f;
	aled.posChange = 1.00f;

#ifdef LEDDEMO_DEF_ENABLE
	ChangeEnable = true;		//アニメーションOFF
	LedDemoAll = true;
#endif

}
void setInitParaDemo(){
// Demo系の初期値設定
	DemoChangeTime = DEMOCHG_COUNT;		//DEMO時間初期値
	DemoStartBank = 0;					//DEMO0
	DemoEndBank = 4;					//DEMO4
	DemoPattern = 0;

}

void setInitParameter(){

	DebugOut = false;			//Debug表示は速度を稼ぐ為にOFF
	setInitParaDemo();			//DEMO系Parameter
	setInitParaAnimation();		//LED Animation Parameter

//初期設定値
	aled.pos = 0.0f;			// LED位置0%
	aled.repeat = 1;			// 全色をLED全部

// 表示する模様の基準の色を指定する
// 今回の例では基準の色は以下の値から変更しない
	aled.color.hue = 0.0f;
	aled.color.sat = 100.0f;
	aled.color.val = 100.0f;

// グラデーション模様の色の変化量を指定する
// 今回の例では以下の虹色模様から変更しない
	aled.colorPattern.hue = 360.0f;
	aled.colorPattern.sat = 0.0f;
	aled.colorPattern.val = 0.0f;

// アニメーションパターン系初期値
	aled.pattern = ALED::patGrad;  // 色相グラデーションパターン1 (100%パターン)
}

void setDemoValue(uint16_t value){

//DEMO番号5～はUSER patternなのでeepromから取得して終わり
	if (value > 4){
		eeprom_getData(BANKSIZE+BANKSIZE*(value-5));			//BANK0 Dataは56Byte
		return;
	}
	
//以下プリセットpattern生成

// グラデーション模様の色の変化量を指定する
// 今回の例では以下の虹色模様から変更しない
	aled.colorPattern.hue = 360.0f;
	aled.colorPattern.sat = 0.0f;
	aled.colorPattern.val = 0.0f;


//アニメーションパターンの設定
//true = 往復 / false=片道
	ChangeHueMode	= false;
	ChangeSatMode	= false;
	ChangeValMode	= true;
	ChangePosMode	= false;

//往復モード時の方向フラグ 必要に応じて戻すこと
//		  ChangeHueMode_sign	= 1.0f;
//		  ChangeSatMode_sign	= 1.0f;
//		  ChangeValMode_sign	= 1.0f;
//		  ChangePosMode_sign	= 1.0f;


//16ms毎の値の変化量設定
	aled.colorChange.hue = 1.0f;
	aled.colorChange.sat = 0.0f;
	aled.colorChange.val = 0.5f;
	aled.posChange = 1.00f;
	aled.pattern = (uint8_t) value;
	
//patternだけ同じだと面白く無いので少し弄る
	if (value == ALED::patSingle) {		// 全体フラッシュ
		aled.colorChange.val = 1.00f;
	}

	if (value == ALED::patGrad) {		// 色相グラデーションパターン1 (100%パターン)
		aled.posChange = 5.00f;
	}

	if (value == ALED::patSym) {		// 色相グラデーションパターン2 (50%パターン)
		aled.posChange = 0.80f;
	}
	if (value == ALED::patPoint1) {		// LED1灯のみ光らせるパターン(他は消灯)
		aled.posChange = 0.50f;
		ChangePosMode	= true;
	}
	if (value == ALED::patPoint2) {		// LED1灯のみデータ更新するパターン(他は前の状態を維持)
		aled.posChange = 2.00f;
		ChangePosMode	= true;
	}
}

void debugDoCMD_InParameter(){

	unsigned char bcd[4];
	char buf[100];
	unsigned char* ddd;
    float ppp;


#ifdef SERIALPRINT_DEBUG
	sprintf(buf, "CMD: 0x%02x   Option: 0x%02d CmdOption:%02d",Cmd,OptLength[Cmd],CmdOption );
	Serial.println(buf);
	Serial.print("DAT:");

	if (Cmd==9){
		marge16bit(CmdOption);
		for (uint8_t i = 0; i < OptLength[Cmd]; i++) {
			sprintf(buf, " 0x%02x", ((decodeCMD(CmdOption+i)) >> 1));
			Serial.print(buf);
		}

	}else if (Cmd==cl_real){

		Serial.print("BUFFER : ");

		for (uint8_t i = 0; i < OptLength[Cmd]; i++) {
			sprintf(buf, " 0x%02x", ((decodeCMD(CmdOption+i)) >> 1));
			Serial.print(buf);
		}
		sprintf(buf, "\n IEEE854 Float bcd: 0x%02x%02x%02x%02x ", (unsigned char) marge16bit(CmdOption),(unsigned char) marge16bit(CmdOption+2),(unsigned char) marge16bit(CmdOption+4),(unsigned char) marge16bit(CmdOption+6));
		Serial.println(buf);


		bcd[0]=(unsigned char) marge16bit(CmdOption+0);
		bcd[1]=(unsigned char) marge16bit(CmdOption+2);
		bcd[2]=(unsigned char) marge16bit(CmdOption+4);
		bcd[3]=(unsigned char) marge16bit(CmdOption+6);
		ddd = (unsigned char*)&ppp;
		ppp = BCDtoDEC(bcd);

		sprintf(buf, " IEEE754 Float    : 0x%02x%02x%02x%02x ", *(ddd+3),*(ddd+2),*(ddd+1),*(ddd+0));
		Serial.print(buf);
		Serial.println(ppp,6);
	}else{
		for (uint8_t i = 0; i < OptLength[Cmd]; i++) {
			sprintf(buf, " 0x%02x", ((decodeCMD(CmdOption+i)) >> 1));
			Serial.print(buf);
		}
	}
	#endif
}

void cmd_cl_debugen(){
	uint16_t value;

// DEBUG OUT設定 (※仕様変更）
	value = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);

	if (value==0) {
		Serial.println("User Debug mode OFF");
		DebugOut = false;

	}else{
		Serial.println("User Debug mode ON");
		DebugOut = true;
	}
//設定値を保存
	EEPROM.put(0x10, DebugOut);

}
void cmd_cl_off(){				//要：実行後Wait命令
	uint8_t tmp_pt;
	float tmp_val;

	// LEDの消去　輝度を0に設定してDrow
	tmp_pt = aled.pattern;
	tmp_val = aled.color.val;

	aled.pattern = ALED::patSingle;  // 全体フラッシュ
	aled.color.val = 0.0f;
	ChangeEnable = false;

// Demo系設定消去
	LedDemoAll = false;
	DemoStartBank = 0;
	DemoEndBank = 0;
	DemoPattern = 0;

//往復モード時の方向フラグ disable時に戻すこと
	ChangeHueMode_sign	= 1.0f;
	ChangeSatMode_sign	= 1.0f;
	ChangeValMode_sign	= 1.0f;
	ChangePosMode_sign	= 1.0f;
	aled.draw();

	aled.reset(true);
//バックアップ値も元に戻す
	aled.pattern = tmp_pt;
	aled.color.val = tmp_val;

}

void cmd_cl_draw(){					////要：実行後Wait命令
// LED描画
	aled.draw();
}

void cmd_cl_pos(){
// LEDポジションセット
	setLEDpos((float)((decodeCMD(CmdOption)) >> 1));
}

void cmd_cl_pat(){
// LEDパターンセット
	uint16_t value;

	value = ((decodeCMD(CmdOption)) >> 1);

		if (value == 0) aled.pattern = ALED::patSingle;  // 全体フラッシュ
		if (value == 1) aled.pattern = ALED::patGrad;  // 色相グラデーションパターン1 (100%パターン)
		if (value == 2) aled.pattern = ALED::patSym;  // 色相グラデーションパターン2 (50%パターン)
		if (value == 3) aled.pattern = ALED::patPoint1;  // LED1灯のみ光らせるパターン(他は消灯)
		if (value == 4) aled.pattern = ALED::patPoint2;  // LED1灯のみデータ更新するパターン(他は前の状態を維持)
}

void cmd_cl_ee_set(){
// EEPROM書き込み (新)
	uint16_t value;

	value = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	if (value < 5) eeprom_setData(BANKSIZE+BANKSIZE*value);
}

void cmd_cl_ee_get(){
// EEPROM読み込み (新)
	uint16_t value;

	value = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	if (value < 5)  eeprom_getData(BANKSIZE+BANKSIZE*value);
}

void cmd_cl_ee_cnf(){
//共通Parameter保存
	eeprom_setCommon();
}

void cmd_cl_eepinit(){
// EEPROM初期化
	setInitParameter();	
	eeprom_init();
}

void cmd_cl_demo(){				//要：実行後Wait命令
// Demo モード設定

	uint16_t value;

// アニメーションパターン
	ChangeEnable = false;		//アニメーションOFFを一旦Disable 
// DEMOパターンセット
	aled.pos = 0.0f;
	aled.repeat = 1;

// 表示する模様の基準の色を指定する
// 今回の例では基準の色は以下の値から変更しない
	aled.color.hue = 0.0f;
	aled.color.sat = 100.0f;
	aled.color.val = 100.0f;

// All demo モードの数値初期化
	DemoCountMs = 0;
//	DemoPattern = 0;
	LedDemoAll = false;

//第1パラメータ、デモスタートパタン番号
	value = ((decodeCMD(CmdOption)) >> 1);
	setDemoValue(value);
	DemoStartBank = value;
	DemoPattern = value;

// 第2パラメータ、デモエンドパタン番号、
// 0や値が第一Parameterより小さい場合はシングルPattern
	value = 0x7f & ((uint16_t)(decodeCMD(CmdOption+1))>>1);
	DemoEndBank = value;

	if (DemoStartBank >= DemoEndBank)	LedDemoAll = false;		// シングルPattern
	else	LedDemoAll = true;									// マルチPattern

// 第3パラメータ、デモの実行時間（秒単位）
	value = 0x7f & ((uint16_t)(decodeCMD(CmdOption+2))>>1);
	if (value == 0 )	DemoChangeTime = DEMOCHG_COUNT;			// 0指定の時は10秒に設定
	else DemoChangeTime = DEMOCHG_COUNT * value / 10;			// 1秒は割り切れないので10倍で一旦計算して割る

	aled.draw();												//一度デモPatternを描画
	ChangeEnable = true;

}


void cmd_cl_c_rgb(){
// LED RGB カラーコードの指定
	uint16_t r_value, g_value, b_value;

	r_value = (decodeCMD(CmdOption));
	g_value = (decodeCMD(CmdOption+1));
	b_value = (decodeCMD(CmdOption+2));
	rgb2hsv(r_value,g_value,b_value);

}
void cmd_cl_c_hue(){
// LED Hue カラーコードの指定
	aled.color.hue = getFloat360();
}

void cmd_cl_c_sat(){
// LED のHSV方式の色の濃さを設定します。
	aled.color.sat = getFloat100();
}

void cmd_cl_c_val(){
// LED のHSV方式の明るさを設定します。
	aled.color.val = getFloat100();
}

void cmd_cl_p_hue(){
// グラデーションパターン時の色相の変化量を指定します。
	aled.colorPattern.hue = getFloat360();
}

void cmd_cl_p_sat(){
// グラデーションパターン時の色相の変化量を指定します。
	aled.colorPattern.sat = getFloat100();
}
void cmd_cl_p_val(){
// グラデーションパターン時の色相の変化量を指定します。
	aled.colorPattern.val = getFloat100();
}

void cmd_cl_p_aval(){
// グラデーションパターン時の色相の変化量を指定します。
	aled.val = getFloat100();
}

void cmd_cl_p_rep(){
// グラデーションパターン時のLED繰り返し数を指定します。
	uint16_t lower;

	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	aled.repeat = lower;
}
void debugPrIntVal(){
	if (DebugOut){
		Serial.print("cl_a_enable :\ncolorChange hue ");
		Serial.print(aled.colorChange.hue,6);
		Serial.print(" | sat ");
		Serial.print(aled.colorChange.sat,6);
		Serial.print(" | val ");
		Serial.print(aled.colorChange.val,6);
		Serial.print(" | pos ");
		Serial.println(aled.posChange,6);

		Serial.print("color hue ");
		Serial.print(aled.color.hue,6);
		Serial.print(" | sat ");
		Serial.print(aled.color.sat,6);
		Serial.print(" | val ");
		Serial.print(aled.color.val,6);
		Serial.print(" | pos ");
		Serial.println(aled.pos,6);

		Serial.print("colorPattern hue ");
		Serial.print(aled.colorPattern.hue,6);
		Serial.print(" | sat ");
		Serial.print(aled.colorPattern.sat,6);
		Serial.print(" | val ");
		Serial.print(aled.colorPattern.val,6);
		Serial.print(" | pos ");
	}
}

void cmd_cl_a_enable(){
// Animation処理の有効化
	ChangeEnable = true;
	if (DemoStartBank >= DemoEndBank)	LedDemoAll = false;		// シングルPattern
	else	LedDemoAll = true;									// マルチPattern
//	debugPrIntVal();											// 現在値のUART値への出力

}
void cmd_cl_a_dis(){
// Animation処理の無効化
	ChangeEnable = false;
	LedDemoAll = false;
//無効化したので方向はすべて正方向に変更する
	ChangeHueMode_sign	= 1.0f;
	ChangeSatMode_sign	= 1.0f;
	ChangeValMode_sign	= 1.0f;
	ChangePosMode_sign	= 1.0f;
}



void cmd_cl_a_hue(){
//Animationパターン用Hue更新値を設定
	aled.colorChange.hue = getFloat360();
//往復モード時の方向フラグ disable時に戻すこと
	ChangeHueMode_sign	= 1.0f;
	ChangeSatMode_sign	= 1.0f;
	ChangeValMode_sign	= 1.0f;
	ChangePosMode_sign	= 1.0f;
}
void cmd_cl_a_sat(){
//Animationパターン用sat更新値を設定
	aled.colorChange.sat = getFloat100();
}
void cmd_cl_a_val(){
//Animationパターン用val更新値を設定
	aled.colorChange.val = getFloat100();
}
void cmd_cl_a_pos(){
//Animationパターン用pos更新値を設定
	aled.posChange = getFloat100();
}

void cmd_cl_a_modeh(){
//Animationパターン用モード設定Hue
	uint16_t lower;
	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	if (lower == 0) ChangeHueMode = false;
	else ChangeHueMode = true;

//往復モード時の方向フラグ disable時に戻すこと
	ChangeHueMode_sign	= 1.0f;

}

void cmd_cl_a_modes(){
//Animationパターン用モード設定
	uint16_t lower;

	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	if (lower == 0) ChangeSatMode = false;
	else ChangeSatMode = true;

//往復モード時の方向フラグ disable時に戻すこと
	ChangeSatMode_sign	= 1.0f;
}

void cmd_cl_a_modev(){
//Animationパターン用モード設定val
	uint16_t lower;

	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);
	if (lower == 0) ChangeValMode = false;
	else ChangeValMode = true;

//往復モード時の方向フラグ disable時に戻すこと
	ChangeValMode_sign	= 1.0f;
}

void cmd_cl_a_modep(){
//Animationパターン用モード設定pos
	uint16_t lower;

	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);

	if (lower == 0) ChangePosMode = false;
	else ChangePosMode = true;

//往復モード時の方向フラグ disable時に戻すこと
	ChangePosMode_sign	= 1.0f;

}

void cmd_cl_a_pause(){
//Animationパターンの一旦停止
	uint16_t lower;

	lower = 0x7f & ((uint16_t)(decodeCMD(CmdOption+0))>>1);

	if (lower != 0) {
		// Animation処理の無効化(cmd_cl_a_enableは方向フラグをclearするので別処理)
		ChangeEnable = false;
		LedDemoAll = false;
		//現在値を保存しておく
		pushHue = aled.color.hue;
		pushSat = aled.color.sat;
		pushVal = aled.color.val;
		pushPos = aled.pos;
	}else{
		//カラーコードとPOS値を元に戻す
		aled.color.hue = pushHue;
		aled.color.sat = pushSat;
		aled.color.val = pushVal;
		aled.pos = pushPos;
		cmd_cl_a_enable();
	}
}
void cmd_cl_p_rgb(){
//RGBモードでのPOS/HSV/DRAWを一括で実施する

	uint16_t r_value, g_value, b_value;
	uint16_t pos;

// LEDポジションセット
	pos = ((decodeCMD(CmdOption)) >> 1);
	setLEDpos((float)((decodeCMD(CmdOption)) >> 1));
	r_value = (decodeCMD(CmdOption+1));
	g_value = (decodeCMD(CmdOption+2));
	b_value = (decodeCMD(CmdOption+3));
	rgb2hsv(r_value,g_value,b_value);
	aled.loadLedData(pos,aled.color);

//	aled.draw();
}

void cmd_cl_p_rgball(){
//RGBモードで10LED分設定を一括で実施する

	uint16_t r_value, g_value, b_value;
	int i;

// LEDポジションセット
	for (i=0;i<10;i++){
		r_value = (decodeCMD(CmdOption+i*3+0));
		g_value = (decodeCMD(CmdOption+i*3+1));
		b_value = (decodeCMD(CmdOption+i*3+2));
		rgb2hsv(r_value,g_value,b_value);
		aled.loadLedData(i,aled.color);
	}
	setLEDpos(0.0f);								//POS位置はとりあえず左にしておく
	aled.draw();

}

void cmd_cl_p_hsv(){
//hsvモードでのPOS/HSVを10LED分一括で実施する
//このコマンドでは他のHSV形式と違いuint値をもらいます。
	uint16_t pos;
// LEDポジションセット
	pos = ((decodeCMD(CmdOption)) >> 1);
	setLEDpos((float)((decodeCMD(CmdOption)) >> 1));
	aled.color.hue = (float)((decodeCMD(CmdOption+1)>>1)+128*(decodeCMD(CmdOption+2)>>1));
	aled.color.sat = (float)(decodeCMD(CmdOption+3)>>1);
	aled.color.val = (float)(decodeCMD(CmdOption+4)>>1);
	aled.loadLedData(pos,aled.color);

//	aled.draw();
}
void cmd_cl_null(){
#ifdef SERIALPRINT_DEBUG
		Serial.println("NULL CMD");
#endif

}

void cmd_cl_p_hsvall(){
//hsvモードでのPOS/HSVを一括で実施する
//このコマンドでは他のHSV形式と違いuint値をもらいます。
	int i;
// LEDポジションセット
	for (i=0;i<10;i++){
		aled.color.hue = (float)((decodeCMD(CmdOption+i*4+0)>>1)+128*(decodeCMD(CmdOption+i*4+1)>>1));
		aled.color.sat = (float)(decodeCMD(CmdOption+i*4+2)>>1);
		aled.color.val = (float)(decodeCMD(CmdOption+i*4+3)>>1);
		aled.loadLedData(i,aled.color);
	}

	setLEDpos(0.0f);								//POS位置はとりあえず左にしておく
	aled.draw();
}


void cmd_cl_real(){
	float temp;
	char buf[64];

	temp = getFloat360();


	if (DebugOut){
// 自動動作テスト用
		Serial.print(temp);
		Serial.print(", ");
		Serial.print(aled.color.hue,8);
		Serial.print(", ");
		Serial.print(aled.color.sat,8);
		Serial.print(", ");
		Serial.print(aled.color.val,8);
		Serial.print(", ");
		Serial.print(aled.pos,8);
		Serial.print(", ");
		Serial.print(aled.pattern);
		Serial.print(", ");
		Serial.print(aled.colorPattern.hue,8);
		Serial.print(", ");
		Serial.print(aled.colorPattern.sat,8);
		Serial.print(", ");
		Serial.print(aled.colorPattern.val,8);
		Serial.print(", ");
		Serial.print(aled.repeat,8);
		Serial.print(", ");
		Serial.print(aled.val,8);
		Serial.print(", ");
		Serial.print(ChangeHueMode);
		Serial.print(", ");
		Serial.print(aled.colorChange.hue);
		Serial.print(", ");
		Serial.print(ChangeSatMode);
		Serial.print(", ");
		Serial.print(aled.colorChange.sat);
		Serial.print(", ");
		Serial.print(ChangeValMode);
		Serial.print(", ");
		Serial.print(aled.colorChange.val);
		Serial.print(", ");
		Serial.print(ChangePosMode);
		Serial.print(", ");
		Serial.print(aled.posChange);
		Serial.print(", ");
		Serial.print(ChangeEnable);
		Serial.print(", ");
		Serial.print(LedDemoAll);
		Serial.print(", ");
		Serial.print(DemoStartBank);
		Serial.print(", ");
		Serial.print(DemoEndBank);
		Serial.println(", ");
	}else{
		Serial.print("real: ");
		Serial.println(temp);
//real CMDの実体
	debugPrIntVal();			// 現在値のUART値への出力
	}
}

void (*cmdExec[])(void)={
	cmd_cl_debugen, // (2Byte) Debug Message
	cmd_cl_off   ,  // (1Byte) LED OFF/DEMO STOP  No1
	cmd_cl_draw  ,  // (1Byte) LED draw       No2
	cmd_cl_pos   ,  // (2Byte) LED position setting No3

	cmd_cl_pat   ,  // (2Byte) LED pattern setting  No4
	cmd_cl_eepinit, // (1Byte) EEPROM 初期化        No5
	cmd_cl_demo  ,  // (4Byte) LED DEMO       No6
	cmd_cl_real  ,  // (9Byte) No7 未使用　倍精度テスト用

	cmd_cl_c_rgb ,  // (4Byte) RGB Coler code   No8
	cmd_cl_c_hue ,  // (9Byte) HSV Coler code hue   No9
	cmd_cl_c_sat ,  // (9Byte) HSV Coler code sat   No10
	cmd_cl_c_val ,  // (9Byte) HSV Coler code val   No11

	cmd_cl_p_hue ,  // (9Byte) Gradation Pattern hue step value   No12
	cmd_cl_p_rep ,  // (2Byte) LED pattern repeat value     No13
	cmd_cl_a_enable,// (1Byte) LED Animation enable No14
	cmd_cl_a_dis ,  // (1Byte) LED Animation disable No15

	cmd_cl_a_hue ,  // (9Byte) HSV Animation hue    No16
	cmd_cl_a_sat ,  // (9Byte) HSV Animation sat    No17
	cmd_cl_a_val ,  // (9Byte) HSV Animation val    No18
	cmd_cl_a_pos ,  // (9Byte) HSV Animation pos    No19

	cmd_cl_a_modeh, // (2Byte) HSV AnimationMode hue   No20
	cmd_cl_a_modes, // (2Byte) HSV AnimationMode sat   No21
	cmd_cl_a_modev, // (2Byte) HSV AnimationMode val   No22
	cmd_cl_a_modep, // (2Byte) HSV AnimationMode pos   No23

	cmd_cl_p_sat,	// (9Byte) Gradation Pattern sat step value   No24
	cmd_cl_p_val,	// (9Byte) Gradation Pattern val step value   No25
	cmd_cl_p_aval,	// (9Byte) Gradation Pattern all val value    No26
	cmd_cl_ee_set,  // (2Byte) EEPROM Write No27

	cmd_cl_ee_get,  // (2Byte) EEPROM Read No28
	cmd_cl_ee_cnf,  // (1Byte) EEPROM Config set No29
	cmd_cl_a_pause,	// (2Byte) AnimationMode pause No30
	cmd_cl_p_rgb,	// (5Byte) Color Parameter set(RGB) No31

	cmd_cl_p_hsv,	// (6Byte) Color Parameter set(HSV) No32
	cmd_cl_null,	// (1Byte) NULL CMD (Start CMDと共通なのでNULLにしている） No33
	cmd_cl_p_rgball, // (31Byte) ALL Color Parameter set & Draw(RGB) No34
	cmd_cl_p_hsvall	 // (41Byte)  ALL Color Parameter set & Draw(HSV) No35
};

void doCMD(){
// 受信Command処理ルーチン
// 内部のSwitchで更に細かい処理に分岐する
	uint16_t upper,lower;
	float ppp;
	uint8_t tmp_pt;
	float tmp_val;

	cmdLedIn;
	//for Debug (InputParameter check)
	debugDoCMD_InParameter();				// InputParameterのデバッグ用表示
	cmdExec[(int)Cmd]();							// CMD実体
	clearCmd();
	cmdLedOut;

#ifdef SERIALPRINT_DEBUG
	Serial.println("\nCMD End");
#endif

}

void ledAnimation() {
// アニメーション用にパラメーターを更新する
//   colorChange, posChange, valChange を使って color, pos , val を変更
	aled.color.hue += (ChangeHueMode_sign * aled.colorChange.hue);
	if (aled.color.hue > 360.0f) {
		if (ChangeHueMode){
			ChangeHueMode_sign = -1.0f;
			aled.color.hue += 2*(ChangeHueMode_sign * aled.colorChange.hue);
		}else{
			ChangeHueMode_sign = 1.0f;
			aled.color.hue -= 360.0f;
		}
	}

	if (aled.color.hue < 0.0f) {
		if (ChangeHueMode){
			ChangeHueMode_sign = 1.0f;
			aled.color.hue += 2*(ChangeHueMode_sign * aled.colorChange.hue);
		}else{
			ChangeHueMode_sign = 1.0f;
			aled.color.hue += 360.0f;
		}
	}

	aled.color.sat += (ChangeSatMode_sign * aled.colorChange.sat);
	if (ChangeSatMode){
		if (aled.color.sat > 99.999f){			//100.0fだと丸め誤差で0になる場合がある
			ChangeSatMode_sign = -1.0f;
			aled.color.sat += 2*(ChangeSatMode_sign * aled.colorChange.sat);
		}
	}else{
		if (aled.color.sat > 100.0f){
			ChangeSatMode_sign = 1.0f;
			aled.color.sat -= 100.0f;
		}
	}

	if (aled.color.sat < 0.0f){
		if (ChangeSatMode){
			ChangeSatMode_sign = 1.0f;
			aled.color.sat += 2*(ChangeSatMode_sign * aled.colorChange.sat);
		}else{
			ChangeSatMode_sign = 1.0f;
			aled.color.sat += 100.0f;
		}
	}

	aled.color.val += (ChangeValMode_sign * aled.colorChange.val);

	if (ChangeValMode){
		if (aled.color.val > 99.999f){			//100.0fだと丸め誤差で0になる場合がある
			ChangeValMode_sign = -1.0f;
			aled.color.val += 2*(ChangeValMode_sign * aled.colorChange.val);
		}
	}else{
		if (aled.color.val > 100.0f) {
			ChangeValMode_sign = 1.0f;
			aled.color.val -= 100.0f;
		}
	}

	if (aled.color.val < 0.0f){
		if (ChangeValMode){
			ChangeValMode_sign = 1.0f;
			aled.color.val += 2*(ChangeValMode_sign * aled.colorChange.val);
		}else{
			ChangeValMode_sign = 1.0f;
			aled.color.val += 100.0f;
		}
	} 


	aled.pos += (ChangePosMode_sign * aled.posChange);
	if (ChangePosMode){
		if (aled.pos > 99.999f){			//100.0fだと丸め誤差で0になる場合がある
			ChangePosMode_sign = -1.0f;
			aled.pos += 2*(ChangePosMode_sign * aled.posChange);
		}
	}else{
		if (aled.pos > 100.0f){
			ChangePosMode_sign = 1.0f;
			aled.pos -= 100.0f;
		}
	}

	if (ChangePosMode){
		if (aled.pos < 0.0f){
			ChangePosMode_sign = 1.0f;
			aled.pos += 2*(ChangePosMode_sign * aled.posChange);
		}
	}else{
		if (aled.pos < 0.0f){
			ChangePosMode_sign = 1.0f;
			aled.pos += 100.0f;
		}
	}

}


//アニメーション系のフラグ
//hue値の変化量
void loop() {
	char buf[60];
	uint16_t tmpMsCount;

	interrupts();													//割り込み許可

// CMD関係の処理
	if ((ResetCmdLength==false) && (CmdLength != CmdHead)){				// CMD入力有り
		timeOutEnable = true;											// TimeOutフラグを有効化
		CurrentCmdHead = CmdHead;										// 今のHead位置を移す
		CmdHead++;														// 次のSearch位置を更新
		tmpMsCount = MsCount;											// TimeOut計測用カウンター値取得
		if ((tmpMsCount + CMD_TIMEOUT) <  TimeOutMs){					// TimeOut値の計算結果が0戻りの場合の対策
			 ResetMsCount = true;										// 次の割り込みでカウンターを初期化
		}else{
			TimeOutMs = MsCount + CMD_TIMEOUT;							// TimeOut値を設定
		}

		//CmdStatus: WAIT_STARTCMD(MagicWord待ち)
		if (CmdStatus == WAIT_STARTCMD){
			// Start CMDが来た時の処理
			if ((decodeCMD(CurrentCmdHead))== 0x43)  {					// StartCMD:0x43
				CmdStatus = WAIT_CMD ;								// 次のステータスへ
			}else{
			// Start CMD以外の書き込みがあった場合の処理（BIOSのRAM Searchとかで普通にある）
#ifdef SERIALPRINT_DEBUG
				sprintf(buf, "WAIT_STARTCMD Unknow: 0x%02x L: %d H: %d to: %d t:%d", (decodeCMD(CurrentCmdHead)),CmdLength,CmdHead,TimeOutMs,MsCount);
				Serial.println(buf);
#endif
				ErrorMS = 1;															// 
				clearCmd();																// 現在のCMDをclear
			}
		}else if (CmdStatus == WAIT_CMD){
			//CmdStatus: WAIT_CMD(CMD書き込み待ち)

			if (((decodeCMD(CurrentCmdHead))>>1) < MAX_CMD)  {								//CMDが有効範囲かチェック

				Cmd = (decodeCMD(CurrentCmdHead));
				Cmd = (Cmd >>1) ;              											//CMD番号
				CmdOption = CmdHead;    												//CMD オプションのスタート位置
				CmdStatus = WAIT_CMDOPTION ;											//CMD OPTION待ちステータス
#ifdef SERIALPRINT_DEBUG
				sprintf(buf, "WAIT_CMD CMD: 0x%02x L: %d H: %d", Cmd,CmdLength,CmdHead);
				Serial.println(buf);
#endif
				if(OptLength[Cmd] == 0) {												//Option無しのCMDの場合即時実行
					doCMD();
				}
			}else{
				// 範囲外の無効コマンドが来た場合
#ifdef SERIALPRINT_ERROR
				if (DebugOut){
					sprintf(buf, "WAIT_CMD Unknow: 0x%02x L: %d H: %d to: %d t:%d", (decodeCMD(CurrentCmdHead)),CmdLength,CmdHead,TimeOutMs,MsCount);
					Serial.println(buf);
				}
#endif
				ErrorMS = 1;
				clearCmd();									//無効コマンド
			}
		}else if (CmdStatus == WAIT_CMDOPTION){
			//CmdStatus: WAIT_CMDOPTION(CMDの引数が来るのを待つ)
			if ((CmdOption+OptLength[Cmd]) == CmdHead) {	//CMD長まで来たら有効化

				doCMD();									//CMD実行
			}
		}else{
			//CmdStatus: その他　（通常は絶対踏まない)
#ifdef SERIALPRINT_ERROR
			if (DebugOut){
				Serial.println("Invaild Status");
			}
#endif
			ErrorMS = 1;
			clearCmd();									//無効コマンド
		}
	}

	// BufferのOverflow検出処理
	// UART処理で時間がかからない限り通常ここは通らない。
	if (CmdHead == CMDBUFFER_SIZE-1){
		#ifdef SERIALPRINT_ERROR
		if (DebugOut){
			Serial.println("CMD Buffer Over");
		}
		#endif

		ErrorMS = 1;
		clearCmd();
	}
	//TimeOut処理

	if ((ResetMsCount==false)&&(timeOutEnable == true)&&(TimeOutMs < MsCount)){
		//CMD Time Out 96ms
#ifdef SERIALPRINT_ERROR
		if(DebugOut){
			Serial.println("CMD Time out");
		}
#endif
		ErrorMS = 1;
		clearCmd();
	}

//16ms単位でやる処理(MsCountはTime Out処理で初期化される場合があるのでMs2Countを参照する)
	if (ChangeMs != Ms2Count){					//16msカウンタが更新された時だけ処理を実施

		if (ErrorMS != 0) {						//ErrorLEDの処理 PORT16にLEDを取り付けると見える
			ErrorMS++;
			errorLedOn;
		}
		if (ErrorMS == ERRORLED_COUNT) {
			ErrorMS = 0;
			errorLedOff;
		}
		

		ChangeMs++;								//比較用カウンタ値更新
		if (ChangeEnable){	
			if (LedDemoAll){					//Demopattern切り替え機能が有効の場合
				if (DemoCountMs != DemoChangeTime) DemoCountMs++;			//デモ切り替え時間カウンタを更新
				else {
					// Demo pattern切り替え処理
					DemoCountMs = 0;												//デモ切り替えカウンタ初期化
					if (DemoPattern == DemoEndBank) DemoPattern = DemoStartBank;	//今の設定値が最後だったら戻る
					else DemoPattern++;												//設定値を次に
					setDemoValue(DemoPattern);										//デモ/保存していた設定値に更新
				}
			}

			ledAnimation();						//LEDのアニメーション処理
			if ((ResetCmdLength==true) || (CmdHead== CmdLength)){					//CMDBufferにデータがある場合はそちらの処理を優先する
				aled.draw();													//LED描画中は割り込みが無効でコマンドが受け取れないので注意(800us)
			}
		}
	}

}
