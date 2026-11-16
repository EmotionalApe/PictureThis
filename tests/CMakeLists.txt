find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# Test 1: MediaCache invariants (LRU eviction, byte budgeting, mtime self-invalidation)
add_executable(test_media_cache
    test_media_cache.cpp
    ../src/media/MediaCache.cpp
)
target_link_libraries(test_media_cache PRIVATE
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME test_media_cache COMMAND test_media_cache)

# Test 2: InspectionScheduler invariants (bounded concurrency, priority queueing, stale cancellations)
add_executable(test_inspection_scheduler
    test_inspection_scheduler.cpp
    ../src/media/InspectionScheduler.cpp
    ../src/media/InspectionWorker.cpp
    ../src/media/MediaCache.cpp
)
target_link_libraries(test_inspection_scheduler PRIVATE
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME test_inspection_scheduler COMMAND test_inspection_scheduler)
