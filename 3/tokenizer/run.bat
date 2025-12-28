@echo off
echo Сборка проекта токенизации...
mkdir build 2>nul
cd build

echo Конфигурация CMake...
cmake .. -G "MinGW Makefiles"

echo Компиляция...
mingw32-make

if exist tokenizer.exe (
    echo Запуск программы...
    tokenizer.exe ..\corpus_clean
) else (
    echo Ошибка компиляции!
)

pause