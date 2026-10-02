/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 * Copyright (C) 2025-2026 General Development Systems
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

#include "config.h"
#include "ScriptExecutionContext.h"

#include "CachedScript.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "PublicURLManager.h"
#include "Settings.h"
#include <wtf/MainThread.h>
#include <wtf/Ref.h>

namespace WebCore {

class ScriptExecutionContext::PendingException {
    WTF_MAKE_NONCOPYABLE(PendingException);
public:
    PendingException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
        : m_errorMessage(errorMessage)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL)
    {
    }
    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
};

ScriptExecutionContext::ScriptExecutionContext()
    : m_inDispatchErrorEvent(false)
    , m_activeDOMObjectsAreSuspended(false)
    , m_reasonForSuspendingActiveDOMObjects(static_cast<ActiveDOMObject::ReasonForSuspension>(-1))
    , m_activeDOMObjectsAreStopped(false)
    , m_activeDOMObjectAdditionForbidden(false)
#if !ASSERT_DISABLED
    , m_inScriptExecutionContextDestructor(false)
#endif
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    , m_activeDOMObjectRemovalForbidden(false)
#endif
{
}

#if ASSERT_DISABLED

inline void ScriptExecutionContext::checkConsistency() const
{
}

#else

void ScriptExecutionContext::checkConsistency() const
{
    for (auto* destructionObserver : m_destructionObservers)
        ASSERT(destructionObserver->scriptExecutionContext() == this);

    for (auto* activeDOMObject : m_activeDOMObjects) {
        ASSERT(activeDOMObject->scriptExecutionContext() == this);
        activeDOMObject->assertSuspendIfNeededWasCalled();
    }
}

#endif

ScriptExecutionContext::~ScriptExecutionContext()
{
    checkConsistency();

#if !ASSERT_DISABLED
    m_inScriptExecutionContextDestructor = true;
#endif

    while (auto* destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->contextDestroyed();

#if !ASSERT_DISABLED
    m_inScriptExecutionContextDestructor = false;
#endif
}

void ScriptExecutionContext::didLoadResourceSynchronously(const ResourceRequest&)
{
}

bool ScriptExecutionContext::canSuspendActiveDOMObjectsForDocumentSuspension(Vector<ActiveDOMObject*>* unsuspendableObjects)
{
    checkConsistency();

    bool canSuspend = true;

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: canSuspend
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT_WITH_SECURITY_IMPLICATION or RELEASE_ASSERT will fire if this happens, but it's important to code
    // canSuspend functions so it will not happen!
    NoEventDispatchAssertion assertNoEventDispatch;
    for (auto* activeDOMObject : m_activeDOMObjects) {
        if (!activeDOMObject->canSuspendForDocumentSuspension()) {
            canSuspend = false;
            if (unsuspendableObjects)
                unsuspendableObjects->append(activeDOMObject);
            else
                break;
        }
    }

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = false;
#endif

    return canSuspend;
}

void ScriptExecutionContext::suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    checkConsistency();

    if (m_activeDOMObjectsAreSuspended) {
        // A page may subsequently suspend DOM objects, say as part of entering the page cache, after the embedding
        // client requested the page be suspended. We ignore such requests so long as the embedding client requested
        // the suspension first. See <rdar://problem/13754896> for more details.
        ASSERT(m_reasonForSuspendingActiveDOMObjects == ActiveDOMObject::PageWillBeSuspended);
        return;
    }

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: suspend
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT_WITH_SECURITY_IMPLICATION or RELEASE_ASSERT will fire if this happens, but it's important to code
    // suspend functions so it will not happen!
    NoEventDispatchAssertion assertNoEventDispatch;
    for (auto* activeDOMObject : m_activeDOMObjects)
        activeDOMObject->suspend(why);

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = false;
#endif

    m_activeDOMObjectsAreSuspended = true;
    m_reasonForSuspendingActiveDOMObjects = why;
}

void ScriptExecutionContext::resumeActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    checkConsistency();

    if (m_reasonForSuspendingActiveDOMObjects != why)
        return;
    m_activeDOMObjectsAreSuspended = false;

    m_activeDOMObjectAdditionForbidden = true;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = true;
#endif

    // We assume that m_activeDOMObjects will not change during iteration: resume
    // functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT_WITH_SECURITY_IMPLICATION or RELEASE_ASSERT will fire if this happens, but it's important to code
    // resume functions so it will not happen!
    NoEventDispatchAssertion assertNoEventDispatch;
    for (auto* activeDOMObject : m_activeDOMObjects)
        activeDOMObject->resume();

    m_activeDOMObjectAdditionForbidden = false;
#if !ASSERT_DISABLED || ENABLE(SECURITY_ASSERTIONS)
    m_activeDOMObjectRemovalForbidden = false;
