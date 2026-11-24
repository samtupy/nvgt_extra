nvgt_add_plugin(unicode
	STATIC_NAME unicode_plugin
	SOURCES unicode.cpp ../../dep/uni_algo/data.cpp
	ADDONS scriptarray
	INCLUDE_DIRECTORIES ../../dep
)
