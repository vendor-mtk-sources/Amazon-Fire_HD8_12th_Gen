subdir-ccflags-y += -Wall -Werror
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Wno-implicit-fallthrough

obj-$(CONFIG_TCPC_MT6360)		+= tcpc_mt6360.o
obj-$(CONFIG_TCPC_MT6362)		+= tcpc_mt6362.o
obj-$(CONFIG_TCPC_MT6370)		+= tcpc_mt6370.o
obj-$(CONFIG_TCPC_RT1711H)		+= tcpc_rt1711h.o

tcpc_class-objs := tcpci_core.o tcpci_typec.o tcpci_timer.o tcpm.o tcpci.o \
		   tcpci_dual_role.o
ifdef CONFIG_TCPC_ALERT_V20
tcpc_class-objs += tcpci_alert_v20.o
else
tcpc_class-objs += tcpci_alert.o
endif

ifdef CONFIG_USB_POWER_DELIVERY
tcpc_class-objs += tcpci_event.o \
		pd_core.o pd_policy_engine.o pd_process_evt.o \
		pd_dpm_core.o pd_dpm_uvdm.o pd_dpm_alt_mode_dp.o pd_dpm_pdo_select.o\
		pd_dpm_reaction.o \
		pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
		pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
		pd_process_evt_dbg.o pd_process_evt_tcp.o pd_process_evt_com.o \
		pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o pd_policy_engine_vcs.o \
		pd_policy_engine_dfp.o pd_policy_engine_dr.o pd_policy_engine_drs.o pd_policy_engine_prs.o \
		pd_policy_engine_dbg.o pd_policy_engine_com.o pd_dpm_alt_mode_dc.o
endif
obj-$(CONFIG_TCPC_CLASS)		+= tcpc_class.o tcpci_late_sync.o pd_dbg_info.o
