// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: MIT
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <uv.h>

static int timer_called;
static int work_completed;
static int result;

static void timer_cb(uv_timer_t* timer) {
    ++timer_called;
    uv_close((uv_handle_t*)timer, NULL);
}

static void work_cb(uv_work_t* work) {
    int* value = work->data;
    *value = 6 * 7;
}

static void after_work_cb(uv_work_t* work, int status) {
    assert(status == 0);
    assert(*(int*)work->data == 42);
    ++work_completed;
}

int main(void) {
    uv_loop_t loop;
    uv_timer_t timer;
    uv_work_t work;
    uv_fs_t request;
    assert(uv_version() == UV_VERSION_HEX);
    assert(uv_loop_init(&loop) == 0);
    assert(uv_timer_init(&loop, &timer) == 0);
    assert(uv_timer_start(&timer, timer_cb, 1, 0) == 0);
    work.data = &result;
    assert(uv_queue_work(&loop, &work, work_cb, after_work_cb) == 0);
    assert(uv_run(&loop, UV_RUN_DEFAULT) == 0);
    assert(timer_called == 1 && work_completed == 1 && result == 42);

    const char payload[] = "installed libuv SDK";
    int fd = uv_fs_open(&loop, &request, "data", O_RDWR | O_CREAT | O_EXCL, 0600, NULL);
    assert(fd >= 0);
    uv_fs_req_cleanup(&request);
    uv_buf_t buffer = uv_buf_init((char*)payload, sizeof(payload));
    assert(uv_fs_write(&loop, &request, fd, &buffer, 1, 0, NULL) == (ssize_t)sizeof(payload));
    uv_fs_req_cleanup(&request);
    char content[sizeof(payload)] = {0};
    buffer = uv_buf_init(content, sizeof(content));
    assert(uv_fs_read(&loop, &request, fd, &buffer, 1, 0, NULL) == (ssize_t)sizeof(payload));
    uv_fs_req_cleanup(&request);
    assert(memcmp(content, payload, sizeof(payload)) == 0);
    assert(uv_fs_close(&loop, &request, fd, NULL) == 0);
    uv_fs_req_cleanup(&request);
    assert(uv_fs_unlink(&loop, &request, "data", NULL) == 0);
    uv_fs_req_cleanup(&request);
    assert(uv_fs_open(&loop, &request, "data", O_RDONLY, 0, NULL) == UV_ENOENT);
    uv_fs_req_cleanup(&request);
    assert(uv_loop_close(&loop) == 0);
    return 0;
}
