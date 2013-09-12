all:
		CXXFLAGS='-fpermissive -fexceptions' node-gyp configure build

clean:
		node-gyp clean
