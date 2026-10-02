/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
 *
 */

#ifndef ScriptExecutionContext_h
#define ScriptExecutionContext_h

#include "ActiveDOMObject.h"
#include "ResourceRequest.h"
#include "SecurityContext.h"
#include "Supplementable.h"
#include "ConsoleTypes.h"
#include <wtf/HashSet.h>

namespace WebCore {

class CachedScript;
class EventQueue;
class EventTarget;
class PublicURLManager;
class SecurityOrigin;
class URL;

class ScriptExecutionContext : public SecurityContext, public Supplementable<ScriptExecutionContext> {
public:
    ScriptExecutionContext();
    virtual ~ScriptExecutionContext();

    virtual bool isDocument() const { return false; }
    virtual bool isWorkerGlobalScope() const { return false; }

    virtual bool isContextThread() const { return true; }
    virtual bool isJSExecutionForbidden() const = 0;

    virtual const URL& url() const = 0;
    virtual URL completeURL(const String& url) const = 0;

    virtual void disableEval(const String& errorMessage) = 0;

    bool sanitizeScriptError(String& errorMessage, int& lineNumber, int& columnNumber, String& sourceURL, CachedScript* = nullptr);
    void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, CachedScript* = nullptr);

    void addConsoleMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, unsigned long requestIdentifier = 0);
    virtual void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0) = 0;

    virtual SecurityOrigin* topOrigin() const = 0;

    virtual bool shouldBypassMainWorldContentSecurityPolicy() const { return false; }

    PublicURLManager& publicURLManager();

    // Active objects are not garbage collected even if inaccessible, e.g. because their activity may result in callbacks being invoked.
    WEBCORE_EXPORT bool canSuspendActiveDOMObjectsForDocumentSuspension(Vector<ActiveDOMObject*>* unsuspendableObjects = nullptr);

    // Active objects can be asked to suspend even if canSuspendActiveDOMObjectsForDocumentSuspension() returns 'false' -
    // step-by-step JS debugging is one example.
    virtual void suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension);
    virtual void resumeActiveDOMObjects(ActiveDOMObject::ReasonForSuspension);
    virtual void stopActiveDOMObjects();

    bool activeDOMObjectsAreSuspended() const { return m_activeDOMObjectsAreSuspended; }
    bool activeDOMObjectsAreStopped() const { return m_activeDOMObjectsAreStopped; }

    // Called from the constructor and destructors of ActiveDOMObject.
    void didCreateActiveDOMObject(ActiveDOMObject&);
    void willDestroyActiveDOMObject(ActiveDOMObject&);

    // Called after the construction of an ActiveDOMObject to synchronize suspend state.
    void suspendActiveDOMObjectIfNeeded(ActiveDOMObject&);

    void didCreateDestructionObserver(ContextDestructionObserver&);
    void willDestroyDestructionObserver(ContextDestructionObserver&);

    virtual void didLoadResourceSynchronously(const ResourceRequest&);

    void ref() { refScriptExecutionContext(); }
    void deref() { derefScriptExecutionContext(); }

    class Task {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        enum CleanupTaskTag { CleanupTask };

        template<typename T, typename = typename std::enable_if<!std::is_base_of<Task, T>::value && std::is_convertible<T, std::function<void (ScriptExecutionContext&)>>::value>::type>
        Task(T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(false)
        {
        }

        Task(std::function<void()> task)
            : m_task([task](ScriptExecutionContext&) { task(); })
            , m_isCleanupTask(false)
        {
        }

        template<typename T, typename = typename std::enable_if<std::is_convertible<T, std::function<void (ScriptExecutionContext&)>>::value>::type>
        Task(CleanupTaskTag, T task)
            : m_task(WTFMove(task))
            , m_isCleanupTask(true)
        {
        }

        Task(Task&& other)
            : m_task(WTFMove(other.m_task))
            , m_isCleanupTask(other.m_isCleanupTask)
        {
        }

        void performTask(ScriptExecutionContext& context) { m_task(context); }
        bool isCleanupTask() const { return m_isCleanupTask; }

    protected:
        std::function<void (ScriptExecutionContext&)> m_task;
        bool m_isCleanupTask;
    };

    virtual void postTask(Task) = 0; // Executes the task on context's thread asynchronously.

    virtual EventQueue& eventQueue() const = 0;

#if ENABLE(SUBTLE_CRYPTO)
    virtual bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) = 0;
    virtual bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) = 0;
#endif

protected:
    class AddConsoleMessageTask : public Task {
    public:
        AddConsoleMessageTask(MessageSource source, MessageLevel level, const StringCapture& message)
            : Task([source, level, message](ScriptExecutionContext& context) {
                context.addConsoleMessage(source, level, message.string());
            })
        {
        }
    };

    ActiveDOMObject::ReasonForSuspension reasonForSuspendingActiveDOMObjects() const { return m_reasonForSuspendingActiveDOMObjects; }

    bool hasPendingActivity() const;

private:
    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, unsigned long requestIdentifier = 0) = 0;
    virtual EventTarget* errorEventTarget() = 0;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber) = 0;
    bool dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, CachedScript*);

    virtual void refScriptExecutionContext() = 0;
    virtual void derefScriptExecutionContext() = 0;

    void checkConsistency() const;

    HashSet<ContextDestructionObserver*> m_destructionObservers;
    HashSet<ActiveDOMObject*> m_activeDOMObjects;

    bool m_inDispatchErrorEvent;
    class PendingException;
    std::unique_ptr<Vector<std::unique_ptr<PendingException>>> m_pendingExceptions;

    bool m_activeDOMObjectsAreSuspended;
    ActiveDOMObject::ReasonForSuspension m_reasonForSuspendingActiveDOMObjects;
    bool m_activeDOMObjectsAreStopped;

    std::unique_ptr<PublicURLManager> m_publicURLManager;

    bool m_activeDOMObjectAdditionForbidden;

#if !ASSERT_DISABLED
    bool m_inScriptExecutionContextDestructor;
#endif
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_activeDOMObjectRemovalForbidden;
#endif
};

} // namespace WebCore

#endif // ScriptExecutionContext_h
