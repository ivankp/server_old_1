.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CXX = g++
CC = gcc
AS = gcc

CFLAGS := -Wall -O3 -flto -fmax-errors=3 -Iinclude
# CFLAGS := -Wall -O0 -g -fmax-errors=3 -Iinclude
CXXFLAGS := -std=c++20 $(CFLAGS)

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

#####################################################################

all: $(patsubst %, bin/%, \
  myserver user \
)

#####################################################################

bin/myserver: $(patsubst %, .build/%.o, \
  file_desc whole_file base64 \
  $(patsubst %, server/%, server http websocket users) \
) lib/libbcrypt.so
LF_myserver := -pthread -Llib -Wl,-rpath=lib
L_myserver := -lssl -lcrypto -lbcrypt

bin/user: .build/server/users.o lib/libbcrypt.so
# C_user := -DNDEBUG
LF_user := -Llib -Wl,-rpath=lib
L_user := -lbcrypt

C__bcrypt := -fomit-frame-pointer -funroll-loops
C_bcrypt/blowfish := $(C__bcrypt) -fPIC
C_bcrypt/gensalt := $(C__bcrypt) -fPIC
C_bcrypt/wrapper := $(C__bcrypt) -fPIC
C_bcrypt/x86 := -fPIC
lib/libbcrypt.so: $(patsubst %, .build/bcrypt/%.o, \
  x86 blowfish gensalt wrapper )

lib/libbcrypt.so: CXX = $(CC)

#####################################################################

.PRECIOUS: .build/%.o lib/lib%.so

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

lib/lib%.so:
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) -shared $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.cc,$^) -o $@

.build/%.o: src/%.c
	@mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.c,$^) -o $@

.build/%.o: src/%.S
	@mkdir -pv $(dir $@)
	$(AS) $(C_$*) -c $(filter %.S,$^) -o $@

-include $(shell [ -d '.build' ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -rfv bin lib .build

