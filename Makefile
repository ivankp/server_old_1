.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CPPFLAGS := -std=c++20 -I../include -Iinclude
CXXFLAGS := -Wall -O3 -flto -fmax-errors=3
# CXXFLAGS := -Wall -O0 -g -fmax-errors=3

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

#####################################################################

all: $(patsubst %, bin/%, \
  myserver user \
)

#####################################################################

bin/myserver: $(patsubst %, .build/%.o, \
  server file_desc http websocket base64 users \
)
LF_myserver := -pthread
L_myserver := -lssl -lcrypto

bin/user: $(patsubst %, .build/%.o, \
  users \
)

#####################################################################

.PRECIOUS: .build/%.o

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

lib/lib%.so: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) -shared $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.cc,$^) -o $@

-include $(shell [ -d '.build' ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -rfv bin lib .build

