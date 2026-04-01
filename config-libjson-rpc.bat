set "ORIG=%CD%"
set "SCRIPT_DIR=%~dp0"
cd "%SCRIPT_DIR%libjson-rpc-cpp"
cmake -B build -DCMAKE_INSTALL_PREFIX=install -DCMAKE_PREFIX_PATH=..\jsoncpp\install
cd "%ORIG%"
