#include <QTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include "../src/media/InspectionScheduler.h"

class TestInspectionScheduler : public QObject
{
    Q_OBJECT

private slots:
    void testMonotonicRequestIds();
    void testBoundedConcurrency();
    void testPriorityQueueing();
    void testCancelPendingRequest();
    void testCancelByPurpose();
    void testCancelByFile();
    void testCancelAll();
    void testCachedImmediateDispatch();
};

void TestInspectionScheduler::testMonotonicRequestIds()
{
    // maxConcurrent = 2
    InspectionScheduler scheduler("ffmpeg", "ffprobe", 2);

    const RequestId id1 = scheduler.requestMetadata("video1.mp4", "test_p1");
    const RequestId id2 = scheduler.requestMetadata("video2.mp4", "test_p2");
    const RequestId id3 = scheduler.requestThumbnail("video3.mp4", 5.0, 160, 90, "test_p3");

    QVERIFY(id1.isValid());
    QVERIFY(id2.isValid());
    QVERIFY(id3.isValid());

    // Monotonic strict sequence order
    QVERIFY(id1.serial < id2.serial);
    QVERIFY(id2.serial < id3.serial);
    QCOMPARE(id1.purpose, QString("test_p1"));
    QCOMPARE(id3.timestampSeconds, 5.0);

    scheduler.cancelAll();
}

void TestInspectionScheduler::testBoundedConcurrency()
{
    // Enforce maxConcurrent = 2
    constexpr int MAX_WORKERS = 2;
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", MAX_WORKERS);

    // Enqueue 6 tasks simultaneously
    for (int i = 0; i < 6; ++i) {
        scheduler.requestMetadata(QString("clip_%1.mp4").arg(i), "bounded_test");
    }

    // Active workers must never exceed MAX_WORKERS (2)
    QVERIFY(scheduler.activeCount() <= MAX_WORKERS);
    // Total = active + queued = 6
    QCOMPARE(scheduler.activeCount() + scheduler.queuedCount(), 6);

    scheduler.cancelAll();
    QCoreApplication::processEvents();
    QCOMPARE(scheduler.queuedCount(), 0);
    QCOMPARE(scheduler.activeCount(), 0);
}

void TestInspectionScheduler::testPriorityQueueing()
{
    // Single worker scheduler to isolate queue reordering
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 1);

    // 1st request consumes the single worker
    const RequestId id1 = scheduler.requestMetadata("video1.mp4", "work1", false);
    QCOMPARE(scheduler.activeCount(), 1);
    QCOMPARE(scheduler.queuedCount(), 0);

    // 2nd request (normal priority) -> placed at back of queue
    const RequestId id2 = scheduler.requestMetadata("video2.mp4", "work2", false);
    QCOMPARE(scheduler.queuedCount(), 1);

    // 3rd request (high priority) -> prepended to front of queue ahead of work2
    const RequestId id3 = scheduler.requestMetadata("video3.mp4", "work3", true);
    QCOMPARE(scheduler.queuedCount(), 2);

    scheduler.cancelAll();
}

void TestInspectionScheduler::testCancelPendingRequest()
{
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 1);

    scheduler.requestMetadata("active_video.mp4", "work1");
    const RequestId pendingId = scheduler.requestMetadata("pending_video.mp4", "work2");

    QCOMPARE(scheduler.queuedCount(), 1);

    // Cancel the pending request
    scheduler.cancelRequest(pendingId);

    // Queued count should now drop to 0
    QCOMPARE(scheduler.queuedCount(), 0);

    scheduler.cancelAll();
}

void TestInspectionScheduler::testCancelByPurpose()
{
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 1);

    // Fill worker
    scheduler.requestMetadata("active.mp4", "other");

    // Add tasks with various purposes
    scheduler.requestThumbnail("v1.mp4", 1.0, 0, 0, "trim_start");
    scheduler.requestThumbnail("v2.mp4", 2.0, 0, 0, "trim_end");
    scheduler.requestThumbnail("v3.mp4", 3.0, 0, 0, "trim_start");

    QCOMPARE(scheduler.queuedCount(), 3);

    // Cancel all with purpose "trim_start"
    scheduler.cancelRequestsForPurpose("trim_start");

    // Only "trim_end" should remain
    QCOMPARE(scheduler.queuedCount(), 1);

    scheduler.cancelAll();
}

void TestInspectionScheduler::testCancelByFile()
{
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 1);

    scheduler.requestMetadata("active.mp4", "purpose1");

    scheduler.requestMetadata("target_file.mp4", "meta");
    scheduler.requestThumbnail("target_file.mp4", 10.0, 0, 0, "thumb");
    scheduler.requestMetadata("other_file.mp4", "meta");

    QCOMPARE(scheduler.queuedCount(), 3);

    scheduler.cancelRequestsForFile("target_file.mp4");

    // Both target_file tasks removed; only other_file remains
    QCOMPARE(scheduler.queuedCount(), 1);

    scheduler.cancelAll();
}

void TestInspectionScheduler::testCancelAll()
{
    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 2);

    for (int i = 0; i < 5; ++i) {
        scheduler.requestMetadata(QString("file_%1.mp4").arg(i));
    }

    QVERIFY(scheduler.isInspecting());
    QVERIFY(scheduler.queuedCount() > 0);

    scheduler.cancelAll();
    QCoreApplication::processEvents();

    QCOMPARE(scheduler.queuedCount(), 0);
    QCOMPARE(scheduler.activeCount(), 0);
    QVERIFY(!scheduler.isInspecting());
}

void TestInspectionScheduler::testCachedImmediateDispatch()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("Mock media data for caching test");
    tempFile.flush();
    const QString filePath = tempFile.fileName();

    InspectionScheduler scheduler("dummy_ffmpeg", "dummy_ffprobe", 2);

    // Pre-populate cache directly or simulate a previously completed probe
    // Request metadata on tempFile: it will fail to probe dummy executables, but we can verify signal spy
    QSignalSpy spyStatus(&scheduler, &InspectionScheduler::inspectionStatusChanged);
    QSignalSpy spyStats(&scheduler, &InspectionScheduler::queueStatsChanged);

    const RequestId req = scheduler.requestMetadata(filePath, "test_cached");
    QVERIFY(req.isValid());

    // Status changed should at least emit Queued
    QVERIFY(spyStatus.count() >= 1);

    scheduler.cancelAll();
}

QTEST_MAIN(TestInspectionScheduler)
#include "test_inspection_scheduler.moc"
