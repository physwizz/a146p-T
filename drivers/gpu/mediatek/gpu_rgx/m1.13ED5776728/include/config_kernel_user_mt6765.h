/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define PVRSRV_ENABLE_CCCB_GROW
#define RGX_FW_FILENAME "rgx.fw"
#define RGX_SH_FILENAME "rgx.sh"
#define LINUX
#define PVR_BUILD_DIR "mtk_android"
#define PVR_BUILD_TYPE "release"
#define PVRSRV_MODNAME "pvrsrvkm"
#define PVRSYNC_MODNAME "pvr_sync"
#define SUPPORT_RGX 1
#define RELEASE
#define SUPPORT_PHYSMEM_TEST
#define RGX_BVNC_CORE_KM_HEADER "cores/rgxcore_km_22.87.104.18.h"
#define RGX_BNC_CONFIG_KM_HEADER "configs/rgxconfig_km_22.V.104.18.h"
#define SUPPORT_DBGDRV_EVENT_OBJECTS
#define PVRSRV_NEED_PVR_DPF
#define SUPPORT_RGXTQ_BRIDGE
#define ENABLE_EMULATED_LARGE_TEXTURES
#define PVRSRV_POISON_ON_ALLOC_VALUE 0xd9
#define PVRSRV_POISON_ON_FREE_VALUE 0x63
#define RGX_NUM_OS_SUPPORTED 1
#define PVRSRV_APPHINT_DRIVERMODE 0x7FFFFFFF
#define RGX_FW_HEAP_SHIFT 25
#define PVRSRV_APPHINT_OSIDREGION0MIN "0x00000000 0x04000000 0x10000000 0x18000000 0x20000000 0x28000000 0x30000000 0x38000000"
#define PVRSRV_APPHINT_OSIDREGION0MAX "0x3FFFFFFF 0x0FFFFFFF 0x17FFFFFF 0x1FFFFFFF 0x27FFFFFF 0x2FFFFFFF 0x37FFFFFF 0x3FFFFFFF"
#define PVRSRV_APPHINT_OSIDREGION1MIN "0x3F000000 0x3F000000 0x3F000000 0x3F000000 0x3F000000 0x3F000000 0x3F000000 0x3F000000"
#define PVRSRV_APPHINT_OSIDREGION1MAX "0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF 0x3FFFFFFF"
#define SUPPORT_POWMON_COMPONENT
#define PVR_POWER_ACTOR
#define PVR_POWER_ACTOR_MEASUREMENT_PERIOD_MS 10U
#define PVR_POWER_MONITOR_HWPERF
#define PVR_LDM_DRIVER_REGISTRATION_NAME "pvrsrvkm"
#define PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN 256
#define SUPPORT_MMU_PENDING_FAULT_PROTECTION
#define ION_DEFAULT_HEAP_NAME "ion_system_heap"
#define ION_DEFAULT_HEAP_ID_MASK (1 << ION_HEAP_TYPE_SYSTEM)
#define CACHEFLUSH_NO_KMRBF_USING_UMVA
#define PVR_BLOB_CACHE_SIZE_MEGABYTES 20
#define HWR_DEFAULT_ENABLED
#define PVRSRV_APPHINT_HWRDEBUGDUMPLIMIT APPHNT_BLDVAR_DBGDUMPLIMIT
#define PVRSRV_APPHINT_ENABLETRUSTEDDEVICEACECONFIG IMG_FALSE
#define PVRSRV_APPHINT_GENERALNON4KHEAPPAGESIZE 0x4000
#define PVRSRV_APPHINT_HWPERFCLIENTBUFFERSIZE 786432
#define PVRSRV_APPHINT_ENABLESIGNATURECHECKS APPHNT_BLDVAR_ENABLESIGNATURECHECKS
#define PVRSRV_APPHINT_SIGNATURECHECKSBUFSIZE RGXFW_SIG_BUFFER_SIZE_MIN
#define PVRSRV_APPHINT_ENABLEFULLSYNCTRACKING IMG_FALSE
#if defined(CONFIG_MTK_ENABLE_GMO)
#define PVRSRV_APPHINT_ENABLEPAGEFAULTDEBUG APPHNT_BLDVAR_ENABLEPAGEFAULTDEBUG
#else
#define PVRSRV_APPHINT_ENABLEPAGEFAULTDEBUG IMG_TRUE
#endif
#define PVRSRV_APPHINT_VALIDATEIRQ 0
#define PVRSRV_APPHINT_DISABLECLOCKGATING 0
#define PVRSRV_APPHINT_DISABLEDMOVERLAP 0
#define PVRSRV_APPHINT_ENABLECDMKILLINGRANDMODE 0
#define PVRSRV_APPHINT_ENABLERANDOMCONTEXTSWITCH 0
#define PVRSRV_APPHINT_ENABLESOFTRESETCNTEXTSWITCH 0
#define PVRSRV_APPHINT_ENABLEFWCONTEXTSWITCH RGXFWIF_INICFG_OS_CTXSWITCH_DM_ALL
#define PVRSRV_APPHINT_VDMCONTEXTSWITCHMODE RGXFWIF_INICFG_VDM_CTX_STORE_MODE_INDEX
#define PVRSRV_APPHINT_ENABLERDPOWERISLAND RGX_RD_POWER_ISLAND_DEFAULT
#define PVRSRV_APPHINT_FIRMWAREPERF FW_PERF_CONF_NONE
#define PVRSRV_APPHINT_FWCONTEXTSWITCHPROFILE RGXFWIF_CTXSWITCH_PROFILE_MEDIUM_EN
#define PVRSRV_APPHINT_HWPERFDISABLECUSTOMCOUNTERFILTER 0
#define PVRSRV_APPHINT_HWPERFFWBUFSIZEINKB 2048
#define PVRSRV_APPHINT_HWPERFHOSTBUFSIZEINKB 2048
#define PVRSRV_APPHINT_HWPERFHOSTTHREADTIMEOUTINMS 50
#define PVRSRV_APPHINT_JONESDISABLEMASK 0
#define PVRSRV_APPHINT_NEWFILTERINGMODE 1
#define PVRSRV_APPHINT_TRUNCATEMODE 0
#define PVRSRV_APPHINT_EMUMAXFREQ 0
#define PVRSRV_APPHINT_GPIOVALIDATIONMODE 0
#define PVRSRV_APPHINT_RGXBVNC ""
#define PVRSRV_APPHINT_CLEANUPTHREADPRIORITY 5
#define PVRSRV_APPHINT_WATCHDOGTHREADPRIORITY 0
#define PVRSRV_APPHINT_ASSERTONHWRTRIGGER IMG_FALSE
#define PVRSRV_APPHINT_ASSERTOUTOFMEMORY IMG_FALSE
#define PVRSRV_APPHINT_CHECKMLIST APPHNT_BLDVAR_DEBUG
#define PVRSRV_APPHINT_DISABLEFEDLOGGING IMG_FALSE
#define PVRSRV_APPHINT_ENABLEAPM RGX_ACTIVEPM_DEFAULT
#define PVRSRV_APPHINT_ENABLEHTBLOGGROUP 0
#define PVRSRV_APPHINT_ENABLELOGGROUP RGXFWIF_LOG_TYPE_NONE
#define PVRSRV_APPHINT_FIRMWARELOGTYPE 0
#define PVRSRV_APPHINT_FWTRACEBUFSIZEINDWORDS RGXFW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS
#define PVRSRV_APPHINT_FBCDCVERSIONOVERRIDE 0
#define PVRSRV_APPHINT_HTBOPERATIONMODE HTB_OPMODE_DROPOLDEST
#define PVRSRV_APPHINT_HTBUFFERSIZE 64
#define PVRSRV_APPHINT_ENABLEFTRACEGPU IMG_FALSE
#define PVRSRV_APPHINT_HWPERFFWFILTER 0
#define PVRSRV_APPHINT_HWPERFHOSTFILTER 0
#define PVRSRV_APPHINT_HWPERFCLIENTFILTER_SERVICES 0
#define PVRSRV_APPHINT_HWPERFCLIENTFILTER_EGL 0
#define PVRSRV_APPHINT_HWPERFCLIENTFILTER_OPENGLES 0
#define PVRSRV_APPHINT_HWPERFCLIENTFILTER_OPENCL 0
#define PVRSRV_APPHINT_HWPERFCLIENTFILTER_VULKAN 0
#define PVRSRV_APPHINT_TIMECORRCLOCK 0
#define PVRSRV_APPHINT_ENABLEFWPOISONONFREE IMG_FALSE
#define PVRSRV_APPHINT_FWPOISONONFREEVALUE 0xBD
#define PVRSRV_APPHINT_ZEROFREELIST IMG_FALSE
#define PVRSRV_APPHINT_GPUUNITSPOWERCHANGE IMG_FALSE
#define PVRSRV_APPHINT_DISABLEPDUMPPANIC IMG_FALSE
#define PVRSRV_APPHINT_CACHEOPCONFIG 0
#define PVRSRV_APPHINT_CACHEOPUMKMHRESHOLDSIZE 0
#define PVRSRV_APPHINT_IGNOREHWREPORTEDBVNC IMG_FALSE
#define PVRSRV_APPHINT_PHYSMEMTESTPASSES APPHNT_PHYSMEMTEST_ENABLE
#define PVRSRV_APPHINT_TESTSLRINTERVAL 0
#define SOC_TIMER_FREQ 20
#define PDVFS_COM_HOST 1
#define PDVFS_COM_AP 2
#define PDVFS_COM_PMC 3
#define PDVFS_COM_IMG_CLKDIV 4
#define PDVFS_COM PDVFS_COM_HOST
#define PVR_GPIO_MODE_GENERAL 1
#define PVR_GPIO_MODE_POWMON_PIN 2
#define PVR_GPIO_MODE PVR_GPIO_MODE_GENERAL
#define PVRSRV_ENABLE_PROCESS_STATS
#define SUPPORT_USC_BREAKPOINT
#define PVR_ANNOTATION_MAX_LEN 63
#define PVRSRV_ENABLE_CCCB_UTILISATION_INFO
#define PVRSRV_ENABLE_CCCB_UTILISATION_INFO_THRESHOLD 90
#define PVRSRV_ENABLE_MEMTRACK_STATS_FILE
#define PVR_LINUX_PHYSMEM_MAX_POOL_PAGES 10240
#define PVR_LINUX_PHYSMEM_MAX_EXCESS_POOL_PAGES 20480
#define PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES
#define PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC
#define PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY
#define PVR_DIRTY_BYTES_FLUSH_THRESHOLD 524288
#define PVR_LINUX_HIGHORDER_ALLOCATION_THRESHOLD 256
#define PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM 2
#if defined(CONFIG_MTK_ENABLE_GMO)
#define PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD 8192
#else
#define PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD 16384
#endif
#define SUPPORT_NATIVE_FENCE_SYNC
#define PVRSRV_STALLED_CCB_ACTION
#define UPDATE_FENCE_CHECKPOINT_COUNT 1
#define PVR_DRM_NAME "pvr"
#if defined(__arm64__) || defined(__aarch64__)
#define DEVICE_MEMSETCPY_ALIGN_IN_BYTES 16
#else
#define DEVICE_MEMSETCPY_ALIGN_IN_BYTES 8
#endif
#define RGX_INITIAL_SLR_HOLDOFF_PERIOD_MS 0
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TQ3D 14
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TQ2D 14
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_CDM 13
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TA 15
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_3D 16
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_KICKSYNC 13
#define PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TDM 14
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TQ3D 17
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TQ2D 17
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_CDM 15
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TA 16
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_3D 17
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_KICKSYNC 13
#define PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TDM 17
#define ANDROID
#define SUPPORT_ION
#define PVR_ANDROID_ION_HEADER "../drivers/staging/android/aosp_ion/ion.h"
#define PVR_ANDROID_ION_PRIV_HEADER "../drivers/staging/android/ion/ion_priv.h"
#define PVR_ANDROID_ION_USE_SG_LENGTH
#define PVR_ANDROID_SYNC_HEADER "linux/sync.h"
#define PVRSRV_ENABLE_PVR_ION_STATS
#define MTK_USE_HW_APM
#define MTK_DEBUG_PROC_PRINT
#define MTK_CONFIG_OF
#define ENABLE_COMMON_DVFS
