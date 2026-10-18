CXX ?= c++
CC ?= cc
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-missing-field-initializers -Wno-implicit-fallthrough
CFLAGS ?= -O2 -Wall -Wextra
INCLUDES = -Isupport -I../common/include -I../lib/inih/include
HEADERS = $(wildcard ../common/include/*.h*) support/switch.h

.PHONY: test clean
test: build/fake_hdr_test build/shadow_gamma_test build/config_test
	./build/fake_hdr_test
	./build/shadow_gamma_test
	./build/config_test

build/fake_hdr_test: fake_hdr_test.cpp ../common/src/color.cpp ../common/src/config_parse.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) fake_hdr_test.cpp ../common/src/color.cpp ../common/src/config_parse.cpp -o $@

build/shadow_gamma_test: shadow_gamma_test.cpp ../common/src/color.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) shadow_gamma_test.cpp ../common/src/color.cpp -o $@

build/ini.o: ../lib/inih/inih/ini.c ../lib/inih/inih/ini.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DINI_MAX_LINE=256 -c $< -o $@

build/config_test: config_test.cpp ../common/src/config.cpp ../common/src/config_parse.cpp build/ini.o $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) config_test.cpp ../common/src/config.cpp ../common/src/config_parse.cpp build/ini.o -o $@

clean:
	rm -rf build
