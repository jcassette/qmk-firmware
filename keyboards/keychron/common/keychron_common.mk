OPT_DEFS += -DAPDAPTIVE_NKRO_ENABLE

KEYCHRON_COMMON_DIR = common
SRC += \
    $(KEYCHRON_COMMON_DIR)/keychron_task.c \
    $(KEYCHRON_COMMON_DIR)/keychron_common.c \

FACTORY_TEST_ENABLE ?= no
ifeq ($(strip $(FACTORY_TEST_ENABLE)), yes)
    OPT_DEFS += -DFACTORY_TEST_ENABLE
    SRC += $(KEYCHRON_COMMON_DIR)/factory_test.c
endif

VPATH += $(TOP_DIR)/keyboards/keychron/$(KEYCHRON_COMMON_DIR)

