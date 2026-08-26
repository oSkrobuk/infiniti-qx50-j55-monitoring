#include <unity.h>

#include <string.h>

#include <Arduino.h>

#include "BuzzerStub.h"
#include "BuildInfo.h"
#include "OtaImage.h"
#include "Version.h"

// Тесты разбора образа прошивки.
//
// По этим функциям веб-интерфейс показывает, что лежит в соседнем OTA-слоте:
// маркер ищется линейным чтением чужого раздела, а длина образа считается
// по заголовкам сегментов. На живом железе такие сценарии не воспроизвести —
// понадобилась бы пара реальных прошивок в слотах, поэтому образы и маркеры
// тесты собирают в памяти

void setUp(void) {}

void tearDown(void) {}

// Собирает маркер: сигнатуру берем из настоящего FW_IMAGE_TAG, тело задает тест
static size_t make_tag(char *buf, const char *body)
{
    memcpy(buf, FW_IMAGE_TAG, OTA_TAG_SIG_LEN);

    const size_t body_len = strlen(body);
    memcpy(buf + OTA_TAG_SIG_LEN, body, body_len);
    buf[OTA_TAG_SIG_LEN + body_len] = '\0';

    return OTA_TAG_SIG_LEN + body_len;
}

// ── Маркер собственной прошивки ──────────────────────────────────────────────

static void test_own_tag_parses(void)
{
    OtaTag tag;

    TEST_ASSERT_TRUE(ota_tag_parse(FW_IMAGE_TAG, strlen(FW_IMAGE_TAG), tag));
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, tag.version.c_str());
    TEST_ASSERT_TRUE(tag.env.length() > 0);
    TEST_ASSERT_TRUE(tag.build.length() > 0);
}

static void test_finds_own_tag_in_buffer(void)
{
    uint8_t buf[512];
    memset(buf, 0xff, sizeof(buf));

    const size_t at = 300;
    memcpy(buf + at, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(at), ota_tag_find(buf, sizeof(buf)));
}

static void test_partial_signature_does_not_shadow_tag(void)
{
    uint8_t buf[512];
    memset(buf, 0xff, sizeof(buf));

    // Обрывок сигнатуры не должен уводить поиск от настоящего маркера
    memcpy(buf + 10, FW_IMAGE_TAG, OTA_TAG_SIG_LEN - 1);

    const size_t at = 300;
    memcpy(buf + at, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(at), ota_tag_find(buf, sizeof(buf)));
}

static void test_no_tag_in_buffer(void)
{
    uint8_t buf[256];
    memset(buf, 0xff, sizeof(buf));

    TEST_ASSERT_EQUAL_INT(-1, ota_tag_find(buf, sizeof(buf)));
}

static void test_signature_must_fit_into_block(void)
{
    uint8_t buf[256];
    memset(buf, 0xff, sizeof(buf));

    const size_t at = 100;
    memcpy(buf + at, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG));

    // Блок обрывается на середине сигнатуры — начало маркера ищет следующее чтение
    TEST_ASSERT_EQUAL_INT(-1, ota_tag_find(buf, at + OTA_TAG_SIG_LEN - 1));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(at), ota_tag_find(buf, at + OTA_TAG_SIG_LEN));
}

static void test_block_shorter_than_signature(void)
{
    TEST_ASSERT_EQUAL_INT(-1, ota_tag_find(reinterpret_cast<const uint8_t *>(FW_IMAGE_TAG),
                                           OTA_TAG_SIG_LEN - 1));
}

// ── Разбор маркера ───────────────────────────────────────────────────────────

static void test_parses_all_three_fields(void)
{
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "2026.3.3|esp32s3-wt32|Aug  6 2026 01:15:19|");

    OtaTag tag;

    TEST_ASSERT_TRUE(ota_tag_parse(buf, len, tag));
    TEST_ASSERT_EQUAL_STRING("2026.3.3", tag.version.c_str());
    TEST_ASSERT_EQUAL_STRING("esp32s3-wt32", tag.env.c_str());
    TEST_ASSERT_EQUAL_STRING("Aug  6 2026 01:15:19", tag.build.c_str());
}

