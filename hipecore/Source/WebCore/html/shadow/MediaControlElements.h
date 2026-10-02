/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2025-2026 General Development Systems
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlElements_h
#define MediaControlElements_h

#if ENABLE(VIDEO)
#include "MediaControlElementTypes.h"
#include "TextTrackRepresentation.h"

namespace WebCore {

class TimeRanges;

// ----------------------------

class MediaControlPanelElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlPanelElement> create(Document&);

    void setCanBeDragged(bool);
    void setIsDisplayed(bool);

    void resetPosition();
    void makeOpaque();
    void makeTransparent();

    virtual bool willRespondToMouseMoveEvents() override { return true; }
    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    explicit MediaControlPanelElement(Document&);

    virtual void defaultEventHandler(Event*) override;

    void startDrag(const LayoutPoint& eventLocation);
    void continueDrag(const LayoutPoint& eventLocation);
    void endDrag();

    void startTimer();
    void stopTimer();
    void transitionTimerFired();

    void setPosition(const LayoutPoint&);

    bool m_canBeDragged;
    bool m_isBeingDragged;
    bool m_isDisplayed;
    bool m_opaque;
    LayoutPoint m_lastDragEventLocation;
    LayoutPoint m_cumulativeDragOffset;

    Timer m_transitionTimer;
};

// ----------------------------

class MediaControlPanelEnclosureElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlPanelEnclosureElement> create(Document&);

private:
    explicit MediaControlPanelEnclosureElement(Document&);
};

// ----------------------------

class MediaControlOverlayEnclosureElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlOverlayEnclosureElement> create(Document&);

private:
    explicit MediaControlOverlayEnclosureElement(Document&);
};

// ----------------------------

class MediaControlTimelineContainerElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlTimelineContainerElement> create(Document&);

    void setTimeDisplaysHidden(bool);

private:
    explicit MediaControlTimelineContainerElement(Document&);

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
};

// ----------------------------

class MediaControlVolumeSliderContainerElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlVolumeSliderContainerElement> create(Document&);

    virtual bool willRespondToMouseMoveEvents() override { return true; }

    // hipecore: hipecore shows the volume slider permanently inline in the control
    // bar (see mediaControlsBase.css), not as a mouseover popup. The controls fire
    // hide()/show() on this container ~4x/second (makeTransparent on every timeupdate)
    // and again on every mute-button hover; each call toggles an inline display style,
    // which relaid out panelVolumeControlContainer - shifting the mute button mid-click
    // so clicks landed on the wrong element. Make the churn a no-op.
    virtual void hide() override { }
    virtual void show() override { }

private:
    explicit MediaControlVolumeSliderContainerElement(Document&);

    virtual void defaultEventHandler(Event*) override;

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
};

// ----------------------------

class MediaControlStatusDisplayElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlStatusDisplayElement> create(Document&);

    void update();

private:
    explicit MediaControlStatusDisplayElement(Document&);

    enum StateBeingDisplayed { Nothing, Loading, LiveBroadcast };
    StateBeingDisplayed m_stateBeingDisplayed;
};

// ----------------------------

class MediaControlPanelMuteButtonElement final : public MediaControlMuteButtonElement {
public:
    static Ref<MediaControlPanelMuteButtonElement> create(Document&, MediaControls*);

    virtual bool willRespondToMouseMoveEvents() override { return true; }

private:
    explicit MediaControlPanelMuteButtonElement(Document&, MediaControls*);

    virtual void defaultEventHandler(Event*) override;

    MediaControls* m_controls;
};

// ----------------------------

class MediaControlVolumeSliderMuteButtonElement final : public MediaControlMuteButtonElement {
public:
    static Ref<MediaControlVolumeSliderMuteButtonElement> create(Document&);

private:
    explicit MediaControlVolumeSliderMuteButtonElement(Document&);
};


// ----------------------------

class MediaControlPlayButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlPlayButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

    virtual void updateDisplayType() override;

private:
    explicit MediaControlPlayButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlOverlayPlayButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlOverlayPlayButtonElement> create(Document&);

    virtual void updateDisplayType() override;

private:
    explicit MediaControlOverlayPlayButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlSeekForwardButtonElement final : public MediaControlSeekButtonElement {
public:
    static Ref<MediaControlSeekForwardButtonElement> create(Document&);

private:
    explicit MediaControlSeekForwardButtonElement(Document&);

    virtual bool isForwardButton() const override { return true; }
};

// ----------------------------

class MediaControlSeekBackButtonElement final : public MediaControlSeekButtonElement {
public:
    static Ref<MediaControlSeekBackButtonElement> create(Document&);

private:
    explicit MediaControlSeekBackButtonElement(Document&);

    virtual bool isForwardButton() const override { return false; }
};

// ----------------------------

class MediaControlRewindButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlRewindButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    explicit MediaControlRewindButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlReturnToRealtimeButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlReturnToRealtimeButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    explicit MediaControlReturnToRealtimeButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlToggleClosedCaptionsButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlToggleClosedCaptionsButtonElement> create(Document&, MediaControls*);

    virtual bool willRespondToMouseClickEvents() override { return true; }

    virtual void updateDisplayType() override;

