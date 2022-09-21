BUILDTYPE ?= Release

.PHONY: all
ifeq ($(BUILDTYPE),Release)
all: aworker
else
all: aworker aworker_g
endif

include ../build/Makefiles/toolchain.mk

.PHONY: aworker aworker_g
aworker: build_type:=Release
aworker_g: build_type:=Debug
aworker aworker_g:
	$(MAKE) -C ../build $@

build:
	$(MAKE) -C $(BUILD_PROJ_DIR) BUILDTYPE=$(BUILDTYPE) anc turf
#	prepare alice native addons and alice/node_modules
	$(MAKE) -C $(REPO_ROOT)/alice BUILDTYPE=$(BUILDTYPE) all node_modules

.PHONY: compile_commands.json
compile_commands.json:
	$(MAKE) -C ../build configure
	if [ ! -r $@ -o ! -L $@ ]; then \
		ln -fs ../build/out/${BUILDTYPE}/$@ $@; fi

.PHONY: lint
lint: cpplint jslint

CPP_FILES=$(shell find src test tools -type f \
			! -name '*.pb.cc' -name '*.cc' \
			-or ! -name '*.pb.h' ! -name 'root_certs.h' -name '*.h')
C_FILES=$(shell find src test -type f -name '*.c')
clang-format: $(CPP_FILES) $(C_FILES)
	$(CLANG_FORMAT) -i --style=file $(CPP_FILES) $(C_FILES)
cpplint: $(CPP_FILES) $(C_FILES)
	$(CPPLINT) --root=. $(CPP_FILES) $(C_FILES)

.PHONY: jslint
jslint: $(BUILD_NODE_MODULES)
	$(ESLINT) --report-unused-disable-directives .

TEST_FILES=$(shell find test -type d -name .tmp -prune -false -o \
			-type f -name '*.js')
jstest: build $(TEST_FILES)
ifeq ($(BUILDTYPE), Debug)
jstest: export PATH:=$(BUILD_PROJ_DIR)/out/Debug:$(PATH)
jstest: export NATIVE_DEBUG=1
jstest: export ALICE_LOG_LEVEL=Debug
jstest: export ALICE_SOCK_CONN_TIMEOUT=30000
else
jstest: export PATH:=$(BUILD_PROJ_DIR)/out/Release:$(PATH)
endif
jstest:
	@rm -rf test/.tmp/*
	env BUILDTYPE=$(BUILDTYPE) node --unhandled-rejections=strict tools/test.js

CCTEST_FLAGS=
CCTEST=$(REPO_ROOT)/build/out/$(BUILDTYPE)/aworker_cctest
cctest: build-cctest
	cd test/cctest; $(CCTEST) $(CCTEST_FLAGS)
cctest-sanity: build-cctest
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all $(CCTEST) $(CCTEST_FLAGS)
ifeq ($(BUILDTYPE), Debug)
build-cctest:
	$(MAKE) -C ../build aworker_cctest_g
else
build-cctest:
	$(MAKE) -C ../build aworker_cctest
endif

.PHONY: benchmarktest
ifeq ($(BUILDTYPE), Debug)
benchmarktest: export PATH:=$(BUILD_PROJ_DIR)/out/Debug:$(PATH)
benchmarktest: export NATIVE_DEBUG=1
else
benchmarktest: export PATH:=$(BUILD_PROJ_DIR)/out/Release:$(PATH)
endif
benchmarktest: build clean-test
	node benchmark/run test all

.PHONY: benchmark
benchmark: build clean-test
	node benchmark/run --format csv all

.PHONY: test sanitytest
test: clean-test cctest jstest benchmarktest
sanitytest: cctest jstest

clean-test:
	$(RM) -rf test/.tmp/*
	$(RM) -rf benchmark/.tmp/*
