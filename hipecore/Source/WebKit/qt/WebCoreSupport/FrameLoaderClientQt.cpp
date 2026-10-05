/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Coypright (C) 2008 Holger Hans Peter Freyther
 * Coypright (C) 2009, 2010 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2025-2026 General Development Systems
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoaderClientQt.h"

#include "CSSPropertyNames.h"
#include "DocumentLoader.h"
#include "EventHandler.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameNetworkingContext.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAppletElement.h"
#include "HTMLFormElement.h"
#include "HTMLPlugInElement.h"
#include "HTTPParsers.h"
#include "HTTPStatusCodes.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "MIMETypeRegistry.h"
#include "MainFrame.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PolicyChecker.h"
#include "QWebFrameAdapter.h"
#include "QWebFrameData.h"
#include "QWebPageAdapter.h"
#include "QWebPageClient.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "SubresourceLoader.h"
#include "UserGestureIndicator.h"
#include "qwebsettings.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QMouseEvent>
#include <QStringList>
#include <wtf/NeverDestroyed.h>

static QMap<unsigned long, QString> dumpAssignedUrls;

// Compare with the file "WebKit/Tools/DumpRenderTree/mac/FrameLoadDelegate.mm".
static QString drtDescriptionSuitableForTestResult(WebCore::Frame* webCoreFrame)
{
    QWebFrameAdapter* frame = QWebFrameAdapter::kit(webCoreFrame);
    QString name = webCoreFrame->tree().uniqueName();

    bool isMainFrame = frame == &frame->pageAdapter->mainFrameAdapter();
    if (isMainFrame) {
        if (!name.isEmpty())
            return QString::fromLatin1("main frame \"%1\"").arg(name);
        return QLatin1String("main frame");
    }
    if (!name.isEmpty())
        return QString::fromLatin1("frame \"%1\"").arg(name);
    return QLatin1String("frame (anonymous)");
}

static QString drtPrintFrameUserGestureStatus(WebCore::Frame* frame)
{
    if (WebCore::UserGestureIndicator::processingUserGesture())
        return QString::fromLatin1("Frame with user gesture \"%1\"").arg(QLatin1String("true"));
    return QString::fromLatin1("Frame with user gesture \"%1\"").arg(QLatin1String("false"));
}

static QString drtDescriptionSuitableForTestResult(const WebCore::URL& kurl)
{
    if (kurl.isEmpty() || !kurl.isLocalFile())
        return kurl.string();
    // Remove the leading path from file urls.
    return QString(kurl.string()).remove(WebCore::FrameLoaderClientQt::dumpResourceLoadCallbacksPath).mid(1);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceError& error)
{
    QString failingURL = error.failingURL().string();
    return QString::fromLatin1("<NSError domain NSURLErrorDomain, code %1, failing URL \"%2\">").arg(error.errorCode()).arg(failingURL);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceRequest& request)
{
    QString url = drtDescriptionSuitableForTestResult(request.url());
    QString httpMethod = request.httpMethod();
    QString mainDocumentUrl = "about:blank";
    return QString::fromLatin1("<NSURLRequest URL %1, main document URL %2, http method %3>").arg(url).arg(mainDocumentUrl).arg(httpMethod);
}

static QString drtDescriptionSuitableForTestResult(const WebCore::ResourceResponse& response)
{
    QString url = drtDescriptionSuitableForTestResult(response.url());
    int httpStatusCode = response.httpStatusCode();
    return QString::fromLatin1("<NSURLResponse %1, http status code %2>").arg(url).arg(httpStatusCode);
}

static QString drtDescriptionSuitableForTestResult(const RefPtr<WebCore::Node> node, int exception)
{
    QString result;
    if (exception) {
        result.append(QLatin1String("ERROR"));
        return result;
    }
    if (!node) {
        result.append(QLatin1String("NULL"));
        return result;
    }
    result.append(node->nodeName());
    RefPtr<WebCore::Node> parent = node->parentNode();
    if (parent) {
        result.append(QLatin1String(" > "));
        result.append(drtDescriptionSuitableForTestResult(parent, 0));
    }
    return result;
}

