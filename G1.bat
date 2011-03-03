@echo off & setlocal

::Extract the current code page.  Hopefully this method will work with other
::languages.  CHCP outputs: "Active code page: #".  Take the last five
::characters (the longest code) and delete up to and including the space.
for /f "delims=" %%j in ('chcp') do set CP=%%j
set CP=%CP:~-5%
set CP=%CP:* =%

x86\ansicon -e The DEC Special Graphics Character Set according to code page %CP%:^

^

	_ ` a b c d e f g h i j k l m n o p q r s t u v w x y z { ^| } ~^

^

	_ ` a b c d e f g h i j k l m n o p q r s t u v w x y z { ^| } ~
