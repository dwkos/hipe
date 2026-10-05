cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)

set(WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/encryptedmedia"
    "${WEBCORE_DIR}/Modules/mediacontrols/"
    "${WEBCORE_DIR}/Modules/mediasession"
    "${WEBCORE_DIR}/Modules/mediasource"
    "${WEBCORE_DIR}/Modules/plugins"
    "${WEBCORE_DIR}/Modules/speech"
    "${WEBCORE_DIR}/Modules/webaudio"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bindings/generic"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/contentextensions"
    "${WEBCORE_DIR}/crypto"
    "${WEBCORE_DIR}/crypto/algorithms"
    "${WEBCORE_DIR}/crypto/keys"
    "${WEBCORE_DIR}/crypto/parameters"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/cssjit"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/default"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/canvas"
    "${WEBCORE_DIR}/html/forms"
    "${WEBCORE_DIR}/html/parser"
    "${WEBCORE_DIR}/html/shadow"
    "${WEBCORE_DIR}/html/track"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/mathml"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/page/csp"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/crypto"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm/filters"
    "${WEBCORE_DIR}/platform/graphics/displaylists"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/platform/text/icu"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/mathml"
    "${WEBCORE_DIR}/rendering/shapes"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/rendering/svg"
    "${WEBCORE_DIR}/replay"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/style"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/animation"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBCORE_DIR}/svg/graphics/filters"
    "${WEBCORE_DIR}/svg/properties"
    "${WEBCORE_DIR}/xml"
    "${WEBCORE_DIR}/xml/parser"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
    "${CMAKE_BINARY_DIR}"
)