namespace WebCore {

bool FrameLoaderClientQt::dumpFrameLoaderCallbacks = false;
bool FrameLoaderClientQt::dumpUserGestureInFrameLoaderCallbacks = false;
bool FrameLoaderClientQt::dumpWillCacheResponseCallbacks = false;
bool FrameLoaderClientQt::dumpResourceLoadCallbacks = false;
bool FrameLoaderClientQt::sendRequestReturnsNullOnRedirect = false;
bool FrameLoaderClientQt::sendRequestReturnsNull = false;
bool FrameLoaderClientQt::dumpResourceResponseMIMETypes = false;
bool FrameLoaderClientQt::deferMainResourceDataLoad = true;
bool FrameLoaderClientQt::dumpHistoryCallbacks = false;

QStringList FrameLoaderClientQt::sendRequestClearHeaders;
QString FrameLoaderClientQt::dumpResourceLoadCallbacksPath;
QMap<QString, QString> FrameLoaderClientQt::URLsToRedirect = QMap<QString, QString>();

FrameLoaderClientQt::FrameLoaderClientQt()
    : m_frame(0)
    , m_webFrame(0)
    , m_isOriginatingLoad(false)
{
}


FrameLoaderClientQt::~FrameLoaderClientQt()
{
}

void FrameLoaderClientQt::setFrame(QWebFrameAdapter* webFrame, Frame* frame)
{
    m_webFrame = webFrame;
    m_frame = frame;

    if (!m_webFrame || !m_webFrame->pageAdapter) {
        qWarning("FrameLoaderClientQt::setFrame frame without Page!");
        return;
    }

    connect(this, SIGNAL(titleChanged(QString)),
        m_webFrame->handle(), SIGNAL(titleChanged(QString)));
}

void FrameLoaderClientQt::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    function(action);
}

bool FrameLoaderClientQt::hasWebView() const
{
    // notImplemented();
    return true;
}

void FrameLoaderClientQt::transitionToCommittedForNewPage()
{
    ASSERT(m_frame);
    ASSERT(m_webFrame);

    QObject* qWebPage = m_webFrame->pageAdapter->handle();
    QBrush brush = qWebPage->property("palette").value<QPalette>().brush(QPalette::Base);
    QColor backgroundColor = brush.style() == Qt::SolidPattern ? brush.color() : QColor();

    const QSize preferredLayoutSize = qWebPage->property("preferredContentsSize").toSize();

    ScrollbarMode hScrollbar = (ScrollbarMode) m_webFrame->scrollBarPolicy(Qt::Horizontal);
    ScrollbarMode vScrollbar = (ScrollbarMode) m_webFrame->scrollBarPolicy(Qt::Vertical);
    bool hLock = hScrollbar != ScrollbarAuto;
    bool vLock = vScrollbar != ScrollbarAuto;

    // The HistoryController will update the scroll position later if needed.
    IntRect currentVisibleContentRect = m_frame->view() ? IntRect(IntPoint::zero(), m_frame->view()->fixedVisibleContentRect().size()) : IntRect();

    m_frame->createView(qWebPage->property("viewportSize").toSize(),
        backgroundColor, !backgroundColor.alpha(),
        preferredLayoutSize.isValid() ? IntSize(preferredLayoutSize) : IntSize(),
        currentVisibleContentRect,
        preferredLayoutSize.isValid(),
        hScrollbar, hLock,
        vScrollbar, vLock);

    bool isMainFrame = m_frame == &m_frame->page()->mainFrame();
    if (isMainFrame &&m_webFrame->pageAdapter->client) {
        bool resizesToContents = m_webFrame->pageAdapter->client->viewResizesToContentsEnabled();

        m_frame->view()->setPaintsEntireContents(resizesToContents);
        m_frame->view()->setDelegatesScrolling(resizesToContents);
    }
}

void FrameLoaderClientQt::dispatchDidBecomeFrameset(bool)
{
}

void FrameLoaderClientQt::forceLayoutForNonHTML()
{
}


