#include "eventLoop.h"
#include "loopThread.h"
#include "timeTask.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
using namespace std::chrono_literals;

void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool WaitUntil(const std::function<bool()> &condition,
               std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (condition())
            return true;
        std::this_thread::sleep_for(10ms);
    }
    return condition();
}

void TestTaskIsIdempotent()
{
    int calls = 0;
    int destroys = 0;
    {
        Timetask task(1, 1, [&] { ++calls; });
        task.setDestroy([&] { ++destroys; });

        task.executeTask();
        task.executeTask();

        Check(calls == 1, "Timetask callback must run only once");
        Check(task.isInExecute(), "Timetask must record execution state");
    }
    Check(destroys == 1, "Timetask destroy callback must run once");
}

void TestRoundCalculation()
{
    Timetask task60(60, 60, [] {});
    Timetask task61(61, 61, [] {});
    Timetask task120(120, 120, [] {});
    Timetask task121(121, 121, [] {});
    task60.setDestroy([] {});
    task61.setDestroy([] {});
    task120.setDestroy([] {});
    task121.setDestroy([] {});

    Check(task60.getCount() == 0, "60-second timer must use zero rounds");
    Check(task61.getCount() == 1, "61-second timer must use one round");
    Check(task120.getCount() == 1, "120-second timer must use one round");
    Check(task121.getCount() == 2, "121-second timer must use two rounds");
}

void TestTimerFiresOnce()
{
    LoopThread thread;
    EventLoop *loop = thread.GetLoop();
    std::atomic<int> calls{0};

    loop->TimerAdd(1, 1, [&] { ++calls; });
    Check(WaitUntil([&] { return calls.load() == 1; }, 3500ms),
          "one-second timer did not fire");
    std::this_thread::sleep_for(300ms);
    Check(calls.load() == 1, "timer callback fired more than once");
}

void TestTimerCancel()
{
    LoopThread thread;
    EventLoop *loop = thread.GetLoop();
    std::atomic<int> calls{0};

    loop->TimerAdd(2, 1, [&] { ++calls; });
    std::this_thread::sleep_for(50ms);
    loop->TimerCancel(2);
    std::this_thread::sleep_for(2200ms);

    Check(calls.load() == 0, "cancelled timer must not fire");
}

void TestTimerRefresh()
{
    LoopThread thread;
    EventLoop *loop = thread.GetLoop();
    std::atomic<int> calls{0};

    loop->TimerAdd(3, 2, [&] { ++calls; });
    std::this_thread::sleep_for(100ms);
    loop->TimerRefresh(3);

    std::this_thread::sleep_for(1300ms);
    Check(calls.load() == 0, "refreshed timer fired before its new deadline");
    Check(WaitUntil([&] { return calls.load() == 1; }, 1800ms),
          "refreshed timer did not fire");
}

void TestShutdownCancelsPendingTimer()
{
    std::atomic<int> calls{0};
    {
        LoopThread thread;
        EventLoop *loop = thread.GetLoop();
        std::atomic<bool> installed{false};

        loop->RunInLoop([&] {
            loop->TimerAdd(4, 2, [&] { ++calls; });
            installed = true;
        });

        Check(WaitUntil([&] { return installed.load(); }, 1000ms),
              "pending timer was not installed");
        thread.stop();
    }

    Check(calls.load() == 0,
          "destroying EventLoop must cancel pending timers");
}
} // namespace

int main()
{
    try
    {
        TestTaskIsIdempotent();
        TestRoundCalculation();
        TestTimerFiresOnce();
        TestTimerCancel();
        TestTimerRefresh();
        TestShutdownCancelsPendingTimer();
        std::cout << "Timer wheel checks passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
