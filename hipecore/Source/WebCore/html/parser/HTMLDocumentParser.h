/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
 */

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "HTMLInputStream.h"
#include "HTMLTokenizer.h"
#include "ScriptableDocumentParser.h"

namespace WebCore {

class DocumentFragment;
class Element;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLPreloadScanner;
class HTMLTreeBuilder;
class HTMLResourcePreloader;
class PumpSession;

// hipecore: this parser used to yield/pause for <script> execution via HTMLScriptRunner.
// hipecore has no script engine and scripting is disabled, so scripts never block the
// parser: isWaitingForScripts() is always false and the HTMLScriptRunner / HTMLScriptRunnerHost
// / CachedResourceClient machinery is gone (Bucket 6). <script> content is still tokenized
// as raw text and inserted as an inert element.

class HTMLDocumentParser : public ScriptableDocumentParser {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<HTMLDocumentParser> create(HTMLDocument&);
    virtual ~HTMLDocumentParser();

    static void parseDocumentFragment(const String&, DocumentFragment&, Element& contextElement, ParserContentPolicy = AllowScriptingContent);

    // For HTMLParserScheduler.
    void resumeParsingAfterYield();

    // For HTMLTreeBuilder.
    HTMLTokenizer& tokenizer();
    virtual TextPosition textPosition() const override final;

protected:
    explicit HTMLDocumentParser(HTMLDocument&);

    virtual void insert(const SegmentedString&) override final;
    virtual void append(RefPtr<StringImpl>&&) override;
    virtual void finish() override;

    HTMLTreeBuilder& treeBuilder();

private:
    HTMLDocumentParser(DocumentFragment&, Element& contextElement, ParserContentPolicy);
    static Ref<HTMLDocumentParser> create(DocumentFragment&, Element& contextElement, ParserContentPolicy);

    // DocumentParser
    virtual void detach() override final;
    virtual bool hasInsertionPoint() override final;
    virtual bool processingData() const override final;
    virtual void prepareToStopParsing() override final;
    virtual void stopParsing() override final;
    virtual bool isWaitingForScripts() const override { return false; }
    virtual void suspendScheduledTasks() override final;
    virtual void resumeScheduledTasks() override final;

    virtual bool shouldAssociateConsoleMessagesWithTextPosition() const override final;

    Document* contextForParsingSession();

    enum SynchronousMode { AllowYield, ForceSynchronous };
    bool canTakeNextToken(SynchronousMode, PumpSession&);
    void pumpTokenizer(SynchronousMode);
    void pumpTokenizerIfPossible(SynchronousMode);
    void constructTreeFromHTMLToken(HTMLTokenizer::TokenPtr&);

    void attemptToEnd();
    void endIfDelayed();
    void end();

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const;
    bool shouldDelayEnd() const;

    HTMLParserOptions m_options;
    HTMLInputStream m_input;

    HTMLTokenizer m_tokenizer;
    std::unique_ptr<HTMLTreeBuilder> m_treeBuilder;
    std::unique_ptr<HTMLPreloadScanner> m_preloadScanner;
    std::unique_ptr<HTMLParserScheduler> m_parserScheduler;
    TextPosition m_textPosition;

    std::unique_ptr<HTMLResourcePreloader> m_preloader;

    bool m_endWasDelayed { false };
    unsigned m_pumpSessionNestingLevel { 0 };
};

inline HTMLTokenizer& HTMLDocumentParser::tokenizer()
{
    return m_tokenizer;
}

inline HTMLTreeBuilder& HTMLDocumentParser::treeBuilder()
{
    ASSERT(m_treeBuilder);
    return *m_treeBuilder;
}

}

#endif
