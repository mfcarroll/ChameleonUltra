#ifndef DATA_CMD_H
#define DATA_CMD_H


// ******************************************************************
//                      CMD for device
//                  Range from 1000 -> 1999
// ******************************************************************
//
#define DATA_CMD_GET_APP_VERSION                (1000)
#define DATA_CMD_CHANGE_DEVICE_MODE             (1001)
#define DATA_CMD_GET_DEVICE_MODE                (1002)
#define DATA_CMD_SET_ACTIVE_SLOT                (1003)
#define DATA_CMD_SET_SLOT_TAG_TYPE              (1004)
#define DATA_CMD_SET_SLOT_DATA_DEFAULT          (1005)
#define DATA_CMD_SET_SLOT_ENABLE                (1006)
#define DATA_CMD_SET_SLOT_TAG_NICK              (1007)
#define DATA_CMD_GET_SLOT_TAG_NICK              (1008)
#define DATA_CMD_SLOT_DATA_CONFIG_SAVE          (1009)
#define DATA_CMD_ENTER_BOOTLOADER               (1010)
#define DATA_CMD_GET_DEVICE_CHIP_ID             (1011)
#define DATA_CMD_GET_DEVICE_ADDRESS             (1012)
#define DATA_CMD_SAVE_SETTINGS                  (1013)
#define DATA_CMD_RESET_SETTINGS                 (1014)
#define DATA_CMD_SET_ANIMATION_MODE             (1015)
#define DATA_CMD_GET_ANIMATION_MODE             (1016)
#define DATA_CMD_GET_GIT_VERSION                (1017)
#define DATA_CMD_GET_ACTIVE_SLOT                (1018)
#define DATA_CMD_GET_SLOT_INFO                  (1019)
#define DATA_CMD_WIPE_FDS                       (1020)
#define DATA_CMD_DELETE_SLOT_TAG_NICK           (1021)

#define DATA_CMD_GET_ENABLED_SLOTS              (1023)
#define DATA_CMD_DELETE_SLOT_SENSE_TYPE         (1024)
#define DATA_CMD_GET_BATTERY_INFO               (1025)
#define DATA_CMD_GET_BUTTON_PRESS_CONFIG        (1026)
#define DATA_CMD_SET_BUTTON_PRESS_CONFIG        (1027)
#define DATA_CMD_GET_LONG_BUTTON_PRESS_CONFIG   (1028)
#define DATA_CMD_SET_LONG_BUTTON_PRESS_CONFIG   (1029)
#define DATA_CMD_SET_BLE_PAIRING_KEY            (1030)
#define DATA_CMD_GET_BLE_PAIRING_KEY            (1031)
#define DATA_CMD_DELETE_ALL_BLE_BONDS           (1032)
#define DATA_CMD_GET_DEVICE_MODEL               (1033)
#define DATA_CMD_GET_DEVICE_SETTINGS            (1034)
#define DATA_CMD_GET_DEVICE_CAPABILITIES        (1035)
#define DATA_CMD_GET_BLE_PAIRING_ENABLE         (1036)
#define DATA_CMD_SET_BLE_PAIRING_ENABLE         (1037)
#define DATA_CMD_GET_ALL_SLOT_NICKS             (1038)
#define DATA_CMD_GET_SLEEP_TIMEOUT              (1039)
#define DATA_CMD_SET_SLEEP_TIMEOUT              (1040)

//
// ******************************************************************