static void test_ignores_data_after_tag(void)
{
    // В образе за маркером сразу идут посторонние байты, в том числе не текст
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "1.2.3|esp32|Jan  1 2026 00:00:00|");

    buf[len]     = 0x01;
    buf[len + 1] = 0x7f;
    buf[len + 2] = 'x';

    OtaTag tag;

    TEST_ASSERT_TRUE(ota_tag_parse(buf, len + 3, tag));
    TEST_ASSERT_EQUAL_STRING("1.2.3", tag.version.c_str());
    TEST_ASSERT_EQUAL_STRING("esp32", tag.env.c_str());
    TEST_ASSERT_EQUAL_STRING("Jan  1 2026 00:00:00", tag.build.c_str());
}

static void test_requires_three_separators(void)
{
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "1.2.3|esp32|Jan  1 2026 00:00:00");

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, len, tag));
}

static void test_rejects_empty_version(void)
{
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "|esp32|Jan  1 2026 00:00:00|");

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, len, tag));
}

static void test_rejects_non_printable_field(void)
{
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "1.2.3|esp32|Jan  1 2026 00:00:00|");

    // Байт из чужих данных в поле версии — маркер не наш
    buf[OTA_TAG_SIG_LEN + 1] = 0x02;

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, len, tag));
}

static void test_rejects_non_ascii_field(void)
{
    // Поля уезжают в JSON как есть, поэтому все, кроме печатного ASCII, отвергается
    char buf[OTA_TAG_MAX_LEN + 1];
    const size_t len = make_tag(buf, "1.2.3|плата|Jan  1 2026 00:00:00|");

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, len, tag));
}

static void test_rejects_truncated_tag(void)
{
    char buf[OTA_TAG_MAX_LEN + 1];
    make_tag(buf, "1.2.3|esp32|Jan  1 2026 00:00:00|");

    // До третьего разделителя чтение не доходит — так выглядит маркер,
    // обрезанный концом прочитанного блока
    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, OTA_TAG_SIG_LEN + 8, tag));
}

static void test_rejects_input_shorter_than_signature(void)
{
    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(FW_IMAGE_TAG, OTA_TAG_SIG_LEN, tag));
}

static void test_does_not_look_past_max_length(void)
{
    // Разделители лежат дальше предельной длины маркера — разбор туда не идет
    char body[OTA_TAG_MAX_LEN + 32];
    memset(body, 'v', OTA_TAG_MAX_LEN);
    memcpy(body + OTA_TAG_MAX_LEN, "|esp32|Jan  1 2026 00:00:00|", 29);

    char buf[OTA_TAG_SIG_LEN + sizeof(body)];
    const size_t len = make_tag(buf, body);

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_parse(buf, len, tag));
}

// ── Потоковый поиск маркера ─────────────────────────────────────────────────

static void test_stream_finds_tag_in_one_chunk(void)
{
    uint8_t image[256];
    memset(image, 0xff, sizeof(image));
    memcpy(image + 80, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG));

    OtaTagStream stream;
    stream.feed(image, sizeof(image));

    TEST_ASSERT_TRUE(stream.found());
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, stream.tag().version.c_str());
}

static void test_stream_finds_tag_across_chunks(void)
{
    uint8_t image[256];
    memset(image, 0xff, sizeof(image));
    memcpy(image + 80, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG));

    OtaTagStream stream;
    stream.feed(image, 85);
    stream.feed(image + 85, 9);
    stream.feed(image + 94, sizeof(image) - 94);

    TEST_ASSERT_TRUE(stream.found());
    TEST_ASSERT_EQUAL_STRING(BUILD_ENV, stream.tag().env.c_str());
}

static void test_stream_finds_tag_byte_by_byte(void)
{
    const uint8_t *tag = reinterpret_cast<const uint8_t *>(FW_IMAGE_TAG);
    OtaTagStream stream;

    for (size_t i = 0; i < strlen(FW_IMAGE_TAG); i++) {
        stream.feed(tag + i, 1);
    }

    TEST_ASSERT_TRUE(stream.found());
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, stream.tag().version.c_str());
}

