//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "spake2client.h"

#include <QCryptographicHash>

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <array>
#include <memory>

namespace
{
constexpr char kMHex[] =
    "02886e2f97ace46e55ba9dd7242579f2993b64e16ef3dcab95afd497333d8fa12f";
constexpr char kNHex[] =
    "03d8bbd6c639c62937b04d997f38c3770719c629d7014d49a24b4f98baa1292b49";
constexpr char kPairingDomain[] = "jochona-beacon-pairing-v1";
constexpr char kConfirmationInfo[] = "ConfirmationKeys";
constexpr quint64 kScryptMaxMemory = 64ULL * 1024ULL * 1024ULL;

using BnPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using BnCtxPtr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;
using GroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;
using PointPtr = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

void setError(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QByteArray fixedWidth(const BIGNUM* value, int width)
{
    QByteArray output(width, Qt::Uninitialized);
    if (BN_bn2binpad(value,
                     reinterpret_cast<unsigned char*>(output.data()),
                     width) != width) {
        return {};
    }
    return output;
}

bool appendLengthPrefixed(QByteArray& transcript, const QByteArray& value)
{
    quint64 length = static_cast<quint64>(value.size());
    for (int i = 0; i < 8; ++i) {
        transcript.append(static_cast<char>((length >> (8 * i)) & 0xff));
    }
    transcript.append(value);
    return true;
}

QByteArray encodePoint(const EC_GROUP* group,
                       const EC_POINT* point,
                       BN_CTX* context)
{
    const size_t size = EC_POINT_point2oct(
        group, point, POINT_CONVERSION_UNCOMPRESSED, nullptr, 0, context);
    if (size != 65) {
        return {};
    }
    QByteArray encoded(static_cast<qsizetype>(size), Qt::Uninitialized);
    if (EC_POINT_point2oct(
            group, point, POINT_CONVERSION_UNCOMPRESSED,
            reinterpret_cast<unsigned char*>(encoded.data()), size,
            context) != size) {
        return {};
    }
    return encoded;
}

PointPtr decodePoint(const EC_GROUP* group,
                     const QByteArray& encoded,
                     BN_CTX* context)
{
    PointPtr point(EC_POINT_new(group), EC_POINT_free);
    if (!point || encoded.isEmpty()
            || EC_POINT_oct2point(
                   group, point.get(),
                   reinterpret_cast<const unsigned char*>(encoded.constData()),
                   static_cast<size_t>(encoded.size()), context) != 1
            || EC_POINT_is_on_curve(group, point.get(), context) != 1
            || EC_POINT_is_at_infinity(group, point.get()) == 1) {
        return PointPtr(nullptr, EC_POINT_free);
    }
    return point;
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& input)
{
    QByteArray output(EVP_MAX_MD_SIZE, Qt::Uninitialized);
    unsigned int outputLength = 0;
    if (HMAC(EVP_sha256(), key.constData(), key.size(),
             reinterpret_cast<const unsigned char*>(input.constData()),
             static_cast<size_t>(input.size()),
             reinterpret_cast<unsigned char*>(output.data()),
             &outputLength) == nullptr || outputLength != 32) {
        return {};
    }
    output.resize(static_cast<qsizetype>(outputLength));
    return output;
}

QByteArray hkdfConfirmationKeys(const QByteArray& authenticationKey,
                                const QByteArray& pairingId)
{
    PkeyCtxPtr context(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr),
                       EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_derive_init(context.get()) <= 0
            || EVP_PKEY_CTX_hkdf_mode(
                   context.get(), EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND) <= 0
            || EVP_PKEY_CTX_set_hkdf_md(context.get(), EVP_sha256()) <= 0) {
        return {};
    }

    const std::array<unsigned char, 32> zeroSalt {};
    const QByteArray info = QByteArray(kConfirmationInfo) + pairingId;
    if (EVP_PKEY_CTX_set1_hkdf_salt(
            context.get(), zeroSalt.data(), zeroSalt.size()) <= 0
            || EVP_PKEY_CTX_set1_hkdf_key(
                   context.get(),
                   reinterpret_cast<const unsigned char*>(
                       authenticationKey.constData()),
                   authenticationKey.size()) <= 0
            || EVP_PKEY_CTX_add1_hkdf_info(
                   context.get(),
                   reinterpret_cast<const unsigned char*>(info.constData()),
                   info.size()) <= 0) {
        return {};
    }

    QByteArray output(32, Qt::Uninitialized);
    size_t outputLength = static_cast<size_t>(output.size());
    if (EVP_PKEY_derive(
            context.get(),
            reinterpret_cast<unsigned char*>(output.data()),
            &outputLength) <= 0 || outputLength != 32) {
        return {};
    }
    return output;
}

QByteArray certificateIdentity(const QByteArray& prefix,
                               const QByteArray& fingerprint)
{
    return prefix + fingerprint.toHex();
}
}