// ******************************************************************
//                      CMD for hf reader
//                  Range from 2000 -> 2999
// ******************************************************************
//
#define DATA_CMD_HF14A_SCAN                     (2000)
#define DATA_CMD_MF1_DETECT_SUPPORT             (2001)
#define DATA_CMD_MF1_DETECT_PRNG                (2002)
#define DATA_CMD_MF1_STATIC_NESTED_ACQUIRE      (2003)
#define DATA_CMD_MF1_DARKSIDE_ACQUIRE           (2004)
#define DATA_CMD_MF1_DETECT_NT_DIST             (2005)
#define DATA_CMD_MF1_NESTED_ACQUIRE             (2006)
#define DATA_CMD_MF1_AUTH_ONE_KEY_BLOCK         (2007)
#define DATA_CMD_MF1_READ_ONE_BLOCK             (2008)
#define DATA_CMD_MF1_WRITE_ONE_BLOCK            (2009)
#define DATA_CMD_HF14A_RAW                      (2010)
#define DATA_CMD_HF14A_SCAN_KEEP                (2016)  /* scan+RATS, keep field alive for APDU exchange */
#define DATA_CMD_HF14A_AUTH_TRACE               (2017)  /* full anticoll + Crypto1 auth, every frame returned for inspection */
#define DATA_CMD_MF1_MANIPULATE_VALUE_BLOCK     (2011)
#define DATA_CMD_MF1_CHECK_KEYS_OF_SECTORS      (2012)
#define DATA_CMD_MF1_HARDNESTED_ACQUIRE         (2013)
#define DATA_CMD_MF1_ENC_NESTED_ACQUIRE         (2014)
#define DATA_CMD_MF1_CHECK_KEYS_ON_BLOCK        (2015)

#define DATA_CMD_HF14A_SET_FIELD_ON             (2100)
#define DATA_CMD_HF14A_SET_FIELD_OFF            (2101)

#define DATA_CMD_HF14A_GET_CONFIG               (2200)
#define DATA_CMD_HF14A_SET_CONFIG               (2201)
#define DATA_CMD_HF14A_SNIFF                    (2020)

//
// ******************************************************************


// ******************************************************************
//                      CMD for lf reader
//                  Range from 3000 -> 3999
// ******************************************************************
//
#define DATA_CMD_EM410X_SCAN                    (3000)
#define DATA_CMD_EM410X_WRITE_TO_T55XX          (3001)
#define DATA_CMD_EM410X_ELECTRA_WRITE_TO_T55XX  (3006)
#define DATA_CMD_HIDPROX_SCAN                   (3002)
#define DATA_CMD_HIDPROX_WRITE_TO_T55XX         (3003)
#define DATA_CMD_PAC_SCAN                       (3014)
#define DATA_CMD_PAC_WRITE_TO_T55XX             (3015)
#define DATA_CMD_VIKING_SCAN                    (3004)
#define DATA_CMD_VIKING_WRITE_TO_T55XX          (3005)
#define DATA_CMD_ADC_GENERIC_READ               (3009)
#define DATA_CMD_GENERIC_READ                   (3007)
#define DATA_CMD_CORR_GENERIC_READ              (3008)
#define DATA_CMD_IOPROX_SCAN                    (3010)
#define DATA_CMD_IOPROX_WRITE_TO_T55XX          (3011)
#define DATA_CMD_IOPROX_DECODE_RAW              (3012)
#define DATA_CMD_IOPROX_COMPOSE_ID              (3013)
#define DATA_CMD_LF_T55XX_WRITE                 (3016)
#define DATA_CMD_IDTECK_WRITE_TO_T55XX          (3018)
#define DATA_CMD_JABLOTRON_SCAN                 (3019)
#define DATA_CMD_JABLOTRON_WRITE_TO_T55XX       (3020)

//
// ******************************************************************


