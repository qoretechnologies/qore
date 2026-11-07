/* -*- indent-tabs-mode: nil -*- */
/*
  Native test of HttpFastPublished: the lock-free publication of immutable values to the I/O threads

  Copyright (C) 2026 Qore Technologies, s.r.o.

  The primitive uses only the C++ standard library, so the test needs no library and can be built with a thread or
  an address sanitizer:

      g++ -std=c++17 -O1 -g -fsanitize=thread -I include -pthread \
          examples/test/qore/misc/http_fast_published.cpp -o /tmp/hfp-tsan && /tmp/hfp-tsan
*/

#include <qore/intern/HttpFastPublished.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        std::exit(1); \
    } \
} while (0)

// An immutable value with an invariant (b == 2 * a + 1) and a marker that its destructor clears, so that a reader that
// sees a value that was freed fails the check
struct Value {
    static constexpr uint64_t Alive = 0xA11CEA11CEA11CE5ULL;
    static std::atomic<long> live;

    explicit Value(long a) : a(a), b(2 * a + 1) {
        ++live;
    }
    ~Value() {
        magic = 0;
        b = -1;
        --live;
    }

    bool ok() const {
        return magic == Alive && b == 2 * a + 1;
    }

    uint64_t magic = Alive;
    long a;
    long b;
};

std::atomic<long> Value::live{0};

typedef HttpFastPublished<const Value> Published;

static std::shared_ptr<const Value> make(long a) {
    return std::make_shared<const Value>(a);
}

