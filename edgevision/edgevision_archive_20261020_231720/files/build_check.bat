@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d C:\Users\minglei.li\WorkBuddy\2026-10-01-task-141\edgevision
cl /std:c++17 /nologo /DEVS_BACKEND_MOCK /Iinclude tests\test_core.cpp src\core\config.cpp src\core\model_registry.cpp src\core\backend_factory.cpp src\core\detection_decoder.cpp src\vision_task.cpp src\tasks\person_plugin.cpp src\backend\mock\mock_backend.cpp /Fe:evs_tests.exe
echo BUILD_EXIT=%ERRORLEVEL%
