@echo off
cls
pushd %~dp0
	devtools\bin\vpc.exe /sdk +everything /mksln Game_SDK.sln /2013
popd
@pause