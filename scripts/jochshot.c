// jochshot — capture the first real window of a process by PID.
// Visual QA for the QML shell; independent of Spaces/frontmost state.
// build: cc -framework CoreGraphics -framework CoreFoundation -framework ImageIO -framework UniformTypeIdentifiers -fobjc-arc -o jochshot scripts/jochshot.c
// usage: jochshot <pid> ; writes /tmp/jochshot.tiff
#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static long dictLong(CFDictionaryRef d, CFStringRef key) {
    const void* v = CFDictionaryGetValue(d, key);
    if (!v) return -1;
    long out = -1;
    CFNumberGetValue((CFNumberRef)v, kCFNumberLongType, &out);
    return out;
}

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: jochshot <pid>\n"); return 2; }
    long want = atol(argv[1]);

    CFArrayRef all = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
    if (!all) { fprintf(stderr, "window list failed (screen recording permission?)\n"); return 1; }

    CFIndex n = CFArrayGetCount(all);
    for (CFIndex i = 0; i < n; i++) {
        CFDictionaryRef d = CFArrayGetValueAtIndex(all, i);
        if (dictLong(d, kCGWindowOwnerPID) == want && dictLong(d, kCGWindowLayer) == 0) {
            uint32_t wid = (uint32_t)dictLong(d, kCGWindowNumber);
            CGImageRef img = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, wid,
                                                     kCGWindowImageBoundsIgnoreFraming);
            if (!img) { fprintf(stderr, "capture failed for wid %u\n", wid); return 1; }
            CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                CFSTR("/tmp/jochshot.tiff"), kCFURLPOSIXPathStyle, false);
            CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.tiff"), 1, NULL);
            CGImageDestinationAddImage(dst, img, NULL);
            int rc = CGImageDestinationFinalize(dst) ? 0 : 1;
            fprintf(stderr, "captured wid=%u rc=%d\n", wid, rc);
            return rc;
        }
    }
    fprintf(stderr, "no layer-0 window for pid %ld\n", want);
    return 1;
}
