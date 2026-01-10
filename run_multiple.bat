@echo off
setlocal

REM Number of processes to launch
set N=4

echo Launching %N% instances of ".\build\debug\bin\eROIL_Tests.exe"...

for /L %%i in (0,1,%N%-1) do (
echo Starting instance %%i
start "" ".\build\debug\bin\eROIL_Tests.exe" %%i
)

echo Done.
exit /b 0