BeaconSpake2Client::BeaconSpake2Client(
        QUuid beaconId,
        QUuid pairingId,
        QByteArray shortCode,
        QByteArray clientSpkiSha256,
        QByteArray beaconSpkiSha256)
    : m_BeaconId(beaconId)
    , m_PairingId(pairingId)
    , m_ShortCode(std::move(shortCode))
    , m_ClientSpkiSha256(std::move(clientSpkiSha256))
    , m_BeaconSpkiSha256(std::move(beaconSpkiSha256))
{
}

BeaconSpake2Client::~BeaconSpake2Client()
{
    OPENSSL_cleanse(m_ShortCode.data(),
                    static_cast<size_t>(m_ShortCode.size()));
    m_ShortCode.clear();
    invalidate();
}

QByteArray BeaconSpake2Client::certificateSpkiSha256(
        const QByteArray& certificateDer)
{
    if (certificateDer.isEmpty()) {
        return {};
    }

    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(certificateDer.constData());
    std::unique_ptr<X509, decltype(&X509_free)> certificate(
        d2i_X509(nullptr, &cursor, certificateDer.size()), X509_free);
    if (!certificate) {
        return {};
    }

    X509_PUBKEY* publicKey = X509_get_X509_PUBKEY(certificate.get());
    const int encodedSize = i2d_X509_PUBKEY(publicKey, nullptr);
    if (publicKey == nullptr || encodedSize <= 0) {
        return {};
    }

    QByteArray encoded(encodedSize, Qt::Uninitialized);
    unsigned char* output =
        reinterpret_cast<unsigned char*>(encoded.data());
    if (i2d_X509_PUBKEY(publicKey, &output) != encodedSize) {
        return {};
    }
    return QCryptographicHash::hash(
        encoded, QCryptographicHash::Sha256);
}

