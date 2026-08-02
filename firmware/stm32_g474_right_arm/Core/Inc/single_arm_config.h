#ifndef SINGLE_ARM_CONFIG_H
#define SINGLE_ARM_CONFIG_H

#include <stdint.h>

#define SINGLE_ARM_JOINT_COUNT 6U

#define ENABLE_SERVO_CENTERING_COMMAND 0U
#define ENABLE_BOOT_ID1_AUTOCONFIG 0U

/*
 * Keep physical motion locked until the right arm's IDs 2..6 directions,
 * operating envelopes, gains and torque limits have passed the same measured
 * acceptance gate as the upstream arm.
 */
#define RIGHT_ARM_MOTION_AUTHORIZED 0U

#if RIGHT_ARM_MOTION_AUTHORIZED > 1U
#error "RIGHT_ARM_MOTION_AUTHORIZED must be 0 or 1"
#endif

/*
 * Fail-closed commissioning mode.
 *
 * This is deliberately separate from RIGHT_ARM_MOTION_AUTHORIZED.  Normal
 * binary motion remains locked while one explicitly selected servo can be
 * probed from the ASCII console.  The probe always asks for a small raw-count
 * increase; its physical direction is an observation, not yet a calibrated
 * ROS-positive direction.
 */
#define RIGHT_ARM_CALIBRATION_MODE 1U
#define RIGHT_ARM_CALIBRATION_SERVO_ID 6U
#define RIGHT_ARM_CALIBRATION_DELTA_RAW 57U
/*
 * +1 probes toward higher raw from the captured start; -1 probes toward
 * lower raw. Joints whose provisional maximum_raw sits at home (WRIST_FLEX,
 * GRIPPER) must use -1 so the probe stays inside the unverified candidate
 * envelope instead of immediately overshooting it.
 */
#define RIGHT_ARM_CALIBRATION_DIRECTION (-1)
#define RIGHT_ARM_CALIBRATION_DURATION_MS UINT32_C(1000)
#define RIGHT_ARM_CALIBRATION_P_GAIN UINT8_C(16)
#define RIGHT_ARM_CALIBRATION_TORQUE_LIMIT_RAW UINT16_C(450)
#define RIGHT_ARM_CALIBRATION_MAX_HOME_OFFSET_RAW UINT16_C(512)

#if RIGHT_ARM_CALIBRATION_MODE > 1U
#error "RIGHT_ARM_CALIBRATION_MODE must be 0 or 1"
#endif

#if (RIGHT_ARM_CALIBRATION_SERVO_ID < 1U) || \
    (RIGHT_ARM_CALIBRATION_SERVO_ID > SINGLE_ARM_JOINT_COUNT)
#error "RIGHT_ARM_CALIBRATION_SERVO_ID must name one installed joint"
#endif

#if (RIGHT_ARM_CALIBRATION_DELTA_RAW < 1U) || \
    (RIGHT_ARM_CALIBRATION_DELTA_RAW > 64U)
#error "Calibration probe must stay within 1..64 raw counts"
#endif

#if (RIGHT_ARM_CALIBRATION_DIRECTION != 1) && \
    (RIGHT_ARM_CALIBRATION_DIRECTION != -1)
#error "RIGHT_ARM_CALIBRATION_DIRECTION must be 1 or -1"
#endif

#if RIGHT_ARM_CALIBRATION_MAX_HOME_OFFSET_RAW > 512U
#error "Calibration start must remain within 512 raw counts of home"
#endif

#if (RIGHT_ARM_CALIBRATION_MODE != 0U) && \
    (RIGHT_ARM_MOTION_AUTHORIZED != 0U)
#error "Calibration and normal motion authorization are mutually exclusive"
#endif

#define HOST_BINARY_FIRMWARE_VERSION UINT32_C(0x00031800)
#define HOST_BINARY_CAPABILITIES UINT32_C(0x000003FF)
#define HOST_BINARY_HEARTBEAT_TIMEOUT_MS UINT32_C(500)
#define HOST_BINARY_RX_BURST_MAX_BYTES UINT8_C(64)

/*
 * A background GET_STATE position sweep already retries one servo read three
 * times. Treat one failed sweep as observable degraded feedback, but require
 * failures in three consecutive host feedback periods before latching. The
 * motion start/final verification paths remain independently fail-closed on
 * their first exhausted sweep.
 */
#define HOST_POSITION_READ_FAILURE_LIMIT UINT8_C(3)

#if HOST_POSITION_READ_FAILURE_LIMIT < 2U
#error "Background position feedback must distinguish transient read failure"
#endif

/*
 * STS3215 동작 중 보호 기준.
 * load 1000은 최대 출력 100%, current 1은 약 6.5mA다.
 * 한 번의 main-loop 호출에서 한 축만 읽고 16ms 슬롯으로 순환한다.
 * 6축 전체는 약 96ms마다 갱신되며 축별 2회 연속 초과 시 중단한다.
 */
#define SERVO_MOTION_SAFETY_SLOT_MS UINT32_C(16)
#define SERVO_MOTION_SAFETY_SWEEP_MS UINT32_C(96)
#define SERVO_MOTION_LOAD_LIMIT_RAW UINT16_C(800)
#define SERVO_MOTION_CURRENT_LIMIT_RAW UINT16_C(320)
#define SERVO_MOTION_LIMIT_CONSECUTIVE UINT8_C(2)

/*
 * A trajectory endpoint is not judged from one early sample. Keep the
 * existing load/current watchdog active while collecting stable position
 * samples, then report the latest error after the bounded settling window.
 */
#define SERVO_FINAL_SETTLE_SAMPLE_MS UINT32_C(100)
#define SERVO_FINAL_SETTLE_MAX_MS UINT32_C(1000)
#define SERVO_FINAL_SETTLE_CONSECUTIVE UINT8_C(2)
#define SERVO_FINAL_ERROR_TOLERANCE_RAW UINT16_C(30)

#if SERVO_FINAL_SETTLE_MAX_MS < SERVO_FINAL_SETTLE_SAMPLE_MS
#error "Final settling window must contain at least one sample"
#endif

#if SERVO_FINAL_SETTLE_CONSECUTIVE < 1U
#error "Final settling requires at least one in-tolerance sample"
#endif

/*
 * Joint torque caps stay below the independent sustained-load stop threshold.
 * The Shoulder/Elbow caps account for the installed camera payload while the
 * load/current watchdog remains unchanged.
 */
#define SERVO_SHOULDER_TORQUE_LIMIT_RAW UINT16_C(780)
#define SERVO_ELBOW_TORQUE_LIMIT_RAW UINT16_C(650)

#if SERVO_SHOULDER_TORQUE_LIMIT_RAW >= SERVO_MOTION_LOAD_LIMIT_RAW
#error "Shoulder torque cap must remain below the load safety threshold"
#endif

#if SERVO_ELBOW_TORQUE_LIMIT_RAW >= SERVO_MOTION_LOAD_LIMIT_RAW
#error "Elbow torque cap must remain below the load safety threshold"
#endif

#endif /* SINGLE_ARM_CONFIG_H */
