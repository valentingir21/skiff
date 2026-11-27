// SPDX-License-Identifier: GPL-3.0-only
// The editor's page: Gir Editor's own web build (dist/, zipped into the plugin), served by JUCE's resource provider
// (juce://juce.backend/…: no server, nothing from the Internet), in a WKWebView that lives as long as the plugin
// instance, not its window: closing the window keeps the page, its connection to the FM-1 and the automation going.
//   page -> plugin: window.__JUCE__.backend.emitEvent("gir", {type, …})
//   plugin -> page: window.__girRecv({type, …}) (evaluateJavascript: it works with the window closed, which JUCE's
//   events and native functions do not)
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

class WebUI : public juce::WebBrowserComponent
{
public:
    WebUI (const juce::var& initData, std::function<void (const juce::var&)> onMessage);

    /** to the page (queued until it says "ready") */
    void post (const juce::var& message);
    void pageReady();

    /** (the tests) the page as it is drawn, into a PNG; done(false) when it could not */
    void snapshot (const juce::File& png, std::function<void (bool)> done);
    /** no throttling of the page while the window is closed (WebUI.mm); true when this WebKit allowed it */
    bool keepAwake();

    // the page stays loaded and is not reloaded when the window opens or closes (JUCE's default reloads it on the
    // first show and blanks it when hidden: both would drop the connection)
    void parentHierarchyChanged() override {}
    void visibilityChanged() override {}
    /** only the editor's own pages here: a link elsewhere opens in the browser */
    bool pageAboutToLoad (const juce::String& url) override;
    void newWindowAttemptingToLoad (const juce::String& url) override;

    static std::optional<Resource> provide (const juce::String& path);
    bool awake = false;
private:
    bool ready = false;
    juce::Array<juce::var> pending;
};
