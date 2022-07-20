if exist *.bin (del *.bin)

..\..\..\..\..\..\tools\cmd_info.exe --no-cmd-id-duplicate-check

if exist *.bin goto label_ok
echo cmd_info failed
..\..\..\..\..\..\tools\cmd_info_err.exe
goto end

:label_ok
echo cmd_info ok

:end