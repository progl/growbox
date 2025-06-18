void handleCoreDump(AsyncWebServerRequest* request)
{
    // Находим раздел дампа ядра
    const esp_partition_t* core_dump_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

    if (!core_dump_partition)
    {
        request->send(404, "text/plain", "Core dump partition not found.");
        return;
    }

    size_t partition_size = core_dump_partition->size;
    Serial.println("WEB /coredump");
    Serial.println("WEB /coredump partition_size " + String(partition_size));

    // Переменная состояния для чтения дампа
    struct CoreDumpContext
    {
        const esp_partition_t* partition;
        size_t offset;
        size_t total_size;
        CoreDumpContext(const esp_partition_t* part, size_t off, size_t tot)
            : partition(part), offset(off), total_size(tot)
        {
        }
    };

    CoreDumpContext* context = new CoreDumpContext(core_dump_partition, 0, partition_size);

    AsyncWebServerResponse* response = new AsyncChunkedResponse(
        "application/octet-stream",
        [context](uint8_t* buffer, size_t maxLen, size_t index) -> size_t
        {
            if (context->offset >= context->total_size)
            {
                delete context;
                return 0;
            }

            size_t bytes_to_read = std::min(maxLen, context->total_size - context->offset);
            if (esp_partition_read(context->partition, context->offset, buffer, bytes_to_read) != ESP_OK)
            {
                delete context;
                return 0;  // Ошибка чтения
            }

            context->offset += bytes_to_read;
            return bytes_to_read;
        });

    request->send(response);
}
