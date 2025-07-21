# Интеграция с MQTT

## Содержание
1. [Обзор](#обзор)
2. [Настройка MQTT](#настройка-mqtt)
3. [Структура топиков](#структура-топиков)
4. [Автообнаружение Home Assistant](#автообнаружение-home-assistant)
5. [Ручная интеграция с системами](#ручная-интеграция-с-системами)
6. [Устранение неисправностей](#устранение-неисправностей)

## Обзор

GrowBox использует MQTT (Message Queuing Telemetry Transport) для интеграции с внешними системами. Основные возможности:

- Публикация показаний всех датчиков
- Управление реле и другими исполнительными устройствами
- Поддержка автообнаружения Home Assistant
- Мониторинг состояния системы

## Настройка MQTT

### Требования к брокеру

- Поддерживаемые брокеры: Mosquitto, EMQX, HiveMQ, Mosca, VerneMQ
- Рекомендуемая версия MQTT: 3.1.1
- Поддержка QoS 1 (хотя бы одна доставка)

### Настройка через веб-интерфейс

1. Перейдите в раздел "Настройки" > "MQTT"
2. Заполните параметры подключения:
   - Адрес сервера (например, `192.168.1.100` или `mqtt.example.com`)
   - Порт (по умолчанию 1883)
   - Имя пользователя и пароль (если требуется)
   - Префикс топиков (по умолчанию `growbox/`)
   - Включить автообнаружение Home Assistant
3. Нажмите "Применить"

### Настройка через конфигурационный файл

Параметры MQTT хранятся в NVS (Non-Volatile Storage) и могут быть изменены через веб-интерфейс. Для ручного изменения используйте API:

```http
POST /api/mqtt/config
Content-Type: application/json

{
  "enabled": true,
  "server": "mqtt.example.com",
  "port": 1883,
  "username": "user",
  "password": "pass",
  "topic_prefix": "growbox/",
  "ha_discovery": true,
  "ha_prefix": "homeassistant"
}
```

## Структура топиков

GrowBox использует следующие топики:

- `{prefix}/sensors` - все показания датчиков (JSON)
- `{prefix}/sensors/{sensor_type}/{sensor_id}` - отдельный датчик
- `{prefix}/relay/{id}/set` - управление реле
- `{prefix}/system/status` - статус системы
- `{prefix}/system/restart` - перезагрузка системы

### Пример сообщения с показаниями датчиков

```json
{
  "temperature": 25.4,
  "humidity": 65.2,
  "ec": 1.8,
  "ph": 6.2,
  "water_temp": 22.1,
  "timestamp": 1678901234
}
```
### Управление реле

Для управления реле отправьте сообщение в топик `{prefix}/relay/{id}/set`:

- `1` или `ON` - включить реле
- `0` или `OFF` - выключить реле
- `TOGGLE` - изменить состояние

## Автообнаружение Home Assistant

GrowBox поддерживает автообнаружение Home Assistant через MQTT Discovery. Для активации:

1. Включите опцию "Автообнаружение Home Assistant" в настройках MQTT
2. Укажите префикс (по умолчанию `homeassistant`)
3. Перезагрузите Home Assistant или нажмите "Обнаружить устройства"

### Поддерживаемые сущности

- Датчики температуры, влажности, EC, pH
- Реле и исполнительные устройства
- Бинарные сенсоры (двери, протечки)
- Кнопки перезагрузки и сброса

## Ручная интеграция с системами

### Node-RED

Пример потока для Node-RED:

```json
[
  {
    "id": "growbox-subscribe",
    "type": "mqtt in",
    "z": "growbox-flow",
    "name": "GrowBox Sensors",
    "topic": "growbox/sensors",
    "qos": "1",
    "datatype": "json",
    "broker": "mqtt-broker",
    "nl": false,
    "rap": true,
    "rh": 0,
    "inputs": 0,
    "x": 240,
    "y": 100,
    "wires": [["process-data"]]
  }
]
```

### OpenHAB

Пример конфигурации в `things/mqtt.things`:

```
Bridge mqtt:broker:growbox_broker [
  host="mqtt.example.com",
  secure=false,
  username="user",
  password="pass"
] {
  Thing mqtt:topic:growbox "GrowBox" {
    Channels:
      Type number : temperature "Temperature" [ stateTopic="growbox/sensors", transformationPattern="JSONPATH:$.temperature"]
      Type number : humidity "Humidity" [ stateTopic="growbox/sensors", transformationPattern="JSONPATH:$.humidity"]
  }
}
```

## Устранение неисправностей

### Нет подключения к брокеру

1. Проверьте логи MQTT в веб-интерфейсе
2. Убедитесь, что брокер доступен из сети
3. Проверьте настройки брандмауэра

### Не приходят сообщения

1. Убедитесь, что подписка активна
2. Проверьте QoS (должен быть 1 или 2)
3. Убедитесь, что топик публикации совпадает с подпиской

### Проблемы с автообнаружением Home Assistant

1. Убедитесь, что включено автообнаружение
2. Проверьте префикс (должен совпадать с настройками Home Assistant)
3. Перезагрузите Home Assistant

### Логирование

Для отладки включите подробное логирование MQTT в настройках системы. Логи доступны в веб-интерфейсе в разделе "Система" > "Логи".
   ```
3. Перезапустите устройство

## Структура топиков

### Основные топики

| Топик | Направление | Описание | Формат данных |
|-------|-------------|----------|---------------|
| `{prefix}/status` | → | Статус устройства | `{"online": true/false, "ip": "..."}` |
| `{prefix}/sensors` | → | Показания датчиков | См. ниже |
| `{prefix}/relay/{id}/set` | ← | Управление реле | `{"state": true/false, "duration": 0}` |
| `{prefix}/config` | ← | Конфигурация | См. раздел "Управление конфигурацией" |

### Формат сообщений датчиков

```json
{
  "temperature": 24.5,
  "humidity": 65.2,
  "ph": 6.2,
  "ec": 1200,
  "water_level": 85,
  "timestamp": "2023-04-15T14:30:00Z"
}
```

### Пример подписки на топики

```bash
# Подписка на все сообщения
topic 'growbox/#'

# Подписка на показания датчиков
topic 'growbox/sensors'

# Подписка на статус устройства
topic 'growbox/status'
```

## Примеры использования

### Отправка команд

Включить реле #1 на 60 секунд:

```bash
mosquitto_pub -h broker.hivemq.com -t 'growbox/relay/1/set' -m '{"state": true, "duration": 60}'
```

### Получение данных

Подписаться на показания датчиков:

```bash
mosquitto_sub -h broker.hivemq.com -t 'growbox/sensors'
```

## Интеграция с Home Assistant

### Автообнаружение (MQTT Discovery)

GrowBox поддерживает автоматическое обнаружение в Home Assistant через MQTT Discovery. Для включения:

1. Убедитесь, что MQTT Discovery включён в Home Assistant
2. Включите MQTT Discovery в настройках GrowBox
3. Перезапустите Home Assistant

### Ручная конфигурация

Добавьте в `configuration.yaml`:

```yaml
mqtt:
  sensor:
    - name: "GrowBox Temperature"
      state_topic: "growbox/sensors"
      unit_of_measurement: "°C"
      value_template: "{{ value_json.temperature }}"
    - name: "GrowBox Humidity"
      state_topic: "growbox/sensors"
      unit_of_measurement: "%"
      value_template: "{{ value_json.humidity }}"

  switch:
    - name: "GrowBox Relay 1"
      command_topic: "growbox/relay/1/set"
      state_topic: "growbox/relay/1/state"
      payload_on: '{"state": true}'
      payload_off: '{"state": false}'
      state_value_template: '{{ value_json.state }}'
      retain: true
```

## Управление конфигурацией

### Получение текущей конфигурации

```bash
mosquitto_sub -h broker.hivemq.com -t 'growbox/config/get'
mosquitto_pub -h broker.hivemq.com -t 'growbox/config/get' -m ''
```

### Обновление конфигурации

```bash
mosquitto_pub -h broker.hivemq.com -t 'growbox/config/set' -m '
{
  "wifi": {
    "ssid": "MyWiFi",
    "password": "securepassword"
  },
  "sensors": {
    "update_interval": 60
  }
}'
```

## Безопасность

### Рекомендации

1. Используйте шифрование (TLS/SSL)
2. Настройте аутентификацию на брокере
3. Используйте уникальные имена пользователей и сложные пароли
4. Ограничьте доступ к брокеру по IP-адресам
5. Регулярно обновляйте ПО брокера

### Настройка TLS

1. Сгенерируйте сертификаты для брокера
2. Загрузите CA-сертификат в GrowBox
3. Настройте подключение с использованием SSL/TLS в настройках MQTT

## Отладка

### Включение логов MQTT

1. Включите отладочный режим в настройках
2. Просматривайте логи через последовательный порт или веб-интерфейс
3. Используйте `mosquitto_sub` с флагом `-d` для отладки

### Пример отладки

```bash
# Просмотр всех сообщений с отладочной информацией
mosquitto_sub -h broker.hivemq.com -t 'growbox/#' -d
```

## Производительность

### Оптимизация

- Увеличьте интервал отправки данных при ограниченной пропускной способности
- Используйте QoS 0 для некритичных данных
- Включите сжатие сообщений при использовании мобильного интернета

### Ограничения

- Максимальный размер сообщения: 256 КБ
- Максимальная частота обновления: 1 Гц
- Поддерживается до 10 одновременных подписчиков

## Расширенные возможности

### Сценарии автоматизации

Используйте Node-RED для создания сложных сценариев на основе данных с датчиков.

### Интеграция с другими системами

- **Telegram** - уведомления и управление
- **IFTTT** - интеграция с сервисами
- **Google Sheets** - логирование данных
- **Grafana** - визуализация данных

## Часто задаваемые вопросы

**Q: Как изменить префикс топиков?**  
A: В настройках MQTT в веб-интерфейсе измените поле "Префикс топиков".

**Q: Как сбросить настройки MQTT?**  
A: Удалите файл конфигурации MQTT и перезагрузите устройство.

**Q: Как обновить прошивку через MQTT?**  
A: Отправьте команду обновления с указанием URL прошивки.

**Q: Как настроить резервное копирование через MQTT?**  
A: Подпишитесь на топик `growbox/backup` для получения резервных копий.