set(WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

set(WebCore_IDL_INCLUDES
    css
    dom
    fileapi
    html
    page
    plugins
    storage
    svg
    xml

    Modules/encryptedmedia
    Modules/mediasession
    Modules/speech
    Modules/webaudio

    bindings/generic
    bindings/js

    html/canvas
    html/shadow
    html/track
)

# hipecore: JS bindings generation is off - all DOM interface .idl removed
# (Bucket 4 Phase 4 Tier B). Hipe manipulates the DOM via the C++ QWebElement
# API, never JavaScript. The C++ enum/name/element factories (EventInterfaces.h,
# HTMLNames.cpp, etc.) come from the .in generators, not these .idl.
set(WebCore_NON_SVG_IDL_FILES
)


# hipecore: the XMLViewer minify helpers (xxd.pl / cssmin.py / jsmin.py) used to
# be shared out of Source/JavaScriptCore/Scripts via the JSC forwarding-header
# stage. JavaScriptCore is gone (Bucket 5.7); they now live in Source/WebCore/Scripts.
set(WebCore_SCRIPTS_DIR "${WEBCORE_DIR}/Scripts")

set(WebCore_SOURCES

    Modules/mediasession/HTMLMediaElementMediaSession.cpp
    Modules/mediasession/MediaRemoteControls.cpp
    Modules/mediasession/MediaSession.cpp

    Modules/mediasource/DOMURLMediaSource.cpp
    Modules/mediasource/MediaSource.cpp
    Modules/mediasource/MediaSourceRegistry.cpp
    Modules/mediasource/SampleMap.cpp
    Modules/mediasource/SourceBuffer.cpp
    Modules/mediasource/SourceBufferList.cpp
    Modules/mediasource/VideoPlaybackQuality.cpp

    Modules/speech/DOMWindowSpeechSynthesis.cpp
    Modules/speech/SpeechSynthesis.cpp
    Modules/speech/SpeechSynthesisEvent.cpp
    Modules/speech/SpeechSynthesisUtterance.cpp
    Modules/speech/SpeechSynthesisVoice.cpp

    Modules/webaudio/AnalyserNode.cpp
    Modules/webaudio/AsyncAudioDecoder.cpp
    Modules/webaudio/AudioBasicInspectorNode.cpp
    Modules/webaudio/AudioBasicProcessorNode.cpp
    Modules/webaudio/AudioBuffer.cpp
    Modules/webaudio/AudioBufferSourceNode.cpp
    Modules/webaudio/AudioContext.cpp
    Modules/webaudio/AudioDestinationNode.cpp
    Modules/webaudio/AudioListener.cpp
    Modules/webaudio/AudioNode.cpp
    Modules/webaudio/AudioNodeInput.cpp
    Modules/webaudio/AudioNodeOutput.cpp
    Modules/webaudio/AudioParam.cpp
    Modules/webaudio/AudioParamTimeline.cpp
    Modules/webaudio/AudioProcessingEvent.cpp
    Modules/webaudio/AudioScheduledSourceNode.cpp
    Modules/webaudio/AudioSummingJunction.cpp
    Modules/webaudio/BiquadDSPKernel.cpp
    Modules/webaudio/BiquadFilterNode.cpp
    Modules/webaudio/BiquadProcessor.cpp
    Modules/webaudio/ChannelMergerNode.cpp
    Modules/webaudio/ChannelSplitterNode.cpp
    Modules/webaudio/ConvolverNode.cpp
    Modules/webaudio/DefaultAudioDestinationNode.cpp
    Modules/webaudio/DelayDSPKernel.cpp
    Modules/webaudio/DelayNode.cpp
    Modules/webaudio/DelayProcessor.cpp
    Modules/webaudio/DynamicsCompressorNode.cpp
    Modules/webaudio/GainNode.cpp
    Modules/webaudio/MediaElementAudioSourceNode.cpp
    Modules/webaudio/MediaStreamAudioSource.cpp
    Modules/webaudio/OfflineAudioCompletionEvent.cpp
    Modules/webaudio/OfflineAudioContext.cpp
    Modules/webaudio/OfflineAudioDestinationNode.cpp
    Modules/webaudio/OscillatorNode.cpp
    Modules/webaudio/PannerNode.cpp
    Modules/webaudio/PeriodicWave.cpp
    Modules/webaudio/RealtimeAnalyser.cpp
    Modules/webaudio/ScriptProcessorNode.cpp
    Modules/webaudio/WaveShaperDSPKernel.cpp
    Modules/webaudio/WaveShaperNode.cpp
    Modules/webaudio/WaveShaperProcessor.cpp

    accessibility/AXObjectCache.cpp
    accessibility/AccessibilityARIAGrid.cpp
    accessibility/AccessibilityARIAGridCell.cpp
    accessibility/AccessibilityARIAGridRow.cpp
    accessibility/AccessibilityImageMapLink.cpp
    accessibility/AccessibilityList.cpp
    accessibility/AccessibilityListBox.cpp
    accessibility/AccessibilityListBoxOption.cpp
    accessibility/AccessibilityMediaControls.cpp
    accessibility/AccessibilityMenuList.cpp
    accessibility/AccessibilityMenuListOption.cpp
    accessibility/AccessibilityMenuListPopup.cpp
    accessibility/AccessibilityMockObject.cpp
    accessibility/AccessibilityNodeObject.cpp
    accessibility/AccessibilityObject.cpp
    accessibility/AccessibilityProgressIndicator.cpp
    accessibility/AccessibilityRenderObject.cpp
    accessibility/AccessibilitySVGRoot.cpp
    accessibility/AccessibilityScrollView.cpp
    accessibility/AccessibilityScrollbar.cpp
    accessibility/AccessibilitySlider.cpp
    accessibility/AccessibilitySpinButton.cpp
    accessibility/AccessibilityTable.cpp
    accessibility/AccessibilityTableCell.cpp
    accessibility/AccessibilityTableColumn.cpp
    accessibility/AccessibilityTableHeaderContainer.cpp
    accessibility/AccessibilityTableRow.cpp
    accessibility/AccessibilityTree.cpp
    accessibility/AccessibilityTreeItem.cpp

    bindings/generic/ActiveDOMCallback.cpp
    bindings/generic/RuntimeEnabledFeatures.cpp

    contentextensions/CombinedFiltersAlphabet.cpp
    contentextensions/CombinedURLFilters.cpp
    contentextensions/CompiledContentExtension.cpp
    contentextensions/ContentExtension.cpp
    contentextensions/ContentExtensionCompiler.cpp
    contentextensions/ContentExtensionError.cpp
    contentextensions/ContentExtensionParser.cpp
    contentextensions/ContentExtensionRule.cpp
    contentextensions/ContentExtensionStyleSheet.cpp
    contentextensions/ContentExtensionsBackend.cpp
    contentextensions/DFA.cpp
    contentextensions/DFABytecodeCompiler.cpp
    contentextensions/DFABytecodeInterpreter.cpp
    contentextensions/DFACombiner.cpp
    contentextensions/DFAMinimizer.cpp
    contentextensions/DFANode.cpp
    contentextensions/NFA.cpp
    contentextensions/NFAToDFA.cpp
    contentextensions/URLFilterParser.cpp

    css/BasicShapeFunctions.cpp
    css/CSSAspectRatioValue.cpp
    css/CSSBasicShapes.cpp
    css/CSSBorderImage.cpp
    css/CSSBorderImageSliceValue.cpp
    css/CSSCalculationValue.cpp
    css/CSSCanvasValue.cpp
    css/CSSCharsetRule.cpp
    css/CSSComputedStyleDeclaration.cpp
    css/CSSContentDistributionValue.cpp
    css/CSSCrossfadeValue.cpp
    css/CSSCursorImageValue.cpp
    css/CSSDefaultStyleSheets.cpp
    css/CSSFilterImageValue.cpp
    css/FontFaceSet.cpp
    css/FontFace.cpp
    css/CSSFontFace.cpp
    css/CSSFontFaceSet.cpp
    css/CSSFontFaceLoadEvent.cpp
    css/CSSFontFaceRule.cpp
    css/CSSFontFaceSource.cpp
    css/CSSFontFaceSrcValue.cpp
    css/CSSFontFeatureValue.cpp
    css/CSSFontSelector.cpp
    css/CSSFontValue.cpp
    css/CSSFunctionValue.cpp
    css/CSSGradientValue.cpp
    css/CSSGridLineNamesValue.cpp
    css/CSSGridTemplateAreasValue.cpp
    css/CSSGroupingRule.cpp
    css/CSSImageGeneratorValue.cpp
    css/CSSImageSetValue.cpp
    css/CSSImageValue.cpp
    css/CSSImportRule.cpp
    css/CSSInheritedValue.cpp
    css/CSSInitialValue.cpp
    css/CSSKeyframeRule.cpp
    css/CSSKeyframesRule.cpp
    css/CSSLineBoxContainValue.cpp
    css/CSSMediaRule.cpp
    css/CSSNamedImageValue.cpp
    css/CSSOMUtils.cpp
    css/CSSPageRule.cpp
    css/CSSParser.cpp
    css/CSSParserValues.cpp
    css/CSSPrimitiveValue.cpp
    css/CSSProperty.cpp
    css/CSSPropertySourceData.cpp
    css/CSSReflectValue.cpp
    css/CSSRevertValue.cpp
    css/CSSRule.cpp
    css/CSSRuleList.cpp
    css/CSSSegmentedFontFace.cpp
    css/CSSSelector.cpp
    css/CSSSelectorList.cpp
    css/CSSShadowValue.cpp
    css/CSSStyleRule.cpp
    css/CSSStyleSheet.cpp
    css/CSSSupportsRule.cpp
    css/CSSTimingFunctionValue.cpp
    css/CSSToLengthConversionData.cpp
    css/CSSToStyleMap.cpp
    css/CSSUnicodeRangeValue.cpp
    css/CSSUnsetValue.cpp
    css/CSSValue.cpp
    css/CSSValueList.cpp
    css/CSSValuePool.cpp
    css/CSSVariableDependentValue.cpp
    css/CSSVariableValue.cpp
    css/DOMWindowCSS.cpp
    css/DocumentRuleSets.cpp
    css/ElementRuleCollector.cpp
    css/FontLoader.cpp
    css/FontVariantBuilder.cpp
    css/LengthFunctions.cpp
    css/MediaFeatureNames.cpp
    css/MediaList.cpp
    css/MediaQuery.cpp
    css/MediaQueryEvaluator.cpp
    css/MediaQueryExp.cpp
    css/MediaQueryList.cpp
    css/MediaQueryMatcher.cpp
    css/PageRuleCollector.cpp
    css/PropertySetCSSStyleDeclaration.cpp
    css/RGBColor.cpp
    css/RuleFeature.cpp
    css/RuleSet.cpp
    css/SVGCSSComputedStyleDeclaration.cpp
    css/SVGCSSParser.cpp
    css/SelectorChecker.cpp
    css/SelectorFilter.cpp
    css/SourceSizeList.cpp
    css/StyleInvalidationAnalysis.cpp
    css/StyleMedia.cpp
    css/StyleProperties.cpp
    css/StylePropertyShorthand.cpp
    css/StyleResolver.cpp
    css/StyleRule.cpp
    css/StyleRuleImport.cpp
    css/StyleSheet.cpp
    css/StyleSheetContents.cpp
    css/StyleSheetList.cpp
    css/TransformFunctions.cpp
    css/ViewportStyleResolver.cpp
    css/WebKitCSSFilterValue.cpp
    css/WebKitCSSMatrix.cpp
    css/WebKitCSSRegionRule.cpp
    css/WebKitCSSTransformValue.cpp
    css/WebKitCSSViewportRule.cpp

    cssjit/SelectorCompiler.cpp

    dom/ActiveDOMCallbackMicrotask.cpp
    dom/ActiveDOMObject.cpp
    dom/AnimationEvent.cpp
    dom/Attr.cpp
    dom/AuthorStyleSheets.cpp
    dom/BeforeTextInsertedEvent.cpp
    dom/BeforeUnloadEvent.cpp
    dom/CDATASection.cpp
    dom/CharacterData.cpp
    dom/CheckedRadioButtons.cpp
    dom/ChildNodeList.cpp
    dom/ClassCollection.cpp
    dom/ClientRect.cpp
    dom/ClientRectList.cpp
    dom/ClipboardEvent.cpp
    dom/CollectionIndexCache.cpp
    dom/Comment.cpp
    dom/ComposedTreeIterator.cpp
    dom/CompositionEvent.cpp
    dom/ContainerNode.cpp
    dom/ContainerNodeAlgorithms.cpp
    dom/ContextDestructionObserver.cpp
    dom/DOMCoreException.cpp
    dom/DOMError.cpp
    dom/DOMImplementation.cpp
    dom/DOMNamedFlowCollection.cpp
    dom/DOMStringList.cpp
    dom/DataTransfer.cpp
    dom/DataTransferItem.cpp
    dom/DatasetDOMStringMap.cpp
    dom/DecodedDataDocumentParser.cpp
    dom/Document.cpp
    dom/DocumentEventQueue.cpp
    dom/DocumentFragment.cpp
    dom/DocumentMarker.cpp
    dom/DocumentMarkerController.cpp
    dom/DocumentOrderedMap.cpp
    dom/DocumentParser.cpp
    dom/DocumentSharedObjectPool.cpp
    dom/DocumentType.cpp
    dom/Element.cpp
    dom/ElementData.cpp
    dom/ElementRareData.cpp
    dom/EntityReference.cpp
    dom/ErrorEvent.cpp
    dom/Event.cpp
    dom/EventContext.cpp
    dom/EventDispatcher.cpp
    dom/EventListenerMap.cpp
    dom/EventNames.cpp
    dom/EventTarget.cpp
    dom/ExceptionBase.cpp
    dom/ExceptionCodePlaceholder.cpp
    dom/ExtensionStyleSheets.cpp
    dom/FocusEvent.cpp
    dom/GenericEventQueue.cpp
    dom/IdTargetObserver.cpp
    dom/IdTargetObserverRegistry.cpp
    dom/InlineStyleSheetOwner.cpp
    dom/KeyboardEvent.cpp
    dom/LiveNodeList.cpp
    dom/Microtasks.cpp
    dom/MouseEvent.cpp
    dom/MouseRelatedEvent.cpp
    dom/MutationEvent.cpp
    dom/NameNodeList.cpp
    dom/NamedFlowCollection.cpp
    dom/NamedNodeMap.cpp
    dom/Node.cpp
    dom/NodeOrString.cpp
    dom/NodeRareData.cpp
    dom/NodeTraversal.cpp
    dom/OverflowEvent.cpp
    dom/PageTransitionEvent.cpp
    dom/Position.cpp
    dom/PositionIterator.cpp
    dom/ProcessingInstruction.cpp
    dom/ProgressEvent.cpp
    dom/PseudoElement.cpp
    dom/QualifiedName.cpp
    dom/Range.cpp
    dom/RegisteredEventListener.cpp
    dom/ScopedEventQueue.cpp
    dom/ScriptElement.cpp
    dom/ScriptExecutionContext.cpp
    dom/ScriptableDocumentParser.cpp
    dom/ScriptedAnimationController.cpp
    dom/SecurityContext.cpp
    dom/SecurityOriginPolicy.cpp
    dom/SelectorQuery.cpp
    dom/ShadowRoot.cpp
    dom/SimulatedClick.cpp
    dom/SlotAssignment.cpp
    dom/SpaceSplitString.cpp
    dom/StaticNodeList.cpp
    dom/StringCallback.cpp
    dom/StyledElement.cpp
    dom/TagCollection.cpp
    dom/Text.cpp
    dom/TextEvent.cpp
    dom/TextNodeTraversal.cpp
    dom/Touch.cpp
    dom/TouchEvent.cpp
    dom/TouchList.cpp
    dom/TransformSourceLibxslt.cpp
    dom/TransitionEvent.cpp
    dom/TreeScope.cpp
    dom/TreeScopeAdopter.cpp
    dom/UIEvent.cpp
    dom/UIEventWithKeyState.cpp
    dom/UserActionElementSet.cpp
    dom/UserGestureIndicator.cpp
    dom/UserTypingGestureIndicator.cpp
    dom/ViewportArguments.cpp
    dom/VisitedLinkState.cpp
    dom/WebKitAnimationEvent.cpp
    dom/WebKitNamedFlow.cpp
    dom/WebKitTransitionEvent.cpp
    dom/WheelEvent.cpp


    editing/AlternativeTextController.cpp
    editing/AppendNodeCommand.cpp
    editing/ApplyBlockElementCommand.cpp
    editing/ApplyStyleCommand.cpp
    editing/BreakBlockquoteCommand.cpp
    editing/CompositeEditCommand.cpp
    editing/CreateLinkCommand.cpp
    editing/DeleteFromTextNodeCommand.cpp
    editing/DeleteSelectionCommand.cpp
    editing/DictationAlternative.cpp
    editing/DictationCommand.cpp
    editing/EditCommand.cpp
    editing/EditingStyle.cpp
    editing/Editor.cpp
    editing/EditorCommand.cpp
    editing/FormatBlockCommand.cpp
    editing/FrameSelection.cpp
    editing/HTMLInterchange.cpp
    editing/IndentOutdentCommand.cpp
    editing/InsertIntoTextNodeCommand.cpp
    editing/InsertLineBreakCommand.cpp
    editing/InsertListCommand.cpp
    editing/InsertNodeBeforeCommand.cpp
    editing/InsertParagraphSeparatorCommand.cpp
    editing/InsertTextCommand.cpp
    editing/MarkupAccumulator.cpp
    editing/MergeIdenticalElementsCommand.cpp
    editing/ModifySelectionListLevel.cpp
    editing/MoveSelectionCommand.cpp
    editing/RemoveCSSPropertyCommand.cpp
    editing/RemoveFormatCommand.cpp
    editing/RemoveNodeCommand.cpp
    editing/RemoveNodePreservingChildrenCommand.cpp
    editing/RenderedPosition.cpp
    editing/ReplaceDeleteFromTextNodeCommand.cpp
    editing/ReplaceInsertIntoTextNodeCommand.cpp
    editing/ReplaceNodeWithSpanCommand.cpp
    editing/ReplaceSelectionCommand.cpp
    editing/SetNodeAttributeCommand.cpp
    editing/SetSelectionCommand.cpp
    editing/SimplifyMarkupCommand.cpp
    editing/SmartReplace.cpp
    editing/SpellChecker.cpp
    editing/SpellingCorrectionCommand.cpp
    editing/SplitElementCommand.cpp
    editing/SplitTextNodeCommand.cpp
    editing/SplitTextNodeContainingElementCommand.cpp
    editing/TextCheckingHelper.cpp
    editing/TextInsertionBaseCommand.cpp
    editing/TextIterator.cpp
    editing/TypingCommand.cpp
    editing/UnlinkCommand.cpp
    editing/VisiblePosition.cpp
    editing/VisibleSelection.cpp
    editing/VisibleUnits.cpp
    editing/WrapContentsInDummySpanCommand.cpp
    editing/htmlediting.cpp
    editing/markup.cpp

    fileapi/AsyncFileStream.cpp
    fileapi/Blob.cpp
    fileapi/BlobURL.cpp
    fileapi/File.cpp
    fileapi/FileException.cpp
    fileapi/FileList.cpp
    fileapi/FileReader.cpp
    fileapi/FileReaderLoader.cpp
    fileapi/ThreadableBlobRegistry.cpp
    fileapi/WebKitBlobBuilder.cpp

    history/HistoryItem.cpp

    html/AttributeDOMTokenList.cpp
    html/BaseButtonInputType.cpp
    html/BaseCheckableInputType.cpp
    html/BaseChooserOnlyDateAndTimeInputType.cpp
    html/BaseClickableWithKeyInputType.cpp
    html/BaseDateAndTimeInputType.cpp
    html/BaseTextInputType.cpp
    html/ButtonInputType.cpp
    html/CheckboxInputType.cpp
    html/ColorInputType.cpp
    html/DOMFormData.cpp
    html/DOMTokenList.cpp
    html/DOMURL.cpp
    html/DateInputType.cpp
    html/DateTimeInputType.cpp
    html/DateTimeLocalInputType.cpp
    html/EmailInputType.cpp
    html/FormAssociatedElement.cpp
    html/FormController.cpp
    html/FormDataList.cpp
    html/GenericCachedHTMLCollection.cpp
    html/HTMLAllCollection.cpp
    html/HTMLAnchorElement.cpp
    html/HTMLAppletElement.cpp
    html/HTMLAreaElement.cpp
    html/HTMLAttachmentElement.cpp
    html/HTMLAudioElement.cpp
    html/HTMLBRElement.cpp
    html/HTMLBaseElement.cpp
    html/HTMLBaseFontElement.cpp
    html/HTMLBodyElement.cpp
    html/HTMLButtonElement.cpp
    html/HTMLCanvasElement.cpp
    html/HTMLCollection.cpp
    html/HTMLDListElement.cpp
    html/HTMLDataElement.cpp
    html/HTMLDataListElement.cpp
    html/HTMLDetailsElement.cpp
    html/HTMLDirectoryElement.cpp
    html/HTMLDivElement.cpp
    html/HTMLDocument.cpp
    html/HTMLElement.cpp
    html/HTMLEmbedElement.cpp
    html/HTMLFieldSetElement.cpp
    html/HTMLFontElement.cpp
    html/HTMLFormControlElement.cpp
    html/HTMLFormControlElementWithState.cpp
    html/HTMLFormControlsCollection.cpp
    html/HTMLFormElement.cpp
    html/HTMLFrameElement.cpp
    html/HTMLFrameElementBase.cpp
    html/HTMLFrameOwnerElement.cpp
    html/HTMLFrameSetElement.cpp
    html/HTMLHRElement.cpp
    html/HTMLHeadElement.cpp
    html/HTMLHeadingElement.cpp
    html/HTMLHtmlElement.cpp
    html/HTMLIFrameElement.cpp
    html/HTMLImageElement.cpp
    html/HTMLImageLoader.cpp
    html/HTMLInputElement.cpp
    html/HTMLLIElement.cpp
    html/HTMLLabelElement.cpp
    html/HTMLLegendElement.cpp
    html/HTMLLinkElement.cpp
    html/HTMLMapElement.cpp
    html/HTMLMarqueeElement.cpp
    html/HTMLMediaElement.cpp
    html/HTMLMenuElement.cpp
    html/HTMLMetaElement.cpp
    html/HTMLMeterElement.cpp
    html/HTMLModElement.cpp
    html/HTMLNameCollection.cpp
    html/HTMLOListElement.cpp
    html/HTMLObjectElement.cpp
    html/HTMLOptGroupElement.cpp
    html/HTMLOptionElement.cpp
    html/HTMLOptionsCollection.cpp
    html/HTMLOutputElement.cpp
    html/HTMLParagraphElement.cpp
    html/HTMLParamElement.cpp
    html/HTMLPictureElement.cpp
    html/HTMLPlugInElement.cpp
    html/HTMLPlugInImageElement.cpp
    html/HTMLPreElement.cpp
    html/HTMLProgressElement.cpp
    html/HTMLQuoteElement.cpp
    html/HTMLScriptElement.cpp
    html/HTMLSelectElement.cpp
    html/HTMLSlotElement.cpp
    html/HTMLSourceElement.cpp
    html/HTMLSpanElement.cpp
    html/HTMLStyleElement.cpp
    html/HTMLSummaryElement.cpp
    html/HTMLTableCaptionElement.cpp
    html/HTMLTableCellElement.cpp
    html/HTMLTableColElement.cpp
    html/HTMLTableElement.cpp
    html/HTMLTableHeaderCellElement.cpp
    html/HTMLTablePartElement.cpp
    html/HTMLTableRowElement.cpp
    html/HTMLTableRowsCollection.cpp
    html/HTMLTableSectionElement.cpp
    html/HTMLTemplateElement.cpp
    html/HTMLTextAreaElement.cpp
    html/HTMLTextFormControlElement.cpp
    html/HTMLTimeElement.cpp
    html/HTMLTitleElement.cpp
    html/HTMLTrackElement.cpp
    html/HTMLUListElement.cpp
    html/HTMLVideoElement.cpp
    html/HTMLWBRElement.cpp
    html/HiddenInputType.cpp
    html/ImageData.cpp
    html/ImageDocument.cpp
    html/ImageInputType.cpp
    html/InputType.cpp
    html/InputTypeNames.cpp
    html/LabelableElement.cpp
    html/LabelsNodeList.cpp
    html/LinkRelAttribute.cpp
    html/MediaController.cpp
    html/MediaDocument.cpp
    html/MediaElementSession.cpp
    html/MediaFragmentURIParser.cpp
    html/MonthInputType.cpp
    html/NumberInputType.cpp
    html/PasswordInputType.cpp
    html/PluginDocument.cpp
    html/PublicURLManager.cpp
    html/RadioInputType.cpp
    html/RadioNodeList.cpp
    html/RangeInputType.cpp
    html/ResetInputType.cpp
    html/RubyElement.cpp
    html/RubyTextElement.cpp
    html/SearchInputType.cpp
    html/StepRange.cpp
    html/SubmitInputType.cpp
    html/TelephoneInputType.cpp
    html/TextDocument.cpp
    html/TextFieldInputType.cpp
    html/TextInputType.cpp
    html/TimeInputType.cpp
    html/TimeRanges.cpp
    html/TypeAhead.cpp
    html/URLInputType.cpp
    html/ValidationMessage.cpp
    html/WeekInputType.cpp

    html/canvas/ANGLEInstancedArrays.cpp
    html/canvas/CanvasContextAttributes.cpp
    html/canvas/CanvasGradient.cpp
    html/canvas/CanvasPathMethods.cpp
    html/canvas/CanvasPattern.cpp
    html/canvas/CanvasProxy.cpp
    html/canvas/CanvasRenderingContext.cpp
    html/canvas/CanvasRenderingContext2D.cpp
    html/canvas/CanvasStyle.cpp
    html/canvas/DOMPath.cpp
    html/canvas/EXTBlendMinMax.cpp
    html/canvas/EXTFragDepth.cpp
    html/canvas/EXTShaderTextureLOD.cpp
    html/canvas/EXTTextureFilterAnisotropic.cpp
    html/canvas/EXTsRGB.cpp
    html/canvas/OESElementIndexUint.cpp
    html/canvas/OESStandardDerivatives.cpp
    html/canvas/OESTextureFloat.cpp
    html/canvas/OESTextureFloatLinear.cpp
    html/canvas/OESTextureHalfFloat.cpp
    html/canvas/OESTextureHalfFloatLinear.cpp
    html/canvas/OESVertexArrayObject.cpp
    html/canvas/WebGL2RenderingContext.cpp
    html/canvas/WebGLBuffer.cpp
    html/canvas/WebGLCompressedTextureATC.cpp
    html/canvas/WebGLCompressedTexturePVRTC.cpp
    html/canvas/WebGLCompressedTextureS3TC.cpp
    html/canvas/WebGLContextAttributes.cpp
    html/canvas/WebGLContextEvent.cpp
    html/canvas/WebGLContextGroup.cpp
    html/canvas/WebGLContextObject.cpp
    html/canvas/WebGLDebugRendererInfo.cpp
    html/canvas/WebGLDebugShaders.cpp
    html/canvas/WebGLDepthTexture.cpp
    html/canvas/WebGLDrawBuffers.cpp
    html/canvas/WebGLExtension.cpp
    html/canvas/WebGLFramebuffer.cpp
    html/canvas/WebGLGetInfo.cpp
    html/canvas/WebGLLoseContext.cpp
    html/canvas/WebGLObject.cpp
    html/canvas/WebGLProgram.cpp
    html/canvas/WebGLQuery.cpp
    html/canvas/WebGLRenderbuffer.cpp
    html/canvas/WebGLRenderingContext.cpp
    html/canvas/WebGLRenderingContextBase.cpp
    html/canvas/WebGLSampler.cpp
    html/canvas/WebGLShader.cpp
    html/canvas/WebGLShaderPrecisionFormat.cpp
    html/canvas/WebGLSharedObject.cpp
    html/canvas/WebGLSync.cpp
    html/canvas/WebGLTexture.cpp
    html/canvas/WebGLTransformFeedback.cpp
    html/canvas/WebGLUniformLocation.cpp
    html/canvas/WebGLVertexArrayObject.cpp
    html/canvas/WebGLVertexArrayObjectBase.cpp
    html/canvas/WebGLVertexArrayObjectOES.cpp

    html/parser/CSSPreloadScanner.cpp
    html/parser/HTMLConstructionSite.cpp
    html/parser/HTMLDocumentParser.cpp
    html/parser/HTMLElementStack.cpp
    html/parser/HTMLEntityParser.cpp
    html/parser/HTMLEntitySearch.cpp
    html/parser/HTMLFormattingElementList.cpp
    html/parser/HTMLMetaCharsetParser.cpp
    html/parser/HTMLParserIdioms.cpp
    html/parser/HTMLParserOptions.cpp
    html/parser/HTMLParserScheduler.cpp
    html/parser/HTMLPreloadScanner.cpp
    html/parser/HTMLResourcePreloader.cpp
    html/parser/HTMLSrcsetParser.cpp
    html/parser/HTMLTokenizer.cpp
    html/parser/HTMLTreeBuilder.cpp
    html/parser/TextDocumentParser.cpp

    html/shadow/AutoFillButtonElement.cpp
    html/shadow/DetailsMarkerControl.cpp
    html/shadow/MediaControlElementTypes.cpp
    html/shadow/MediaControlElements.cpp
    html/shadow/MediaControls.cpp
    # hipecore: with ENABLE_MEDIA_CONTROLS_SCRIPT off, this (the only concrete
    # MediaControls subclass, "Apple" is historical) provides MediaControls::create()
    # for the C++ <video>/<audio> control shadow DOM. Upstream Qt built the JS controls.
    html/shadow/MediaControlsApple.cpp
    html/shadow/MeterShadowElement.cpp
    html/shadow/ProgressShadowElement.cpp
    html/shadow/SliderThumbElement.cpp
    html/shadow/SpinButtonElement.cpp
    html/shadow/TextControlInnerElements.cpp

    loader/CrossOriginAccessControl.cpp
    loader/CrossOriginPreflightResultCache.cpp
    loader/DocumentLoadTiming.cpp
    loader/DocumentLoader.cpp
    loader/DocumentThreadableLoader.cpp
    loader/DocumentWriter.cpp
    loader/EmptyClients.cpp
    loader/FormState.cpp
    loader/FormSubmission.cpp
    loader/FrameLoadRequest.cpp
    loader/FrameLoader.cpp
    loader/FrameLoaderStateMachine.cpp
    loader/HistoryController.cpp
    loader/ImageLoader.cpp
    loader/LoaderStrategy.cpp
    loader/MediaResourceLoader.cpp
    loader/NavigationAction.cpp
    loader/NavigationScheduler.cpp
    loader/PolicyCallback.cpp
    loader/PolicyChecker.cpp
    loader/ProgressTracker.cpp
    loader/ResourceLoadNotifier.cpp
    loader/ResourceLoader.cpp
    loader/SinkDocument.cpp
    loader/SubframeLoader.cpp
    loader/SubresourceLoader.cpp
    loader/TextResourceDecoder.cpp
    loader/TextTrackLoader.cpp
    loader/ThreadableLoader.cpp

    loader/cache/CachedCSSStyleSheet.cpp
    loader/cache/CachedFont.cpp
    loader/cache/CachedImage.cpp
    loader/cache/CachedRawResource.cpp
    loader/cache/CachedResource.cpp
    loader/cache/CachedResourceHandle.cpp
    loader/cache/CachedResourceLoader.cpp
    loader/cache/CachedResourceRequest.cpp
    loader/cache/CachedResourceRequestInitiators.cpp
    loader/cache/CachedSVGDocument.cpp
    loader/cache/CachedSVGDocumentReference.cpp
    loader/cache/CachedSVGFont.cpp
    loader/cache/CachedXSLStyleSheet.cpp
    loader/cache/MemoryCache.cpp

    mathml/MathMLElement.cpp
    mathml/MathMLInlineContainerElement.cpp
    mathml/MathMLMathElement.cpp
    mathml/MathMLMencloseElement.cpp
    mathml/MathMLSelectElement.cpp
    mathml/MathMLTextElement.cpp

    page/AutoscrollController.cpp
    page/CaptionUserPreferences.cpp
    page/Chrome.cpp
    page/ContextMenuContext.cpp
    page/ContextMenuController.cpp
    page/Crypto.cpp
    page/DOMSelection.cpp
    page/DOMWindow.cpp
    page/DOMWindowProperty.cpp
    page/DatabaseProvider.cpp
    page/DebugPageOverlays.cpp
    page/DragController.cpp
    page/EventHandler.cpp
    page/FocusController.cpp
    page/Frame.cpp
    page/FrameDestructionObserver.cpp
    page/FrameSnapshotting.cpp
    page/FrameTree.cpp
    page/FrameView.cpp
    page/History.cpp
    page/MainFrame.cpp
    page/MouseEventWithHitTestResults.cpp
    page/OriginAccessEntry.cpp
    page/Page.cpp
    page/PageConfiguration.cpp
    page/PageConsoleClient.cpp
    page/PageGroup.cpp
    page/PageGroupLoadDeferrer.cpp
    page/PageOverlay.cpp
    page/PageOverlayController.cpp
    page/PageSerializer.cpp
    page/PageThrottler.cpp
    page/PageVisibilityState.cpp
    page/PointerLockController.cpp
    page/ResourceUsageData.cpp
    page/ResourceUsageOverlay.cpp
    page/ResourceUsageThread.cpp
    page/SecurityOrigin.cpp
    page/SecurityOriginData.cpp
    page/SecurityPolicy.cpp
    page/Settings.cpp
    page/SpatialNavigation.cpp
    page/SuspendableTimer.cpp
    page/TextIndicator.cpp
    page/UserContentController.cpp
    page/UserContentURLPattern.cpp
    page/VisitedLinkStore.cpp
    page/WheelEventDeltaFilter.cpp
    page/WheelEventTestTrigger.cpp
    page/WindowFeatures.cpp
    page/WindowFocusAllowedIndicator.cpp

    page/animation/AnimationBase.cpp
    page/animation/AnimationController.cpp
    page/animation/CSSPropertyAnimation.cpp
    page/animation/CompositeAnimation.cpp
    page/animation/ImplicitAnimation.cpp
    page/animation/KeyframeAnimation.cpp

    page/csp/ContentSecurityPolicy.cpp
    page/csp/ContentSecurityPolicyDirectiveList.cpp
    page/csp/ContentSecurityPolicyMediaListDirective.cpp
    page/csp/ContentSecurityPolicyResponseHeaders.cpp
    page/csp/ContentSecurityPolicySource.cpp
    page/csp/ContentSecurityPolicySourceList.cpp
    page/csp/ContentSecurityPolicySourceListDirective.cpp

    page/scrolling/AxisScrollSnapOffsets.cpp
    page/scrolling/ScrollLatchingState.cpp
    page/scrolling/ScrollingConstraints.cpp
    page/scrolling/ScrollingCoordinator.cpp
    page/scrolling/ScrollingStateFixedNode.cpp
    page/scrolling/ScrollingStateFrameScrollingNode.cpp
    page/scrolling/ScrollingStateNode.cpp
    page/scrolling/ScrollingStateOverflowScrollingNode.cpp
    page/scrolling/ScrollingStateScrollingNode.cpp
    page/scrolling/ScrollingStateStickyNode.cpp
    page/scrolling/ScrollingStateTree.cpp
    page/scrolling/ScrollingThread.cpp
    page/scrolling/ScrollingTree.cpp
    page/scrolling/ScrollingTreeFrameScrollingNode.cpp
    page/scrolling/ScrollingTreeNode.cpp
    page/scrolling/ScrollingTreeOverflowScrollingNode.cpp
    page/scrolling/ScrollingTreeScrollingNode.cpp
    page/scrolling/ThreadedScrollingTree.cpp

    platform/CalculationValue.cpp
    platform/Clock.cpp
    platform/ClockGeneric.cpp
    platform/ContentType.cpp
    platform/ContextMenu.cpp
    platform/ContextMenuItem.cpp
    platform/CrossThreadCopier.cpp
    platform/Cursor.cpp
    platform/DateComponents.cpp
    platform/Decimal.cpp
    platform/DisplaySleepDisabler.cpp
    platform/DragData.cpp
    platform/DragImage.cpp
    platform/FileStream.cpp
    platform/FileSystem.cpp
    platform/Language.cpp
    platform/Length.cpp
    platform/LengthPoint.cpp
    platform/LengthSize.cpp
    platform/LinkHash.cpp
    platform/Logging.cpp
    platform/MIMETypeRegistry.cpp
    platform/MainThreadSharedTimer.cpp
    platform/MemoryPressureHandler.cpp
    platform/NotImplemented.cpp
    platform/PlatformEvent.cpp
    platform/PlatformSpeechSynthesisUtterance.cpp
    platform/PlatformSpeechSynthesisVoice.cpp
    platform/PlatformSpeechSynthesizer.cpp
    platform/PlatformStrategies.cpp
    platform/RemoteCommandListener.cpp
    platform/RuntimeApplicationChecks.cpp
    platform/SchemeRegistry.cpp
    platform/ScrollAnimator.cpp
    platform/ScrollView.cpp
    platform/ScrollableArea.cpp
    platform/Scrollbar.cpp
    platform/ScrollbarTheme.cpp
    platform/ScrollbarThemeComposite.cpp
    platform/SharedBuffer.cpp
    platform/SharedBufferChunkReader.cpp
    platform/SystemSleepListener.cpp
    platform/Theme.cpp
    platform/ThreadGlobalData.cpp
    platform/ThreadTimers.cpp
    platform/Timer.cpp
    platform/URL.cpp
    platform/UUID.cpp
    platform/UserActivity.cpp
    platform/Widget.cpp

    platform/animation/Animation.cpp
    platform/animation/AnimationList.cpp
    platform/animation/TimingFunction.cpp

    platform/audio/AudioBus.cpp
    platform/audio/AudioChannel.cpp
    platform/audio/AudioDSPKernelProcessor.cpp
    platform/audio/AudioFIFO.cpp
    platform/audio/AudioHardwareListener.cpp
    platform/audio/AudioPullFIFO.cpp
    platform/audio/AudioResampler.cpp
    platform/audio/AudioResamplerKernel.cpp
    platform/audio/AudioUtilities.cpp
    platform/audio/Biquad.cpp
    platform/audio/Cone.cpp
    platform/audio/DirectConvolver.cpp
    platform/audio/Distance.cpp
    platform/audio/DownSampler.cpp
    platform/audio/DynamicsCompressor.cpp
    platform/audio/DynamicsCompressorKernel.cpp
    platform/audio/EqualPowerPanner.cpp
    platform/audio/FFTConvolver.cpp
    platform/audio/FFTFrame.cpp
    platform/audio/HRTFDatabase.cpp
    platform/audio/HRTFDatabaseLoader.cpp
    platform/audio/HRTFElevation.cpp
    platform/audio/HRTFKernel.cpp
    platform/audio/HRTFPanner.cpp
    platform/audio/MultiChannelResampler.cpp
    platform/audio/Panner.cpp
    platform/audio/PlatformMediaSession.cpp
    platform/audio/PlatformMediaSessionManager.cpp
    platform/audio/Reverb.cpp
    platform/audio/ReverbAccumulationBuffer.cpp
    platform/audio/ReverbConvolver.cpp
    platform/audio/ReverbConvolverStage.cpp
    platform/audio/ReverbInputBuffer.cpp
    platform/audio/SincResampler.cpp
    platform/audio/UpSampler.cpp
    platform/audio/VectorMath.cpp
    platform/audio/ZeroPole.cpp

    platform/graphics/BitmapImage.cpp
    platform/graphics/Color.cpp
    platform/graphics/CrossfadeGeneratedImage.cpp
    platform/graphics/DisplayRefreshMonitorClient.cpp
    platform/graphics/FloatPoint.cpp
    platform/graphics/FloatPoint3D.cpp
    platform/graphics/FloatPolygon.cpp
    platform/graphics/FloatQuad.cpp
    platform/graphics/FloatRect.cpp
    platform/graphics/FloatRoundedRect.cpp
    platform/graphics/FloatSize.cpp
    platform/graphics/Font.cpp
    platform/graphics/FontCache.cpp
    platform/graphics/FontCascade.cpp
    platform/graphics/FontCascadeFonts.cpp
    platform/graphics/FontDescription.cpp
    platform/graphics/FontFeatureSettings.cpp
    platform/graphics/FontGenericFamilies.cpp
    platform/graphics/FontRanges.cpp
    platform/graphics/GeneratedImage.cpp
    platform/graphics/GeometryUtilities.cpp
    platform/graphics/Gradient.cpp
    platform/graphics/GradientImage.cpp
    platform/graphics/GraphicsContext.cpp
    platform/graphics/GraphicsLayer.cpp
    platform/graphics/GraphicsLayerTransform.cpp
    platform/graphics/GraphicsLayerUpdater.cpp
    platform/graphics/GraphicsTypes.cpp
    platform/graphics/ISOVTTCue.cpp
    platform/graphics/Image.cpp
    platform/graphics/ImageBuffer.cpp
    platform/graphics/ImageOrientation.cpp
    platform/graphics/IntPoint.cpp
    platform/graphics/IntRect.cpp
    platform/graphics/IntSize.cpp
    platform/graphics/LayoutPoint.cpp
    platform/graphics/LayoutRect.cpp
    platform/graphics/LayoutSize.cpp
    platform/graphics/MediaPlayer.cpp
    platform/graphics/NamedImageGeneratedImage.cpp
    platform/graphics/Path.cpp
    platform/graphics/PathTraversalState.cpp
    platform/graphics/PathUtilities.cpp
    platform/graphics/Pattern.cpp
    platform/graphics/PlatformTimeRanges.cpp
    platform/graphics/Region.cpp
    platform/graphics/RoundedRect.cpp
    platform/graphics/SVGGlyph.cpp
    platform/graphics/ShadowBlur.cpp
    platform/graphics/StringTruncator.cpp
    platform/graphics/SurrogatePairAwareTextIterator.cpp
    platform/graphics/TextRun.cpp
    platform/graphics/WidthIterator.cpp

    platform/graphics/cpu/arm/filters/FELightingNEON.cpp

    platform/graphics/displaylists/DisplayList.cpp
    platform/graphics/displaylists/DisplayListItems.cpp
    platform/graphics/displaylists/DisplayListRecorder.cpp
    platform/graphics/displaylists/DisplayListReplayer.cpp

    platform/graphics/filters/DistantLightSource.cpp
    platform/graphics/filters/FEBlend.cpp
    platform/graphics/filters/FEColorMatrix.cpp
    platform/graphics/filters/FEComponentTransfer.cpp
    platform/graphics/filters/FEComposite.cpp
    platform/graphics/filters/FEConvolveMatrix.cpp
    platform/graphics/filters/FEDiffuseLighting.cpp
    platform/graphics/filters/FEDisplacementMap.cpp
    platform/graphics/filters/FEDropShadow.cpp
    platform/graphics/filters/FEFlood.cpp
    platform/graphics/filters/FEGaussianBlur.cpp
    platform/graphics/filters/FELighting.cpp
    platform/graphics/filters/FEMerge.cpp
    platform/graphics/filters/FEMorphology.cpp
    platform/graphics/filters/FEOffset.cpp
    platform/graphics/filters/FESpecularLighting.cpp
    platform/graphics/filters/FETile.cpp
    platform/graphics/filters/FETurbulence.cpp
    platform/graphics/filters/FilterEffect.cpp
    platform/graphics/filters/FilterOperation.cpp
    platform/graphics/filters/FilterOperations.cpp
    platform/graphics/filters/PointLightSource.cpp
    platform/graphics/filters/SourceAlpha.cpp
    platform/graphics/filters/SourceGraphic.cpp
    platform/graphics/filters/SpotLightSource.cpp

    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/graphics/transforms/AffineTransform.cpp
    platform/graphics/transforms/Matrix3DTransformOperation.cpp
    platform/graphics/transforms/MatrixTransformOperation.cpp
    platform/graphics/transforms/PerspectiveTransformOperation.cpp
    platform/graphics/transforms/RotateTransformOperation.cpp
    platform/graphics/transforms/ScaleTransformOperation.cpp
    platform/graphics/transforms/SkewTransformOperation.cpp
    platform/graphics/transforms/TransformOperations.cpp
    platform/graphics/transforms/TransformState.cpp
    platform/graphics/transforms/TransformationMatrix.cpp
    platform/graphics/transforms/TranslateTransformOperation.cpp

    platform/mock/ScrollAnimatorMock.cpp
    platform/mock/ScrollbarThemeMock.cpp

    platform/network/AuthenticationChallengeBase.cpp
    platform/network/BlobData.cpp
    platform/network/BlobDataFileReference.cpp
    platform/network/BlobRegistry.cpp
    platform/network/BlobRegistryImpl.cpp
    platform/network/BlobResourceHandle.cpp
    platform/network/CacheValidation.cpp
    platform/network/CredentialBase.cpp
    platform/network/CredentialStorage.cpp
    platform/network/DataURL.cpp
    platform/network/DataURLDecoder.cpp
    platform/network/FormData.cpp
    platform/network/FormDataBuilder.cpp
    platform/network/HTTPHeaderMap.cpp
    platform/network/HTTPParsers.cpp
    platform/network/ParsedContentRange.cpp
    platform/network/ParsedContentType.cpp
    platform/network/ProtectionSpaceBase.cpp
    platform/network/ResourceErrorBase.cpp
    platform/network/ResourceHandle.cpp
    platform/network/ResourceHandleClient.cpp
    platform/network/ResourceRequestBase.cpp
    platform/network/ResourceResponseBase.cpp
    platform/network/SynchronousLoaderClient.cpp

    platform/text/BidiContext.cpp
    platform/text/DateTimeFormat.cpp
    platform/text/LineEnding.cpp
    platform/text/LocaleToScriptMappingDefault.cpp
    platform/text/PlatformLocale.cpp
    platform/text/QuotedPrintable.cpp
    platform/text/SegmentedString.cpp
    platform/text/TextBoundaries.cpp
    platform/text/TextBreakIterator.cpp
    platform/text/TextCodec.cpp
    platform/text/TextCodecICU.cpp
    platform/text/TextCodecLatin1.cpp
    platform/text/TextCodecUTF16.cpp
    platform/text/TextCodecUTF8.cpp
    platform/text/TextCodecUserDefined.cpp
    platform/text/TextEncoding.cpp
    platform/text/TextEncodingDetectorICU.cpp
    platform/text/TextEncodingRegistry.cpp
    platform/text/TextStream.cpp

    platform/text/icu/UTextProvider.cpp
    platform/text/icu/UTextProviderLatin1.cpp
    platform/text/icu/UTextProviderUTF16.cpp

    plugins/PluginData.cpp

    rendering/AutoTableLayout.cpp
    rendering/BidiRun.cpp
    rendering/BorderEdge.cpp
    rendering/ClipRect.cpp
    rendering/CounterNode.cpp
    rendering/EllipsisBox.cpp
    rendering/FilterEffectRenderer.cpp
    rendering/FixedTableLayout.cpp
    rendering/FloatingObjects.cpp
    rendering/FlowThreadController.cpp
    rendering/HitTestLocation.cpp
    rendering/HitTestResult.cpp
    rendering/HitTestingTransformState.cpp
    rendering/ImageQualityController.cpp
    rendering/InlineBox.cpp
    rendering/InlineElementBox.cpp
    rendering/InlineFlowBox.cpp
    rendering/InlineIterator.cpp
    rendering/InlineTextBox.cpp
    rendering/LayoutRepainter.cpp
    rendering/LayoutState.cpp
    rendering/OrderIterator.cpp
    rendering/PointerEventsHitRules.cpp
    rendering/RenderAttachment.cpp
    rendering/RenderBlock.cpp
    rendering/RenderBlockFlow.cpp
    rendering/RenderBlockLineLayout.cpp
    rendering/RenderBox.cpp
    rendering/RenderBoxModelObject.cpp
    rendering/RenderButton.cpp
    rendering/RenderCombineText.cpp
    rendering/RenderCounter.cpp
    rendering/RenderDeprecatedFlexibleBox.cpp
    rendering/RenderDetailsMarker.cpp
    rendering/RenderElement.cpp
    rendering/RenderEmbeddedObject.cpp
    rendering/RenderFieldset.cpp
    rendering/RenderFileUploadControl.cpp
    rendering/RenderFlexibleBox.cpp
    rendering/RenderFlowThread.cpp
    rendering/RenderFrame.cpp
    rendering/RenderFrameBase.cpp
    rendering/RenderFrameSet.cpp
    rendering/RenderGeometryMap.cpp
    rendering/RenderGrid.cpp
    rendering/RenderHTMLCanvas.cpp
    rendering/RenderIFrame.cpp
    rendering/RenderImage.cpp
    rendering/RenderImageResource.cpp
    rendering/RenderImageResourceStyleImage.cpp
    rendering/RenderInline.cpp
    rendering/RenderLayer.cpp
    rendering/RenderLayerBacking.cpp
    rendering/RenderLayerCompositor.cpp
    rendering/RenderLayerFilterInfo.cpp
    rendering/RenderLayerModelObject.cpp
    rendering/RenderLineBoxList.cpp
    rendering/RenderLineBreak.cpp
    rendering/RenderListBox.cpp
    rendering/RenderListItem.cpp
    rendering/RenderListMarker.cpp
    rendering/RenderMarquee.cpp
    rendering/RenderMedia.cpp
    rendering/RenderMediaControlElements.cpp
    rendering/RenderMediaControls.cpp
    rendering/RenderMenuList.cpp
    rendering/RenderMeter.cpp
    rendering/RenderMultiColumnFlowThread.cpp
    rendering/RenderMultiColumnSet.cpp
    rendering/RenderMultiColumnSpannerPlaceholder.cpp
    rendering/RenderNamedFlowFragment.cpp
    rendering/RenderNamedFlowThread.cpp
    rendering/RenderObject.cpp
    rendering/RenderProgress.cpp
    rendering/RenderQuote.cpp
    rendering/RenderRegion.cpp
    rendering/RenderRegionSet.cpp
    rendering/RenderReplaced.cpp
    rendering/RenderReplica.cpp
    rendering/RenderRuby.cpp
    rendering/RenderRubyBase.cpp
    rendering/RenderRubyRun.cpp
    rendering/RenderRubyText.cpp
    rendering/RenderScrollbar.cpp
    rendering/RenderScrollbarPart.cpp
    rendering/RenderScrollbarTheme.cpp
    rendering/RenderSearchField.cpp
    rendering/RenderSelectionInfo.cpp
    rendering/RenderSlider.cpp
    rendering/RenderSnapshottedPlugIn.cpp
    rendering/RenderTable.cpp
    rendering/RenderTableCaption.cpp
    rendering/RenderTableCell.cpp
    rendering/RenderTableCol.cpp
    rendering/RenderTableRow.cpp
    rendering/RenderTableSection.cpp
    rendering/RenderText.cpp
    rendering/RenderTextControl.cpp
    rendering/RenderTextControlMultiLine.cpp
    rendering/RenderTextControlSingleLine.cpp
    rendering/RenderTextFragment.cpp
    rendering/RenderTextLineBoxes.cpp
    rendering/RenderTheme.cpp
    rendering/RenderTreeAsText.cpp
    rendering/RenderVTTCue.cpp
    rendering/RenderVideo.cpp
    rendering/RenderView.cpp
    rendering/RenderWidget.cpp
    rendering/RootInlineBox.cpp
    rendering/ScrollBehavior.cpp
    rendering/SelectionSubtreeRoot.cpp
    rendering/SimpleLineLayout.cpp
    rendering/SimpleLineLayoutFlowContents.cpp
    rendering/SimpleLineLayoutFunctions.cpp
    rendering/SimpleLineLayoutResolver.cpp
    rendering/SimpleLineLayoutTextFragmentIterator.cpp
    rendering/TextAutosizer.cpp
    rendering/TextDecorationPainter.cpp
    rendering/TextPaintStyle.cpp
    rendering/TextPainter.cpp
    rendering/break_lines.cpp

    rendering/line/LineBreaker.cpp
    rendering/line/LineInfo.cpp
    rendering/line/LineWidth.cpp
    rendering/line/TrailingObjects.cpp

    rendering/mathml/MathMLOperatorDictionary.cpp
    rendering/mathml/RenderMathMLBlock.cpp
    rendering/mathml/RenderMathMLFenced.cpp
    rendering/mathml/RenderMathMLFraction.cpp
    rendering/mathml/RenderMathMLMath.cpp
    rendering/mathml/RenderMathMLMenclose.cpp
    rendering/mathml/RenderMathMLOperator.cpp
    rendering/mathml/RenderMathMLRadicalOperator.cpp
    rendering/mathml/RenderMathMLRoot.cpp
    rendering/mathml/RenderMathMLRow.cpp
    rendering/mathml/RenderMathMLScripts.cpp
    rendering/mathml/RenderMathMLSpace.cpp
    rendering/mathml/RenderMathMLSquareRoot.cpp
    rendering/mathml/RenderMathMLToken.cpp
    rendering/mathml/RenderMathMLUnderOver.cpp

    rendering/shapes/BoxShape.cpp
    rendering/shapes/PolygonShape.cpp
    rendering/shapes/RasterShape.cpp
    rendering/shapes/RectangleShape.cpp
    rendering/shapes/Shape.cpp
    rendering/shapes/ShapeOutsideInfo.cpp

    rendering/style/BasicShapes.cpp
    rendering/style/ContentData.cpp
    rendering/style/CounterDirectives.cpp
    rendering/style/FillLayer.cpp
    rendering/style/GridResolvedPosition.cpp
    rendering/style/KeyframeList.cpp
    rendering/style/NinePieceImage.cpp
    rendering/style/QuotesData.cpp
    rendering/style/RenderStyle.cpp
    rendering/style/RenderStyleConstants.cpp
    rendering/style/SVGRenderStyle.cpp
    rendering/style/SVGRenderStyleDefs.cpp
    rendering/style/ShadowData.cpp
    rendering/style/ShapeValue.cpp
    rendering/style/StyleBackgroundData.cpp
    rendering/style/StyleBoxData.cpp
    rendering/style/StyleCachedImage.cpp
    rendering/style/StyleCachedImageSet.cpp
    rendering/style/StyleDeprecatedFlexibleBoxData.cpp
    rendering/style/StyleFilterData.cpp
    rendering/style/StyleFlexibleBoxData.cpp
    rendering/style/StyleGeneratedImage.cpp
    rendering/style/StyleGridData.cpp
    rendering/style/StyleGridItemData.cpp
    rendering/style/StyleInheritedData.cpp
    rendering/style/StyleMarqueeData.cpp
    rendering/style/StyleMultiColData.cpp
    rendering/style/StyleRareInheritedData.cpp
    rendering/style/StyleRareNonInheritedData.cpp
    rendering/style/StyleScrollSnapPoints.cpp
    rendering/style/StyleSurroundData.cpp
    rendering/style/StyleTransformData.cpp
    rendering/style/StyleVisualData.cpp
    rendering/style/WillChangeData.cpp

    rendering/svg/RenderSVGBlock.cpp
    rendering/svg/RenderSVGContainer.cpp
    rendering/svg/RenderSVGEllipse.cpp
    rendering/svg/RenderSVGForeignObject.cpp
    rendering/svg/RenderSVGGradientStop.cpp
    rendering/svg/RenderSVGHiddenContainer.cpp
    rendering/svg/RenderSVGImage.cpp
    rendering/svg/RenderSVGInline.cpp
    rendering/svg/RenderSVGInlineText.cpp
    rendering/svg/RenderSVGModelObject.cpp
    rendering/svg/RenderSVGPath.cpp
    rendering/svg/RenderSVGRect.cpp
    rendering/svg/RenderSVGResource.cpp
    rendering/svg/RenderSVGResourceClipper.cpp
    rendering/svg/RenderSVGResourceContainer.cpp
    rendering/svg/RenderSVGResourceFilter.cpp
    rendering/svg/RenderSVGResourceFilterPrimitive.cpp
    rendering/svg/RenderSVGResourceGradient.cpp
    rendering/svg/RenderSVGResourceLinearGradient.cpp
    rendering/svg/RenderSVGResourceMarker.cpp
    rendering/svg/RenderSVGResourceMasker.cpp
    rendering/svg/RenderSVGResourcePattern.cpp
    rendering/svg/RenderSVGResourceRadialGradient.cpp
    rendering/svg/RenderSVGResourceSolidColor.cpp
    rendering/svg/RenderSVGRoot.cpp
    rendering/svg/RenderSVGShape.cpp
    rendering/svg/RenderSVGText.cpp
    rendering/svg/RenderSVGTextPath.cpp
    rendering/svg/RenderSVGTransformableContainer.cpp
    rendering/svg/RenderSVGViewportContainer.cpp
    rendering/svg/SVGInlineFlowBox.cpp
    rendering/svg/SVGInlineTextBox.cpp
    rendering/svg/SVGPathData.cpp
    rendering/svg/SVGRenderSupport.cpp
    rendering/svg/SVGRenderTreeAsText.cpp
    rendering/svg/SVGRenderingContext.cpp
    rendering/svg/SVGResources.cpp
    rendering/svg/SVGResourcesCache.cpp
    rendering/svg/SVGResourcesCycleSolver.cpp
    rendering/svg/SVGRootInlineBox.cpp
    rendering/svg/SVGTextChunk.cpp
    rendering/svg/SVGTextChunkBuilder.cpp
    rendering/svg/SVGTextLayoutAttributes.cpp
    rendering/svg/SVGTextLayoutAttributesBuilder.cpp
    rendering/svg/SVGTextLayoutEngine.cpp
    rendering/svg/SVGTextLayoutEngineBaseline.cpp
    rendering/svg/SVGTextLayoutEngineSpacing.cpp
    rendering/svg/SVGTextMetrics.cpp
    rendering/svg/SVGTextMetricsBuilder.cpp
    rendering/svg/SVGTextQuery.cpp
    rendering/svg/SVGTextRunRenderingContext.cpp

    replay/UserInputBridge.cpp

    storage/StorageNamespaceProvider.cpp

    style/AttributeChangeInvalidation.cpp
    style/ClassChangeInvalidation.cpp
    style/IdChangeInvalidation.cpp
    style/InlineTextBoxStyle.cpp
    style/RenderTreePosition.cpp
    style/StyleChange.cpp
    style/StyleFontSizeFunctions.cpp
    style/StyleResolveForDocument.cpp
    style/StyleSharingResolver.cpp
    style/StyleTreeResolver.cpp

    svg/SVGAElement.cpp
    svg/SVGAltGlyphDefElement.cpp
    svg/SVGAltGlyphElement.cpp
    svg/SVGAltGlyphItemElement.cpp
    svg/SVGAngle.cpp
    svg/SVGAnimateColorElement.cpp
    svg/SVGAnimateElement.cpp
    svg/SVGAnimateElementBase.cpp
    svg/SVGAnimateMotionElement.cpp
    svg/SVGAnimateTransformElement.cpp
    svg/SVGAnimatedAngle.cpp
    svg/SVGAnimatedBoolean.cpp
    svg/SVGAnimatedColor.cpp
    svg/SVGAnimatedEnumeration.cpp
    svg/SVGAnimatedInteger.cpp
    svg/SVGAnimatedIntegerOptionalInteger.cpp
    svg/SVGAnimatedLength.cpp
    svg/SVGAnimatedLengthList.cpp
    svg/SVGAnimatedNumber.cpp
    svg/SVGAnimatedNumberList.cpp
    svg/SVGAnimatedNumberOptionalNumber.cpp
    svg/SVGAnimatedPath.cpp
    svg/SVGAnimatedPointList.cpp
    svg/SVGAnimatedPreserveAspectRatio.cpp
    svg/SVGAnimatedRect.cpp
    svg/SVGAnimatedString.cpp
    svg/SVGAnimatedTransformList.cpp
    svg/SVGAnimatedType.cpp
    svg/SVGAnimatedTypeAnimator.cpp
    svg/SVGAnimationElement.cpp
    svg/SVGCircleElement.cpp
    svg/SVGClipPathElement.cpp
    svg/SVGColor.cpp
    svg/SVGComponentTransferFunctionElement.cpp
    svg/SVGCursorElement.cpp
    svg/SVGDefsElement.cpp
    svg/SVGDescElement.cpp
    svg/SVGDocument.cpp
    svg/SVGDocumentExtensions.cpp
    svg/SVGElement.cpp
    svg/SVGEllipseElement.cpp
    svg/SVGException.cpp
    svg/SVGExternalResourcesRequired.cpp
    svg/SVGFEBlendElement.cpp
    svg/SVGFEColorMatrixElement.cpp
    svg/SVGFEComponentTransferElement.cpp
    svg/SVGFECompositeElement.cpp
    svg/SVGFEConvolveMatrixElement.cpp
    svg/SVGFEDiffuseLightingElement.cpp
    svg/SVGFEDisplacementMapElement.cpp
    svg/SVGFEDistantLightElement.cpp
    svg/SVGFEDropShadowElement.cpp
    svg/SVGFEFloodElement.cpp
    svg/SVGFEFuncAElement.cpp
    svg/SVGFEFuncBElement.cpp
    svg/SVGFEFuncGElement.cpp
    svg/SVGFEFuncRElement.cpp
    svg/SVGFEGaussianBlurElement.cpp
    svg/SVGFEImageElement.cpp
    svg/SVGFELightElement.cpp
    svg/SVGFEMergeElement.cpp
    svg/SVGFEMergeNodeElement.cpp
    svg/SVGFEMorphologyElement.cpp
    svg/SVGFEOffsetElement.cpp
    svg/SVGFEPointLightElement.cpp
    svg/SVGFESpecularLightingElement.cpp
    svg/SVGFESpotLightElement.cpp
    svg/SVGFETileElement.cpp
    svg/SVGFETurbulenceElement.cpp
    svg/SVGFilterElement.cpp
    svg/SVGFilterPrimitiveStandardAttributes.cpp
    svg/SVGFitToViewBox.cpp
    svg/SVGFontData.cpp
    svg/SVGFontElement.cpp
    svg/SVGFontFaceElement.cpp
    svg/SVGFontFaceFormatElement.cpp
    svg/SVGFontFaceNameElement.cpp
    svg/SVGFontFaceSrcElement.cpp
    svg/SVGFontFaceUriElement.cpp
    svg/SVGForeignObjectElement.cpp
    svg/SVGGElement.cpp
    svg/SVGGlyphElement.cpp
    svg/SVGGlyphRefElement.cpp
    svg/SVGGradientElement.cpp
    svg/SVGGraphicsElement.cpp
    svg/SVGHKernElement.cpp
    svg/SVGImageElement.cpp
    svg/SVGImageLoader.cpp
    svg/SVGLangSpace.cpp
    svg/SVGLength.cpp
    svg/SVGLengthContext.cpp
    svg/SVGLengthList.cpp
    svg/SVGLineElement.cpp
    svg/SVGLinearGradientElement.cpp
    svg/SVGLocatable.cpp
    svg/SVGMPathElement.cpp
    svg/SVGMarkerElement.cpp
    svg/SVGMaskElement.cpp
    svg/SVGMetadataElement.cpp
    svg/SVGMissingGlyphElement.cpp
    svg/SVGNumberList.cpp
    svg/SVGPaint.cpp
    svg/SVGParserUtilities.cpp
    svg/SVGPathBlender.cpp
    svg/SVGPathBuilder.cpp
    svg/SVGPathByteStreamBuilder.cpp
    svg/SVGPathByteStreamSource.cpp
    svg/SVGPathElement.cpp
    svg/SVGPathParser.cpp
    svg/SVGPathSegList.cpp
    svg/SVGPathSegListBuilder.cpp
    svg/SVGPathSegListSource.cpp
    svg/SVGPathStringBuilder.cpp
    svg/SVGPathStringSource.cpp
    svg/SVGPathTraversalStateBuilder.cpp
    svg/SVGPathUtilities.cpp
    svg/SVGPatternElement.cpp
    svg/SVGPointList.cpp
    svg/SVGPolyElement.cpp
    svg/SVGPolygonElement.cpp
    svg/SVGPolylineElement.cpp
    svg/SVGPreserveAspectRatio.cpp
    svg/SVGRadialGradientElement.cpp
    svg/SVGRectElement.cpp
    svg/SVGSVGElement.cpp
    svg/SVGScriptElement.cpp
    svg/SVGSetElement.cpp
    svg/SVGStopElement.cpp
    svg/SVGStringList.cpp
    svg/SVGStyleElement.cpp
    svg/SVGSwitchElement.cpp
    svg/SVGSymbolElement.cpp
    svg/SVGTRefElement.cpp
    svg/SVGTSpanElement.cpp
    svg/SVGTests.cpp
    svg/SVGTextContentElement.cpp
    svg/SVGTextElement.cpp
    svg/SVGTextPathElement.cpp
    svg/SVGTextPositioningElement.cpp
    svg/SVGTitleElement.cpp
    svg/SVGToOTFFontConversion.cpp
    svg/SVGTransform.cpp
    svg/SVGTransformDistance.cpp
    svg/SVGTransformList.cpp
    svg/SVGTransformable.cpp
    svg/SVGURIReference.cpp
    svg/SVGUseElement.cpp
    svg/SVGVKernElement.cpp
    svg/SVGViewElement.cpp
    svg/SVGViewSpec.cpp
    svg/SVGZoomAndPan.cpp
    svg/SVGZoomEvent.cpp

    svg/animation/SMILTime.cpp
    svg/animation/SMILTimeContainer.cpp
    svg/animation/SVGSMILElement.cpp

    svg/graphics/SVGImage.cpp
    svg/graphics/SVGImageCache.cpp
    svg/graphics/SVGImageForContainer.cpp

    svg/graphics/filters/SVGFEImage.cpp
    svg/graphics/filters/SVGFilter.cpp
    svg/graphics/filters/SVGFilterBuilder.cpp

    svg/properties/SVGAnimatedProperty.cpp
    svg/properties/SVGAttributeToPropertyMap.cpp
    svg/properties/SVGPathSegListPropertyTearOff.cpp
    svg/properties/SVGPropertyInfo.cpp

    xml/DOMParser.cpp
    xml/NativeXPathNSResolver.cpp
    xml/XMLErrors.cpp
    xml/XMLSerializer.cpp
    xml/XPathEvaluator.cpp
    xml/XPathExpression.cpp
    xml/XPathExpressionNode.cpp
    xml/XPathFunctions.cpp
    xml/XPathNSResolver.cpp
    xml/XPathNodeSet.cpp
    xml/XPathParser.cpp
    xml/XPathPath.cpp
    xml/XPathPredicate.cpp
    xml/XPathResult.cpp
    xml/XPathStep.cpp
    xml/XPathUtil.cpp
    xml/XPathValue.cpp
    xml/XPathVariableReference.cpp
    xml/XSLImportRule.cpp
    xml/XSLStyleSheetLibxslt.cpp
    xml/XSLTExtensions.cpp
    xml/XSLTProcessor.cpp
    xml/XSLTProcessorLibxslt.cpp
    xml/XSLTUnicodeSort.cpp

    xml/parser/XMLDocumentParser.cpp
    xml/parser/XMLDocumentParserLibxml2.cpp
    xml/parser/XMLDocumentParserScope.cpp
)

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad"
    )

    list(APPEND WebCore_IDL_INCLUDES
        Modules/gamepad
    )

    list(APPEND WebCore_SOURCES
        Modules/gamepad/Gamepad.cpp
        Modules/gamepad/GamepadButton.cpp
        Modules/gamepad/GamepadEvent.cpp
        Modules/gamepad/GamepadManager.cpp
        Modules/gamepad/NavigatorGamepad.cpp

        platform/GamepadProvider.cpp
    )
