set(SRCS	BannerLoader.cpp
			BannerLoaderGC.cpp
			BannerLoaderWii.cpp
			Blob.cpp
			CISOBlob.cpp
			WbfsBlob.cpp
			CompressedBlob.cpp
			DiscScrubber.cpp
			DriveBlob.cpp
			FileBlob.cpp
			FileMonitor.cpp
			FileSystemGCWii.cpp
			FileSystemWiiU.cpp			
			Filesystem.cpp
			NANDContentLoader.cpp
			VolumeCommon.cpp
			VolumeCreator.cpp
			VolumeDirectory.cpp
			VolumeGC.cpp
			VolumeWad.cpp
			VolumeWiiCrypted.cpp
			VolumeWiiU.cpp
			VolumeWiiUCrypted.cpp
			WiiWad.cpp)

add_dolphin_library(discio "${SRCS}" "")
