export PATH="./xtensa-esp-elf-gdb/bin:$PATH"
python scripts/decode_dump.py


/Users/mmatveyev/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line \
  -pfiaC -e .pio/build/box-ogurec-wifi/firmware.elf \
0x400815dd 0x400d435a 0x40116e66 0x40083e24