/*
	MSXLED Sample program
	Copyright 2023 @V9938
	
	V1.0		1st version
*/

#include <stdio.h>
#include<string.h>
#include <msx.h>
#include <sys/ioctl.h>

#define VERSION "V1.0"
#define DATE "2023/02"

//LED OFF
#define cmd_cl_off 0x03
#define cl_off_size 0x01

//LED position setting
#define cl_pos 0x07
#define cl_pos_size 0x02

//RGB Coler code
#define cl_c_rgb 0x11
#define cl_c_rgb_size 0x04

//LED pattern setting
#define cl_pat 0x09
#define cl_pat_size 0x02

//LED draw
#define cl_draw 0x05
#define cl_draw_size 0x01

//(5Byte) LED Color Parameter set & Draw(RGB) No31
// format: pos(0-9),r,g,b 
#define cmd_cl_p_rgb 0x3f
#define cl_p_rgb_size 0x05

// (31Byte) ALL Color Parameter set & Draw(RGB) No34
#define cl_p_rgball 0x45
#define cl_p_rgball_size 31


//Global
unsigned char SelectSlot;		//Select Slot number
unsigned char ledSlot;			//LED Slot number


//Assembler SubRoutine
void setSlot(){
#asm
    ld a,(_SelectSlot)	;Slot Number
    ld (selSlot),a		;RST30H用にスロット用番号をセットしておく

	ld a,(0fcc1h)		;EXPTBL 
    ld (biosSlot),a		;MSXDOSからBIOS Callする為にMAINROMスロットを事前セットしておく
    ld (biosSlot2),a	;
	ret
#endasm
}
void sendLedCMD(unsigned char cmd,unsigned char size,unsigned char *Address)
{
// MSX LEDのServiceRoutineを使ってLEDコマンドを送信します。
#asm
	ld ix,2
	add ix, sp
	ld a, (ix+4)		;cmd
	ld b, (ix+2)		;size
	ld h, (ix+1)		;address_h
	ld l, (ix+0)		;address_L

	rst	030h
selSlot:
	db		00
	dw		04020h		;LED ROMのServiceルーチンアドレス
	ei
	ret
sltpush: 
	db		00
extpush: 
	db		00
sltmask: 
	db		00
extmask:
	db		00
#endasm
}

unsigned char getStick() __z88dk_fastcall __naked {
// カーソルとジョイスティックの現在の情報を入手します。
#asm
	di
	ld a,0				;KEYBOARD
	call callGtstick	;BIOS GTSTCK
	push af
	ld a,1				;JOY1
	call callGtstick
	pop bc
	add a,b
	push af
	ld a,2				;JOY2
	call callGtstick
	pop bc
	add a,b
	ld l,a
	ei
	ret
	
callGtstick:
	rst 030h
biosSlot:
	db		00
	dw		00d5h		;GTSTCK
	ret
	
#endasm
}

unsigned char getTrig() __z88dk_fastcall __naked {
// スペースキーとジョイスティックのトリガーの現在の情報を入手します。
#asm
	di
	ld a,0				;KEYBOARD
	call callGtTrig
	push af
	ld a,1				;JOY1
	call callGtTrig
	pop bc
	add a,b
	push af
	ld a,2				;JOY2
	call callGtTrig
	pop bc
	add a,b
	ld l,a
	ei
	ret
	
callGtTrig:
	rst 030h
biosSlot2:
	db		00
	dw		00d8h		;GTTRIG
	ret
	
#endasm
}

int chkLedROM() __z88dk_fastcall __naked {
//MSXLEDを探します。
#asm
	di
	in a,(0a8h)			; 現在のスロット状態をバックアップするL
	push af				; 
	ld a,(0ffffh)		; 拡張スロットはBIT反転
	cpl
	push af

    ld a,(_SelectSlot)	;Slot Number
    ld hl,04000h		;Page1
    call 0024h			;call ENASLT

	ld de,ledChkStr-1	;Check Strings
    ld hl,04010h		;Page1
    ld bc,6
chkStrloop:
	inc de				;
	ld a,(de)			;
	cpi					;
	jr nz,notFindROM	;
	jp pe,chkStrloop	;
	

	ld hl,0				; find ROM
	jr restoreSlt

notFindROM:
	ld hl,0ffffh		; Not find ROM

restoreSlt:
	pop af				; 保存していた拡張スロット
	ld b,a				;
	pop af				; 保存していた基本スロット
	out (0a8h),a		; 基本スロットを戻す
	ld a,b				
	ld (0ffffh),a		; 拡張スロットを戻す
	ei
	ret

ledChkStr:
	db	'M','S','X','L','E','D'
#endasm
}

void findLedROM(){
	unsigned char i;
	
	if ((SelectSlot & 0xf0) == 0x80){
		//Expantion Slot Check
		for (i=0;i<4;i++){
			if (chkLedROM() == 0) {
				ledSlot = SelectSlot;
			}
			SelectSlot = SelectSlot + 4;
		}
	}else{
		//Master Slot Check
		if (chkLedROM() == 0) {
			ledSlot = SelectSlot;
		}
	}
}

