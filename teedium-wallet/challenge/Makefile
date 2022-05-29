CFG_TA_MBEDTLS ?= y
CFG_TEE_TA_LOG_LEVEL ?= 4

# The UUID for the Trusted Application
BINARY=7dc089d2-883b-4f7b-8154-ea1db9f1e7c3

-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
	@echo 'Note: TA_DEV_KIT_DIR=$(TA_DEV_KIT_DIR)'
endif
