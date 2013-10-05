all:
		CXXFLAGS='-fpermissive -fexceptions' node-gyp configure build

debug:
		CXXFLAGS='-fpermissive -fexceptions' node-gyp configure build --debug

clean:
		node-gyp clean
