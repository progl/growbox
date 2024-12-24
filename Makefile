# Makefile

SRC_DIR := ./src
FORMAT_CMD := find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} \;
# Файл с хэшем коммита
FW_COMMIT_FILE = ./src/fw_commit.h

# Команда для получения хэша текущего коммита
COMMIT_HASH = $(shell git rev-parse HEAD 2>/dev/null | head -c 6)

# Цель форматирования
format:
	@echo "Форматирование файлов .cpp и .h в папке $(SRC_DIR)..."
	$(FORMAT_CMD)
	@echo "Форматирование завершено."

commit:
	@echo "Обновление $(FW_COMMIT_FILE) с текущим хэшем коммита: $(COMMIT_HASH)"
	@echo "#ifndef FW_COMMIT_H" > $(FW_COMMIT_FILE)
	@echo "#define FW_COMMIT_H" >> $(FW_COMMIT_FILE)
	@echo "" >> $(FW_COMMIT_FILE)
	@echo "// Current firmware commit hash" >> $(FW_COMMIT_FILE)
	@echo "const char *firmware_commit = \"$(COMMIT_HASH)\";" >> $(FW_COMMIT_FILE)
	@echo "" >> $(FW_COMMIT_FILE)
	@echo "#endif  // FW_COMMIT_H" >> $(FW_COMMIT_FILE)