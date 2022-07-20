@echo off
setlocal enabledelayedexpansion

del /f /s /q  user_file\[60000]*.xls.bin

set TOOLS_PATH=..\..\..\..\tools

echo make asr.bin
%TOOLS_PATH%\asr_merge.exe -i asr

echo make dnn.bin
%TOOLS_PATH%\dnn_merge.exe -i dnn -a asr

echo make user_file.bin

cd user_file\cmd_info
call cmd_info.bat
cd ..\..\
copy user_file\cmd_info\*.bin user_file\
%TOOLS_PATH%\file_merge.exe -i user_file


echo make voice.bin
cd voice
set PATH=..\%TOOLS_PATH%;%PATH%
::exit /b


if exist "mp3" (
    rd /s /q mp3 >nul
)


echo ��ѡ�񲥱�����Ƶ��ʽ:
echo 1. mp3   (Ĭ�ϸ�ʽ,ѹ���ȴ�Լ10:1,���ʽϺ�,�������ϴ�,������������)
echo 2. ��������������ļ��޸��£��˲�������������Խ�ʡʱ�䣩

 
set fmt=1
set /p fmt=default=1:

echo %fmt%

if %fmt% == 1 (
	if not exist "mp3" (
		mkdir "mp3"
	)
	for /d %%i in (*) do (
		echo %%i | findstr "\[.*\].*" > nul && call:to_mp3 mp3 %%i
	)
	..\%TOOLS_PATH%\group_merge.exe -i mp3 -o ..\voice
) else if %fmt% equ 2 (
	goto:eof
) else (
	echo error: ��֧�ֵ���Ƶ��ʽ
)

cd ..

@echo on

exit /b

:to_mp3
echo %1\%2
if not exist %1\%2 (
	mkdir %1\%2
)
for %%j in (%2\*.wav) do (
	set ttt=%%j
	set output_name=%1\!ttt:~0,-4!.mp3
	echo !output_name!
	..\!TOOLS_PATH!\wav_to_mp3_flac.exe -f mp3 -i !ttt! -o !output_name!
)
goto:eof

@echo on
