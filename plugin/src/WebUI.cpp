// SPDX-License-Identifier: GPL-3.0-only
#include "WebUI.h"
#include <BinaryData.h>

using namespace juce;

static WebBrowserComponent::Options options (const var& initData, std::function<void (const var&)> onMessage)
{
    return WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withInitialisationData ("gir", initData)
        .withEventListener ("gir", [cb = std::move (onMessage)] (const var& v) { cb (v); })
        .withResourceProvider ([] (const String& p) { return WebUI::provide (p); });
}

WebUI::WebUI (const var& initData, std::function<void (const var&)> onMessage)
    : WebBrowserComponent (options (initData, std::move (onMessage)))
{
    awake = keepAwake();
    goToURL (getResourceProviderRoot());
}

void WebUI::post (const var& m)
{
    if (! ready)
    {
        pending.add (m);
        return;
    }
    evaluateJavascript ("window.__girRecv && window.__girRecv(" + JSON::toString (m, true) + ")");
}

void WebUI::pageReady()
{
    ready = true;
    auto p = std::move (pending);
    pending.clear();
    for (auto& m : p)
        post (m);
}

bool WebUI::pageAboutToLoad (const String& url)
{
    if (url.startsWith (getResourceProviderRoot()) || url.startsWith ("about:"))
        return true;
    if (url.startsWithIgnoreCase ("https:") || url.startsWithIgnoreCase ("http:") || url.startsWithIgnoreCase ("mailto:"))
        URL (url).launchInDefaultBrowser();
    return false;
}

void WebUI::newWindowAttemptingToLoad (const String& url)
{
    if (url.startsWithIgnoreCase ("https:") || url.startsWithIgnoreCase ("http:"))
        URL (url).launchInDefaultBrowser();
}

static String mimeOf (const String& path)
{
    const auto e = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (e == "html") return "text/html";
    if (e == "js" || e == "mjs") return "text/javascript";
    if (e == "css") return "text/css";
    if (e == "wasm") return "application/wasm";
    if (e == "svg") return "image/svg+xml";
    if (e == "json") return "application/json";
    if (e == "png") return "image/png";
    if (e == "jpg" || e == "jpeg") return "image/jpeg";
    if (e == "woff2") return "font/woff2";
    if (e == "woff") return "font/woff";
    if (e == "wav") return "audio/wav";
    return "application/octet-stream";
}

std::optional<WebBrowserComponent::Resource> WebUI::provide (const String& rawPath)
{
    static std::unique_ptr<ZipFile> zip = [] {
        auto s = std::make_unique<MemoryInputStream> (BinaryData::web_zip, (size_t) BinaryData::web_zipSize, false);
        return std::make_unique<ZipFile> (s.release(), true);
    }();
    auto path = URL::removeEscapeChars (rawPath.upToFirstOccurrenceOf ("?", false, false)
                                               .upToFirstOccurrenceOf ("#", false, false)).trimCharactersAtStart ("/");
    if (path.isEmpty())
        path = "index.html";
    if (path.contains (".."))
        return std::nullopt;
    auto* entry = zip->getEntry (path);
    if (entry == nullptr)
        return std::nullopt;
    std::unique_ptr<InputStream> in (zip->createStreamForEntry (*entry));
    if (in == nullptr)
        return std::nullopt;
    MemoryBlock mb;
    in->readIntoMemoryBlock (mb);
    std::vector<std::byte> data (mb.getSize());
    std::memcpy (data.data(), mb.getData(), mb.getSize());
    return Resource { std::move (data), mimeOf (path) };
}
