#include "MediaCache.h"

#include <QFileInfo>
#include <QMutexLocker>

// ---------------------------------------------------------------------------
// CacheKeyHash — Boost-style multiplicative hash combining
// ---------------------------------------------------------------------------
std::size_t CacheKeyHash::operator()(const CacheKey& k) const noexcept
{
    // Combine three independent hashes using the standard golden-ratio mix.
    // This minimises clustering in the unordered_map bucket distribution.
    auto combine = [](std::size_t seed, std::size_t v) noexcept -> std::size_t {
        return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
    };

    std::size_t h = std::hash<std::string>{}(k.filePath.toStdString());
    h = combine(h, std::hash<qint64>{}(k.mtime));
    h = combine(h, std::hash<qint64>{}(k.timestampMs));
    return h;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

// Conservative in-memory byte estimate for a pixmap (ARGB32 layout).
qint64 pixmapBytes(const QPixmap& pix) noexcept
{
    if (pix.isNull()) {
        return 0;
    }
    return static_cast<qint64>(pix.width()) * pix.height() * 4LL;
}

} // namespace

// ---------------------------------------------------------------------------
// MediaCache
// ---------------------------------------------------------------------------
MediaCache::MediaCache(int maxMetadataCount, int maxThumbnailCount, qint64 maxThumbnailBytes)
    : m_metaStore(maxMetadataCount)
    , m_thumbStore(maxThumbnailCount, maxThumbnailBytes)
{}

CacheKey MediaCache::makeKey(const QString& filePath, double timestampSeconds)
{
    CacheKey key;
    key.filePath    = filePath;
    key.mtime       = QFileInfo(filePath).lastModified().toMSecsSinceEpoch();
    // Round to the nearest millisecond to avoid floating-point precision mismatches
    // between the timestamp stored in a RequestId and the one supplied on lookup.
    key.timestampMs = static_cast<qint64>(timestampSeconds * 1000.0 + 0.5);
    return key;
}

std::optional<VideoMetadata> MediaCache::getMetadata(const CacheKey& key)
{
    QMutexLocker<QMutex> lock(&m_mutex);
    return m_metaStore.get(key);
}

std::optional<QPixmap> MediaCache::getThumbnail(const CacheKey& key)
{
    QMutexLocker<QMutex> lock(&m_mutex);
    return m_thumbStore.get(key);
}

void MediaCache::putMetadata(const CacheKey& key, const VideoMetadata& meta)
{
    QMutexLocker<QMutex> lock(&m_mutex);
    m_metaStore.put(key, meta, 0); // metadata structs are small; byte budget not tracked
}

void MediaCache::putThumbnail(const CacheKey& key, const QPixmap& pixmap)
{
    QMutexLocker<QMutex> lock(&m_mutex);
    m_thumbStore.put(key, pixmap, pixmapBytes(pixmap));
}

void MediaCache::invalidate(const QString& filePath)
{
    QMutexLocker<QMutex> lock(&m_mutex);
    m_metaStore.removeByPath(filePath);
    m_thumbStore.removeByPath(filePath);
}

void MediaCache::clear()
{
    QMutexLocker<QMutex> lock(&m_mutex);
    m_metaStore.clear();
    m_thumbStore.clear();
}

int MediaCache::metadataCount() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    return m_metaStore.count();
}

int MediaCache::thumbnailCount() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    return m_thumbStore.count();
}

qint64 MediaCache::thumbnailBytes() const
{
    QMutexLocker<QMutex> lock(&m_mutex);
    return m_thumbStore.usedBytes();
}
