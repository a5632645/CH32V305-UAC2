#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct StereoSample {
    int32_t left;
    int32_t right;
};

struct Kfifo {
    uint32_t wpos;
    uint32_t rpos;
    uint32_t mask;
    struct StereoSample data[];
};

static inline uint32_t Kfifo_Size(struct Kfifo* f) {
    return f->wpos - f->rpos;
}

static inline uint32_t Kfifo_FreeSpace(struct Kfifo* f) {
    return (f->mask + 1) - Kfifo_Size(f);
}

static inline bool Kfifo_IsFull(struct Kfifo* f) {
    return (f->wpos - f->rpos) == (f->mask + 1);
}

static inline bool Kfifo_IsEmpty(struct Kfifo* f) {
    return f->wpos == f->rpos;
}

static inline void Kfifo_Push(struct Kfifo* f, struct StereoSample v) {
    f->data[f->wpos & f->mask] = v;
    asm volatile("" ::: "memory");
    ++f->wpos;
}

static inline struct StereoSample Kfifo_Pop(struct Kfifo* f) {
    struct StereoSample v = f->data[f->rpos & f->mask];
    asm volatile("" ::: "memory");
    ++f->rpos;
    return v;
}

static inline uint32_t Kfifo_TryPush(struct Kfifo* f, const struct StereoSample* src, uint32_t count) {
    uint32_t free = Kfifo_FreeSpace(f);
    if (count > free) count = free;

    uint32_t wpos = f->wpos & f->mask;
    uint32_t till_end = (f->mask + 1) - wpos;

    if (count <= till_end) {
        memcpy(&f->data[wpos], src, count * sizeof(struct StereoSample));
    }
    else {
        memcpy(&f->data[wpos], src, till_end * sizeof(struct StereoSample));
        memcpy(&f->data[0], src + till_end, (count - till_end) * sizeof(struct StereoSample));
    }

    asm volatile("" ::: "memory");
    f->wpos += count;
    return count;
}

static struct StereoSample* Kfifo_ContinueReadBegin(struct Kfifo* f, uint32_t* count) {
    uint32_t rpos = f->rpos & f->mask;
    uint32_t size = Kfifo_Size(f);
    uint32_t till_end = (f->mask + 1) - rpos;

    *count = (size < till_end) ? size : till_end;
    return &f->data[rpos];
}

static void Kfifo_ContinueReadEnd(struct Kfifo* f, uint32_t count) {
    asm volatile("" ::: "memory");
    f->rpos += count;
}
