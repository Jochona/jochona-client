//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: normalized per-host capability model (proposal §4.4, §6.9,
// §7.4). A single value type covers all three adapter tiers -- Sunshine is
// the empty/baseline case, Apollo adds bits as HostProber confirms them,
// Vibepollo is Apollo plus the ABR (runtime bitrate) extension -- so callers
// never need to branch on family except to explain *why* a bit is unset.
//
// Every field here is something HostProber (hostprober.h) can actually
// observe on the wire; nothing is inferred from version strings alone,
// since GfeVersion/GfeVersionMinor strings are the only baseline Sunshine
// signal and are not enough to tell Sunshine, Apollo, and Vibepollo apart.
//
// The Permission enum mirrors Apollo's crypto::PERM bit layout exactly
// (upstream src/crypto.h, verified against source), because the
// /serverinfo <Permission> tag is emitted as the raw
// std::to_string((uint32_t)perm) of that same enum (upstream src/nvhttp.cpp).
// Storing it losslessly lets HostAdapterManager gate capabilities against
// the *current* client's actual permissions instead of guessing.
//
#pragma once

#include <QDateTime>
#include <QFlags>
#include <QJsonObject>
#include <QMetaType>
#include <QList>
#include <QVector>
#include <QStringList>

class HostCapabilities
{
    Q_GADGET

public:
    enum class Family {
        Unknown,
        Sunshine,
        Apollo,
        Vibepollo,
        Jochona
    };
    Q_ENUM(Family)

    // Endpoint-observed feature bits. Each one corresponds 1:1 to a probe
    // HostProber runs; see hostprober.cpp for the endpoint -> bit mapping.
    enum Capability {
        NoCapabilities             = 0x0000,
        RunningAppState            = 0x0001, // GET /state succeeded (Apollo+)
        Clipboard                  = 0x0002, // GET/POST /actions/clipboard, permission-gated
        VirtualDisplayCapable      = 0x0004, // <VirtualDisplayCapable> in /serverinfo
        VirtualDisplayDriverReady  = 0x0008, // <VirtualDisplayDriverReady> in /serverinfo
        DisplayModes               = 0x0010, // GET /serverdisplaymodes
        ServerResolution           = 0x0020, // GET /serverresolution
        ServerAudio                = 0x0040, // GET /serveraudio
        ClientAudio                = 0x0080, // GET /clientaudio
        VolumeControl              = 0x0100, // GET /actions/volumes
        ActionToggle               = 0x0200, // GET /actions/toggle
        ActionCancel               = 0x0400, // GET /actions/cancel, gated by PERM::launch
        ActionBitrates             = 0x0800, // GET /action/bitrates
        RuntimeBitrate             = 0x1000, // GET /api/abr/capabilities advertises "runtime_bitrate" (Vibepollo)
        JochonaManifest            = 0x2000, // compatible /jochona/v1/capabilities
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_FLAG(Capabilities)

    // Bit-for-bit copy of Apollo's crypto::PERM (upstream src/crypto.h).
    // Group prefixes (_input=1<<8, _operation=1<<16, _action=1<<24) are
    // deliberately not reproduced here; only the leaf bits we can act on.
    enum Permission : quint32 {
        NoPermissions   = 0,

        InputController = 1u << 8,
        InputTouch      = 1u << 9,
        InputPen        = 1u << 10,
        InputMouse      = 1u << 11,
        InputKeyboard   = 1u << 12,

        ClipboardSet    = 1u << 16,
        ClipboardRead   = 1u << 17,
        FileUpload      = 1u << 18,
        FileDownload    = 1u << 19,
        ServerCommand   = 1u << 20,

        ListApps        = 1u << 24,
        ViewStream      = 1u << 25,
        LaunchApps      = 1u << 26,
    };
    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_FLAG(Permissions)

    // How much of the probe plan actually completed. Distinct from Family:
    // a host can be Confirmed as plain Sunshine (the whole plan ran, and
    // every Apollo-only endpoint legitimately came back absent).
    enum class Confidence {
        Unknown,   // Never probed, or /serverinfo itself was unreachable
        Partial,   // /serverinfo answered but the run didn't finish (aborted/timed out)
        Confirmed  // The full probe plan for the detected family completed
    };
    Q_ENUM(Confidence)

