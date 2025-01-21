void handleCoreDump()
{
    // Находим раздел дампа ядра
    const esp_partition_t *core_dump_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
    if (!core_dump_partition)
    {
        server.send(404, "text/plain", "Core dump partition not found.");
        return;
    }

    // Информация о разделе
    size_t partition_size = core_dump_partition->size;
    char partition_info_msg[128];
    snprintf(partition_info_msg, sizeof(partition_info_msg), "Core dump partition found. Size: %u bytes",
             (unsigned int)partition_size);

    // Заголовки ответа
    server.setContentLength(partition_size);           // Указываем размер ответа
    server.send(200, "application/octet-stream", "");  // Заголовки без тела

    // Чтение и передача дампа чанками
    const size_t chunk_size = 1024;  // Размер чанка (1KB)
    uint8_t *buffer = (uint8_t *)malloc(chunk_size);
    if (!buffer)
    {
        server.send(500, "text/plain", "Failed to allocate buffer.");
        return;
    }

    size_t offset = 0;
    while (offset < partition_size)
    {
        size_t bytes_to_read = std::min(chunk_size, partition_size - offset);

        // Чтение чанка из партиции
        esp_err_t err = esp_partition_read(core_dump_partition, offset, buffer, bytes_to_read);
        if (err != ESP_OK)
        {
            free(buffer);
            server.client().stop();  // Завершаем соединение
            return;
        }

        // Отправка чанка клиенту
        server.client().write(buffer, bytes_to_read);

        offset += bytes_to_read;
    }

    free(buffer);            // Очистка памяти
    server.client().stop();  // Завершаем соединение после передачи
}