static void test_stream_rejects_missing_and_truncated_tag(void)
{
    uint8_t image[256];
    memset(image, 0xff, sizeof(image));

    OtaTagStream stream;
    stream.feed(image, sizeof(image));
    TEST_ASSERT_FALSE(stream.found());

    stream.reset();
    stream.feed(reinterpret_cast<const uint8_t *>(FW_IMAGE_TAG), OTA_TAG_SIG_LEN + 5);
    TEST_ASSERT_FALSE(stream.found());
}

static void test_ota_env_compatibility(void)
{
    TEST_ASSERT_TRUE(ota_envs_compatible("esp32", "esp32"));
    TEST_ASSERT_TRUE(ota_envs_compatible("esp32", "esp32-mock"));
    TEST_ASSERT_TRUE(ota_envs_compatible("esp32-mock", "esp32"));
    TEST_ASSERT_TRUE(ota_envs_compatible("esp32s3-wt32", "esp32s3-wt32-mock"));
    TEST_ASSERT_TRUE(ota_envs_compatible("esp32s3-wt32-mock", "esp32s3-wt32"));

    TEST_ASSERT_FALSE(ota_envs_compatible("esp32", "esp32s3-wt32"));
    TEST_ASSERT_FALSE(ota_envs_compatible("esp32s3-wt32", "esp32"));
    TEST_ASSERT_FALSE(ota_envs_compatible("esp32", "unknown"));
    TEST_ASSERT_FALSE(ota_envs_compatible(nullptr, "esp32"));
}

// ── Длина образа ─────────────────────────────────────────────────────────────

// Образ в памяти вместо раздела на флеше
struct FakeImage {
    const uint8_t *data;
    size_t         size;
    bool           fail;
};

static bool fake_read(const void *ctx, uint32_t offset, void *dst, size_t len)
{
    const FakeImage *image = static_cast<const FakeImage *>(ctx);

    if (image->fail) return false;
    if (offset + len > image->size) return false;

    memcpy(dst, image->data + offset, len);
    return true;
}

// Раскладывает заголовок образа и заголовки сегментов заданной длины.
// Сколько должно получиться в итоге, каждый тест считает сам
static void build_image(uint8_t *buf, size_t buf_size, const uint32_t *segments,
                        uint8_t count, bool hash)
{
    memset(buf, 0, buf_size);
    buf[0] = 0xe9;
    buf[1] = count;
    buf[OTA_IMAGE_HEADER_LEN - 1] = hash ? 1 : 0;

    uint32_t offset = OTA_IMAGE_HEADER_LEN;

    for (uint8_t i = 0; i < count; i++) {
        if (offset + 8 > buf_size) return;

        const uint32_t header[2] = {0x3f400020u, segments[i]};
        memcpy(buf + offset, header, sizeof(header));
        offset += sizeof(header) + segments[i];
    }
}