elseif (ENABLE_GAMEPAD_DEPRECATED)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad/deprecated"
    )

    list(APPEND WebCore_IDL_INCLUDES
        Modules/gamepad/deprecated
    )

    # Modules/gamepad/deprecated/*.idl removed - the deprecated navigator.webkitGetGamepads()
    # JS binding is gone permanently (Bucket 4 Phase 4, batch 13). The C++ classes stay, guarded
    # by ENABLE(GAMEPAD_DEPRECATED), so re-enabling the flag gives pure-C++ deprecated-gamepad
    # support with no JS surface.
    list(APPEND WebCore_SOURCES
        Modules/gamepad/deprecated/Gamepad.cpp
        Modules/gamepad/deprecated/GamepadList.cpp
        Modules/gamepad/deprecated/NavigatorGamepad.cpp
    )
endif ()

set(WebCore_SVG_IDL_FILES
)

set(WebCore_IDL_FILES
    ${WebCore_NON_SVG_IDL_FILES}
    ${WebCore_SVG_IDL_FILES}
)

if (ENABLE_ALLINONE_BUILD)
    message(STATUS "All-in-One build is enabled.")
    set(allInOnes
        accessibility/AccessibilityAllInOne.cpp

        css/CSSAllInOne.cpp

        dom/DOMAllInOne.cpp

        editing/EditingAllInOne.cpp

        html/HTMLElementsAllInOne.cpp

        mathml/MathMLAllInOne.cpp

        platform/text/TextAllInOne.cpp

        rendering/RenderingAllInOne.cpp

        rendering/style/StyleAllInOne.cpp

        rendering/svg/RenderSVGAllInOne.cpp

        svg/SVGAllInOne.cpp
    )
    foreach (allInOne ${allInOnes})
        PROCESS_ALLINONE_FILE("${WebCore_SOURCES}" ${allInOne} newSources ${AllInOne})
        set(WebCore_SOURCES
            ${allInOne}
            ${newSources}
        )
    endforeach ()
