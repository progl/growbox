 
from esp_coredump import CoreDump
 

d_id = 1514
fn = '/Users/mmatveyev/Downloads/core_dump (21).bin'
# Instantiate the coredump object
coredump = CoreDump(chip='esp32',
                    #  core='/Users/mmatveyev/Downloads/core_dump.bin',
                     core=fn ,
                     core_format='raw', 
                     prog=f'/Users/mmatveyev/growbox_github/growbox-1/scripts/saved/{d_id}/{d_id}-firmware.elf'
                     )
coredump.info_corefile()  #  print the info of the test app corefile
# coredump.dbg_corefile()  #  run GDB debug session with provided ELF file