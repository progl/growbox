import ssl

hostname = 'ponics.online'
port = 443

# Получаем сертификат сервера в формате PEM
pem_cert = ssl.get_server_certificate((hostname, port))
print("Сертификат получен:", pem_cert)

# Разбиваем сертификат на строки, оборачиваем каждую строку в двойные кавычки
# и добавляем в конец каждой строки символ перевода строки (\n)
lines = pem_cert.strip().splitlines()
c_string_lines = [f"\"{line}\\n\"" for line in lines]
c_cert = "\n".join(c_string_lines)

# Формируем содержимое C-заголовочного файла
header_content = f"const char* ca_cert = \n{c_cert};\n"

# Сохраняем сертификат в файл по указанному пути
with open('./src/etc/http_cert.h', 'w') as f:
    f.write(header_content)

print(f"Сертификат сохранён в {f.name}")