endif ()

set(WebCore_CSS_PROPERTY_NAMES
    ${WEBCORE_DIR}/css/CSSPropertyNames.in
)

set(WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/CSSValueKeywords.in
)

set(WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/html.css
    ${WEBCORE_DIR}/css/mathml.css
    ${WEBCORE_DIR}/css/mediaControls.css
    ${WEBCORE_DIR}/css/quirks.css
    ${WEBCORE_DIR}/css/svg.css
)

set(WebCore_LIBRARIES
    ${ICU_LIBRARIES}
    WTF${DEBUG_SUFFIX}
)
# hipecore: JavaScriptCore${DEBUG_SUFFIX} dropped from the link (Bucket 5.6). The
# typed-array + ConsoleTypes headers WebCore still pulls from <runtime/*> are
# header-only; there is no libJavaScriptCore symbol dependency any more.

if (ENABLE_ENCRYPTED_MEDIA OR ENABLE_ENCRYPTED_MEDIA_V2)
    list(APPEND WebCore_SOURCES
        Modules/encryptedmedia/CDM.cpp
        Modules/encryptedmedia/CDMPrivateClearKey.cpp
        Modules/encryptedmedia/CDMPrivateMediaPlayer.cpp
        Modules/encryptedmedia/CDMSessionClearKey.cpp
        Modules/encryptedmedia/MediaKeyMessageEvent.cpp
        Modules/encryptedmedia/MediaKeyNeededEvent.cpp
        Modules/encryptedmedia/MediaKeySession.cpp
        Modules/encryptedmedia/MediaKeys.cpp

        html/MediaKeyEvent.cpp
    )