void wait(int waitnum){
	unsigned char tmp;
	unsigned char *jiffy;
	
	jiffy = (unsigned char *) 0xfc9e;				//JIFFY
	tmp = *jiffy;
	tmp = tmp + (unsigned char) waitnum;
	if (*jiffy > tmp)
		while (*jiffy < 0x80);
		
	while (*jiffy < tmp);

}
int main(int argc,char *argv[])
{

	unsigned char rgbData[4*10];			//RGBの順で格納
	unsigned char ledPosition;				//LED位置
	int i;
	unsigned char stick;
	unsigned char databuf[4];
	
	printf("MSX Gaming LED Cartridge Sample %s\n",VERSION);
	printf("Copyrigth %s @v9938\n\n",DATE);

	printf("Search Cartridge ... ");
	ledSlot = 0;
	//Slot 1 Search
	SelectSlot = *((unsigned char *)0xfcc2) | 0x01;		//EXPTBL (SLOT1)
	findLedROM();
	//Slot 2 Search
	SelectSlot = *((unsigned char *)0xfcc3) | 0x02;		//EXPTBL (SLOT2)
	findLedROM();
	//Slot 3 Search
	SelectSlot = *((unsigned char *)0xfcc4) | 0x03;		//EXPTBL (SLOT3)
	findLedROM();

	if (ledSlot == 0) {								// Not find SimpleROM
		printf("NOT find\n");
		printf("Bye...\n");
		return -1;
	}else{
		printf("Find!\n");
		printf("\nSlot: %02x",ledSlot);
	}

	//Slot set
	SelectSlot = ledSlot;
	setSlot();
	
	
	//LEDを消灯します。
	sendLedCMD(cmd_cl_off,cl_off_size,databuf);		//LED消去CMD
	wait(1);										//完了待ちWait

	//パターンセレクト
	databuf[0] = 0;									//単色描画指定
	sendLedCMD(cl_pat,cl_pat_size,databuf);			//パターン選択
	sendLedCMD(cl_draw,cl_draw_size,databuf);		//一回書き直す
	wait(1);										//完了待ちWait

	//パターンセレクト
	databuf[0] = 4;									//上書きモード
	sendLedCMD(cl_pat,cl_pat_size,databuf);			//パターン選択

	//10LED分、初期パターンのRGBデータを準備
	ledPosition = 0;

	for (i=0;i<=9;i++){
		rgbData[3*i+0] = 0;			//青色に初期化
		rgbData[3*i+1] = 0;
		rgbData[3*i+2] = 127;
	}
	
	//初期パターンを描画
	sendLedCMD(cl_p_rgball,cl_p_rgball_size,&rgbData[0]);		//10LED一括データ入力コマンド&描画（要実行待ち）
//	sendLedCMD(cl_draw,cl_draw_size,databuf);					//LED描画コマンド（要実行待ち）
	wait(1);													//LED描画コマンド指定後は完了を待つ、データ入力～Drawまで実測9ms程度

	//次の描画用の色データを作成

	rgbData[4*0+0] = 0;			//0番のLED
	rgbData[4*0+1] = 0;			//青色
	rgbData[4*0+2] = 0;
	rgbData[4*0+3] = 127;

	rgbData[4*1+0] = 0;			//0番のLED
	rgbData[4*1+1] = 127;		//赤色
	rgbData[4*1+2] = 0;
	rgbData[4*1+3] = 0;
		

	//Flash Erase
	printf("\nCursor = LED Move");
	printf("\nTrig = Program END");
	printf("\n");
	
	while (1){
		if (getTrig() != 0) {
			printf("\n\nDone. Thank you using!\n");
			return 0;
		}
		stick = getStick();
		if (stick != 0) {			//Stickデータ読み取り
			//POSDATAをセット
			rgbData[4*0+0] = ledPosition;							//データ更新するLED位置を指定
			sendLedCMD(cmd_cl_p_rgb,cl_p_rgb_size,&rgbData[4*0]);	//青色に戻す

			if (stick == 3){										//右が押された
				if (ledPosition == 9) ledPosition = 0;
				else ledPosition = ledPosition +1;
			}

			if (stick == 7){										//左が押された
				if (ledPosition == 0) ledPosition = 9;
				else ledPosition = ledPosition -1;
			}

			printf("\rPosition = %d",ledPosition);

			rgbData[4*1+0] = ledPosition;							//データ更新するLED位置を指定
			sendLedCMD(cmd_cl_p_rgb,cl_p_rgb_size,&rgbData[4*1]);	//赤色に戻す
			sendLedCMD(cl_draw,cl_draw_size,databuf);				//LED描画
			//カセット側のCMD処理待ちのWAIT
			wait(1);
		}
		
	}
	

}
