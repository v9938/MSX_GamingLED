# ATMEGA328Pプログラム
IC1 ATMEGA328Pのプログラムです。Arduino環境で作成されています。  

HEXファイルを書き込み時のFuse設定は下記の通りです。  
FUSE LOW  = 0xFF  
FUSE HIGH = 0xD6  
EXTENED FUSE = 0xFD  
LOCK BIT = 0xCF  

RGB LEDのライブラリとして、Indoor CorgiさんのALED-AddressableRGB-Libraryを使わせていただいています。

[GitHub](https://github.com/IndoorCorgi/ALED-AddressableRGB-Library)  
[Indoor CorgiさんのアドレサブルRGB LED解説](https://www.indoorcorgielec.com/resources/アドレサブルrgb/)  

より複雑な発光パターンを作成したい場合は上記内容を参照ください。
