# 拡張BASICコマンドの解説  
  
##拡張されるBASICのコマンドについて  
Cartridgeに搭載されているROMにて、以下の命令が拡張されます。  
  
各コマンドは処理完了までには数ms程度の時間がかかります。  
命令側でWait処理をいれていません。内部にBufferを持っていて順次処理をしていきます  
が、一度に送信しすぎるとCMDをロストする場合があるので注意してください。  
  
# MSXLED 拡張命令  
## CALL LED_DEBUG(<0-1>)  

### 機能   
LED DEBUG UARTを表示を有効化します。  

### 引数  
0 : デバッグメッセージ OFF  
1 : デバッグメッセージ ON  
  
## CALL LED_OFF  
  
### 機能   
LEDを消灯します。  
  
## CALL LED_DRAW  
  
### 機能   
LEDを描画します。  
  
## CALL LED_POS (\<led location 0-9>)  
### 機能    
描画するLEDのポジションを設定します。左から0,1,2...になります  

### 引数  
led location = 制御するLEDの位置(0-9)

## CALL LED_PT (\<pattern>)  
### 機能    
描画するLEDパターンを定義します。  

### 引数  
pattern = LEDの表示パターン  
0 : パターンSingle ... 全LED単一色の塗り潰しです。  
1 : パターンGrad   ... 色相グラデーションパターン1 (100%パターン)  
2 : パターンSym    ... 色相グラデーションパターン2 (50%パターン)  
3 : パターンPoint1 ... LED1灯のみ光らせるパターン(他は消灯)  
4 : パターンPoint2 ... LED1灯のみデータ更新するパターン(他は前の状態を維持)  

## CALL LED_DEMO (\<pattern>,[End Pattern],[Demo time])  

### 機能    
LEDのデモを実行します。  

### 引数  
pattern = デモパターンの指定  
0 :    パターンSingle ... 全LED単一色の塗り潰しです。  
1 :    パターンGrad   ... 色相グラデーションパターン1 (100%パターン)  
2 :    パターンSym    ... 色相グラデーションパターン2 (50%パターン)  
3 :    パターンPoint1 ... LED1灯のみ光らせるパターン(他は消灯)  
4 :    パターンPoint2 ... LED1灯のみデータ更新するパターン(他は前の状態を維持)  
5～9 : ユーザパターン ... ユーザparameter0-4で保存した値  
  
End Pattern =  終了パターン番号を指定、省略した場合はシングルパターンになります  
  
Demo time = パターンの切り替え時間を指定(0-127)、省略した場合は10秒で切り替わります。  
  
  
## CALL LED_DEMOON  
  
### 機能    
現在の設定値でLEDのデモを設定します。  
設定すると、電源を入れておくと次回も同じデモが勝手にスタートします。  
  
## CALL LED_DEMOOFF  
  
### 機能    
LEDのデモ設定を切ります。  
  
## CALL LED_SAVEINIT  
  
### 機能    
LEDのユーザ設定値を初期値に戻します。  
  
## CALL LEDC_RGB (r,g,b)  

### 機能    
RGB値でカラーコードを設定します。システム内部ではHSV方式で管理しているため  
CALL LEDC_HUE/SAT/VALで設定される値は上書きされます。  

### 引数  
r = Red   (0-127)  
g = Green (0-127)  
b = Blue  (0-127)  

## CALL LEDC_HUE(\<hue>)  

### 機能    
HSV方式の色相を設定します。  

### 引数  
hue = 色相 0-360  

## CALL LEDC_SAT (\<sat>)  

### 機能    
HSV方式の色の濃さを設定します。  

### 引数  
sat = 色の濃さ 0-100  
  
## CALL LEDC_VAL (\<val>)  

### 機能    
HSV方式の明るさを設定します。  

### 引数  
val = 明るさ 0-100  
  
## CALL LEDPT_HUE(\<hue>)  

### 機能    
グラデーションパターン時の色相の変化量を指定します。  

### 引数  
hue = 色相 0-360  

## CALL LEDPT_REP (0-10)  
### 機能    
描画するLEDの模様の繰り返し回数を設定します  
  
## CALL LEDTEST (\<val>)  

### 機能    
テスト用当該スロットに1Byteデータ書き込みを実施します。  
通信の仕様上7bit分しか通信できません  

### 引数  
val = 0-127  

## CALL LED_REAL (\<val>)  

### 機能    
クロスチェック用のテストコマンド、  
Debugを有効にすると内部値がUARTに出力されます  

### 引数  
val = 0-360  
  
## CALL LEDA_ON  

### 機能    
今設定しているparameter値に合わせてアニメーションを実行します。  
  
## CALL LEDA_OFF  

### 機能    
アニメーションを停止とします。(LEDは、点灯状態)  
  
## CALL LEDA_HUE (\<mode>,\<val>)  

### 機能    
16ms毎のHUE値の変化量と変化の仕方を設定します。  
  
### 引数  
mode = 
0: 値がMAXなったら0に戻ります  
1: 値がMAXになったマイナスされ、0になると元に戻ります。  

val =  
HUE値 -360.000-360.000  

  
## CALL LEDA_SAT (\<mode>,\<val>)  

### 機能    
16ms毎のSAT値の変化量と変化の仕方を設定します。  

### 引数  
mode =  
0: 値がMAXなったら0に戻ります  
1: 値がMAXになったマイナスされ、0になると元に戻ります。  

val = SAT値 -100.000-100.000  

  
## CALL LEDA_VAL (\<mode>,\<val>)  

### 機能    
16ms毎のVAL値の変化量と変化の仕方を設定します。  
  
### 引数  
mode =   
0: 値がMAXなったら0に戻ります  
1: 値がMAXになったマイナスされ、0になると元に戻ります。  

val = VAL値 -100.000-100.000  
  
## CALL LEDA_POS (\<mode>,\<val>)  

### 機能    
16ms毎のPOS値の変化量と変化の仕方を設定します。  
  
### 引数  
mode =  
0: 値がMAXなったら0に戻ります  
1: 値がMAXになったマイナスされ、0になると元に戻ります。  

val = POS値 -100.000-100.000  
  
## CALL LEDPT_SAT (\<val>)  

### 機能    
LEDのグラデーションパターン時のSAT値の変化量を設定します。  
  
### 引数  
val = SAT値の変化範囲指定 0-100
  
## CALL LEDPT_VAL (\<val>)  
  
### 機能    
LEDのグラデーションパターン時のVAL値の変化量を設定します。  

### 引数  
val = VAL値の変化範囲指定 0-100

## CALL LEDPT_AVAL (\<val>)  

### 機能    
LED patternのVAL100%時の明るさを設定します。  

### 引数  
val = 全体の明るさ 0-100

## CALL LEDPT_SAVE (\<val>)  

### 機能    
ユーザパターンをEEPROMに保存します。  
保存した値はDEMOパターン5-9として使用できます。  
  
### 引数  
val = 保存番号0-4   
  
## CALL LEDPT_LOAD (\<val>)  

### 機能    
ユーザパターンをEEPROMからロードします。  

### 引数  
val = 保存番号0-4   
  
## CALL LEDA_PAUSE(\<Value>)  

### 機能    
Animation機能を一次的に停止します。  
停止時のLEDC系とLED_POS値が保存され、再開時の戻します。  
LEDPT系のParameterは保存されないので注意してください。
  
### 引数
val =   
0: Animation動作再開  
1: Animation動作PAUSE  
  
  