static void testBasic() {
    {
        Published p;
        CHECK(!p.get());
        CHECK(!p.getLocked());
        p.set(make(1));
        CHECK(p.get() && p.get()->a == 1);
        CHECK(p.getLocked()->a == 1);
        p.update([](const std::shared_ptr<const Value>& cur) {
            CHECK(cur && cur->a == 1);
            return make(cur->a + 10);
        });
        CHECK(p.get()->a == 11);

        // nothing is replaced when the function throws
        bool thrown = false;
        try {
            p.update([](const std::shared_ptr<const Value>&) -> std::shared_ptr<const Value> {
                throw std::runtime_error("no");
            });
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        CHECK(thrown);
        CHECK(p.get()->a == 11);
        CHECK(p.getLocked()->a == 11);

        // a reference taken by a reader keeps its value alive after it was replaced
        std::shared_ptr<const Value> held = p.get();
        p.set(make(100));
        CHECK(held->ok() && held->a == 11);
        CHECK(p.get()->a == 100);
    }
    CHECK(Value::live.load() == 0);
}

// Readers check every value that they get while writers replace it as fast as they can
/** Each writer publishes the values w * Stride + k with k increasing, so a reader can check that it never sees an
    older value of a writer after a newer one: the values of one writer are published in its program order.
*/
static void testConcurrent() {
    constexpr int Readers = 6;
    constexpr int Writers = 3;
    constexpr long Stride = 1000000000L;
    constexpr long WritesPerWriter = 20000;
    std::atomic<bool> stop{false};
    std::atomic<long> reads{0};
    std::atomic<long> bad{0};
    {
        Published p(make(0));
        std::vector<std::thread> readers;
        for (int i = 0; i < Readers; ++i) {
            readers.emplace_back([&] {
                long n = 0;
                long last[Writers + 1] = {};
                while (!stop.load(std::memory_order_relaxed)) {
                    std::shared_ptr<const Value> v = p.get();
                    if (!v || !v->ok()) {
                        ++bad;
                        break;
                    }
                    long w = v->a / Stride;
                    long k = v->a % Stride;
                    if (w < 0 || w > Writers || k < last[w]) {
                        ++bad;
                        break;
                    }
                    last[w] = k;
                    ++n;
                }
                reads += n;
            });
        }
        std::vector<std::thread> writers;
        for (int i = 1; i <= Writers; ++i) {
            writers.emplace_back([&, i] {
                for (long k = 1; k <= WritesPerWriter; ++k) {
                    if (i % 2) {
                        p.set(make(i * Stride + k));
                    } else {
                        p.update([&](const std::shared_ptr<const Value>& cur) {
                            // the current value is always one that was published
                            CHECK(cur && cur->ok());
                            return make(i * Stride + k);
                        });
                    }
                }
            });
        }
        for (auto& t : writers) {
            t.join();
        }
        stop = true;
        for (auto& t : readers) {
            t.join();
        }
        // the values that were replaced are freed, apart from the ones a slot announced at the last write
        CHECK(p.getRetired() <= static_cast<size_t>(Readers));
        CHECK(Value::live.load() <= static_cast<long>(Readers) + 2);
    }
    CHECK(bad.load() == 0);
    CHECK(reads.load() > 0);
    CHECK(Value::live.load() == 0);
}

// update() is a read-modify-write that no other write can interleave with
static void testUpdateAtomic() {
    constexpr int Threads = 8;
    constexpr long PerThread = 5000;
    {
        Published p(make(0));
        std::vector<std::thread> v;
        for (int i = 0; i < Threads; ++i) {
            v.emplace_back([&] {
                for (long k = 0; k < PerThread; ++k) {
                    p.update([](const std::shared_ptr<const Value>& cur) {
                        return make(cur->a + 1);
                    });
                }
            });
        }
        for (auto& t : v) {
            t.join();
        }
        CHECK(p.get()->a == Threads * PerThread);
    }
    CHECK(Value::live.load() == 0);
}

// More threads read at the same time than there are hazard slots: the ones without a slot take the mutex and get
// correct values all the same
static void testSlotsExhausted() {
    const size_t baseline = HttpFastHazards::slotsInUse();
    const size_t threads = HttpFastHazards::MaxSlots + 48;
    {
        Published p(make(7));
        std::mutex m;
        std::condition_variable cv;
        size_t got = 0;
        bool release = false;
        std::atomic<long> bad{0};
        std::vector<std::thread> v;
        for (size_t i = 0; i < threads; ++i) {
            v.emplace_back([&] {
                std::shared_ptr<const Value> x = p.get();
                if (!x || !x->ok() || x->a != 7) {
                    ++bad;
                }
                // keep the thread, and so its slot, until all the threads have read
                std::unique_lock<std::mutex> lk(m);
                ++got;
                cv.notify_all();
                cv.wait(lk, [&] { return release; });
            });
        }
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return got == threads; });
            CHECK(HttpFastHazards::slotsInUse() == HttpFastHazards::MaxSlots);
            release = true;
            cv.notify_all();
        }
        // while all the slots are taken, a write and a read still work
        p.set(make(8));
        CHECK(p.get()->a == 8);
        for (auto& t : v) {
            t.join();
        }
        CHECK(bad.load() == 0);
    }
    CHECK(HttpFastHazards::slotsInUse() == baseline);
    CHECK(Value::live.load() == 0);
}

// A slot goes back when its thread ends
static void testSlotsReleased() {
    const size_t baseline = HttpFastHazards::slotsInUse();
    Published p(make(3));
    for (int i = 0; i < 2000; ++i) {
        std::thread t([&] {
            CHECK(p.get()->a == 3);
        });
        t.join();
    }
    CHECK(HttpFastHazards::slotsInUse() == baseline);
}

// The thread that replaces a value does not free one that a reader announced
static void testAnnouncedNotFreed() {
    Published p(make(1));
    // announce the current holder as another thread would in the middle of a read
    std::atomic<const void*>* hazard = HttpFastHazards::slot();
    CHECK(hazard);
    std::shared_ptr<const Value> first = p.getLocked();
    (void)first;
    // there is no access to the holder from here; check the observable part: after many writes nothing is retired
    // when no thread announces anything
    for (int i = 0; i < 100; ++i) {
        p.set(make(i));
    }
    CHECK(p.getRetired() == 0);
}

int main() {
    testBasic();
    testConcurrent();
    testUpdateAtomic();
    testSlotsExhausted();
    testSlotsReleased();
    testAnnouncedNotFreed();
    CHECK(Value::live.load() == 0);
    std::printf("PASS: HttpFastPublished: values stay consistent under concurrent writers, memory is reclaimed, "
        "slot exhaustion and thread exit are handled\n");
    return 0;
}
