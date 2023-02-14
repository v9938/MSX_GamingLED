# BASIC以外から扱う場合の情報  

## LED Gamming Cartridgeの通信フォーマット  
000h 043h 000h CMD番号 ... の順番でこのCartridgeのスロットに書き込むことで実行する。  
000hの部分はbit0を0のデータであれば何でも良いです。  

書き込みアドレスはBIOSでは07F00hを使っています。  
回路上はアドレスをデコードをしていないので書き込みアドレスは適当でも動作します。  

bit0を1にしたデータを取り込む仕様なので、有効データは7bit分です。  
8bit分のデータを送信する場合は ([bit6-0],1),(000000b,[bit7],1)に分けて送信が必要です。  
Data-Data間は10us程度のWaitが必要です、そのためLDIRは使えません。  

各コマンドは処理完了までには数ms程度の時間がかかります。  
内部にBufferを持っていて順次処理をしていきますが、  
64Byte分なので一度に送信しすぎると、コマンドをロストするので注意してください。  

## LED Cartridgeの識別子  
0x4010～の文字列が'MSXLED"というスロットを探してください。  
当該スロットが本Cartridgeになります。  

## Service Routineについて  
0x4020にCMD送信のServiceルーチンが置いてあります。  
### LEDコマンド送信 (4020h)  

<function>  
LEDCartridgeにコマンドを送信します。  
  
	A   実行するコマンド
	B   コマンドサイズ（送信データサイズ）
	HL  送信CMDバッファーの先頭アドレス  

	破壊されるレジスタ：  すべて  

各コマンドの引数は下記フォーマットでRAM上に置き、HLレジスタに先頭アドレス、  
Aレジスタにはコマンド、Bレジスタに送信データサイズをセットしCALLすれば、LED Cartridgeにコマンドがセットされます。  
送信データは、0-127の数値であることが必要です。ただし、9Byte指定のみ4ByteのBCD floatフォーマットが引数です。

	1byte：無し  
	2byte：'[parameter1(7bit 0-127)]'  
	4byte：'[parameter1(7bit 0-127)] [parameter2(7bit 0-127)] [parameter3(7bit 0-127)]'  
	9byte：'[BCD float数値(4byte)]' ※内部で8Byteに変換します  
	その他：データの並び順に送信します。


## コマンド番号一覧  


|コマンドラベル  | コマンド番号 | 命令長 | 内容 |
| - | - | - | - |
|c_cmd_st   |     043h	| - | CMDスタート識別子（Serviceルーチン仕様時は不用）  
|  |  |  |  |
|cl_debugen	|		001h | 1Byte | LED Debug UART On |
|cl_off      |     003h | 1Byte | LED OFF/DEMO STOP |
|cl_draw     |     005h | 1Byte | LED draw |
|cl_pos      |     007h | 2Byte | LED position setting |
|cl_pat      |     009h | 2Byte | LED pattern setting |
|cl_eepinit  |     00bh | 1Byte | EEPROM Init |
|cl_demo     |     00dh | 4Byte | LED DEMO |
|cl_real     |     00fh | 9Byte | LED REAL Test |
|  |  |  |  |
|cl_c_rgb    |     011h | 4Byte | RGB Coler code |
|cl_c_hue    |     013h | 9Byte | HSV Coler code hue |
|cl_c_sat    |     015h | 9Byte | HSV Coler code sat |
|cl_c_val    |     017h | 9Byte | HSV Coler code val |
|cl_p_hue    |     019h | 9Byte | Gradation Pattern hue step value |
|cl_p_rep    |     01bh | 2Byte | LED pattern repeat value |
|cl_a_ena| 01dh | 1Byte | LED Animation enable |
|cl_a_dis    | 01fh | 1Byte | LED Animation disable |
|  |  |  |  |
|cl_a_hue    | 021h | 9Byte | HSV Animation hue |
|cl_a_sat    | 023h | 9Byte | HSV Animation sat |
|cl_a_val    | 025h | 9Byte | HSV Animation val |
|cl_a_pos    | 027h | 9Byte | HSV Animation pos |
|cl_a_modeh  | 029h | 2Byte | HSV AnimationMode hue |
|cl_a_modes  | 02bh | 2Byte | HSV AnimationMode sat |
|cl_a_modev  | 02dh | 2Byte | HSV AnimationMode val |
|cl_a_modep  | 02fh | 2Byte | HSV AnimationMode val |
|  |  |  |  |
|cl_p_sat	| 031h | 9Byte | Gradation Pattern sat step value |
|cl_p_val	| 033h | 9Byte | Gradation Pattern val step value |
|cl_p_aval	| 035h | 9Byte | Gradation Pattern all val value  |
|cl_ee_set	| 037h | 2Byte | EEPROM Write |
|cl_ee_get	| 039h | 2Byte | EEPROM Read |
|cl_ee_cnf	| 03bh | 1Byte | EEPROM Config set |
|cl_a_pause  | 03dh | 2Byte | AnimationMode pause |
|cl_p_rgb	| 03fh | 5Byte | (V1.3～) Color Parameter set (RGB format) |
|  |  |  |  |
|cl_p_hsv	| 041h | 6Byte | (V1.3～)  Color Parameter set (HSV format) |
|cl_null	| 043h | 1Byte |  (V1.3～) NULL CMD (Start CMDと共通なのでNULLにしている）  |
|cl_p_rgball | 045h | 31Byte |  (V1.4～) ALL Color Parameter set & Draw(RGB)  |
|cl_p_hsvall | 047h | 41Byte |  (V1.4～) ALL Color Parameter set & Draw(HSV) |
