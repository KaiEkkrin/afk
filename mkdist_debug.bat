mkdir dist\x64\Debug
mkdir dist\x64\Debug\src
copy /y x64\Debug\afk.exe dist\x64\Debug
copy /y x64\Debug\afk.pdb dist\x64\Debug
xcopy /E /F /Y src\shaders dist\x64\Debug\src\shaders\
xcopy /E /F /Y src\compute dist\x64\Debug\src\compute\
copy /y ..\..\glew-1.10.0\bin\Release\x64\glew32.dll dist\x64\Debug\
