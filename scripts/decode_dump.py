 
from esp_coredump import CoreDump
 

# Instantiate the coredump object
coredump = CoreDump(chip='esp32',
                     core="/Users/mmatveyev/Downloads/core-dump",
                     core_format='raw', 
                     prog='.pio/build/box-ogurec-wifi/firmware.elf')
coredump.info_corefile()  #  print the info of the test app corefile
# coredump.dbg_corefile()  #  run GDB debug session with provided ELF file