static void test_counts_segments_and_hash(void)
{
    uint8_t     data[1024];
    const uint32_t segments[2] = {100, 40};
    build_image(data, sizeof(data), segments, 2, true);

    FakeImage image = {data, sizeof(data), false};

    // 24 заголовок + (8 + 100) + (8 + 40) = 180, дополнение до 192, плюс SHA-256
    TEST_ASSERT_EQUAL_UINT32(224, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_counts_image_without_hash(void)
{
    uint8_t     data[1024];
    const uint32_t segments[2] = {100, 40};
    build_image(data, sizeof(data), segments, 2, false);

    FakeImage image = {data, sizeof(data), false};

    TEST_ASSERT_EQUAL_UINT32(192, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_adds_full_block_when_already_aligned(void)
{
    uint8_t     data[1024];
    const uint32_t segments[1] = {96};
    build_image(data, sizeof(data), segments, 1, false);

    FakeImage image = {data, sizeof(data), false};

    // 24 + 8 + 96 = 128 уже кратно 16, но контрольному байту нужен свой блок
    TEST_ASSERT_EQUAL_UINT32(144, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_counts_image_without_segments(void)
{
    uint8_t data[1024];
    build_image(data, sizeof(data), nullptr, 0, false);

    FakeImage image = {data, sizeof(data), false};

    TEST_ASSERT_EQUAL_UINT32(32, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_rejects_foreign_magic(void)
{
    uint8_t     data[1024];
    const uint32_t segments[1] = {100};
    build_image(data, sizeof(data), segments, 1, true);

    data[0] = 0xaa;

    FakeImage image = {data, sizeof(data), false};

    TEST_ASSERT_EQUAL_UINT32(0, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_rejects_erased_partition(void)
{
    uint8_t data[1024];
    memset(data, 0xff, sizeof(data));

    FakeImage image = {data, sizeof(data), false};

    TEST_ASSERT_EQUAL_UINT32(0, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_rejects_segment_larger_than_partition(void)
{
    uint8_t     data[1024];
    const uint32_t segments[1] = {0xfffff000u};
    build_image(data, sizeof(data), segments, 1, true);

    FakeImage image = {data, sizeof(data), false};

    // Длина сегмента из мусорного образа не должна переполнить счетчик смещения
    TEST_ASSERT_EQUAL_UINT32(0, ota_image_length(fake_read, &image, sizeof(data)));
}

static void test_rejects_image_longer_than_partition(void)
{
    uint8_t     data[1024];
    const uint32_t segments[2] = {100, 40};
    build_image(data, sizeof(data), segments, 2, true);

    FakeImage image = {data, sizeof(data), false};

    // Сегменты в раздел укладываются, а SHA-256 за ним уже не помещается
    TEST_ASSERT_EQUAL_UINT32(0, ota_image_length(fake_read, &image, 200));
}

static void test_zero_when_read_fails(void)
{
    uint8_t     data[1024];
    const uint32_t segments[1] = {100};
    build_image(data, sizeof(data), segments, 1, true);

    FakeImage image = {data, sizeof(data), true};

    TEST_ASSERT_EQUAL_UINT32(0, ota_image_length(fake_read, &image, sizeof(data)));
}

// ── Поиск маркера в образе ───────────────────────────────────────────────────

// Кладет настоящий маркер прошивки в образ по заданному смещению
static void put_tag(uint8_t *buf, size_t at)
{
    memcpy(buf + at, FW_IMAGE_TAG, strlen(FW_IMAGE_TAG) + 1);
}

// Образ на два блока с запасом — маркер в тестах кладется в разные места
static uint8_t s_scan_data[3 * OTA_SCAN_CHUNK];

static FakeImage make_scan_image(size_t tag_at)
{
    memset(s_scan_data, 0xff, sizeof(s_scan_data));
    put_tag(s_scan_data, tag_at);

    FakeImage image = {s_scan_data, sizeof(s_scan_data), false};
    return image;
}

static void test_scan_finds_tag_in_first_block(void)
{
    FakeImage image = make_scan_image(100);
    OtaTag    tag;

    TEST_ASSERT_TRUE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, tag.version.c_str());
}

static void test_scan_finds_tag_across_block_boundary(void)
{
    // Маркер начинается за четыре байта до конца первого блока: без перекрытия
    // при чтении он оказался бы разрезан
    FakeImage image = make_scan_image(OTA_SCAN_CHUNK - 4);
    OtaTag    tag;

    TEST_ASSERT_TRUE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, tag.version.c_str());
}

static void test_scan_finds_tag_at_block_start(void)
{
    FakeImage image = make_scan_image(OTA_SCAN_CHUNK);
    OtaTag    tag;

    TEST_ASSERT_TRUE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, tag.version.c_str());
}

static void test_scan_finds_tag_in_last_block(void)
{
    FakeImage image = make_scan_image(2 * OTA_SCAN_CHUNK + 500);
    OtaTag    tag;

    TEST_ASSERT_TRUE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
    TEST_ASSERT_EQUAL_STRING(FW_VERSION, tag.version.c_str());
}

static void test_scan_stops_at_image_end(void)
{
    // За длиной образа лежит нестертый хвост прошлой прошивки — его маркер
    // к текущему образу отношения не имеет
    FakeImage image = make_scan_image(2 * OTA_SCAN_CHUNK);
    OtaTag    tag;

    TEST_ASSERT_FALSE(ota_tag_scan(fake_read, &image, OTA_SCAN_CHUNK, tag));
}

static void test_scan_rejects_tag_cut_by_image_end(void)
{
    // Сигнатура в образ попала, а тело маркера обрезано — версию отсюда не берем
    FakeImage image = make_scan_image(OTA_SCAN_CHUNK - 20);
    OtaTag    tag;

    TEST_ASSERT_FALSE(ota_tag_scan(fake_read, &image, OTA_SCAN_CHUNK, tag));
}

static void test_scan_without_tag(void)
{
    memset(s_scan_data, 0xff, sizeof(s_scan_data));

    FakeImage image = {s_scan_data, sizeof(s_scan_data), false};
    OtaTag    tag;

    TEST_ASSERT_FALSE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
}

static void test_scan_when_read_fails(void)
{
    FakeImage image = make_scan_image(100);
    image.fail = true;

    OtaTag tag;

    TEST_ASSERT_FALSE(ota_tag_scan(fake_read, &image, sizeof(s_scan_data), tag));
}

static void test_scan_with_empty_image(void)
{
    FakeImage image = make_scan_image(100);
    OtaTag    tag;

    TEST_ASSERT_FALSE(ota_tag_scan(fake_read, &image, 0, tag));
}

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_own_tag_parses);
    RUN_TEST(test_finds_own_tag_in_buffer);
    RUN_TEST(test_partial_signature_does_not_shadow_tag);
    RUN_TEST(test_no_tag_in_buffer);
    RUN_TEST(test_signature_must_fit_into_block);
    RUN_TEST(test_block_shorter_than_signature);

    RUN_TEST(test_parses_all_three_fields);
    RUN_TEST(test_ignores_data_after_tag);
    RUN_TEST(test_requires_three_separators);
    RUN_TEST(test_rejects_empty_version);
    RUN_TEST(test_rejects_non_printable_field);
    RUN_TEST(test_rejects_non_ascii_field);
    RUN_TEST(test_rejects_truncated_tag);
    RUN_TEST(test_rejects_input_shorter_than_signature);
    RUN_TEST(test_does_not_look_past_max_length);

    RUN_TEST(test_stream_finds_tag_in_one_chunk);
    RUN_TEST(test_stream_finds_tag_across_chunks);
    RUN_TEST(test_stream_finds_tag_byte_by_byte);
    RUN_TEST(test_stream_rejects_missing_and_truncated_tag);
    RUN_TEST(test_ota_env_compatibility);

    RUN_TEST(test_counts_segments_and_hash);
    RUN_TEST(test_counts_image_without_hash);
    RUN_TEST(test_adds_full_block_when_already_aligned);
    RUN_TEST(test_counts_image_without_segments);
    RUN_TEST(test_rejects_foreign_magic);
    RUN_TEST(test_rejects_erased_partition);
    RUN_TEST(test_rejects_segment_larger_than_partition);
    RUN_TEST(test_rejects_image_longer_than_partition);
    RUN_TEST(test_zero_when_read_fails);

    RUN_TEST(test_scan_finds_tag_in_first_block);
    RUN_TEST(test_scan_finds_tag_across_block_boundary);
    RUN_TEST(test_scan_finds_tag_at_block_start);
    RUN_TEST(test_scan_finds_tag_in_last_block);
    RUN_TEST(test_scan_stops_at_image_end);
    RUN_TEST(test_scan_rejects_tag_cut_by_image_end);
    RUN_TEST(test_scan_without_tag);
    RUN_TEST(test_scan_when_read_fails);
    RUN_TEST(test_scan_with_empty_image);

    return UNITY_END();
}