    enum class ManifestStatus {
        Absent,
        Invalid,
        Incompatible,
        Compatible
    };

    struct EncoderTuple
    {
        QString id;
        QString codec;
        QString profile;
        int bitDepth = 8;
        QString chroma;
        int width = 0;
        int height = 0;
        int fps = 0;
        bool hdr = false;
        QStringList capture;

        int videoFormat() const;
        bool supportsCapture(bool virtualDisplay) const;
        QJsonObject toJson() const;
        static EncoderTuple fromJson(const QJsonObject& object, bool* ok = nullptr);

        bool operator==(const EncoderTuple& other) const;
    };

    HostCapabilities() = default;

    Family family = Family::Unknown;
    Capabilities capabilities = NoCapabilities;
    Permissions permissions = NoPermissions;
    Confidence confidence = Confidence::Unknown;
    QDateTime lastProbed;

    // Populated only when RuntimeBitrate is set: the raw "version" and
    // "features" array from GET /api/abr/capabilities.
    int abrVersion = 0;
    QStringList abrFeatures;

    ManifestStatus manifestStatus = ManifestStatus::Absent;
    int schemaMajor = 0;
    int schemaMinor = 0;
    QString hostSoftware;
    QString hostBuild;
    QString hostIdentity;
    QString capacityState;
    int maxSessions = 0;
    QString activeApplication;
    QStringList permissionNames;
    QVector<EncoderTuple> encoderTuples;

    bool hasCapability(Capability capability) const { return capabilities.testFlag(capability); }

    // True if the current client holds *any* bit in mask, not all of them.
    // Apollo's own gating (e.g. clipboard is read OR write) is per-bit, so
    // QFlags::testFlag's all-bits-set semantics is the wrong check here.
    bool hasAnyPermission(Permissions mask) const { return (permissions & mask) != Permissions(NoPermissions); }

    // Friendly accessors matching Apollo's WebUI permission names. Apollo's
    // nvhttp.cpp gates CancelApp on PERM::launch as well as LaunchApp, so
    // there is no separate "exit" bit to mirror.
    bool allowLaunch() const { return hasAnyPermission(LaunchApps); }
    bool allowExit() const { return hasAnyPermission(LaunchApps); }
    bool allowKeyboard() const { return hasAnyPermission(InputKeyboard); }
    bool allowClipboard() const { return hasAnyPermission(Permissions(ClipboardRead | ClipboardSet)); }

    ManifestStatus applyJochonaManifest(const QJsonObject& object,
                                        const QString& expectedIdentity,
                                        QString* error = nullptr);
    QString selectEncoderTuple(int width,
                               int height,
                               int fps,
                               const QList<int>& preferredVideoFormats,
                               bool hdr,
                               bool virtualDisplay) const;

    static QString familyName(Family family);
    static Family familyFromName(const QString& name);

    static QString confidenceName(Confidence confidence);
    static Confidence confidenceFromName(const QString& name);

    // Round-trips through stable string keys (not raw bit positions) so a
    // cached QSettings entry survives this enum growing new bits across
    // app versions. permissionMask is the one exception: it is already
    // Apollo's own wire integer, so it is stored verbatim.
    QJsonObject toJson() const;
    static HostCapabilities fromJson(const QJsonObject& object);

    bool operator==(const HostCapabilities& other) const;
    bool operator!=(const HostCapabilities& other) const { return !(*this == other); }

    // Confidence-merge policy for a freshly finished probe run against
    // whatever is already cached (proposal §4.4/§6.9): a probe that could
    // not even reach /serverinfo this time (a sleeping Host, a Wi-Fi
    // hiccup, an app-resume race) reports Confidence::Unknown and must
    // never regress a previously Confirmed capability set -- the Host
    // hasn't actually changed, this run just failed to reconfirm it.
    // Every other combination trusts the fresh probe outright, including
    // Confirmed replacing Confirmed (picks up e.g. a changed encoder
    // tuple set) and anything replacing a first-ever Unknown/Partial entry.
    static HostCapabilities mergeProbeResult(const HostCapabilities& cached,
                                             const HostCapabilities& probed);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HostCapabilities::Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(HostCapabilities::Permissions)
Q_DECLARE_METATYPE(HostCapabilities)
