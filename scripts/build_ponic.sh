make format
~/.platformio/penv/bin/platformio run --environment esp32-local
curl -F "file=@.pio/build/esp32-local/firmware.elf" -F "token=123456" https://ponics.online/wega/upload_firmware