PROJECT=tunsafe

CROSS_PREFIX :=
CXX=$(CROSS_PREFIX)g++
STRIP=$(CROSS_PREFIX)strip
CCFLAGS = -O3 -DNDEBUG -I . $(CFLAGS) -DWITH_NETWORK_BSD=1 
LLDFLAGS = $(LDFLAGS)

SOCKS5_DIR := third_party/hev-socks5-tunnel
SOCKS5_TUNNEL_LIB := $(SOCKS5_DIR)/bin/libhev-socks5-tunnel.a
SOCKS5_TASK_LIB := $(SOCKS5_DIR)/third-part/hev-task-system/bin/libhev-task-system.a
SOCKS5_LWIP_LIB := $(SOCKS5_DIR)/third-part/lwip/bin/liblwip.a
SOCKS5_YAML_LIB := $(SOCKS5_DIR)/third-part/yaml/bin/libyaml.a
SOCKS5_LIBS := $(SOCKS5_TUNNEL_LIB) $(SOCKS5_TASK_LIB) $(SOCKS5_LWIP_LIB) $(SOCKS5_YAML_LIB)

TARGET ?= Linux
ifeq ($(TARGET), Linux)
    CCFLAGS += -ffunction-sections -fdata-sections
    LLDFLAGS += -Wl,--gc-sections -lrt -pthread -latomic -ldl
else ifeq ($(TARGET), FreeBSD)
    CCFLAGS += -mssse3
    LLDFLAGS += -lrt -pthread
else ifeq ($(TARGET), Darwin)
    CCFLAGS += -Wno-deprecated-declarations -fno-exceptions -fno-rtti -ffunction-sections
    LLDFLAGS += 
else ifeq ($(TARGET), Windows)
    CCFLAGS += 
    LLDFLAGS += 
else
    $(未知的系统: $(TARGET))
endif
	
ARCH ?=	

BUILDMSG="\e[1;31mBUILD\e[0m %s\n"
LINKMSG="\e[1;34mLINK\e[0m  \e[1;32m%s\e[0m\n"
STRIPMSG="\e[1;34mSTRIP\e[0m \e[1;32m%s\e[0m\n"
CLEANMSG="\e[1;34mCLEAN\e[0m %s\n"

ENABLE_STATIC :=
ifeq ($(ENABLE_STATIC),1)
	CCFLAGS+=-static
endif

V :=
ECHO_PREFIX := @
ifeq ($(V),1)
	undefine ECHO_PREFIX
endif

all: tunsafe

tunsafe: clean
	@printf $(BUILDMSG) tunsafe
	$(ECHO_PREFIX) if [ "$(TARGET)" = "Linux" ]; then \
		$(MAKE) --no-print-directory -C $(SOCKS5_DIR) static CROSS_PREFIX=$(CROSS_PREFIX); \
	fi
	$(ECHO_PREFIX) if [ "$(TARGET)" = "Linux" ]; then \
		if [ "$(ARCH)" = "x86" ]; then \
			$(CXX) $(CCFLAGS) -o tunsafe tunsafe_amalgam.cpp $(SOCKS5_LIBS) $(LLDFLAGS) crypto/aesgcm/aesni_gcm-x64-linux.s crypto/aesgcm/aesni-x64-linux.s crypto/aesgcm/ghash-x64-linux.s crypto/poly1305/poly1305-x64-linux.s crypto/chacha20/chacha20-x64-linux.s ; \
		elif [ "$(ARCH)" = "arm" ]; then \
			$(CXX) $(CCFLAGS) -D__ARM_NEON -o tunsafe tunsafe_amalgam.cpp $(SOCKS5_LIBS) $(LLDFLAGS) crypto/poly1305/poly1305-arm-linux.S crypto/chacha20/chacha20-arm-linux.S ; \
		elif [ "$(ARCH)" = "arm64" ]; then \
			$(CXX) $(CCFLAGS) -o tunsafe tunsafe_amalgam.cpp $(SOCKS5_LIBS) $(LLDFLAGS) crypto/poly1305/poly1305-arm64-linux.S crypto/chacha20/chacha20-arm64-linux.S ; \
		elif [ "$(ARCH)" = "mips" ]; then \
			$(CXX) $(CCFLAGS) -march=mips32r2 -o tunsafe tunsafe_amalgam.cpp $(SOCKS5_LIBS) $(LLDFLAGS) crypto/poly1305/poly1305-mips32.S crypto/chacha20/chacha20-mips.S ; \
		else \
			echo "未知的CPU架构: $(ARCH)" ; \
			exit 1 ; \
		fi ; \
	fi
	@printf $(STRIPMSG) tunsafe
	$(ECHO_PREFIX) $(STRIP) tunsafe
	$(ECHO_PREFIX) if command -v file >/dev/null 2>&1; then file tunsafe; fi
clean:
	$(ECHO_PREFIX) rm -f tunsafe *.o
	$(ECHO_PREFIX) if [ -d "$(SOCKS5_DIR)" ]; then \
		$(MAKE) --no-print-directory -C $(SOCKS5_DIR) clean; \
	fi
	@printf $(CLEANMSG) $(PROJECT)