bool BeaconSpake2Client::begin(QByteArray* clientShare,
                               QString* error,
                               const QByteArray& fixedScalar)
{
    invalidate();
    if (clientShare == nullptr || m_BeaconId.isNull() || m_PairingId.isNull()
            || m_ShortCode.isEmpty()
            || m_ClientSpkiSha256.size() != 32
            || m_BeaconSpkiSha256.size() != 32) {
        setError(error, QStringLiteral("Invalid SPAKE2 pairing inputs"));
        return false;
    }

    GroupPtr group(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1),
                   EC_GROUP_free);
    BnCtxPtr context(BN_CTX_new(), BN_CTX_free);
    BnPtr order(BN_new(), BN_free);
    if (!group || !context || !order
            || EC_GROUP_get_order(group.get(), order.get(), context.get()) != 1) {
        setError(error, QStringLiteral("OpenSSL could not initialize P-256"));
        return false;
    }

    const QByteArray salt = QByteArray(kPairingDomain)
        + '\0' + m_BeaconId.toRfc4122() + '\0' + m_PairingId.toRfc4122();
    QByteArray derived(40, Qt::Uninitialized);
    if (EVP_PBE_scrypt(
            m_ShortCode.constData(), static_cast<size_t>(m_ShortCode.size()),
            reinterpret_cast<const unsigned char*>(salt.constData()),
            static_cast<size_t>(salt.size()), 32768, 8, 1,
            kScryptMaxMemory,
            reinterpret_cast<unsigned char*>(derived.data()),
            static_cast<size_t>(derived.size())) != 1) {
        setError(error, QStringLiteral("OpenSSL scrypt failed"));
        return false;
    }

    BnPtr derivedNumber(
        BN_bin2bn(reinterpret_cast<const unsigned char*>(derived.constData()),
                  derived.size(), nullptr),
        BN_free);
    BnPtr w(BN_new(), BN_free);
    BnPtr x(BN_new(), BN_free);
    if (!derivedNumber || !w || !x
            || BN_nnmod(w.get(), derivedNumber.get(), order.get(),
                        context.get()) != 1
            || BN_is_zero(w.get())) {
        setError(error, QStringLiteral("Could not derive SPAKE2 password scalar"));
        return false;
    }

    if (!fixedScalar.isEmpty()) {
        if (fixedScalar.size() != 32
                || BN_bin2bn(
                       reinterpret_cast<const unsigned char*>(
                           fixedScalar.constData()),
                       fixedScalar.size(), x.get()) == nullptr
                || BN_is_zero(x.get()) || BN_cmp(x.get(), order.get()) >= 0) {
            setError(error, QStringLiteral("Invalid deterministic SPAKE2 scalar"));
            return false;
        }
    } else {
        do {
            if (BN_priv_rand_range(x.get(), order.get()) != 1) {
                setError(error,
                         QStringLiteral("OpenSSL could not generate SPAKE2 scalar"));
                return false;
            }
        } while (BN_is_zero(x.get()));
    }

    const QByteArray mEncoded = QByteArray::fromHex(kMHex);
    PointPtr m = decodePoint(group.get(), mEncoded, context.get());
    PointPtr xG(EC_POINT_new(group.get()), EC_POINT_free);
    PointPtr wM(EC_POINT_new(group.get()), EC_POINT_free);
    PointPtr pA(EC_POINT_new(group.get()), EC_POINT_free);
    if (!m || !xG || !wM || !pA
            || EC_POINT_mul(group.get(), xG.get(), x.get(), nullptr, nullptr,
                            context.get()) != 1
            || EC_POINT_mul(group.get(), wM.get(), nullptr, m.get(), w.get(),
                            context.get()) != 1
            || EC_POINT_add(group.get(), pA.get(), xG.get(), wM.get(),
                            context.get()) != 1) {
        setError(error, QStringLiteral("Could not compute SPAKE2 Client share"));
        return false;
    }

    m_X = fixedWidth(x.get(), 32);
    m_W = fixedWidth(w.get(), 32);
    m_ClientShare = encodePoint(group.get(), pA.get(), context.get());
    if (m_X.size() != 32 || m_W.size() != 32
            || m_ClientShare.size() != 65) {
        invalidate();
        setError(error, QStringLiteral("Could not encode SPAKE2 Client state"));
        return false;
    }

    m_Started = true;
    *clientShare = m_ClientShare;
    setError(error, QString());
    return true;
}

