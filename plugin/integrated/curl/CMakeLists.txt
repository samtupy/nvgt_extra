if(NVGT_TARGET STREQUAL "windows")
	set(libraries kernel32 user32 crypt32 advapi32 iphlpapi netapi32 uuid wldap32 ws2_32 normaliz bcrypt curl zs secur32)
elseif(NVGT_TARGET STREQUAL "macos")
	set(libraries curl z ssl crypto)
	set(link_options -Wl,-undefined,error)
else()
	set(libraries :libcurl.a :libz.a :libssl.a :libcrypto.a)
	set(link_options -Wl,--no-undefined)
endif()
nvgt_add_plugin(nvgt_curl
	SOURCES internet.cpp
	ADDONS scriptarray
	DEFINITIONS CURL_STATICLIB
	SHARED_LIBRARIES ${libraries}
	STATIC_LIBRARIES curl
	SHARED_LINK_OPTIONS ${link_options}
	FRAMEWORKS Foundation Security SystemConfiguration
)
