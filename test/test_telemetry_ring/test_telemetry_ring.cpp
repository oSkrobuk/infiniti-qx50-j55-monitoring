#include <unity.h>

#include "BuzzerStub.h"
#include "TelemetryRing.h"

static StoredTelemetryRecord record(uint32_t uptime, int16_t value)
{
    StoredTelemetryRecord result{};
    result.uptime_ms = uptime;
    result.values[0] = value;
    return result;
}

void test_ring_keeps_sequence_and_overwrites_oldest()
{
    TelemetryRing ring;
    TEST_ASSERT_TRUE(ring.init(2));
    TEST_ASSERT_FALSE(ring.append(record(10, 1)));
    TEST_ASSERT_FALSE(ring.append(record(20, 2)));
    TEST_ASSERT_TRUE(ring.append(record(30, 3)));
    TEST_ASSERT_EQUAL_UINT32(1, ring.oldest_seq());
    TEST_ASSERT_EQUAL_UINT32(3, ring.next_seq());
    StoredTelemetryRecord out{};
    TEST_ASSERT_FALSE(ring.get(0, out));
    TEST_ASSERT_TRUE(ring.get(2, out));
    TEST_ASSERT_EQUAL_INT16(3, out.values[0]);
}

void test_acknowledged_overwrite_is_not_lost()
{
    TelemetryRing ring;
    TEST_ASSERT_TRUE(ring.init(1));
    ring.append(record(10, 1));
    ring.ack(0);
    TEST_ASSERT_FALSE(ring.append(record(20, 2)));
    TEST_ASSERT_EQUAL_UINT32(0, ring.lost_unacked_count());
}

void test_reset_starts_a_new_sequence()
{
    TelemetryRing ring;
    TEST_ASSERT_TRUE(ring.init(2));
    ring.append(record(10, 1));
    ring.reset();
    TEST_ASSERT_TRUE(ring.empty());
    TEST_ASSERT_EQUAL_UINT32(0, ring.next_seq());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_ring_keeps_sequence_and_overwrites_oldest);
    RUN_TEST(test_acknowledged_overwrite_is_not_lost);
    RUN_TEST(test_reset_starts_a_new_sequence);
    return UNITY_END();
}