endif ()

list(APPEND WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/SVGCSSValueKeywords.in
)

if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        html/canvas/ANGLEInstancedArrays.cpp
        html/canvas/EXTBlendMinMax.cpp
        html/canvas/EXTFragDepth.cpp
        html/canvas/EXTShaderTextureLOD.cpp
        html/canvas/EXTTextureFilterAnisotropic.cpp
        html/canvas/EXTsRGB.cpp
        html/canvas/OESElementIndexUint.cpp
        html/canvas/OESStandardDerivatives.cpp
        html/canvas/OESTextureFloat.cpp
        html/canvas/OESTextureFloatLinear.cpp
        html/canvas/OESTextureHalfFloat.cpp
        html/canvas/OESTextureHalfFloatLinear.cpp
        html/canvas/OESVertexArrayObject.cpp
        html/canvas/WebGL2RenderingContext.cpp
        html/canvas/WebGLBuffer.cpp
        html/canvas/WebGLCompressedTextureATC.cpp
        html/canvas/WebGLCompressedTexturePVRTC.cpp
        html/canvas/WebGLCompressedTextureS3TC.cpp
        html/canvas/WebGLContextAttributes.cpp
        html/canvas/WebGLContextEvent.cpp
        html/canvas/WebGLContextGroup.cpp
        html/canvas/WebGLContextObject.cpp
        html/canvas/WebGLDebugRendererInfo.cpp
        html/canvas/WebGLDebugShaders.cpp
        html/canvas/WebGLDepthTexture.cpp
        html/canvas/WebGLDrawBuffers.cpp
        html/canvas/WebGLExtension.cpp
        html/canvas/WebGLFramebuffer.cpp
        html/canvas/WebGLGetInfo.cpp
        html/canvas/WebGLLoseContext.cpp
        html/canvas/WebGLObject.cpp
        html/canvas/WebGLProgram.cpp
        html/canvas/WebGLQuery.cpp
        html/canvas/WebGLRenderbuffer.cpp
        html/canvas/WebGLRenderingContext.cpp
        html/canvas/WebGLRenderingContextBase.cpp
        html/canvas/WebGLSampler.cpp
        html/canvas/WebGLShader.cpp
        html/canvas/WebGLShaderPrecisionFormat.cpp
        html/canvas/WebGLSharedObject.cpp
        html/canvas/WebGLSync.cpp
        html/canvas/WebGLTexture.cpp
        html/canvas/WebGLTransformFeedback.cpp
        html/canvas/WebGLUniformLocation.cpp
        html/canvas/WebGLVertexArrayObject.cpp
        html/canvas/WebGLVertexArrayObjectBase.cpp
        html/canvas/WebGLVertexArrayObjectOES.cpp
    )
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebCore_SOURCES
        html/track/AudioTrack.cpp
        html/track/AudioTrackList.cpp
        html/track/BufferedLineReader.cpp
        html/track/DataCue.cpp
        html/track/InbandDataTextTrack.cpp
        html/track/InbandGenericTextTrack.cpp
        html/track/InbandTextTrack.cpp
        html/track/InbandWebVTTTextTrack.cpp
        html/track/LoadableTextTrack.cpp
        html/track/TextTrack.cpp
        html/track/TextTrackCue.cpp
        html/track/TextTrackCueGeneric.cpp
        html/track/TextTrackCueList.cpp
        html/track/TextTrackList.cpp
        html/track/TrackBase.cpp
        html/track/TrackEvent.cpp
        html/track/TrackListBase.cpp
        html/track/VTTCue.cpp
        html/track/VTTRegion.cpp
        html/track/VTTRegionList.cpp
        html/track/VTTScanner.cpp
        html/track/VideoTrack.cpp
        html/track/VideoTrackList.cpp
        html/track/WebVTTElement.cpp
        html/track/WebVTTParser.cpp
        html/track/WebVTTTokenizer.cpp

        loader/cache/CachedTextTrack.cpp

        platform/graphics/TextTrackRepresentation.cpp
    )
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D)
    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_OPENGL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${OPENGL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${OPENGL_LIBRARIES}
        )
        add_definitions(${OPENGL_DEFINITIONS})
    elseif (USE_OPENGL_ES_2)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${OPENGLES2_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${OPENGLES2_LIBRARIES}
        )
        add_definitions(${OPENGLES2_DEFINITIONS})
    endif ()

    if (USE_EGL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${EGL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${EGL_LIBRARIES}
        )
        add_definitions(${EGL_DEFINITIONS})

        list(APPEND WebCore_LIBRARIES GLESv2 EGL)
        #fix linker under buildroot not linking to correct libraries.
    endif ()

    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gpu"
    )

    list(APPEND ANGLESupport_SOURCES
        ${THIRDPARTY_DIR}/ANGLE/src/common/angleutils.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/debug.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/event_tracer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/mathutil.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/MemoryBuffer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/utilities.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/tls.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ArrayReturnValueToOutParameter.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ASTMetadataHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayout.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayoutHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CallDAG.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CodeGen.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Compiler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Diagnostics.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DirectiveHandler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/EmulatePrecision.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_lex.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_tab.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ForLoopUnroll.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/FlagStd140Structs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InfoSink.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Initialize.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeDll.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeParseContext.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeVariables.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Intermediate.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermTraverse.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermNode.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/intermOut.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/LoopInfo.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Operator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSLBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/parseConst.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ParseContext.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PoolAlloc.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PruneEmptyDeclarations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/QualifierAlive.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RegenerateStructNames.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveSwitchFallThrough.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteElseBlocks.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ScalarizeVecAndMatConstructorArgs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SearchSymbol.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateArrayInitialization.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateDeclarations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderLang.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderVars.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SimplifyArrayAssignment.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/StructureHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SymbolTable.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Types.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuitAST.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuit.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UniformHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/util.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UtilsHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateLimitations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateOutputs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateSwitch.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VariableInfo.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VariablePacker.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VersionGLSL.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/depgraph/DependencyGraph.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/depgraph/DependencyGraphBuilder.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/depgraph/DependencyGraphOutput.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/depgraph/DependencyGraphTraverse.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DiagnosticsBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveHandlerBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/ExpressionParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Input.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Lexer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Macro.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/MacroExpander.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Preprocessor.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Token.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Tokenizer.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/timing/RestrictFragmentShaderTiming.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/timing/RestrictVertexShaderTiming.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/libANGLE/Platform.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/third_party/compiler/ArrayBoundsClamper.cpp
    )

    list(APPEND WebCore_SOURCES
        platform/graphics/ANGLEWebKitBridge.cpp
        platform/graphics/FormatConverter.cpp
        platform/graphics/GraphicsContext3D.cpp
    )
