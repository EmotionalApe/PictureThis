#include <QTest>
#include <QTemporaryFile>
#include <QDateTime>
#include <QThread>
#include <QPixmap>
#include <QPainter>
#include "../src/media/MediaCache.h"

class TestMediaCache : public QObject
{
    Q_OBJECT

private slots:
    void testCacheKeyEquality();
    void testLruGetPut();
    void testLruRecencyPromotion();
    void testLruCountEviction();
    void testLruByteBudgetEviction();
    void testLruUpdateExistingByteBudget();
    void testLruRemoveByPath();
    void testMediaCacheMtimeInvalidation();
    void testMediaCacheThumbnailByteTracking();
};

void TestMediaCache::testCacheKeyEquality()
{
    const CacheKey k1{"C:/video/clip1.mp4", 1700000000, 0};
    const CacheKey k2{"C:/video/clip1.mp4", 1700000000, 0};
    const CacheKey kDiffPath{"C:/video/clip2.mp4", 1700000000, 0};
    const CacheKey kDiffMtime{"C:/video/clip1.mp4", 1700000500, 0};
    const CacheKey kDiffTs{"C:/video/clip1.mp4", 1700000000, 1500};

    QCOMPARE(k1, k2);
    QVERIFY(!(k1 == kDiffPath));
    QVERIFY(!(k1 == kDiffMtime));
    QVERIFY(!(k1 == kDiffTs));

    CacheKeyHash hasher;
    QCOMPARE(hasher(k1), hasher(k2));
}

void TestMediaCache::testLruGetPut()
{
    LruCache<QString> cache(10);
    const CacheKey k1{"file1.mp4", 100, 0};
    const CacheKey k2{"file2.mp4", 200, 0};

    QVERIFY(!cache.get(k1).has_value());

    cache.put(k1, "Video Data 1");
    QCOMPARE(cache.count(), 1);

    auto val = cache.get(k1);
    QVERIFY(val.has_value());
    QCOMPARE(*val, QString("Video Data 1"));

    QVERIFY(!cache.get(k2).has_value());
}

void TestMediaCache::testLruRecencyPromotion()
{
    // Capacity 3: test that getting an entry promotes it to MRU and prevents its eviction
    LruCache<int> cache(3);
    const CacheKey kA{"a.mp4", 1, 0};
    const CacheKey kB{"b.mp4", 2, 0};
    const CacheKey kC{"c.mp4", 3, 0};
    const CacheKey kD{"d.mp4", 4, 0};

    cache.put(kA, 10);
    cache.put(kB, 20);
    cache.put(kC, 30);
    // Order from MRU to LRU: C, B, A

    // Access A -> promotes A to MRU: A, C, B
    auto valA = cache.get(kA);
    QVERIFY(valA.has_value());
    QCOMPARE(*valA, 10);

    // Insert D -> should evict LRU, which is now B!
    cache.put(kD, 40);

    QCOMPARE(cache.count(), 3);
    QVERIFY(cache.get(kA).has_value()); // A survived
    QVERIFY(cache.get(kC).has_value()); // C survived
    QVERIFY(cache.get(kD).has_value()); // D is present
    QVERIFY(!cache.get(kB).has_value()); // B was evicted!
}

void TestMediaCache::testLruCountEviction()
{
    constexpr int MAX_ITEMS = 5;
    LruCache<int> cache(MAX_ITEMS);

    for (int i = 0; i < 10; ++i) {
        const CacheKey k{QString("file_%1.mp4").arg(i), 1000 + i, 0};
        cache.put(k, i * 10);
    }

    QCOMPARE(cache.count(), MAX_ITEMS);

    // Oldest items 0..4 should be evicted
    for (int i = 0; i < 5; ++i) {
        const CacheKey k{QString("file_%1.mp4").arg(i), 1000 + i, 0};
        QVERIFY(!cache.get(k).has_value());
    }

    // Newest items 5..9 should be retained
    for (int i = 5; i < 10; ++i) {
        const CacheKey k{QString("file_%1.mp4").arg(i), 1000 + i, 0};
        auto val = cache.get(k);
        QVERIFY(val.has_value());
        QCOMPARE(*val, i * 10);
    }
}