void FrameLoaderClientQt::setCopiesOnScroll()
{
    // Apparently this is mac specific.
}


void FrameLoaderClientQt::detachedFromParent2()
{
}


void FrameLoaderClientQt::detachedFromParent3()
{
}

void FrameLoaderClientQt::dispatchDidDispatchOnloadEvents()
{
    // Don't need this one.
    if (dumpFrameLoaderCallbacks)
        printf("%s - didHandleOnloadEventsForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));
}


void FrameLoaderClientQt::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didReceiveServerRedirectForProvisionalLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    notImplemented();
}


void FrameLoaderClientQt::dispatchDidCancelClientRedirect()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didCancelClientRedirectForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    notImplemented();
}


void FrameLoaderClientQt::dispatchWillPerformClientRedirect(const URL& url, double, double)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - willPerformClientRedirectToURL: %s \n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), qPrintable(drtDescriptionSuitableForTestResult(url)));

    if (dumpUserGestureInFrameLoaderCallbacks)
        printf("%s - in willPerformClientRedirect\n", qPrintable(drtPrintFrameUserGestureStatus(m_frame)));

    notImplemented();
}

void FrameLoaderClientQt::dispatchDidNavigateWithinPage()
{
    if (!m_webFrame)
        return;

    FrameLoader& loader = m_frame->loader();
    bool loaderCompleted = !(loader.activeDocumentLoader() && loader.activeDocumentLoader()->isLoadingInAPISense());

    if (!loaderCompleted)
        return;

    dispatchDidCommitLoad();
    dispatchDidFinishLoad();
}

void FrameLoaderClientQt::dispatchDidChangeLocationWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didChangeLocationWithinPageForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (!m_webFrame)
        return;

    m_webFrame->emitUrlChanged();
}


void FrameLoaderClientQt::dispatchDidPushStateWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - dispatchDidPushStateWithinPage\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientQt::dispatchDidReplaceStateWithinPage()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - dispatchDidReplaceStateWithinPage\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientQt::dispatchDidPopStateWithinPage()
{
    // No need to call dispatchDidNavigateWithinPage here, it's already been done in loadInSameDocument().
}

void FrameLoaderClientQt::dispatchWillClose()
{
}


void FrameLoaderClientQt::dispatchDidStartProvisionalLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didStartProvisionalLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (dumpUserGestureInFrameLoaderCallbacks)
        printf("%s - in didStartProvisionalLoadForFrame\n", qPrintable(drtPrintFrameUserGestureStatus(m_frame)));

    m_lastRequestedUrl = m_frame->loader().activeDocumentLoader()->url();

    if (!m_webFrame)
        return;
    emitLoadStarted();

    m_webFrame->didStartProvisionalLoad();
}


void FrameLoaderClientQt::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    // FIXME: Use direction of title.
    if (dumpFrameLoaderCallbacks)
        printf("%s - didReceiveTitle: %s\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), qPrintable(QString(title.string())));

    if (!m_webFrame)
        return;

    emit titleChanged(title.string());
}


void FrameLoaderClientQt::dispatchDidCommitLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didCommitLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_frame->tree().parent() || !m_webFrame)
        return;

    m_webFrame->emitUrlChanged();

    // We should assume first the frame has no title. If it has, then the above dispatchDidReceiveTitle()
    // will be called very soon with the correct title.
    // This properly resets the title when we navigate to a URI without a title.
    emit titleChanged(QString());
}


void FrameLoaderClientQt::dispatchDidFinishDocumentLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFinishDocumentLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (QWebPageAdapter::drtRun) {
        int unloadEventCount = m_frame->document()->domWindow()->pendingUnloadEventListeners();
        if (unloadEventCount)
            printf("%s - has %u onunload handler(s)\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)), unloadEventCount);
    }

    if (m_frame->tree().parent() || !m_webFrame)
        return;

}


void FrameLoaderClientQt::dispatchDidFinishLoad()
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFinishLoadForFrame\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (!m_webFrame)
        return;

    emitLoadFinished(true);
}

