#include "OtaImage.h"

#include <stdlib.h>
#include <string.h>

#include "BuildInfo.h"
#include "Version.h"

// Внешняя связь и used — чтобы строку не выбросили компилятор с линкером:
// обращение к ней идет только из поиска и разбора, и без этих пометок маркер
// мог бы не доехать до образа
extern const char FW_IMAGE_TAG[];
const char FW_IMAGE_TAG[] __attribute__((used)) =
    "QX50-FW-TAG:" FW_VERSION "|" BUILD_ENV "|" BUILD_STAMP "|";

bool ota_tag_parse(const char *tag, size_t len, OtaTag &out)
{
    if (tag == nullptr || len <= OTA_TAG_SIG_LEN) return false;
    if (len > OTA_TAG_MAX_LEN) len = OTA_TAG_MAX_LEN;

    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t body_len = len - OTA_TAG_SIG_LEN;
    memcpy(buf, tag + OTA_TAG_SIG_LEN, body_len);
    buf[body_len] = '\0';

    // За третьим разделителем в образе идут посторонние данные — все, что после
    // него, разбор не касается
    char *fields[3] = {nullptr, nullptr, nullptr};
    char *pos = buf;

    for (size_t i = 0; i < 3; i++) {
        char *sep = strchr(pos, '|');
        if (sep == nullptr) return false;
        *sep = '\0';
        fields[i] = pos;
        pos = sep + 1;
    }

    if (fields[0][0] == '\0') return false;

    for (size_t i = 0; i < 3; i++) {
        for (const char *c = fields[i]; *c != '\0'; c++) {
            if (*c < 0x20 || *c > 0x7e) return false;
        }
    }

    out.version = fields[0];
    out.env     = fields[1];
    out.build   = fields[2];
    return true;
}

OtaTagStream::OtaTagStream()
{
    reset();
}

void OtaTagStream::reset()
{
    signature_len_  = 0;
    candidate_len_  = 0;
    separator_count_ = 0;
    collecting_     = false;
    found_          = false;
    tag_            = OtaTag();
}

void OtaTagStream::feed(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0 || found_) return;

    for (size_t i = 0; i < len; i++) {
        const uint8_t byte = data[i];

        if (collecting_) {
            if (candidate_len_ >= OTA_TAG_MAX_LEN) {
                candidate_len_   = 0;
                separator_count_ = 0;
                collecting_      = false;
                signature_len_   = 0;
                continue;
            }

            candidate_[candidate_len_++] = static_cast<char>(byte);

            if (byte == '|') {
                separator_count_++;

                if (separator_count_ == 3) {
                    candidate_[candidate_len_] = '\0';

                    OtaTag parsed;
                    if (ota_tag_parse(candidate_, candidate_len_, parsed)) {
                        tag_   = parsed;
                        found_ = true;
                        return;
                    }

                    candidate_len_   = 0;
                    separator_count_ = 0;
                    collecting_      = false;
                    signature_len_   = 0;
                }
            }

            continue;
        }

        if (signature_len_ < OTA_TAG_SIG_LEN) {
            signature_[signature_len_++] = byte;
        } else {
            memmove(signature_, signature_ + 1, OTA_TAG_SIG_LEN - 1);
            signature_[OTA_TAG_SIG_LEN - 1] = byte;
        }

        if (signature_len_ == OTA_TAG_SIG_LEN &&
            memcmp(signature_, FW_IMAGE_TAG, OTA_TAG_SIG_LEN) == 0) {
            memcpy(candidate_, signature_, OTA_TAG_SIG_LEN);
            candidate_len_   = OTA_TAG_SIG_LEN;
            separator_count_ = 0;
            collecting_      = true;
            signature_len_   = 0;
        }
    }
}

bool OtaTagStream::found() const
{
    return found_;
}

const OtaTag &OtaTagStream::tag() const
{
    return tag_;
}

static uint8_t ota_env_family(const char *env)
{
    if (env == nullptr) return 0;

    if (strcmp(env, "esp32") == 0 || strcmp(env, "esp32-mock") == 0) return 1;

    if (strcmp(env, "esp32s3-wt32") == 0 || strcmp(env, "esp32s3-wt32-mock") == 0) return 2;

    return 0;
}

