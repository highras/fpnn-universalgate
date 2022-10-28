EXES_SERVER = UniversalGate

FPNN_DIR = ../fpnn
DEPLOYMENT_DIR = ../deployment/rpm
CFLAGS +=
CXXFLAGS +=
CPPFLAGS += -I$(FPNN_DIR)/core -I$(FPNN_DIR)/proto -I$(FPNN_DIR)/base -I$(FPNN_DIR)/proto/msgpack -I$(FPNN_DIR)/proto/rapidjson -I$(FPNN_DIR)/extends
LIBS += -L$(FPNN_DIR)/core -L$(FPNN_DIR)/proto -L$(FPNN_DIR)/base -lfpnn  -L$(FPNN_DIR)/extends -lextends

OBJS_SERVER = UniversalProxyGate.o ProxyGateProcessor.o

all: $(EXES_SERVER)

deploy:
	cp -rf $(EXES_SERVER) $(DEPLOYMENT_DIR)/bin/
	cp -rf universalProxyGate.conf $(DEPLOYMENT_DIR)/conf/

clean:
	$(RM) *.o $(EXES_SERVER)
include $(FPNN_DIR)/def.mk