void FrameLoaderClientQt::dispatchDidLayout(LayoutMilestones milestones)
{
    if (!m_webFrame)
        return;

    if (milestones & DidFirstVisuallyNonEmptyLayout)
        m_webFrame->emitInitialLayoutCompleted();
}

void FrameLoaderClientQt::dispatchShow()
{
    notImplemented();
}


void FrameLoaderClientQt::cancelPolicyCheck()
{
    // qDebug() << "FrameLoaderClientQt::cancelPolicyCheck";
}


void FrameLoaderClientQt::dispatchWillSubmitForm(PassRefPtr<FormState>, FramePolicyFunction function)
{
    notImplemented();
    // FIXME: This is surely too simple.
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientQt::setMainFrameDocumentReady(bool)
{
    // This is only interesting once we provide an external API for the DOM.
}


void FrameLoaderClientQt::willChangeTitle(DocumentLoader*)
{
    // No need for, dispatchDidReceiveTitle is the right callback.
}


void FrameLoaderClientQt::didChangeTitle(DocumentLoader*)
{
    // No need for, dispatchDidReceiveTitle is the right callback.
}


void FrameLoaderClientQt::finishedLoading(DocumentLoader*)
{
}

bool FrameLoaderClientQt::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    notImplemented();
    return false;
}
    
bool FrameLoaderClientQt::canShowMIMEType(const String& MIMEType) const
{
    String type = MIMEType;
    type.convertToASCIILowercase(); // FIXME: Do we really need it?
    if (MIMETypeRegistry::canShowMIMEType(type))
        return true;

    return false;
}

bool FrameLoaderClientQt::representationExistsForURLScheme(const String&) const
{
    return false;
}


String FrameLoaderClientQt::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}


void FrameLoaderClientQt::frameLoadCompleted()
{
    //no-op
}


void FrameLoaderClientQt::restoreViewState()
{
    if (!m_webFrame)
        return;
    m_webFrame->pageAdapter->emitRestoreFrameStateRequested(m_webFrame);
}


void FrameLoaderClientQt::provisionalLoadStarted()
{
    // Don't need to do anything here.
}


void FrameLoaderClientQt::didFinishLoad()
{
    // notImplemented();
}


void FrameLoaderClientQt::prepareForDataSourceReplacement()
{
}

void FrameLoaderClientQt::setTitle(const StringWithDirection& title, const URL& url)
{
    // Used by Apple WebKit to update the title of an existing history item.
    // QtWebKit doesn't accomodate this on history items. If it ever does,
    // it should be privateBrowsing-aware. For now, we are just passing
    // globalhistory layout tests.
    // FIXME: Use direction of title.
    if (dumpHistoryCallbacks) {
        printf("WebView updated the title for history URL \"%s\" to \"%s\".\n",
            qPrintable(drtDescriptionSuitableForTestResult(url)),
            qPrintable(QString(title.string())));
    }
}


void FrameLoaderClientQt::frameLoaderDestroyed()
{
    // Delete QWebFrame (handle()), which owns QWebFramePrivate, which
    // _is_ a QWebFrameAdapter.
    if (m_webFrame)
        delete m_webFrame->handle();
    m_frame = 0;
    m_webFrame = 0;

    delete this;
}

bool FrameLoaderClientQt::canHandleRequest(const WebCore::ResourceRequest&) const
{
    return true;
}

void FrameLoaderClientQt::registerForIconNotification(bool shouldRegister)
{
    //no-op
}

void FrameLoaderClientQt::onIconLoadedForPageURL(const QString& url)
{
    //no-op
}


void FrameLoaderClientQt::updateGlobalHistory()
{
    //no-op
}

void FrameLoaderClientQt::updateGlobalHistoryRedirectLinks()
{
    //no-op
}

bool FrameLoaderClientQt::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    return true;
}

void FrameLoaderClientQt::didDetectXSS(const URL&, bool)
{
    if (dumpFrameLoaderCallbacks)
        printf("didDetectXSS\n");

    notImplemented();
}

bool FrameLoaderClientQt::canCachePage() const
{
    return true;
}

void FrameLoaderClientQt::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
}