endif ()

if (USE_WOFF2)
    if (WOFF2DEC_FOUND)
        list(APPEND WebCore_INCLUDE_DIRECTORIES "${WOFF2DEC_INCLUDE_DIRS}")
        list(APPEND WebCore_LIBRARIES "${WOFF2DEC_LIBRARIES}")
    else ()
        list(APPEND WebCore_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/woff2/include")
        list(APPEND WebCore_LIBRARIES woff2)
    endif ()
endif ()

# hipecore: the WebCoreTestSupport library (window.internals / internals.settings - the
# JS-only layout-test introspection API) is gone. It was reachable only through JavaScript
# or DumpRenderTree/WebKitTestRunner, all removed. Source/WebCore/testing/ and the
# test-only platform/mock/ files are deleted; ENABLE_TEST_SUPPORT no longer exists.

# Modules that the bindings generator scripts may use
set(SCRIPTS_RESOLVE_SUPPLEMENTAL
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
)
set(SCRIPTS_BINDINGS
    ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
    ${WEBCORE_DIR}/bindings/scripts/CodeGeneratorJS.pm
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
    ${WEBCORE_DIR}/bindings/scripts/InFilesCompiler.pm
    ${WEBCORE_DIR}/bindings/scripts/InFilesParser.pm
    ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
)

set(MAKE_NAMES_DEPENDENCIES
    ${WEBCORE_DIR}/bindings/scripts/Hasher.pm
    ${WEBCORE_DIR}/bindings/scripts/StaticString.pm
)
set(IDL_FILES_TMP ${DERIVED_SOURCES_WEBCORE_DIR}/idl_files.tmp)
set(SUPPLEMENTAL_DEPENDENCY_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/supplemental_dependency.tmp)
set(WINDOW_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowConstructors.idl)
set(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/WorkerGlobalScopeConstructors.idl)
set(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DedicatedWorkerGlobalScopeConstructors.idl)
set(IDL_ATTRIBUTES_FILE ${WEBCORE_DIR}/bindings/scripts/IDLAttributes.txt)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()


MAKE_HASH_TOOLS(${WEBCORE_DIR}/platform/ColorData)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/ColorData.cpp)


