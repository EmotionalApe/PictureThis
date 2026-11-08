#pragma once

#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QPixmap>
#include <QString>

#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "MediaService.h" // VideoMetadata

// ---------------------------------------------------------------------------
// CacheKey
//
// Identifies a cached entry by file path, file modification time (mtime), and
// — for thumbnails — the requested timestamp in milliseconds.
//
// Using mtime as part of the key means entries self-invalidate the moment a
// file is modified on disk: the next makeKey() call yields a different mtime
// and the old entry is simply never matched again (evicted later via LRU).
// ---------------------------------------------------------------------------
struct CacheKey {
    QString filePath;
    qint64  mtime       = 0; // ms since epoch from QFileInfo::lastModified()
    qint64  timestampMs = 0; // 0 for metadata; rounded ms for thumbnails

    bool operator==(const CacheKey& o) const noexcept
    {
        return filePath == o.filePath
            && mtime == o.mtime
            && timestampMs == o.timestampMs;
    }
};

struct CacheKeyHash {
    std::size_t operator()(const CacheKey& k) const noexcept;
};

// ---------------------------------------------------------------------------
// LruCache<V>
//
// A fixed-capacity LRU cache with an optional byte-budget eviction policy.
// Uses the classic std::list + std::unordered_map structure for O(1) get and
// O(1) put (amortised): the list encodes recency order (front = MRU), and the
// map gives constant-time lookup of the corresponding list iterator.
//
// Eviction removes the least-recently-used entry from the back of the list
// until both the count and byte-budget constraints are satisfied.
// ---------------------------------------------------------------------------
template <typename V>
class LruCache
{
public:
    // maxCount: maximum number of entries (0 = unlimited).
    // maxBytes: maximum total byte budget (0 = unlimited).
    explicit LruCache(int maxCount, qint64 maxBytes = 0)
        : m_maxCount(maxCount), m_maxBytes(maxBytes) {}

    // Returns the cached value on hit, moving it to the MRU position.
    // Returns std::nullopt on miss.
    std::optional<V> get(const CacheKey& key)
    {
        auto it = m_index.find(key);
        if (it == m_index.end()) {
            return std::nullopt;
        }
        // Splice to front (O(1) in std::list) — promotes to MRU.
        m_list.splice(m_list.begin(), m_list, it->second);
        return it->second->value;
    }

    // Inserts or updates an entry and moves it to the MRU position.
    // valueBytes is used for the byte-budget eviction policy (pass 0 if N/A).
    void put(const CacheKey& key, V value, qint64 valueBytes = 0)
    {
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            // Key already present: update in-place and re-promote.
            m_usedBytes -= it->second->bytes;
            it->second->value = std::move(value);
            it->second->bytes = valueBytes;
            m_usedBytes += valueBytes;
            m_list.splice(m_list.begin(), m_list, it->second);
            return;
        }

        // New entry: prepend to list (MRU position) and register in the index.
        m_list.push_front({key, std::move(value), valueBytes});
        m_index.emplace(key, m_list.begin());
        m_usedBytes += valueBytes;

        evictUntilFit();
    }

    // Removes all entries whose filePath matches. Used for explicit invalidation.
    void removeByPath(const QString& filePath)
    {
        for (auto listIt = m_list.begin(); listIt != m_list.end(); ) {
            if (listIt->key.filePath == filePath) {
                m_usedBytes -= listIt->bytes;
                m_index.erase(listIt->key);
                listIt = m_list.erase(listIt);
            } else {
                ++listIt;
            }
        }
    }

    void clear()
    {
        m_list.clear();
        m_index.clear();
        m_usedBytes = 0;
    }

    int    count()     const { return static_cast<int>(m_index.size()); }
    qint64 usedBytes() const { return m_usedBytes; }

private:
    struct Node {
        CacheKey key;
        V        value;
        qint64   bytes = 0;
    };

    using List = std::list<Node>;
    using Iter = typename List::iterator;

    int    m_maxCount;
    qint64 m_maxBytes;
    qint64 m_usedBytes = 0;

    List m_list;
    std::unordered_map<CacheKey, Iter, CacheKeyHash> m_index;

    // Evict LRU entries (back of list) until both constraints are satisfied.
    void evictUntilFit()
    {
        while (!m_list.empty()) {
            const bool overCount = m_maxCount > 0
                                && static_cast<int>(m_index.size()) > m_maxCount;
            const bool overBytes = m_maxBytes > 0 && m_usedBytes > m_maxBytes;
            if (!overCount && !overBytes) {
                break;
            }

            auto& lru = m_list.back();
            m_usedBytes -= lru.bytes;
            m_index.erase(lru.key);
            m_list.pop_back();
        }
    }
};

// ---------------------------------------------------------------------------
// MediaCache
//
// Concrete wrapper over two LruCache instantiations — one for VideoMetadata
// (small structs, count-limited to 256) and one for QPixmap thumbnails
// (large objects, limited by both count and a 64 MiB byte budget).
//
// A QMutex guards both stores. This is a no-op overhead when all callers run
// on the same event-loop thread, but ensures the cache remains correct if
// InspectionWorkers are ever promoted to dedicated QThreads.
// ---------------------------------------------------------------------------
class MediaCache
{
public:
    explicit MediaCache(
        int    maxMetadataCount  = 256,
        int    maxThumbnailCount = 50,
        qint64 maxThumbnailBytes = 64LL * 1024 * 1024
    );

    // Returns std::nullopt on cache miss.
    std::optional<VideoMetadata> getMetadata(const CacheKey& key);
    std::optional<QPixmap>       getThumbnail(const CacheKey& key);

    void putMetadata(const CacheKey& key, const VideoMetadata& meta);
    void putThumbnail(const CacheKey& key, const QPixmap& pixmap);

    // Removes all entries for a given file path from both stores.
    void invalidate(const QString& filePath);
    void clear();

    // Builds a CacheKey by reading the file's mtime from disk.
    // timestampSeconds == 0.0 for metadata requests.
    static CacheKey makeKey(const QString& filePath, double timestampSeconds = 0.0);

    // Diagnostic accessors
    int    metadataCount()  const;
    int    thumbnailCount() const;
    qint64 thumbnailBytes() const;

private:
    LruCache<VideoMetadata> m_metaStore;
    LruCache<QPixmap>       m_thumbStore;
    mutable QMutex          m_mutex;
};