// FIXME: This function should be moved into WebCore.
void FrameLoaderClientQt::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    loader->commitData(data, length);

    // If we are sending data to MediaDocument, we should stop here and cancel the request.
    if (m_frame->document()->isMediaDocument())
        loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));
}

// Error codes historically supplied by QNetworkReply::NetworkError - kept as local constants
// since HipeCore no longer links Qt5Network, to preserve the existing "QtNetwork"-domain
// ResourceErrors' codes unchanged.
enum {
    QtNetworkOperationCanceledError =                             5,
    QtNetworkContentNotFoundError =                             203
};

WebCore::ResourceError FrameLoaderClientQt::cancelledError(const WebCore::ResourceRequest& request)
{
    ResourceError error = ResourceError("QtNetwork", QtNetworkOperationCanceledError, request.url(),
        QCoreApplication::translate("QWebFrame", "Request cancelled", 0));
    error.setIsCancellation(true);
    return error;
}

// This was copied from file "WebKit/Source/WebKit/mac/Misc/WebKitErrors.h".
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
    WebKitErrorPluginWillHandleLoad =                           203
};

WebCore::ResourceError FrameLoaderClientQt::blockedError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotUseRestrictedPort, request.url(),
        QCoreApplication::translate("QWebFrame", "Request blocked", 0));
}


WebCore::ResourceError FrameLoaderClientQt::cannotShowURLError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotShowURL, request.url(),
        QCoreApplication::translate("QWebFrame", "Cannot show URL", 0));
}

WebCore::ResourceError FrameLoaderClientQt::interruptedForPolicyChangeError(const WebCore::ResourceRequest& request)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorFrameLoadInterruptedByPolicyChange, request.url(),
        QCoreApplication::translate("QWebFrame", "Frame load interrupted by policy change", 0));
}

WebCore::ResourceError FrameLoaderClientQt::cannotShowMIMETypeError(const WebCore::ResourceResponse& response)
{
    return ResourceError("WebKitErrorDomain", WebKitErrorCannotShowMIMEType, response.url(),
        QCoreApplication::translate("QWebFrame", "Cannot show mimetype", 0));
}

WebCore::ResourceError FrameLoaderClientQt::fileDoesNotExistError(const WebCore::ResourceResponse& response)
{
    return ResourceError("QtNetwork", QtNetworkContentNotFoundError, response.url(),
        QCoreApplication::translate("QWebFrame", "File does not exist", 0));
}

WebCore::ResourceError FrameLoaderClientQt::pluginWillHandleLoadError(const WebCore::ResourceResponse& response)
{
    return ResourceError("WebKit", WebKitErrorPluginWillHandleLoad, response.url(),
        QCoreApplication::translate("QWebFrame", "Loading is handled by the media engine", 0));
}

bool FrameLoaderClientQt::shouldFallBack(const WebCore::ResourceError& error)
{
    using Error = NeverDestroyed<const ResourceError>;
    static Error cancelledError(this->cancelledError(ResourceRequest()));
    static Error pluginWillHandleLoadError(this->pluginWillHandleLoadError(ResourceResponse()));
    static Error errorInterruptedForPolicyChange(this->interruptedForPolicyChangeError(ResourceRequest()));

    if (error.errorCode() == cancelledError.get().errorCode() && error.domain() == cancelledError.get().domain())
        return false;

    if (error.errorCode() == errorInterruptedForPolicyChange.get().errorCode() && error.domain() == errorInterruptedForPolicyChange.get().domain())
        return false;

    if (error.errorCode() == pluginWillHandleLoadError.get().errorCode() && error.domain() == pluginWillHandleLoadError.get().domain())
        return false;

    return true;
}

Ref<WebCore::DocumentLoader> FrameLoaderClientQt::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    Ref<DocumentLoader> loader = DocumentLoader::create(request, substituteData);
    if (!deferMainResourceDataLoad || substituteData.isValid())
        loader->setDeferMainResourceDataLoad(false);
    return loader;
}

