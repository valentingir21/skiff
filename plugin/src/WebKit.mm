// SPDX-License-Identifier: GPL-3.0-only
// WKWebView itself: its throttling of hidden pages off; (the tests) the page as it draws it, into a PNG
#include "WebUI.h"
#import <WebKit/WebKit.h>
#import <AppKit/AppKit.h>
#import <objc/message.h>

using namespace juce;

static WKWebView* findWebView (NSView* v)
{
    if (v == nil)
        return nil;
    if ([v isKindOfClass: [WKWebView class]])
        return (WKWebView*) v;
    for (NSView* s in [v subviews])
        if (auto* w = findWebView (s))
            return w;
    return nil;
}

static WKWebView* webViewOf (juce::Component& c)
{
    for (auto* ch : c.getChildren())
        if (auto* nv = dynamic_cast<NSViewComponent*> (ch))
            if (auto* w = findWebView ((NSView*) nv->getView()))
                return w;
    return nil;
}

// The page keeps working with the window closed (the automation, the unit's pushes): WebKit's throttling of hidden
// pages (their timers slowed to a minute, their process suppressed) is turned off for this view. WKPreferences'
// private setters, each only if this WebKit has it: without them the page still answers the automation (it runs on
// the plugin's messages), only its own timers wait for the window.
bool WebUI::keepAwake()
{
    auto* wk = webViewOf (*this);
    if (wk == nil)
        return false;
    WKPreferences* p = wk.configuration.preferences;
    bool any = false;
    for (const char* s : { "_setHiddenPageDOMTimerThrottlingEnabled:", "_setHiddenPageDOMTimerThrottlingAutoIncreases:",
                           "_setPageVisibilityBasedProcessSuppressionEnabled:", "_setBackgroundTimerThrottlingEnabled:" })
    {
        SEL sel = sel_registerName (s);
        if ([p respondsToSelector: sel])
        {
            ((void (*) (id, SEL, BOOL)) objc_msgSend) (p, sel, NO);
            any = true;
        }
    }
    return any;
}

void WebUI::snapshot (const File& png, std::function<void (bool)> done)
{
    WKWebView* wk = webViewOf (*this);
    if (wk == nil)
        return done (false);
    auto cb = std::make_shared<std::function<void (bool)>> (std::move (done));
    auto path = png.getFullPathName();
    [wk takeSnapshotWithConfiguration: nil
                    completionHandler: ^(NSImage* img, NSError*) {
                        bool ok = false;
                        if (img != nil)
                        {
                            CGImageRef cg = [img CGImageForProposedRect: nil context: nil hints: nil];
                            if (cg != nullptr)
                            {
                                NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage: cg];
                                NSData* data = [rep representationUsingType: NSBitmapImageFileTypePNG properties: @{}];
                                ok = [data writeToFile: [NSString stringWithUTF8String: path.toRawUTF8()] atomically: YES];
                            }
                        }
                        (*cb) (ok);
                    }];
}
