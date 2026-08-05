#include "OtaSlots.h"

#include <string.h>

#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "OtaImage.h"

// Чтение раздела для ota_image_length() и ota_tag_scan()
static bool partition_read(const void *ctx, uint32_t offset, void *dst, size_t len)
{
    const esp_partition_t *part = static_cast<const esp_partition_t *>(ctx);
    return esp_partition_read(part, offset, dst, len) == ESP_OK;
}

// Разобранный маркер соседнего слота.
//
// Скан читает до полутора мегабайт флеша, поэтому результат запоминается.
// Ключ — хеш образа из esp_app_desc: перезаписали слот новой прошивкой,
// хеш изменился, маркер ищется заново
struct TagCache {
    bool    filled;
    uint8_t sha[32];
    bool    found;
    OtaTag  tag;
};

static TagCache s_tag_cache[2];

// Маркер раздела с учетом кеша. desc уже прочитан вызывающим кодом
static bool tag_lookup(const esp_partition_t *part, const esp_app_desc_t &desc,
                       uint32_t limit, OtaTag &tag)
{
    const size_t idx = static_cast<size_t>(part->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0);
    TagCache *cache = idx < 2 ? &s_tag_cache[idx] : nullptr;

    if (cache != nullptr && cache->filled &&
        memcmp(cache->sha, desc.app_elf_sha256, sizeof(cache->sha)) == 0) {
        tag = cache->tag;
        return cache->found;
    }

    // Ноль означает, что длину образа определить не вышло — тогда читаем раздел целиком
    if (limit == 0 || limit > part->size) limit = part->size;

    const bool found = ota_tag_scan(partition_read, part, limit, tag);

    if (cache != nullptr) {
        cache->filled = true;
        cache->found  = found;
        cache->tag    = found ? tag : OtaTag();
        memcpy(cache->sha, desc.app_elf_sha256, sizeof(cache->sha));
    }

    return found;
}

size_t ota_slots_collect(OtaSlotInfo *out, size_t max)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot    = esp_ota_get_boot_partition();

    size_t count = 0;
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);

    for (; it != nullptr && count < max; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);

        if (part->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
            part->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_1) {
            continue;
        }

        OtaSlotInfo &info = out[count++];
        info.label   = part->label;
        info.running = (running != nullptr && running->address == part->address);
        info.boot    = (boot != nullptr && boot->address == part->address);
        info.size    = part->size;
        info.known   = false;

        esp_app_desc_t desc;
        info.valid = (esp_ota_get_partition_description(part, &desc) == ESP_OK);
        info.used  = info.valid ? ota_image_length(partition_read, part, part->size) : 0;

        OtaTag tag;

        if (info.running) {
            // Свой маркер лежит в памяти — сканировать раздел незачем
            info.known = ota_tag_parse(FW_IMAGE_TAG, strlen(FW_IMAGE_TAG), tag);
        } else if (info.valid) {
            info.known = tag_lookup(part, desc, info.used, tag);
        }

        if (info.known) {
            info.version = tag.version;
            info.env     = tag.env;
            info.build   = tag.build;
        }
    }

    // esp_partition_next() освобождает итератор сам, когда разделы кончились,
    // но из цикла можно выйти и раньше — по заполнению out
    if (it != nullptr) esp_partition_iterator_release(it);

    return count;
}

bool ota_slots_set_boot(const char *label, String &err)
{
    if (label == nullptr || label[0] == '\0') {
        err = "слот не указан";
        return false;
    }

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);

    if (part == nullptr ||
        part->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        part->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_1) {
        err = "нет такого слота";
        return false;
    }

    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(part, &desc) != ESP_OK) {
        err = "в слоте нет прошивки";
        return false;
    }

    const esp_err_t rc = esp_ota_set_boot_partition(part);
    if (rc != ESP_OK) {
        err = esp_err_to_name(rc);
        Serial.printf("[OTA] Переключить слот на %s не удалось: %s\r\n", part->label, err.c_str());
        return false;
    }

    Serial.printf("[OTA] Загрузочный слот переключен на %s\r\n", part->label);
    return true;
}