# Generate XMLViewerCSS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    MAIN_DEPENDENCY xml/XMLViewer.css
    DEPENDS ${WebCore_SCRIPTS_DIR}/xxd.pl ${WebCore_SCRIPTS_DIR}/cssmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${WebCore_SCRIPTS_DIR}/cssmin.py < ${WEBCORE_DIR}/xml/XMLViewer.css > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    COMMAND ${PERL_EXECUTABLE} ${WebCore_SCRIPTS_DIR}/xxd.pl XMLViewer_css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h)


# Generate XMLViewerJS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    MAIN_DEPENDENCY xml/XMLViewer.js
    DEPENDS ${WebCore_SCRIPTS_DIR}/xxd.pl ${WebCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${WebCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/xml/XMLViewer.js > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    COMMAND ${PERL_EXECUTABLE} ${WebCore_SCRIPTS_DIR}/xxd.pl XMLViewer_js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h)


# Generate HTML entity table
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    DEPENDS ${WEBCORE_DIR}/html/parser/create-html-entity-table
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/html/parser/create-html-entity-table -o ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp)

# Generate HTTP header names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in
    DEPENDS ${WEBCORE_DIR}/platform/network/create-http-header-name-table
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/platform/network/create-http-header-name-table ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp)

# Generate CSS property names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.in ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.h ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeprop.pl
    DEPENDS ${WebCore_CSS_PROPERTY_NAMES}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_PROPERTY_NAMES} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.in
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/css/makeprop.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --gperf-executable "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${DERIVED_SOURCES_WEBCORE_DIR}/CSSGrammar.cpp CSSPropertyNames.h)


