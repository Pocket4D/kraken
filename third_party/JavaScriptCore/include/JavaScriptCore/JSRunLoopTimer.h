/*
 * Copyright (C) 2012, 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class JSLock;
class VM;

class JSRunLoopTimer : public ThreadSafeRefCounted<JSRunLoopTimer> {
public:
    typedef void TimerNotificationType();
    using TimerNotificationCallback = RefPtr<WTF::SharedTask<TimerNotificationType>>;

    class Manager {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(Manager);
#if USE(CF)
        static void timerDidFireCallback(CFRunLoopTimerRef, void*);
#else
        void timerDidFireCallback();
#endif
        Manager() = default;

        void timerDidFire();

    public:
        using EpochTime = Seconds;

        static Manager& shared();
        void registerVM(VM&);
        void unregisterVM(VM&);
        void scheduleTimer(JSRunLoopTimer&, Seconds nextFireTime);
        void cancelTimer(JSRunLoopTimer&);

        Optional<Seconds> timeUntilFire(JSRunLoopTimer&);

#if USE(CF)
        void didChangeRunLoop(VM&, CFRunLoopRef newRunLoop);
#endif

    private:
        Lock m_lock;

        struct PerVMData {
            PerVMData() = default;
#if USE(CF)
            PerVMData(Manager&) { }
#else
            PerVMData(Manager&);
#endif
            PerVMData(PerVMData&&) = default;
            PerVMData& operator=(PerVMData&&) = default;

            ~PerVMData();

#if USE(CF)
            void setRunLoop(Manager*, CFRunLoopRef);
            RetainPtr<CFRunLoopTimerRef> timer;
            RetainPtr<CFRunLoopRef> runLoop;
            CFRunLoopTimerContext context;
#else
            RunLoop* runLoop;
            std::unique_ptr<RunLoop::Timer<Manager>> timer;
#endif
            Vector<std::pair<Ref<JSRunLoopTimer>, EpochTime>> timers;
        };

        HashMap<Ref<JSLock>, PerVMData> m_mapping;
    };

    JSRunLoopTimer(VM*);
    JS_EXPORT_PRIVATE virtual ~JSRunLoopTimer();
    virtual void doWork(VM&) = 0;

    void setTimeUntilFire(Seconds intervalInSeconds);
    void cancelTimer();
    bool isScheduled() const { return m_isScheduled; }

    // Note: The only thing the timer notification callback cannot do is
    // call setTimeUntilFire(). This will cause a deadlock. It would not
    // be hard to make this work, however, there are no clients that need
    // this behavior. We should implement it only if we find that we need it.
    JS_EXPORT_PRIVATE void addTimerSetNotification(TimerNotificationCallback);
    JS_EXPORT_PRIVATE void removeTimerSetNotification(TimerNotificationCallback);

    JS_EXPORT_PRIVATE Optional<Seconds> timeUntilFire();

protected:
    static const Seconds s_decade;
    Ref<JSLock> m_apiLock;

private:
    friend class Manager;

    void timerDidFire();

    HashSet<TimerNotificationCallback> m_timerSetCallbacks;
    Lock m_timerCallbacksLock;

    Lock m_lock;
    bool m_isScheduled { false };
};
    
} // namespace JSC
