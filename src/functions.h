/**
 * Convert float to string using provided buffer
 * @param x - float value to convert
 * @param precision - number of decimal places (0-6)
 * @param buffer - output buffer (must be at least 20 bytes)
 * @param bufferSize - size of the output buffer
 * @return true if conversion successful, false if buffer is too small
 */
bool fFTS(float x, byte precision, char* buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize < 20)
    {  // 20 - минимальный разумный размер
        return false;
    }
    return dtostrf(x, 0, precision, buffer) != nullptr;
}