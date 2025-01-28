source ~/.platformio/penv/bin/activate  
pio run -t mergebin  --environment esp32-local
esptool.py --port "/dev/cu.usbserial-0001" --baud 460800     0x3A2000   /Users/mmatveyev/PycharmProjects/web-calc/esp32-local/96-nvs.bin
esptool.py --port "/dev/cu.usbserial-0001" --baud 112500  write_flash   0x10000   .pio/build/esp32-local/firmware.bin

esptool.py  --port "/dev/cu.usbserial-0001" --baud 112500 write_flash 0x0  firmware-merged.bin 
esptool.py  --port "/dev/cu.usbserial-0001" --baud 112500 write_flash 0x0  firmware-merged.bin 
esptool.py  --port "COM15" --baud 112500 write_flash 0x0  .pio/build/esp32-local/firmware-merged.bin 