// ******************************************************************
//                      CMD for hf emulator
//                  Range from 4000 -> 4999
// ******************************************************************
//
#define DATA_CMD_MF1_WRITE_EMU_BLOCK_DATA       (4000)
#define DATA_CMD_HF14A_SET_ANTI_COLL_DATA       (4001)
#define DATA_CMD_MF1_SET_DETECTION_ENABLE       (4004)
#define DATA_CMD_MF1_GET_DETECTION_COUNT        (4005)
#define DATA_CMD_MF1_GET_DETECTION_LOG          (4006)
#define DATA_CMD_MF1_GET_DETECTION_ENABLE       (4007)
#define DATA_CMD_MF1_READ_EMU_BLOCK_DATA        (4008)
#define DATA_CMD_MF1_GET_EMULATOR_CONFIG        (4009)
#define DATA_CMD_MF1_GET_GEN1A_MODE             (4010)
#define DATA_CMD_MF1_SET_GEN1A_MODE             (4011)
#define DATA_CMD_MF1_GET_GEN2_MODE              (4012)
#define DATA_CMD_MF1_SET_GEN2_MODE              (4013)
#define DATA_CMD_MF1_GET_BLOCK_ANTI_COLL_MODE   (4014)
#define DATA_CMD_MF1_SET_BLOCK_ANTI_COLL_MODE   (4015)
#define DATA_CMD_MF1_GET_WRITE_MODE             (4016)
#define DATA_CMD_MF1_SET_WRITE_MODE             (4017)
#define DATA_CMD_HF14A_GET_ANTI_COLL_DATA       (4018)
#define DATA_CMD_MF0_NTAG_GET_UID_MAGIC_MODE    (4019)
#define DATA_CMD_MF0_NTAG_SET_UID_MAGIC_MODE    (4020)
#define DATA_CMD_MF0_NTAG_READ_EMU_PAGE_DATA    (4021)
#define DATA_CMD_MF0_NTAG_WRITE_EMU_PAGE_DATA   (4022)
#define DATA_CMD_MF0_NTAG_GET_VERSION_DATA      (4023)
#define DATA_CMD_MF0_NTAG_SET_VERSION_DATA      (4024)
#define DATA_CMD_MF0_NTAG_GET_SIGNATURE_DATA    (4025)
#define DATA_CMD_MF0_NTAG_SET_SIGNATURE_DATA    (4026)
#define DATA_CMD_MF0_NTAG_GET_COUNTER_DATA      (4027)
#define DATA_CMD_MF0_NTAG_SET_COUNTER_DATA      (4028)
#define DATA_CMD_MF0_NTAG_RESET_AUTH_CNT        (4029)
#define DATA_CMD_MF0_NTAG_GET_PAGE_COUNT        (4030)
#define DATA_CMD_MF0_NTAG_RESET_AUTH_CNT        (4029)
#define DATA_CMD_MF0_NTAG_GET_PAGE_COUNT        (4030)
#define DATA_CMD_MF0_NTAG_GET_WRITE_MODE        (4031)
#define DATA_CMD_MF0_NTAG_SET_WRITE_MODE        (4032)
#define DATA_CMD_MF0_NTAG_SET_DETECTION_ENABLE  (4033)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_COUNT   (4034)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_LOG     (4035)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_ENABLE  (4036)
#define DATA_CMD_MF0_NTAG_GET_EMULATOR_CONFIG   (4037)
#define DATA_CMD_MF1_SET_FIELD_OFF_DO_RESET     (4038)
#define DATA_CMD_MF1_GET_FIELD_OFF_DO_RESET     (4039)
#define DATA_CMD_MF1_GET_PRNG_TYPE              (4040)  // 0=static 1=weak(LFSR) 2=hard(rand)
#define DATA_CMD_MF1_SET_PRNG_TYPE              (4041)
#define DATA_CMD_SEOS_READ_EMU_DATA             (4042)
#define DATA_CMD_SEOS_WRITE_EMU_DATA            (4043)
#define DATA_CMD_SEOS_WRITE_EMU_KEYS            (4044)
//
// ******************************************************************


// ******************************************************************
//                      CMD for lf emulator
//                  Range from 5000 -> 5999
// ******************************************************************
//

//
// ******************************************************************
/* ISO14443-4 T=CL emulation commands */
#define DATA_CMD_HF14A_4_APDU_RECV              (6000)  /* non-blocking poll: firmware->host APDU */
#define DATA_CMD_HF14A_4_APDU_SEND              (6001)  /* host->firmware APDU response */
#define DATA_CMD_HF14A_4_SET_ANTI_COLL          (6002)  /* set UID/ATQA/SAK/ATS */
#define DATA_CMD_HF14A_4_STATIC_RESP            (6003)  /* add/clear static APDU response pair */
#define DATA_CMD_HF14A_4_READER_APDU            (6004)  /* select+RATS+send APDU, keep field   */
#define DATA_CMD_HF14A_4_EMV_SCAN               (6005)  /* full EMV scan in one call            */

