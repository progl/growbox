bash scripts/increment_commit_number.sh
make format
source ~/.platformio/penv/bin/activate
cd web_test/growbox; npm run build; cd ../../
gunzip -c web_test/growbox/dist-local/static_common_gen.h.gz > src/web/static_common_gen.h

rm  web_test/growbox/dist-local/static_common_gen.h.gz 

# Remove existing data directory
rm -rf data/

# Create necessary directories
mkdir -p data/assets/fonts
SRC="web_test/growbox/dist-local"
SRC_CDN="web_test/growbox/dist-cdn"
DST="data"

# Папки, которые нужно создать внутри data
dirs=(
  ""
  "assets/css"
  "assets/js"
  "assets/imgs"
  "assets/fonts"
)

# Создаём все папки
for d in "${dirs[@]}"; do
  mkdir -p "$DST/$d"
done

# Копируем .gz из корня dist
cp "$SRC/index.html.gz" "$DST/index.html.gz"
cp "$SRC/assets/css"/*.gz "$DST/assets/css/"
cp "$SRC/assets/js"/*.gz "$DST/assets/js/"
cp "$SRC/assets/imgs"/*.gz "$DST/assets/imgs/"
cp "$SRC/assets/fonts"/*.gz "$DST/assets/fonts/"

rm "$DST/assets/imgs/javascript.svg.gz"

# Run the first command in the background
~/.platformio/penv/bin/pio run -t buildfs --environment esp32-local 

# Run the second command in the background
~/.platformio/penv/bin/pio run -t mergebin --environment esp32-local 


local commit_number
commit_number=$(< commit_number.txt)
mkdir -p scripts/saved/$commit_number/
cp .pio/build/esp32-local/firmware.elf scripts/saved/$commit_number/$commit_number-firmware.elf
cp .pio/build/esp32-local/firmware-merged.bin scripts/saved/$commit_number/$commit_number-firmware-merged.bin
cp .pio/build/esp32-local/littlefs.bin scripts/saved/$commit_number/$commit_number-littlefs.bin
cp .pio/build/esp32-local/firmware.bin scripts/saved/$commit_number/$commit_number-firmware.bin