void TestMediaCache::testLruByteBudgetEviction()
{
    // Maximum 1000 bytes budget, unlimited item count
    LruCache<QString> cache(0, 1000);

    const CacheKey k1{"file1.mp4", 1, 0};
    const CacheKey k2{"file2.mp4", 2, 0};
    const CacheKey k3{"file3.mp4", 3, 0};

    // Insert item 1 (400 bytes)
    cache.put(k1, "chunk1", 400);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.usedBytes(), 400);

    // Insert item 2 (400 bytes)
    cache.put(k2, "chunk2", 400);
    QCOMPARE(cache.count(), 2);
    QCOMPARE(cache.usedBytes(), 800);

    // Insert item 3 (400 bytes) -> total 1200 > 1000 budget!
    // Evicts LRU (item 1: 400 bytes), remaining bytes = 800 <= 1000
    cache.put(k3, "chunk3", 400);

    QCOMPARE(cache.count(), 2);
    QCOMPARE(cache.usedBytes(), 800);
    QVERIFY(!cache.get(k1).has_value()); // item 1 evicted
    QVERIFY(cache.get(k2).has_value());  // item 2 kept
    QVERIFY(cache.get(k3).has_value());  // item 3 kept
}

void TestMediaCache::testLruUpdateExistingByteBudget()
{
    LruCache<QString> cache(0, 1000);
    const CacheKey k1{"file1.mp4", 1, 0};

    cache.put(k1, "Initial payload", 300);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.usedBytes(), 300);

    // Update in-place with new size
    cache.put(k1, "Expanded payload", 650);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.usedBytes(), 650);
    QCOMPARE(*cache.get(k1), QString("Expanded payload"));
}

void TestMediaCache::testLruRemoveByPath()
{
    LruCache<QString> cache(10, 5000);

    const CacheKey kA1{"A.mp4", 1, 0};
    const CacheKey kA2{"A.mp4", 1, 1000};
    const CacheKey kB1{"B.mp4", 2, 0};

    cache.put(kA1, "thumb_A_0", 500);
    cache.put(kA2, "thumb_A_1000", 600);
    cache.put(kB1, "thumb_B_0", 700);

    QCOMPARE(cache.count(), 3);
    QCOMPARE(cache.usedBytes(), 1800);

    // Invalidate all entries for "A.mp4"
    cache.removeByPath("A.mp4");

    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.usedBytes(), 700);
    QVERIFY(!cache.get(kA1).has_value());
    QVERIFY(!cache.get(kA2).has_value());
    QVERIFY(cache.get(kB1).has_value());
}

void TestMediaCache::testMediaCacheMtimeInvalidation()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("Test video payload dummy data");
    tempFile.flush();
    const QString filePath = tempFile.fileName();

    MediaCache mediaCache;

    // Make key from disk file
    const CacheKey key1 = MediaCache::makeKey(filePath);
    QVERIFY(key1.mtime > 0);

    VideoMetadata meta;
    meta.width = 1920;
    meta.height = 1080;
    meta.videoCodec = "h264";
    meta.durationSeconds = 60.0;

    mediaCache.putMetadata(key1, meta);

    // Cache hit on identical file
    auto cached = mediaCache.getMetadata(key1);
    QVERIFY(cached.has_value());
    QCOMPARE(cached->width, 1920);
    QCOMPARE(cached->videoCodec, QString("h264"));

    // Now modify the file to trigger mtime change
    QThread::msleep(15);
    tempFile.write("Additional bytes appended to change file modification time");
    tempFile.flush();

    // Querying with fresh key based on new disk mtime
    const CacheKey key2 = MediaCache::makeKey(filePath);
    if (key2.mtime != key1.mtime) {
        // Mtime changed -> self-invalidation verified!
        auto staleLookup = mediaCache.getMetadata(key2);
        QVERIFY(!staleLookup.has_value());
    }
}

void TestMediaCache::testMediaCacheThumbnailByteTracking()
{
    MediaCache mediaCache(256, 50, 64LL * 1024 * 1024);

    // Create a 100x100 32-bit test pixmap (100 * 100 * 4 = 40,000 bytes)
    QPixmap pix(100, 100);
    pix.fill(Qt::cyan);

    const CacheKey key{"sample.mp4", 500, 1500};
    mediaCache.putThumbnail(key, pix);

    QCOMPARE(mediaCache.thumbnailCount(), 1);
    QCOMPARE(mediaCache.thumbnailBytes(), 40000LL);

    auto cached = mediaCache.getThumbnail(key);
    QVERIFY(cached.has_value());
    QCOMPARE(cached->width(), 100);
    QCOMPARE(cached->height(), 100);

    mediaCache.clear();
    QCOMPARE(mediaCache.thumbnailCount(), 0);
    QCOMPARE(mediaCache.thumbnailBytes(), 0LL);
}

QTEST_MAIN(TestMediaCache)
#include "test_media_cache.moc"