#define DATA_CMD_EM410X_SET_EMU_ID              (5000)
#define DATA_CMD_EM410X_GET_EMU_ID              (5001)
#define DATA_CMD_HIDPROX_SET_EMU_ID             (5002)
#define DATA_CMD_HIDPROX_GET_EMU_ID             (5003)
#define DATA_CMD_VIKING_SET_EMU_ID              (5004)
#define DATA_CMD_VIKING_GET_EMU_ID              (5005)
#define DATA_CMD_PAC_SET_EMU_ID                 (5006)
#define DATA_CMD_PAC_GET_EMU_ID                 (5007)
#define DATA_CMD_IOPROX_SET_EMU_ID              (5008)
#define DATA_CMD_IOPROX_GET_EMU_ID              (5009)
#define DATA_CMD_JABLOTRON_SET_EMU_ID           (5010)
#define DATA_CMD_JABLOTRON_GET_EMU_ID           (5011)
#define DATA_CMD_IDTECK_SET_EMU_ID              (5012)
#define DATA_CMD_IDTECK_GET_EMU_ID              (5013)
#define DATA_CMD_INDALA_SET_EMU_ID              (5014)
#define DATA_CMD_INDALA_GET_EMU_ID              (5015)
#define DATA_CMD_INDALA224_SET_EMU_ID           (5016)
#define DATA_CMD_INDALA224_GET_EMU_ID           (5017)
#define DATA_CMD_KERI_SET_EMU_ID                (5018)
#define DATA_CMD_KERI_GET_EMU_ID                (5019)
#define DATA_CMD_NEXWATCH_SET_EMU_ID            (5020)
#define DATA_CMD_NEXWATCH_GET_EMU_ID            (5021)
#define DATA_CMD_GALLAGHER_SET_EMU_ID           (5022)
#define DATA_CMD_GALLAGHER_GET_EMU_ID           (5023)
#define DATA_CMD_SECURAKEY_SET_EMU_ID           (5024)
#define DATA_CMD_SECURAKEY_GET_EMU_ID           (5025)
#define DATA_CMD_NORALSY_SET_EMU_ID             (5026)
#define DATA_CMD_NORALSY_GET_EMU_ID             (5027)
/* ⭐ The first FSK2a protocol to EMULATE — the PSK1 and ASK families are both above. */
#define DATA_CMD_AWID_SET_EMU_ID                (5028)
#define DATA_CMD_AWID_GET_EMU_ID                (5029)
/* ⭐ The biphase family's emulator. Its emitter uses counter_top 32 where AWID's uses 8 and
 * 10, which is the whole point of having it: if this reads where AWID does not, the defect is
 * the counter_top magnitude (C221). */
#define DATA_CMD_GPROXII_SET_EMU_ID             (5030)
#define DATA_CMD_GPROXII_GET_EMU_ID             (5031)
#define DATA_CMD_FDXB_SET_EMU_ID                (5032)
#define DATA_CMD_FDXB_GET_EMU_ID                (5033)

#define DATA_CMD_EM4X05_SCAN                    (3030)
#define DATA_CMD_EM4X05_READSNIFF               (3032)
#define DATA_CMD_LF_SNIFF                       (3031)
#define DATA_CMD_INDALA_SCAN                    (3033)
#define DATA_CMD_INDALA_WRITE_TO_T55XX          (3034)

/* ⭐⭐ RESEARCH INSTRUMENTATION — COMPILE-GATED, NOT DELETED.
 *
 * Three commands and one reader probe exist only to measure this firmware's own LF path, and
 * they earned their keep: `LF_READER_CAPTURE` is what cracked C211 after six hypotheses had
 * been refuted by measurement, and the GProxII failure-energy payload is what separates "the
 * capture was wrong" from "the decode was wrong" on a silent read — the distinction C206
 * turned on. Deleting them to ship would throw away the only tools that can re-open those
 * questions later.
 *
 * ⛔ They must still cost a shipping image NOTHING, so they are gated rather than removed.
 * This is the tree's own idiom: `app_cmd.c` already gates whole handlers and their dispatch
 * rows behind `#if defined(PROJECT_CHAMELEON_ULTRA)` for the Ultra/Lite split, and this branch
 * added `#if !INDALA224_READER_TRUSTED` in `lf_indala_data.c`.
 *
 * ⭐ DEFAULT OFF, so a plain build is the shipping build and the upstream diff is one deleted
 * `-D` rather than a hunt through five files. This branch's `application/Makefile` sets it to
 * 1, which is the single line an upstream PR drops. */
