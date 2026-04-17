if(UNIX)
	target_sources(qterm
		PRIVATE
			qtermlocalbackend.cpp
	)
endif()