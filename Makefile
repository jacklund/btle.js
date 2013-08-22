all:
		CXXFLAGS=-fpermissive node-gyp --debug configure build

clean:
		node-gyp clean
