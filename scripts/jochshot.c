// jochshot — capture the frontmost layer-0 window of a process to PNG.
// Dev/QA tool for visual verification; no third-party dependencies.
//
//   cc -Wno-deprecated-declarations -o jochshot jochshot.c \
//      -framework CoreGraphics -framework CoreFoundation -framework AppKit
//   ./jochshot <pid> [out.png]
//
// CGWindowListCreateImage is marked unavailable by the macOS 15 SDK headers,
// but the symbol still ships in CoreGraphics, so it is resolved dynamically.

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef CGImageRef (*CreateImageFn)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);

static long dictLong(CFDictionaryRef d, CFStringRef key) {
    const void* v = CFDictionaryGetValue(d, key);
    if (!v) return -1;
    long out = -1;
    CFNumberGetValue((CFNumberRef)v, kCFNumberLongType, &out);
    return out;
}

static Boolean writePng(CGImageRef img, const char* path) {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8),
        kCFURLPOSIXPathStyle, false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (!dst) { CFRelease(url); return false; }
    CGImageDestinationAddImage(dst, img, NULL);
    Boolean ok = CGImageDestinationFinalize(dst);
    CFRelease(dst);
    CFRelease(url);
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: jochshot <pid> [out.png]\n");
        return 2;
    }
    long want = atol(argv[1]);
    char outPath[512];
    if (argc > 2)
        snprintf(outPath, sizeof(outPath), "%s", argv[2]);
    else
        snprintf(outPath, sizeof(outPath), "/tmp/jochshot-%ld.png", want);

    CreateImageFn createImage = (CreateImageFn)dlsym(RTLD_DEFAULT, "CGWindowListCreateImage");
    if (!createImage) { fprintf(stderr, "CGWindowListCreateImage not resolvable\n"); return 2; }

    CFArrayRef wins = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
    if (!wins) { fprintf(stderr, "no window list (screen-recording permission?)\n"); return 1; }

    int rc = 1;
    for (CFIndex i = 0; i < CFArrayGetCount(wins); i++) {
        CFDictionaryRef d = CFArrayGetValueAtIndex(wins, i);
        if (dictLong(d, kCGWindowOwnerPID) == want && dictLong(d, kCGWindowLayer) == 0) {
            uint32_t wid = (uint32_t)dictLong(d, kCGWindowNumber);
            CGImageRef img = createImage(CGRectNull, kCGWindowListOptionIncludingWindow, wid,
                                         kCGWindowImageBoundsIgnoreFraming);
            if (!img) { fprintf(stderr, "wid %u: capture NULL, trying next\n", wid); continue; }
            if (writePng(img, outPath)) {
                fprintf(stderr, "captured wid=%u -> %s\n", wid, outPath);
                rc = 0;
            } else {
                fprintf(stderr, "png encode failed for wid %u\n", wid);
            }
            CGImageRelease(img);
            if (rc == 0) break;
        }
    }
    if (rc != 0) fprintf(stderr, "no layer-0 window for pid %ld\n", want);
    CFRelease(wins);
    return rc;
}