#endif
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    checkConsistency();

    if (m_activeDOMObjectsAreStopped)
        return;
    m_activeDOMObjectsAreStopped = true;

    // Make a frozen copy of the objects so we can iterate while new ones might be destroyed.
    Vector<ActiveDOMObject*> possibleActiveDOMObjects;
    copyToVector(m_activeDOMObjects, possibleActiveDOMObjects);

    m_activeDOMObjectAdditionForbidden = true;

    // We assume that new objects will not be added to m_activeDOMObjects during iteration:
    // stop functions should not add new active DOM objects, nor execute arbitrary JavaScript.
    // An ASSERT_WITH_SECURITY_IMPLICATION or RELEASE_ASSERT will fire if this happens, but it's important to code stop functions
    // so it will not happen!
    NoEventDispatchAssertion assertNoEventDispatch;
    for (auto* activeDOMObject : possibleActiveDOMObjects) {
        // Check if this object was deleted already. If so, just skip it.
        // Calling contains on a possibly-already-deleted object is OK because we guarantee
        // no new object can be added, so even if a new object ends up allocated with the
        // same address, that will be *after* this function exits.
        if (!m_activeDOMObjects.contains(activeDOMObject))
            continue;
        activeDOMObject->stop();
    }

    m_activeDOMObjectAdditionForbidden = false;
}

void ScriptExecutionContext::suspendActiveDOMObjectIfNeeded(ActiveDOMObject& activeDOMObject)
{
    ASSERT(m_activeDOMObjects.contains(&activeDOMObject));
    if (m_activeDOMObjectsAreSuspended)
        activeDOMObject.suspend(m_reasonForSuspendingActiveDOMObjects);
    if (m_activeDOMObjectsAreStopped)
        activeDOMObject.stop();
}

void ScriptExecutionContext::didCreateActiveDOMObject(ActiveDOMObject& activeDOMObject)
{
    // The m_activeDOMObjectAdditionForbidden check is a RELEASE_ASSERT because of the
    // consequences of having an ActiveDOMObject that is not correctly reflected in the set.
    // If we do have one of those, it can possibly be a security vulnerability. So we'd
    // rather have a crash than continue running with the set possibly compromised.
    ASSERT(!m_inScriptExecutionContextDestructor);
    RELEASE_ASSERT(!m_activeDOMObjectAdditionForbidden);
    m_activeDOMObjects.add(&activeDOMObject);
}

void ScriptExecutionContext::willDestroyActiveDOMObject(ActiveDOMObject& activeDOMObject)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_activeDOMObjectRemovalForbidden);
    m_activeDOMObjects.remove(&activeDOMObject);
}

void ScriptExecutionContext::didCreateDestructionObserver(ContextDestructionObserver& observer)
{
    ASSERT(!m_inScriptExecutionContextDestructor);
    m_destructionObservers.add(&observer);
}

void ScriptExecutionContext::willDestroyDestructionObserver(ContextDestructionObserver& observer)
{
    m_destructionObservers.remove(&observer);
}

bool ScriptExecutionContext::sanitizeScriptError(String& errorMessage, int& lineNumber, int& columnNumber, String& sourceURL, CachedScript* cachedScript)
{
    URL targetURL = completeURL(sourceURL);
    if (securityOrigin()->canRequest(targetURL) || (cachedScript && cachedScript->passesAccessControlCheck(*securityOrigin())))
        return false;
    errorMessage = "Script error.";
    sourceURL = String();
    lineNumber = 0;
    columnNumber = 0;
    return true;
}

void ScriptExecutionContext::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, CachedScript* cachedScript)
{
    if (m_inDispatchErrorEvent) {
        if (!m_pendingExceptions)
            m_pendingExceptions = std::make_unique<Vector<std::unique_ptr<PendingException>>>();
        m_pendingExceptions->append(std::make_unique<PendingException>(errorMessage, lineNumber, columnNumber, sourceURL));
        return;
    }

    // First report the original exception and only then all the nested ones.
    if (!dispatchErrorEvent(errorMessage, lineNumber, columnNumber, sourceURL, cachedScript))
        logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber);

    if (!m_pendingExceptions)
        return;

    std::unique_ptr<Vector<std::unique_ptr<PendingException>>> pendingExceptions = WTFMove(m_pendingExceptions);
    for (auto& exception : *pendingExceptions)
        logExceptionToConsole(exception->m_errorMessage, exception->m_sourceURL, exception->m_lineNumber, exception->m_columnNumber);
}

void ScriptExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, unsigned long requestIdentifier)
{
    addMessage(source, level, message, sourceURL, lineNumber, columnNumber, requestIdentifier);
}

bool ScriptExecutionContext::dispatchErrorEvent(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, CachedScript* cachedScript)
{
    EventTarget* target = errorEventTarget();
    if (!target)
        return false;

    String message = errorMessage;
    int line = lineNumber;
    int column = columnNumber;
    String sourceName = sourceURL;
    sanitizeScriptError(message, line, column, sourceName, cachedScript);

    ASSERT(!m_inDispatchErrorEvent);
    m_inDispatchErrorEvent = true;
    Ref<ErrorEvent> errorEvent = ErrorEvent::create(message, sourceName, line, column);
    target->dispatchEvent(errorEvent);
    m_inDispatchErrorEvent = false;
    return errorEvent->defaultPrevented();
}

PublicURLManager& ScriptExecutionContext::publicURLManager()
{
    if (!m_publicURLManager)
        m_publicURLManager = PublicURLManager::create(this);
    return *m_publicURLManager;
}

bool ScriptExecutionContext::hasPendingActivity() const
{
    checkConsistency();

    for (auto* activeDOMObject : m_activeDOMObjects) {
        if (activeDOMObject->hasPendingActivity())
            return true;
    }

    return false;
}

} // namespace WebCore