#ifndef LF_RESEARCH_CMDS_ENABLED
#define LF_RESEARCH_CMDS_ENABLED 0
#endif

#define DATA_CMD_IDTECK_SCAN                    (3035)
#define DATA_CMD_INDALA224_SCAN                 (3036)
#if LF_RESEARCH_CMDS_ENABLED
#define DATA_CMD_LF_EMU_DEBUG                   (3037)
#define DATA_CMD_LF_RADIO_DEBUG                 (3038)
#endif
#define DATA_CMD_INDALA224_WRITE_TO_T55XX       (3039)
#define DATA_CMD_KERI_SCAN                      (3040)
#define DATA_CMD_KERI_WRITE_TO_T55XX            (3041)
#define DATA_CMD_NEXWATCH_SCAN                  (3042)
#define DATA_CMD_NEXWATCH_WRITE_TO_T55XX        (3043)
#define DATA_CMD_GALLAGHER_SCAN                 (3044)
#define DATA_CMD_GALLAGHER_WRITE_TO_T55XX       (3045)
#define DATA_CMD_SECURAKEY_SCAN                 (3046)
#define DATA_CMD_SECURAKEY_WRITE_TO_T55XX       (3047)
#define DATA_CMD_NORALSY_SCAN                   (3048)
#define DATA_CMD_NORALSY_WRITE_TO_T55XX         (3049)
/* ⚠ Scan only — no InstaFob writer ships; see lf_indala_data.h for why. */
#define DATA_CMD_INSTAFOB_SCAN                  (3050)
#define DATA_CMD_AWID_SCAN                      (3051)
#define DATA_CMD_PARADOX_SCAN                   (3052)
#define DATA_CMD_PYRAMID_SCAN                   (3053)
/* ⭐ The refusal that stood here is RETIRED (C333). It read: "nothing on this bench can read an
 * FDX-A tag back — the Proxmark's `lf fdx` is FDX-B — so a writer would certify itself." The
 * Proxmark has a complete FDX-A under `lf destron`: demod, reader, **clone** and sim. It was
 * looked for in one place. ⇒ The writer below is verified by an independent tool like every
 * other. ⚠ InstaFob's refusal is NOT retired — the Proxmark really has no command for it. */
#define DATA_CMD_FDXA_SCAN                      (3054)
#define DATA_CMD_AWID_WRITE_TO_T55XX            (3055)
#define DATA_CMD_PARADOX_WRITE_TO_T55XX         (3056)
#define DATA_CMD_PYRAMID_WRITE_TO_T55XX         (3057)
/* ⭐ The BIPHASE family opens here — GProxII, the fourth line coding on this device. */
#define DATA_CMD_GPROXII_SCAN                   (3058)
#define DATA_CMD_GPROXII_WRITE_TO_T55XX         (3059)
/* ⚠ INSTRUMENTATION for C209 — run the READER's capture and return the samples undecoded.
 * `lf sniff` is the OTHER path, which is precisely why it cannot answer the question. */
#if LF_RESEARCH_CMDS_ENABLED
#define DATA_CMD_LF_READER_CAPTURE              (3060)
/* ⚠ INSTRUMENTATION for C305 — send a T5577 regular-read INTO a live capture and return the
 * raw samples. The tag answers in whatever modulation its config selects, so the demodulation
 * is host-side; for block 0 that config is exactly what is being asked for. */
#define DATA_CMD_LF_T55XX_READ_CAPTURE          (3063)
#endif
#define DATA_CMD_FDXB_SCAN                      (3061)
#define DATA_CMD_FDXB_WRITE_TO_T55XX            (3062)
#define DATA_CMD_FDXA_WRITE_TO_T55XX            (3064)

#endif
