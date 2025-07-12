# Makefile

SRC_DIR := ./src
VERSION="alpha"
FORMAT_CMD := find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} \;
# Файл с хэшем коммита
FW_COMMIT_FILE = ./src/fw_commit.h

# Команда для получения хэша текущего коммита
COMMIT_HASH = $(shell git rev-parse HEAD 2>/dev/null | head -c 7)
COMMIT_NUMBER := $(shell cat commit_number.txt)


commit_number:
	@echo  "$(VERSION):$(COMMIT_NUMBER):$(COMMIT_HASH)"

build:
	bash scripts/build.sh

flash:
	bash scripts/cu-001.sh

b_f:
	bash scripts/build.sh
	bash scripts/cu-001.sh
# Цель форматирования

format:
	@echo "Форматирование файлов .cpp и .h в папке $(SRC_DIR)..."
	$(FORMAT_CMD)
	@echo "Форматирование завершено."commit:
	$(eval COMMIT_NUMBER := $(shell cat commit_number.txt))
	@echo "Обновление $(FW_COMMIT_FILE) с текущим порядковым номером коммита: $(COMMIT_NUMBER)"
	@echo "#ifndef FW_COMMIT_H" > $(FW_COMMIT_FILE)
	@echo "#define FW_COMMIT_H" >> $(FW_COMMIT_FILE)
	@echo "" >> $(FW_COMMIT_FILE)
	@echo "// Current firmware version" >> $(FW_COMMIT_FILE)
	@echo "String firmware_commit = \"$(VERSION):$(COMMIT_NUMBER):$(COMMIT_HASH)\";" >> $(FW_COMMIT_FILE)
	@echo "" >> $(FW_COMMIT_FILE)
	@echo "#endif  // FW_COMMIT_H" >> $(FW_COMMIT_FILE)
	@echo "Обновление $(FW_COMMIT_FILE) с текущим порядковым номером коммита: $(VERSION):$(COMMIT_NUMBER):$(COMMIT_HASH);"