void FrameLoaderClientQt::convertMainResourceLoadToDownload(DocumentLoader*, SessionID, const ResourceRequest& request, const ResourceResponse&)
{
    if (!m_webFrame)
        return;

    // HipeCore has no network backend, so a main resource load is never
    // backed by a real QNetworkReply that could be handed off as a download.
    qWarning("Attempted to download unsupported URL %s", request.url().string().characters8());
}

void FrameLoaderClientQt::assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest& request)
{
    if (dumpResourceLoadCallbacks)
        dumpAssignedUrls[identifier] = drtDescriptionSuitableForTestResult(request.url());
}

static void blockRequest(WebCore::ResourceRequest& request)
{
    request.setURL(QUrl());
}

static bool isLocalhost(const QString& host)
{
    return host == QLatin1String("127.0.0.1") || host == QLatin1String("localhost");
}

static bool hostIsUsedBySomeTestsToGenerateError(const QString& host)
{
    return host == QLatin1String("255.255.255.255");
}

void FrameLoaderClientQt::dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest& newRequest, const WebCore::ResourceResponse& redirectResponse)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - willSendRequest %s redirectResponse %s\n",
            qPrintable(dumpAssignedUrls[identifier]),
            qPrintable(drtDescriptionSuitableForTestResult(newRequest)),
            (redirectResponse.isNull()) ? "(null)" : qPrintable(drtDescriptionSuitableForTestResult(redirectResponse)));

    if (sendRequestReturnsNull) {
        blockRequest(newRequest);
        return;
    }

    if (sendRequestReturnsNullOnRedirect && !redirectResponse.isNull()) {
        printf("Returning null for this redirect\n");
        blockRequest(newRequest);
        return;
    }

    QUrl url = newRequest.url();
    QString host = url.host();
    QString urlScheme = url.scheme().toLower();

    if (QWebPageAdapter::drtRun
        && !host.isEmpty()
        && (urlScheme == QLatin1String("http") || urlScheme == QLatin1String("https"))) {

        QUrl testURL = m_webFrame->pageAdapter->mainFrameAdapter().frameLoaderClient->lastRequestedUrl();
        QString testHost = testURL.host();
        QString testURLScheme = testURL.scheme().toLower();

        if (!isLocalhost(host)
            && !hostIsUsedBySomeTestsToGenerateError(host)
            && ((testURLScheme != QLatin1String("http") && testURLScheme != QLatin1String("https")) || isLocalhost(testHost))) {
            printf("Blocked access to external URL %s\n", qPrintable(drtDescriptionSuitableForTestResult(newRequest.url())));
            blockRequest(newRequest);
            return;
        }
    }

    for (int i = 0; i < sendRequestClearHeaders.size(); ++i)
        newRequest.setHTTPHeaderField(sendRequestClearHeaders.at(i).toLocal8Bit().constData(), QString());

    if (QWebPageAdapter::drtRun) {
        QMap<QString, QString>::const_iterator it = URLsToRedirect.constFind(url.toString());
        if (it != URLsToRedirect.constEnd())
            newRequest.setURL(QUrl(it.value()));
    }
    // Seems like the Mac code doesn't do anything here by default neither.
    // qDebug() << "FrameLoaderClientQt::dispatchWillSendRequest" << request.isNull() << url;
}

bool FrameLoaderClientQt::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientQt::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse& response)
{

    m_response = response;
    if (dumpWillCacheResponseCallbacks)
        printf("%s - willCacheResponse: called\n",
            qPrintable(dumpAssignedUrls[identifier]));

    if (dumpResourceLoadCallbacks)
        printf("%s - didReceiveResponse %s\n",
            qPrintable(dumpAssignedUrls[identifier]),
            qPrintable(drtDescriptionSuitableForTestResult(response)));

    if (dumpResourceResponseMIMETypes) {
        printf("%s has MIME type %s\n",
            qPrintable(QString(response.url().lastPathComponent())),
            qPrintable(QString(response.mimeType())));
    }
}

void FrameLoaderClientQt::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int)
{
}

void FrameLoaderClientQt::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - didFinishLoading\n",
            (dumpAssignedUrls.contains(identifier) ? qPrintable(dumpAssignedUrls[identifier]) : "<unknown>"));
}

