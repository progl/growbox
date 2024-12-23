# Makefile

# Путь к папке с исходниками
SRC_DIR := ./src

# Команда для форматирования
FORMAT_CMD := find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} \;

# Цель форматирования
format:
	@echo "Форматирование файлов .cpp и .h в папке $(SRC_DIR)..."
	$(FORMAT_CMD)
	@echo "Форматирование завершено."
