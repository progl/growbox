#!/usr/bin/python3

# Adds PlatformIO post-processing to merge all the ESP flash images into a single image.
import os
import csv
from argparse import Namespace
Import("env", "projenv")

# Retrieve board configuration
board_config = env.BoardConfig()

# Define paths for firmware and merged binary
firmware_bin = "${BUILD_DIR}/${PROGNAME}.bin"
merged_bin = os.environ.get("MERGED_BIN_PATH", "${BUILD_DIR}/${PROGNAME}-merged.bin")


# Function to merge the binary images
def merge_bin_action(source, target, env):
    # Prepare the flash images list, including extra flash images

    # Execute the merge command
  
    flash_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        "$ESP32_APP_OFFSET",
        source[0].get_abspath(),
    ]

    # Add littlefs.bin to the list of flash images
    flash_images.append(f"0x310000 {env.subst('$BUILD_DIR')}/LittleFS.bin")
    flash_images.append(f"0x3A2000 data2/data2/wifi.bin")

    # Print the list of flash images for debugging
    print("Flash images to be merged:")
    for i in flash_images:
        print(i)

    # Merge the images using platformio and objcopy
    merge_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$OBJCOPY"',
            "--chip",
            board_config.get("build.mcu", "esp32"),
            "merge_bin",
            "-o",
            merged_bin,
            "--flash_mode",
            board_config.get("build.flash_mode", "dio"),
            "--flash_freq",
            "${__get_board_f_flash(__env__)}",
            "--flash_size",
            board_config.get("upload.flash_size", "4MB"),
            *flash_images,
        ]
    )
    # Execute the merge command
    env.Execute(merge_cmd)


def convert_csv_to_nvs(csv_file, nvs_file):
    # Считываем CSV файл
    with open(csv_file, 'r') as f:
        data =  f.readlines()
    
    # Формируем JSON структуру для NVS
    nvs_data = []
    for row in data:
        print('row1', row)
        print('row', row.split(','))
        d= row.split(',')
        nvs_data.append({
                "key": d[0],
                "type": 'data',
                "encoding": "string",
                "value": str(d[1].strip())
            })
    
    # Создаем временный CSV файл
    temp_csv = 'temp_nvs.csv'
    with open(temp_csv, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['key', 'type', 'encoding', 'value'])
        writer.writeheader()
        writer.writerow({"key": "settings", "type": "namespace", "encoding": "", "value": ""})
        for item in nvs_data:
            print('item', item)
            writer.writerow(item)
    
    # Создаем директорию data если её нет

    
    # Подготовка аргументов для функции `generate`
    args = Namespace(
        input=temp_csv,
        output=nvs_file,
        size="0x15000",
        version=2,
        outdir=os.path.dirname(nvs_file)
    )
    
    # Импортируем и вызываем функцию generate
    from nvs_partition_gen import generate
    generate(args)
    
    # Удаляем временные файлы
    # os.remove(temp_csv)

if not os.path.exists('data2/wifi.bin'):
    convert_csv_to_nvs('data2/wifi.csv', 'data2/wifi.bin')
# Temporarily disabled NVS partition generation to focus on compilation


# Register the custom target 'mergebin' with PlatformIO
env.AddCustomTarget(
    name="mergebin",
    dependencies=firmware_bin,
    actions=merge_bin_action,
    title="Merge binary",
    description="Build combined image",
    always_build=True,
)