bool BeaconSpake2Client::finish(const QByteArray& beaconShare,
                                const QByteArray& beaconConfirmation,
                                QByteArray* clientConfirmation,
                                QString* error)
{
    if (!m_Started || clientConfirmation == nullptr
            || beaconConfirmation.size() != 32) {
        invalidate();
        setError(error, QStringLiteral("SPAKE2 pairing is not in progress"));
        return false;
    }

    GroupPtr group(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1),
                   EC_GROUP_free);
    BnCtxPtr context(BN_CTX_new(), BN_CTX_free);
    BnPtr x(BN_bin2bn(
                reinterpret_cast<const unsigned char*>(m_X.constData()),
                m_X.size(), nullptr),
            BN_free);
    BnPtr w(BN_bin2bn(
                reinterpret_cast<const unsigned char*>(m_W.constData()),
                m_W.size(), nullptr),
            BN_free);
    if (!group || !context || !x || !w) {
        invalidate();
        setError(error, QStringLiteral("OpenSSL could not restore SPAKE2 state"));
        return false;
    }

    PointPtr pB = decodePoint(group.get(), beaconShare, context.get());
    PointPtr n = decodePoint(group.get(), QByteArray::fromHex(kNHex),
                             context.get());
    PointPtr wN(EC_POINT_new(group.get()), EC_POINT_free);
    PointPtr adjusted(EC_POINT_new(group.get()), EC_POINT_free);
    PointPtr shared(EC_POINT_new(group.get()), EC_POINT_free);
    if (!pB || !n || !wN || !adjusted || !shared
            || EC_POINT_mul(group.get(), wN.get(), nullptr, n.get(), w.get(),
                            context.get()) != 1
            || EC_POINT_invert(group.get(), wN.get(), context.get()) != 1
            || EC_POINT_add(group.get(), adjusted.get(), pB.get(), wN.get(),
                            context.get()) != 1
            || EC_POINT_is_at_infinity(group.get(), adjusted.get()) == 1
            || EC_POINT_mul(group.get(), shared.get(), nullptr, adjusted.get(),
                            x.get(), context.get()) != 1
            || EC_POINT_is_at_infinity(group.get(), shared.get()) == 1) {
        invalidate();
        setError(error, QStringLiteral("Invalid SPAKE2 Beacon share"));
        return false;
    }

    const QByteArray sharedEncoded =
        encodePoint(group.get(), shared.get(), context.get());
    const QByteArray clientIdentity = certificateIdentity(
        QByteArrayLiteral("jochona-client:"), m_ClientSpkiSha256);
    const QByteArray beaconIdentity =
        QByteArrayLiteral("jochona-beacon:")
        + m_BeaconId.toString(QUuid::WithoutBraces).toLower().toUtf8()
        + ':' + m_BeaconSpkiSha256.toHex();

    QByteArray transcript;
    appendLengthPrefixed(transcript, clientIdentity);
    appendLengthPrefixed(transcript, beaconIdentity);
    appendLengthPrefixed(transcript, m_ClientShare);
    appendLengthPrefixed(transcript, beaconShare);
    appendLengthPrefixed(transcript, sharedEncoded);
    appendLengthPrefixed(transcript, m_W);

    const QByteArray digest =
        QCryptographicHash::hash(transcript, QCryptographicHash::Sha256);
    const QByteArray authenticationKey = digest.mid(16, 16);
    const QByteArray confirmationKeys = hkdfConfirmationKeys(
        authenticationKey, m_PairingId.toRfc4122());
    if (sharedEncoded.size() != 65 || confirmationKeys.size() != 32) {
        invalidate();
        setError(error, QStringLiteral("Could not derive SPAKE2 confirmation keys"));
        return false;
    }

    const QByteArray expectedBeaconConfirmation =
        hmacSha256(confirmationKeys.mid(16, 16), transcript);
    const QByteArray ownConfirmation =
        hmacSha256(confirmationKeys.left(16), transcript);
    const bool confirmed = expectedBeaconConfirmation.size() == 32
            && ownConfirmation.size() == 32
            && CRYPTO_memcmp(expectedBeaconConfirmation.constData(),
                             beaconConfirmation.constData(), 32) == 0;
    invalidate();
    if (!confirmed) {
        setError(error, QStringLiteral("Beacon confirmation did not match"));
        return false;
    }

    *clientConfirmation = ownConfirmation;
    setError(error, QString());
    return true;
}

void BeaconSpake2Client::invalidate()
{
    const bool clearShortCode = m_Started;
    OPENSSL_cleanse(m_X.data(), static_cast<size_t>(m_X.size()));
    OPENSSL_cleanse(m_W.data(), static_cast<size_t>(m_W.size()));
    m_X.clear();
    m_W.clear();
    m_ClientShare.clear();
    m_Started = false;
    if (clearShortCode) {
        OPENSSL_cleanse(m_ShortCode.data(),
                        static_cast<size_t>(m_ShortCode.size()));
        m_ShortCode.clear();
    }
}