bool ota_envs_compatible(const char *running_env, const char *image_env)
{
    const uint8_t running_family = ota_env_family(running_env);
    const uint8_t image_family   = ota_env_family(image_env);

    return running_family != 0 && running_family == image_family;
}

int ota_tag_find(const uint8_t *buf, size_t len)
{
    if (buf == nullptr || len < OTA_TAG_SIG_LEN) return -1;

    // Позиция, дальше которой сигнатура в блок уже не помещается
    const size_t last = len - OTA_TAG_SIG_LEN;

    for (size_t i = 0; i <= last;) {
        const uint8_t *hit =
            static_cast<const uint8_t *>(memchr(buf + i, FW_IMAGE_TAG[0], last - i + 1));
        if (hit == nullptr) break;

        const size_t off = static_cast<size_t>(hit - buf);
        if (memcmp(buf + off, FW_IMAGE_TAG, OTA_TAG_SIG_LEN) == 0) return static_cast<int>(off);

        i = off + 1;
    }

    return -1;
}

bool ota_tag_scan(OtaImageReader read, const void *ctx, uint32_t limit, OtaTag &out)
{
    if (read == nullptr || limit < OTA_TAG_SIG_LEN) return false;

    // Буфер берем из кучи: обработчики HTTP живут в задаче с небольшим стеком
    uint8_t *buf = static_cast<uint8_t *>(malloc(OTA_SCAN_CHUNK + OTA_TAG_MAX_LEN));
    if (buf == nullptr) return false;

    bool found = false;

    // Блок читается с перекрытием в длину маркера, а шаг остается равным
    // OTA_SCAN_CHUNK: маркер, начавшийся у границы блока, попадает в чтение целиком
    for (uint32_t pos = 0; pos < limit && !found; pos += OTA_SCAN_CHUNK) {
        const uint32_t left = limit - pos;
        const size_t   want = left < OTA_SCAN_CHUNK + OTA_TAG_MAX_LEN
                                  ? static_cast<size_t>(left)
                                  : OTA_SCAN_CHUNK + OTA_TAG_MAX_LEN;

        if (want < OTA_TAG_SIG_LEN) break;
        if (!read(ctx, pos, buf, want)) break;

        for (size_t i = 0; i + OTA_TAG_SIG_LEN <= want;) {
            const int at = ota_tag_find(buf + i, want - i);
            if (at < 0) break;

            const size_t off = i + static_cast<size_t>(at);

            // Маркер у самого конца блока может оказаться обрезанным — тогда
            // разбор откажет, а следующее чтение найдет его целиком
            if (ota_tag_parse(reinterpret_cast<const char *>(buf + off), want - off, out)) {
                found = true;
                break;
            }

            i = off + 1;
        }
    }

    free(buf);
    return found;
}

uint32_t ota_image_length(OtaImageReader read, const void *ctx, uint32_t part_size)
{
    uint8_t header[OTA_IMAGE_HEADER_LEN];
    if (read == nullptr || !read(ctx, 0, header, sizeof(header))) return 0;

    // Первый байт образа приложения всегда 0xE9, дальше идет число сегментов
    if (header[0] != 0xe9) return 0;

    uint32_t offset = OTA_IMAGE_HEADER_LEN;

    for (uint8_t i = 0; i < header[1]; i++) {
        // Заголовок сегмента: адрес загрузки и длина данных
        uint32_t segment[2];
        if (!read(ctx, offset, segment, sizeof(segment))) return 0;

        // Длину сегмента проверяем до сложения: в мусорном образе она может
        // быть такой, что смещение переполнится и проверка ниже пропустит его
        if (segment[1] > part_size) return 0;

        offset += sizeof(segment) + segment[1];
        if (offset > part_size) return 0;
    }

    // Контрольный байт дополняет образ до границы 16 байт и лежит последним
    offset = (offset + 16) & ~static_cast<uint32_t>(15);

    // Последний байт заголовка — признак того, что за образом лежит SHA-256
    if (header[OTA_IMAGE_HEADER_LEN - 1] == 1) offset += 32;

    return offset <= part_size ? offset : 0;
}
