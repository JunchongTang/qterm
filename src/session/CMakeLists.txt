target_sources(qterm
    PRIVATE
        qtermsessionbackend.cpp
        qtermsession.cpp
)

add_subdirectory(local)
add_subdirectory(remote)