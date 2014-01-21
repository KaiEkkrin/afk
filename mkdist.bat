mkdir dist\x64\Release
mkdir dist\x64\Release\src
copy /y x64\Release\afk.exe dist\x64\Release
copy /y x64\Release\afk.pdb dist\x64\Release
xcopy /E /F /Y src\shaders dist\x64\Release\src\shaders\
xcopy /E /F /Y src\compute dist\x64\Release\src\compute\
copy /y ..\..\glew-1.10.0\bin\Release\x64\glew32.dll dist\x64\Release\
