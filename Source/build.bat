call toolchain.bat
msbuild LivePathTracer.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
