target_sources(oc_objects PRIVATE
    Extras.cpp
    decode.cpp
    kills.cpp
)
target_link_libraries(oc_objects PUBLIC winmm mfplat mfreadwrite mfuuid ole32)
