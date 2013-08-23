all:
		CXXFLAGS='-fpermissive -fexceptions' node-gyp --debug configure build

clean:
		node-gyp clean
