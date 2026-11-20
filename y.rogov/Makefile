UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),SunOS)
    LDLIBS += -lsocket -lnsl
endif

all: build/1.out\
     build/2.out\
     build/3.out\
     build/4.out\
	 build/5.out\
	 build/6.out\
	 build/7.out\
	 build/9.out\
	 build/10.out\
	 build/17.out\
	 build/18.out\
	 build/21.out\
	 build/25.out\
	 build/30_client.out\
	 build/30_server.out\
	 build/31_client.out\
	 build/31_server.out\
	 build/32_client.out\
	 build/32_server.out\

build/%.out : %/main.c
	@mkdir -p build
	cc $< -o $@ $(LDLIBS)

build/%_client.out : %/client.c
	@mkdir -p build
	cc $< -o $@ $(LDLIBS)

build/%_server.out : %/server.c
	@mkdir -p build
	cc $< -o $@ $(LDLIBS)

clean: 
	rm -r build