void FrameLoaderClientQt::dispatchDidFailLoading(WebCore::DocumentLoader* loader, unsigned long identifier, const WebCore::ResourceError& error)
{
    if (dumpResourceLoadCallbacks)
        printf("%s - didFailLoadingWithError: %s\n",
            (dumpAssignedUrls.contains(identifier) ? qPrintable(dumpAssignedUrls[identifier]) : "<unknown>"),
            qPrintable(drtDescriptionSuitableForTestResult(error)));
}

bool FrameLoaderClientQt::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientQt::dispatchDidFailProvisionalLoad(const WebCore::ResourceError&)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFailProvisionalLoadWithError\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_webFrame)
        emitLoadFinished(false);
}

void FrameLoaderClientQt::dispatchDidFailLoad(const WebCore::ResourceError&)
{
    if (dumpFrameLoaderCallbacks)
        printf("%s - didFailLoadWithError\n", qPrintable(drtDescriptionSuitableForTestResult(m_frame)));

    if (m_webFrame)
        emitLoadFinished(false);
}

WebCore::Frame* FrameLoaderClientQt::dispatchCreatePage(const WebCore::NavigationAction&)
{
    if (!m_webFrame)
        return 0;
    QWebPageAdapter* newPage = m_webFrame->pageAdapter->createWindow(/* modalDialog = */ false);
    if (!newPage)
        return 0;
    return newPage->mainFrameAdapter().frame;
}

void FrameLoaderClientQt::dispatchDecidePolicyForResponse(const WebCore::ResourceResponse& response, const WebCore::ResourceRequest&, FramePolicyFunction function)
{
    // We need to call directly here.
    switch (response.httpStatusCode()) {
    case HTTPResetContent:
        // FIXME: a 205 response requires that the requester reset the document view.
        // Fallthrough
    case HTTPNoContent:
        callPolicyFunction(function, PolicyIgnore);
        return;
    }

    if (WebCore::contentDispositionType(response.httpHeaderField(HTTPHeaderName::ContentDisposition)) == WebCore::ContentDispositionAttachment)
        callPolicyFunction(function, PolicyDownload);
    else if (canShowMIMEType(response.mimeType()))
        callPolicyFunction(function, PolicyUse);
    else
        callPolicyFunction(function, PolicyDownload);
}

// Navigation lock. A client's document belongs to the client: it changes only through instructions, never
// on the document's own initiative. So a frame navigates only when the embedder asks it to through the Qt
// API (QWebFrame::load(), setHtml(), setContent(): QWebFrameAdapter sets m_navigationAllowed around those),
// or for a child frame's first load (createFrame() below). Links, form submissions, <meta> refresh, URLs
// dropped on a page, later changes to an iframe's src and any other navigation a document starts are
// refused, as are all requests for a new window.
void FrameLoaderClientQt::dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const WTF::String&, FramePolicyFunction function)
{
    callPolicyFunction(function, PolicyIgnore);
}

void FrameLoaderClientQt::dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState>, FramePolicyFunction function)
{
    Q_ASSERT(m_webFrame);

    if (!m_navigationAllowed) {
        if (action.type() == NavigationType::LinkClicked && request.url().hasFragmentIdentifier()) {
            ResourceRequest emptyRequest;
            m_frame->loader().activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
        }
        callPolicyFunction(function, PolicyIgnore);
        return;
    }
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientQt::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
{
    notImplemented();
}

RefPtr<Frame> FrameLoaderClientQt::createFrame(const URL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    if (!m_webFrame)
        return 0;

    QWebFrameData frameData(m_frame->page(), m_frame, ownerElement, name);

    frameData.referrer = referrer;
    frameData.allowsScrolling = allowsScrolling;
    frameData.marginWidth = marginWidth;
    frameData.marginHeight = marginHeight;

    QWebFrameAdapter* childWebFrame = m_webFrame->createChildFrame(&frameData);
    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!childWebFrame->frame->page()) {
        QPointer<QObject> qWebFrame = childWebFrame->handle();
        frameData.frame.release();
        ASSERT_UNUSED(qWebFrame, !qWebFrame);
        return 0;
    }

    m_webFrame->pageAdapter->emitFrameCreated(childWebFrame);

    // FIXME: Set override encoding if we have one.

    URL urlToLoad = url;
    if (urlToLoad.isEmpty())
        urlToLoad = blankURL();

    // The child frame's first load is allowed through the navigation lock.
    FrameLoaderClientQt& childClient = static_cast<FrameLoaderClientQt&>(frameData.frame->loader().client());
    childClient.m_navigationAllowed = true;
    m_frame->loader().loadURLIntoChildFrame(urlToLoad, frameData.referrer, frameData.frame.get());
    childClient.m_navigationAllowed = false;

    // The frame's onload handler may have removed it from the document.
    if (!frameData.frame->tree().parent())
        return 0;

    return frameData.frame;
}

