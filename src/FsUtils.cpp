#include "FsUtils.h"

#include <LittleFS.h>

// Максимальная длина пути вместе с суффиксом ".tmp"
// Реальные пути короткие: "/config.json.tmp" — 16 символов
static constexpr size_t FS_TMP_PATH_MAX = 40;

bool fs_write_json_atomic(const char *path, const JsonDocument &doc)
{
    char tmp_path[FS_TMP_PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    File f = LittleFS.open(tmp_path, "w");
    if (!f) {
        Serial.printf("[FS] ОШИБКА: не удалось открыть %s для записи\r\n", tmp_path);
        return false;
    }

    // Сравниваем записанное с ожидаемым размером — так ловится обрыв записи
    // при нехватке места на разделе
    const size_t expected = measureJson(doc);
    const size_t written  = serializeJson(doc, f);
    f.close();

    if (written == 0 || written != expected) {
        Serial.printf("[FS] ОШИБКА: записано %u из %u байт в %s\r\n",
                      static_cast<unsigned>(written), static_cast<unsigned>(expected), tmp_path);
        LittleFS.remove(tmp_path);
        return false;
    }

    // Переименование в LittleFS атомарно: целевой файл либо заменяется целиком,
    // либо остается прежним — промежуточного состояния на флеше не бывает
    if (LittleFS.rename(tmp_path, path)) {
        return true;
    }

    // Запасной путь на случай, если реализация не заменяет существующий файл:
    // удаляем цель и повторяем переименование
    LittleFS.remove(path);
    if (LittleFS.rename(tmp_path, path)) {
        return true;
    }

    Serial.printf("[FS] ОШИБКА: не удалось переименовать %s в %s\r\n", tmp_path, path);
    LittleFS.remove(tmp_path);
    return false;
}
