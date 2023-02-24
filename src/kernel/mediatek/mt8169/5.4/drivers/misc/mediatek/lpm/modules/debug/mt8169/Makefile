LPM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/lpm
LPM_MODULES_FOLDER=$(LPM_BUILD_FOLDER)/modules/

MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3

ccflags-y += -I$(LPM_BUILD_FOLDER)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/
ccflags-y += -I$(LPM_MODULES_FOLDER)/common/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/mt8169/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/mt8169/

ccflags-y += -I$(LPM_BUILD_FOLDER)/mtk_lpm_fs/
include $(LPM_BUILD_FOLDER)/mtk_lpm_fs/Makefile
MTK_LPM_FS = $(mtk_lpm_fs_objs:%.o=../../../mtk_lpm_fs/%.o)

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt8169
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

${BUILD_LPM_DBG_MODUDLE_NAME}-objs += $(MTK_LPM_FS)

${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt8169_lpm_compat.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt8169_dbg_init.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt8169_dbg_spm_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt8169_lpm_logger.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_dbg_rc_fs_mt8169.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_dbg_fs_common_mt8169.o