ObjectContentType FrameLoaderClientQt::objectContentType(const URL& url, const String& mimeTypeIn)
{
    // qDebug()<<" ++++++++++++++++ url is "<<url.string()<<", mime = "<<mimeTypeIn;
    QFileInfo fi(url.path());
    String extension = fi.suffix();
    if (url.isEmpty() && !mimeTypeIn.length())
        return ObjectContentNone;

    String mimeType = mimeTypeIn;
    if (!mimeType.length())
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(extension);

    if (!mimeType.length())
        return ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentImage;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;

    if (url.protocol() == "about")
        return ObjectContentFrame;

    return ObjectContentNone;
}

// hipecore has no plugins: <object>, <embed> and <applet> show an image, a frame, their fallback content or the
// missing-plug-in placeholder. (Native surfaces inside an element are made by QWebElement::x11EmbedTargetXid().)
RefPtr<Widget> FrameLoaderClientQt::createPlugin(const IntSize&, HTMLPlugInElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    return nullptr;
}

void FrameLoaderClientQt::redirectDataToPlugin(Widget*)
{
    // No plugin is ever created (see createPlugin()), so there is never one to redirect load data to.
}

PassRefPtr<Widget> FrameLoaderClientQt::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const URL&, const Vector<String>&, const Vector<String>&)
{
    return nullptr;
}

String FrameLoaderClientQt::overrideMediaType() const
{
    if (m_webFrame && m_webFrame->pageAdapter && m_webFrame->pageAdapter->settings)
        return m_webFrame->pageAdapter->settings->cssMediaType();

    return String();
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientQt::createNetworkingContext()
{
    // HipeCore has no network backend, so there is no FrameNetworkingContext
    // implementation left to hand out (FrameNetworkingContextQt is gone along
    // with the QNetworkAccessManager-backed loader it existed to serve).
    // FrameLoader::networkingContext() tolerates a null result; every caller
    // in this tree already null-checks it before use.
    return nullptr;
}

QWebFrameAdapter* FrameLoaderClientQt::webFrame() const
{
    return m_webFrame;
}

void FrameLoaderClientQt::emitLoadStarted()
{
    m_webFrame->emitLoadStarted(m_isOriginatingLoad);
}

void FrameLoaderClientQt::emitLoadFinished(bool ok)
{
    // Signal handlers can lead to a new load, that will use the member again.
    const bool wasOriginatingLoad = m_isOriginatingLoad;
    m_isOriginatingLoad = false;

    m_webFrame->emitLoadFinished(wasOriginatingLoad, ok);
}

void FrameLoaderClientQt::willReplaceMultipartContent()
{
    notImplemented();
}

void FrameLoaderClientQt::didReplaceMultipartContent()
{
    notImplemented();
}

WebCore::ResourceError FrameLoaderClientQt::blockedByContentBlockerError(const WebCore::ResourceRequest &)
{
    notImplemented();
    return WebCore::ResourceError();
}

void FrameLoaderClientQt::updateCachedDocumentLoader(WebCore::DocumentLoader &)
{
    notImplemented();
}


}

#include "moc_FrameLoaderClientQt.cpp"
