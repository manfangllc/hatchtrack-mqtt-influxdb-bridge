DIR := $(CURDIR)

CC := gcc
AR := ar
CFLAGS_DEFAULT := -ggdb3 -O0 -Wall -Wshadow
CFLAGS_APP := $(CFLAGS_DEFAULT)
CFLAGS_AWS = $(CFLAGS_DEFAULT)
CFLAGS_AWS += -I $(BUILD_AWS_SDK_INC_DIR) -I $(APP_INC_DIR) -I ./build/include/
LDFLAGS_APP = -L $(BUILD_APP_DEP_LIB_DIR)
LDFLAGS_APP += -lmbedtls -lmbedcrypto -lmbedx509 -lawssdk

BUILD_APP_BIN := hatchtrack-mqtt-influxdb-bridge
BUILD_APP_OBJ_DIR := ./obj
BUILD_APP_BIN_DIR := ./build/bin
BUILD_APP_DEP_LIB_DIR := ./build/lib
BUILD_MBEDTLS_LIB := libmbedtls.a
BUILD_MBEDTLS_INC_DIR := ./build/include/mbedtls
BUILD_MBEDTLS_LIB_DIR := $(BUILD_APP_DEP_LIB_DIR)
BUILD_AWS_SDK_LIB := libawssdk.a
BUILD_AWS_SDK_INC_DIR := ./build/include/aws-sdk
BUILD_AWS_SDK_OBJ_DIR := ./obj
BUILD_AWS_SDK_LIB_DIR := $(BUILD_APP_DEP_LIB_DIR)

