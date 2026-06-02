#include "unity.h"
#include "iobuf.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* full_write must deliver every byte to a regular file. */
static void test_full_write_writes_all(void)
{
    char tmpl[] = "/tmp/mat_iobuf_XXXXXX";
    int fd = mkstemp(tmpl);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);

    const char payload[] = "the quick brown fox\n\x00\x01\xfftail";
    const size_t n = sizeof payload; /* include the embedded NUL + trailing */
    TEST_ASSERT_EQUAL_INT(0, mat_full_write(fd, payload, n));

    TEST_ASSERT_EQUAL_INT64((long long)n, lseek(fd, 0, SEEK_CUR));

    char back[64];
    TEST_ASSERT_EQUAL_INT64(0, lseek(fd, 0, SEEK_SET));
    ssize_t r = read(fd, back, sizeof back);
    TEST_ASSERT_EQUAL_INT64((long long)n, (long long)r);
    TEST_ASSERT_EQUAL_MEMORY(payload, back, n);

    close(fd);
    unlink(tmpl);
}

/* Zero-length write is a successful no-op. */
static void test_full_write_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, mat_full_write(STDOUT_FILENO, "", 0));
}

/* Buffer sizing stays within the documented bounds for any fd. */
static void test_iobuf_size_bounds(void)
{
    size_t s = mat_iobuf_size(STDIN_FILENO, STDOUT_FILENO);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(4096u, (unsigned)s);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(1u << 20, (unsigned)s);

    /* A regular file should yield a healthy buffer, never below default. */
    char tmpl[] = "/tmp/mat_iobuf_reg_XXXXXX";
    int fd = mkstemp(tmpl);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
    size_t rs = mat_iobuf_size(fd, STDOUT_FILENO);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(128u << 10, (unsigned)rs);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(1u << 20, (unsigned)rs);
    close(fd);
    unlink(tmpl);
}

static void test_pipe_write_roundtrip(void)
{
    int pfd[2];
    TEST_ASSERT_EQUAL_INT(0, pipe(pfd));
    const char data[] = "hello from pipe_write";
    TEST_ASSERT_EQUAL_INT(0, mat_pipe_write(pfd[1], data, sizeof data - 1));
    close(pfd[1]);
    char buf[64];
    ssize_t r = read(pfd[0], buf, sizeof buf);
    close(pfd[0]);
    TEST_ASSERT_EQUAL_INT64((long long)(sizeof data - 1), (long long)r);
    TEST_ASSERT_EQUAL_MEMORY(data, buf, sizeof data - 1);
}

static void test_pipe_write_to_file(void)
{
    char tmpl[] = "/tmp/mat_pw_XXXXXX";
    int fd = mkstemp(tmpl);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
    const char data[] = "file fallback";
    TEST_ASSERT_EQUAL_INT(0, mat_pipe_write(fd, data, sizeof data - 1));
    lseek(fd, 0, SEEK_SET);
    char buf[32];
    ssize_t r = read(fd, buf, sizeof buf);
    TEST_ASSERT_EQUAL_INT64((long long)(sizeof data - 1), (long long)r);
    TEST_ASSERT_EQUAL_MEMORY(data, buf, sizeof data - 1);
    close(fd);
    unlink(tmpl);
}

static void test_pipe_write_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, mat_pipe_write(STDOUT_FILENO, "", 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_write_writes_all);
    RUN_TEST(test_full_write_zero);
    RUN_TEST(test_iobuf_size_bounds);
    RUN_TEST(test_pipe_write_roundtrip);
    RUN_TEST(test_pipe_write_to_file);
    RUN_TEST(test_pipe_write_zero);
    return UNITY_END();
}
