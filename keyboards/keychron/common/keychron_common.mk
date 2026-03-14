OPT_DEFS += -DAPDAPTIVE_NKRO_ENABLE

KEYCHRON_COMMON_DIR = common
SRC += \
    $(KEYCHRON_COMMON_DIR)/keychron_task.c \
    $(KEYCHRON_COMMON_DIR)/keychron_common.c \

VPATH += $(TOP_DIR)/keyboards/keychron/$(KEYCHRON_COMMON_DIR)