private:
    explicit MediaControlToggleClosedCaptionsButtonElement(Document&, MediaControls*);

    virtual void defaultEventHandler(Event*) override;

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
    MediaControls* m_controls;
#endif
};

// ----------------------------

class MediaControlClosedCaptionsContainerElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlClosedCaptionsContainerElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    MediaControlClosedCaptionsContainerElement(Document&);
};

// ----------------------------

class MediaControlClosedCaptionsTrackListElement final : public MediaControlDivElement {
public:
    static Ref<MediaControlClosedCaptionsTrackListElement> create(Document&, MediaControls*);

    virtual bool willRespondToMouseClickEvents() override { return true; }

    void updateDisplay();

private:
    MediaControlClosedCaptionsTrackListElement(Document&, MediaControls*);

    void rebuildTrackListMenu();

    virtual void defaultEventHandler(Event*) override;

    typedef Vector<RefPtr<Element>> TrackMenuItems;
    TrackMenuItems m_menuItems;
#if ENABLE(VIDEO_TRACK)
    typedef HashMap<RefPtr<Element>, RefPtr<TextTrack>> MenuItemToTrackMap;
    MenuItemToTrackMap m_menuToTrackMap;
#endif
    MediaControls* m_controls;
};

// ----------------------------

class MediaControlTimelineElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlTimelineElement> create(Document&, MediaControls*);

    virtual bool willRespondToMouseClickEvents() override;

    void setPosition(double);
    void setDuration(double);

    // hipecore: -webkit-appearance:none on this element (see mediaControlsBase.css) means
    // RenderThemeQt::paintMediaSliderTrack() -- which would otherwise paint the buffered
    // highlight from the native theme -- never runs. Paints it as an inline background-image
    // gradient instead, the same CSS-driven approach the rest of this element's look uses.
    void updateBufferedDisplay(PassRefPtr<TimeRanges>, double duration);

private:
    explicit MediaControlTimelineElement(Document&, MediaControls*);

    virtual void defaultEventHandler(Event*) override;

    MediaControls* m_controls;
};

// ----------------------------

class MediaControlFullscreenButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlFullscreenButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

    void setIsFullscreen(bool);

private:
    explicit MediaControlFullscreenButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlPanelVolumeSliderElement final : public MediaControlVolumeSliderElement {
public:
    static Ref<MediaControlPanelVolumeSliderElement> create(Document&);

private:
    explicit MediaControlPanelVolumeSliderElement(Document&);
};
// ----------------------------

class MediaControlFullscreenVolumeSliderElement final : public MediaControlVolumeSliderElement {
public:
    static Ref<MediaControlFullscreenVolumeSliderElement> create(Document&);

private:
    explicit MediaControlFullscreenVolumeSliderElement(Document&);
};

// ----------------------------

class MediaControlFullscreenVolumeMinButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlFullscreenVolumeMinButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    explicit MediaControlFullscreenVolumeMinButtonElement(Document&);
    virtual void defaultEventHandler(Event*) override;
};

// ----------------------------

class MediaControlFullscreenVolumeMaxButtonElement final : public MediaControlInputElement {
public:
    static Ref<MediaControlFullscreenVolumeMaxButtonElement> create(Document&);

    virtual bool willRespondToMouseClickEvents() override { return true; }

private:
    explicit MediaControlFullscreenVolumeMaxButtonElement(Document&);

    virtual void defaultEventHandler(Event*) override;
};


// ----------------------------

class MediaControlTimeRemainingDisplayElement final : public MediaControlTimeDisplayElement {
public:
    static Ref<MediaControlTimeRemainingDisplayElement> create(Document&);

private:
    explicit MediaControlTimeRemainingDisplayElement(Document&);
};

// ----------------------------

class MediaControlCurrentTimeDisplayElement final : public MediaControlTimeDisplayElement {
public:
    static Ref<MediaControlCurrentTimeDisplayElement> create(Document&);

private:
    explicit MediaControlCurrentTimeDisplayElement(Document&);
};

// ----------------------------

#if ENABLE(VIDEO_TRACK)

class MediaControlTextTrackContainerElement final : public MediaControlDivElement, public TextTrackRepresentationClient {
public:
    static Ref<MediaControlTextTrackContainerElement> create(Document&);

    void updateDisplay();
    void updateSizes(bool forceUpdate = false);
    void enteredFullscreen();
    void exitedFullscreen();

private:
    void updateTimerFired();
    void updateActiveCuesFontSize();

    explicit MediaControlTextTrackContainerElement(Document&);

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;

    virtual RefPtr<Image> createTextTrackRepresentationImage() override;
    virtual void textTrackRepresentationBoundsChanged(const IntRect&) override;
    void updateTextTrackRepresentation();
    void clearTextTrackRepresentation();
    void updateStyleForTextTrackRepresentation();
    std::unique_ptr<TextTrackRepresentation> m_textTrackRepresentation;

    Timer m_updateTimer;
    IntRect m_videoDisplaySize;
    int m_fontSize;
    bool m_fontSizeIsImportant;
    bool m_updateTextTrackRepresentationStyle;
};

#endif

} // namespace WebCore

#endif // ENABLE(VIDEO)

#endif // MediaControlElements_h
