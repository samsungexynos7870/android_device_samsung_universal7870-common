#!/bin/bash

COMMON_A6LTE_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/SAMFW.COM_SM-A600F_AFG_A600FJXU9CVB1_fac"
COMMON_A6LTE_P_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/SAMFW.COM_SM-A600FN_H3G_A600FNXXS5BTC2_fac"
COMMON_A7Y17LTE_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/Samfw.com_SM-A720S_SKC_A720SKSU5CTL2_fac"
COMMON_M10LTE_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/Samfw.com_SM-M105F_BKD_M105FDDS4CVB1_fac"
COMMON_J7DUOLTE_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/SAMFW.COM_SM-J720F_DBT_J720FDDS7CUL1_fac"
COMMON_GRACERLTE_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/SAMFW.COM_SM-N935F_CAM_N935FXXU8CVG2_fac"
COMMON_OSS_PATH="/media/flominator/40810d67-1e17-42cc-8147-8500cdc3df4b/phoenix_firmware_dumper-main/samsung_slsi_oss_a11_17112025"


./../extract-files.sh \
-pfs proprietary-files_a6lte.txt "${COMMON_A6LTE_PATH}" \
-pfs proprietary-files_a6lte_p.txt "${COMMON_A6LTE_P_PATH}" \
-pfs proprietary-files_a7y17lte.txt "${COMMON_A7Y17LTE_PATH}" \
-pfs proprietary-files_m10lte.txt "${COMMON_M10LTE_PATH}" \
-pfs proprietary-files_j7duolte.txt "${COMMON_J7DUOLTE_PATH}" \
-pfs proprietary-files_gracerlte.txt "${COMMON_GRACERLTE_PATH}" \
-pfs proprietary-files_oss.txt "${COMMON_OSS_PATH}"

