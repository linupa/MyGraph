cflags= -g -arch x86_64 \
		-isysroot /Developer/SDKs/MacOSX10.6.sdk -Wl,-search_paths_first -Wl,-headerpad_max_install_names \
		-framework Carbon -framework Cocoa -framework ApplicationServices /usr/local/lib/libfltk.a -lpthread

graph: graph.cpp
	g++ -o $@ $^ $(cflags)