# Generate CSS value keywords
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.gperf
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makevalues.pl
    DEPENDS ${WebCore_CSS_VALUE_KEYWORDS}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_VALUE_KEYWORDS} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/css/makevalues.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSPrimitiveValueMappings.h CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${DERIVED_SOURCES_WEBCORE_DIR}/CSSGrammar.cpp CSSValueKeywords.h)

# Generate CSS Selector pseudo type name to value maps.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py
    DEPENDS ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py
    DEPENDS ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp)

# Generate user agent styles
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_USER_AGENT_STYLE_SHEETS} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${WebCore_USER_AGENT_STYLE_SHEETS}
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp UserAgentStyleSheetsData.cpp UserAgentStyleSheets.h)

if (WebCore_USER_AGENT_SCRIPTS)
    # Necessary variables:
    # WebCore_USER_AGENT_SCRIPTS containing the JavaScript sources list
    # WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES containing the source file that will load the scripts to add the proper
    #   dependency and having them built at the right moment

    MAKE_JS_FILE_ARRAYS(
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScriptsData.cpp
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.h
        WebCore
        WebCore_USER_AGENT_SCRIPTS
        WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES
    )
    list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScriptsData.cpp)
endif ()

GENERATE_GRAMMAR(cssyy ${WEBCORE_DIR}/css/CSSGrammar.y.in ${DERIVED_SOURCES_WEBCORE_DIR}/CSSGrammar.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSGrammar.cpp "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSGrammar.cpp)

GENERATE_GRAMMAR(xpathyy ${WEBCORE_DIR}/xml/XPathGrammar.y ${DERIVED_SOURCES_WEBCORE_DIR}/XPathGrammar.h ${DERIVED_SOURCES_WEBCORE_DIR}/XPathGrammar.cpp "")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XPathGrammar.cpp)


set(IDL_INCLUDES "")
foreach (_include ${WebCore_IDL_INCLUDES})
    list(APPEND IDL_INCLUDES --include=${WEBCORE_DIR}/${_include})
endforeach ()

set(FEATURE_DEFINES_JAVASCRIPT "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")

# Create JavaScript C++ code given an IDL input
foreach (_idl ${WebCore_IDL_FILES})
    set(IDL_FILES_LIST "${IDL_FILES_LIST}${WEBCORE_DIR}/${_idl}\n")
endforeach ()

foreach (_idl ${ObjC_Bindings_IDL_FILES})
    set(IDL_FILES_LIST "${IDL_FILES_LIST}${WEBCORE_DIR}/${_idl}\n")
endforeach ()

file(WRITE ${IDL_FILES_TMP} ${IDL_FILES_LIST})

add_custom_command(
    OUTPUT ${SUPPLEMENTAL_DEPENDENCY_FILE} ${WINDOW_CONSTRUCTORS_FILE} ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE} ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    DEPENDS ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl ${SCRIPTS_PREPROCESS_IDLS} ${WebCore_IDL_FILES} ${ObjC_Bindings_IDL_FILES} ${WEBCORE_DIR}/CMakeLists.txt
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/bindings/scripts/preprocess-idls.pl --defines "${FEATURE_DEFINES_JAVASCRIPT}" --idlFilesList ${IDL_FILES_TMP} --supplementalDependencyFile ${SUPPLEMENTAL_DEPENDENCY_FILE} --windowConstructorsFile ${WINDOW_CONSTRUCTORS_FILE} --workerGlobalScopeConstructorsFile ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
--dedicatedWorkerGlobalScopeConstructorsFile ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    VERBATIM)

set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
)

GENERATE_BINDINGS(WebCore_SOURCES
    ""
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${FEATURE_DEFINES_JAVASCRIPT}"
    ${DERIVED_SOURCES_WEBCORE_DIR} JS JS cpp
    ${IDL_ATTRIBUTES_FILE}
    ${SUPPLEMENTAL_DEPENDENCY_FILE}
    ${ADDITIONAL_BINDINGS_DEPENDENCIES})


ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/html/HTMLTreeBuilder.cpp MathMLNames.cpp)


GENERATE_DOM_NAMES(HTML ${WEBCORE_DIR}/html/HTMLAttributeNames.in ${WEBCORE_DIR}/html/HTMLTagNames.in "")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLElementFactory.cpp)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventNames.in EventInterfaces.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventInterfaces.h)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventTargetFactory.in EventTargetInterfaces.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/dom/EventNames.cpp EventTargetInterfaces.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/dom/DOMAllInOne.cpp EventTargetInterfaces.h)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventTargetInterfaces.h)


GENERATE_EXCEPTION_CODE_DESCRIPTION(${WEBCORE_DIR}/dom/DOMExceptions.in ExceptionCodeDescription.cpp)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/ExceptionCodeDescription.cpp)


GENERATE_SETTINGS_MACROS(${WEBCORE_DIR}/page/Settings.in SettingsMacros.h)
# hipecore: page/Settings.h #includes the generated SettingsMacros.h. Upstream relied on
# the IDL-binding generation chain to force this to be generated; with JS bindings
# generation off (Bucket 4 Phase 4 Tier B) that chain is gone, so wire the header into the
# build graph directly - same pattern as EventInterfaces.h / EventTargetInterfaces.h above.
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SettingsMacros.h)


GENERATE_FONT_NAMES(${WEBCORE_DIR}/css/WebKitFontFamilyNames.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.cpp)


GENERATE_DOM_NAMES(MathML ${WEBCORE_DIR}/mathml/mathattrs.in ${WEBCORE_DIR}/mathml/mathtags.in "")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLNames.cpp)
if (ENABLE_MATHML)
    list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLElementFactory.cpp)
endif ()

# SVG extra defines need to map to a numerical value for correct preprocessing of svgtags.in.
set(SVG_EXTRA_DEFINES "")
if (ENABLE_SVG_FONTS)
    list(APPEND SVG_EXTRA_DEFINES "ENABLE_SVG_FONTS=1")
endif ()

GENERATE_DOM_NAMES(SVG ${WEBCORE_DIR}/svg/svgattrs.in ${WEBCORE_DIR}/svg/svgtags.in "${SVG_EXTRA_DEFINES}")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGNames.cpp)

list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGElementFactory.cpp)

GENERATE_DOM_NAMES(XLink ${WEBCORE_DIR}/svg/xlinkattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XLinkNames.cpp)


GENERATE_DOM_NAMES(XMLNS ${WEBCORE_DIR}/xml/xmlnsattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNSNames.cpp)


GENERATE_DOM_NAMES(XML ${WEBCORE_DIR}/xml/xmlattrs.in)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNames.cpp)

WEBKIT_WRAP_SOURCELIST(${WebCore_IDL_FILES} ${WebCore_SOURCES})

if (MSVC)
    ADD_PRECOMPILED_HEADER("WebCorePrefix.h" "WebCorePrefix.cpp" WebCore_SOURCES)
endif ()

WEBKIT_FRAMEWORK(WebCore)

if (NOT MSVC AND WIN32 AND ${PORT} STREQUAL "Qt")
    ADD_PREFIX_HEADER(WebCore "${WEBCORE_DIR}/WebCorePrefix.h")
endif ()

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    ADD_TARGET_PROPERTIES(WebCore COMPILE_FLAGS "-fno-tree-sra")
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D AND NOT ${PORT} STREQUAL "Win")
    add_library(ANGLESupport STATIC ${ANGLESupport_SOURCES})
    set_target_properties(ANGLESupport PROPERTIES FOLDER "WebCore")

    # Suppress null conversion warnings for sources in Source/ThirdParty/ANGLE
    if (COMPILER_IS_CLANG)
        ADD_TARGET_PROPERTIES(ANGLESupport COMPILE_FLAGS "-Wno-null-conversion")
    endif ()

    target_include_directories(ANGLESupport PRIVATE
        "${THIRDPARTY_DIR}/ANGLE/include"
        "${THIRDPARTY_DIR}/ANGLE/src"
    )
    list(APPEND WebCore_LIBRARIES ANGLESupport)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(ANGLESupport IGNORECXX_WARNINGS)
    QT_ADD_EXTRA_WEBKIT_TARGET_EXPORT(ANGLESupport)

    if (WIN32)
        ADD_TARGET_PROPERTIES(ANGLESupport COMPILE_DEFINITIONS ANGLE_WEBKIT_WIN=1)
    endif ()
endif ()

target_link_libraries(WebCore ${WebCore_LIBRARIES})

if (SHARED_CORE)
    set_target_properties(WebCore PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS WebCore DESTINATION "${LIB_INSTALL_DIR}")
endif ()

QT_ADD_EXTRA_WEBKIT_TARGET_EXPORT(WebCore)

# [ARM] Build SVGPathElement.cpp with -O2 due to a GCC bug
# https://bugs.webkit.org/show_bug.cgi?id=145377
if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND WTF_CPU_ARM AND (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.3"))
    set_source_files_properties(svg/SVGPathElement.cpp PROPERTIES COMPILE_FLAGS "-O2")
endif ()