APP_SRC_DIR := ./src
APP_INC_DIR := ./inc
APP_SRC := $(wildcard $(APP_SRC_DIR)/*.c)
APP_OBJ := $(addsuffix .o, $(basename $(addprefix $(BUILD_APP_OBJ_DIR)/, $(notdir $(APP_SRC)))))
APP_LIBS = $(BUILD_APP_DEP_LIB_DIR)/libawssdk.a
APP_LIBS += $(BUILD_APP_DEP_LIB_DIR)/libmbedcrypto.a
APP_LIBS += $(BUILD_APP_DEP_LIB_DIR)/libmbedtls.a
APP_LIBS += $(BUILD_APP_DEP_LIB_DIR)/libmbedx509.a
APP_LIBS += $(BUILD_APP_DEP_LIB_DIR)/libmbedx509.a
APP_LIBS += $(BUILD_APP_DEP_LIB_DIR)/libcjson.a

AWS_DIR = $(DIR)/lib/aws-iot-device-sdk-embedded-C
AWS_SRC_DIR := $(AWS_DIR)/src
AWS_INC_DIR := $(AWS_DIR)/include
AWS_SRC := $(wildcard $(AWS_SRC_DIR)/*.c)
AWS_OBJ := $(addsuffix .o, $(basename $(addprefix ./obj/, $(notdir $(AWS_SRC)))))
AWS_INC := $(wildcard $(AWS_INC_DIR)/*.h)
AWS_INC_CP := $(addprefix $(BUILD_AWS_SDK_INC_DIR)/, $(notdir $(AWS_INC)))

AWS_JSMN_SRC_DIR := $(AWS_DIR)/external_libs/jsmn
AWS_JSMN_INC_DIR := $(AWS_DIR)/external_libs/jsmn
AWS_JSMN_SRC := $(wildcard $(AWS_JSMN_SRC_DIR)/*.c)
AWS_JSMN_OBJ := $(addsuffix .o, $(basename $(addprefix ./obj/, $(notdir $(AWS_JSMN_SRC)))))
AWS_JSMN_INC := $(wildcard $(AWS_JSMN_INC_DIR)/*.h)
AWS_JSMN_INC_CP := $(addprefix $(BUILD_AWS_SDK_INC_DIR)/, $(notdir $(AWS_JSMN_INC)))

AWS_MBEDTLS_SRC_DIR := $(AWS_DIR)/platform/linux/mbedtls
AWS_MBEDTLS_INC_DIR := $(AWS_DIR)/platform/linux/mbedtls
AWS_MBEDTLS_SRC := $(wildcard $(AWS_MBEDTLS_SRC_DIR)/*.c)
AWS_MBEDTLS_OBJ:= $(addsuffix .o, $(basename $(addprefix ./obj/, $(notdir $(AWS_MBEDTLS_SRC)))))
AWS_MBEDTLS_INC := $(wildcard $(AWS_MBEDTLS_INC_DIR)/*.h)
AWS_MBEDTLS_INC_CP := $(addprefix $(BUILD_AWS_SDK_INC_DIR)/, $(notdir $(AWS_MBEDTLS_INC)))

AWS_COMMON_SRC_DIR := $(AWS_DIR)/platform/linux/common
AWS_COMMON_INC_DIR := $(AWS_DIR)/platform/linux/common
AWS_COMMON_SRC := $(wildcard $(AWS_COMMON_SRC_DIR)/*.c)
AWS_COMMON_OBJ := $(addsuffix .o, $(basename $(addprefix ./obj/, $(notdir $(AWS_COMMON_SRC)))))
AWS_COMMON_INC := $(wildcard $(AWS_COMMON_INC_DIR)/*.h)
AWS_COMMON_INC_CP := $(addprefix $(BUILD_AWS_SDK_INC_DIR)/, $(notdir $(AWS_COMMON_INC)))

.PHONY: all mbedtls-patch

all: mbedtls cjson aws-sdk app

app: app-bin-dir $(APP_OBJ) $(BUILD_APP_BIN_DIR)/$(BUILD_APP_BIN)
	@echo "built $(BUILD_APP_BIN_DIR)/$(BUILD_APP_BIN)" 

app-bin-dir:
	@mkdir -v -p $(BUILD_APP_BIN_DIR)

aws-sdk: aws-sdk-inc aws-sdk-lib
	@echo "static library installed to $(BUILD_AWS_SDK_LIB_DIR)"

aws-sdk-lib: $(AWS_OBJ) $(AWS_JSMN_OBJ) $(AWS_MBEDTLS_OBJ) $(AWS_COMMON_OBJ)
	$(AR) rcs $(BUILD_AWS_SDK_LIB_DIR)/$(BUILD_AWS_SDK_LIB) $^

aws-sdk-inc: aws-sdk-inc-dir $(AWS_INC_CP) $(AWS_JSMN_INC_CP) $(AWS_MBEDTLS_INC_CP) $(AWS_COMMON_INC_CP)
	@echo "headers copied to $(BUILD_AWS_SDK_INC_DIR)"

aws-sdk-inc-dir:
	@mkdir -v -p $(BUILD_AWS_SDK_INC_DIR)

mbedtls: mbedtls-patch
	$(MAKE) install DESTDIR=$(DIR)/build -C $(DIR)/lib/mbedtls

mbedtls-patch:
	$(SHELL) -c '$(DIR)/scripts/patch-mbedtls.sh'

cjson:
	$(MAKE) -C $(DIR)/lib/cJSON
	cp $(DIR)/lib/cJSON/libcjson.a ./build/lib/
	cp $(DIR)/lib/cJSON/cJSON.h ./build/include/

clean:
	@rm -rfv ./build/bin
	@rm -rfv ./build/lib
	@rm -rfv ./build/include/
	@rm -rfv ./obj/*.o
	$(MAKE) -C ./lib/cJSON clean
	# undo patch
	$(SHELL) -c 'cd ./lib/mbedtls; git reset --hard HEAD'

$(BUILD_APP_BIN_DIR)/$(BUILD_APP_BIN): $(APP_OBJ)
	$(CC) -o $@ $^ $(APP_LIBS) $(CFLAGS_APP) $(LDFLAGS_APP)

$(BUILD_APP_OBJ_DIR)/%.o : $(APP_SRC_DIR)/%.c
	$(CC) $(CFLAGS_AWS) -c $< -o $@

$(BUILD_AWS_SDK_OBJ_DIR)/%.o : $(AWS_SRC_DIR)/%.c
	$(CC) $(CFLAGS_AWS) -c $< -o $@

$(BUILD_AWS_SDK_OBJ_DIR)/%.o : $(AWS_JSMN_SRC_DIR)/%.c
	$(CC) $(CFLAGS_AWS) -c $< -o $@

$(BUILD_AWS_SDK_OBJ_DIR)/%.o : $(AWS_MBEDTLS_SRC_DIR)/%.c
	$(CC) $(CFLAGS_AWS) -c $< -o $@

$(BUILD_AWS_SDK_OBJ_DIR)/%.o : $(AWS_COMMON_SRC_DIR)/%.c
	$(CC) $(CFLAGS_AWS) -c $< -o $@

$(BUILD_AWS_SDK_INC_DIR)/%.h : $(AWS_INC_DIR)/%.h
	@cp -v $< $@

$(BUILD_AWS_SDK_INC_DIR)/%.h : $(AWS_JSMN_INC_DIR)/%.h
	@cp -v $< $@

$(BUILD_AWS_SDK_INC_DIR)/%.h : $(AWS_MBEDTLS_INC_DIR)/%.h
	@cp -v $< $@

$(BUILD_AWS_SDK_INC_DIR)/%.h : $(AWS_COMMON_INC_DIR)/%.h
	@cp -v $< $@
