from pathlib import Path
import re
import unittest


CONFIG = Path(
    "firmware/stm32_g474_right_arm/Core/Inc/single_arm_config.h"
).read_text()
APP = Path(
    "firmware/stm32_g474_right_arm/Core/Src/single_arm_app.c"
).read_text()


class RightArmCalibrationLockContractTests(unittest.TestCase):
    def test_normal_motion_stays_locked_while_calibrating_id2(self) -> None:
        self.assertIn("#define RIGHT_ARM_MOTION_AUTHORIZED 0U", CONFIG)
        self.assertIn("#define RIGHT_ARM_CALIBRATION_MODE 1U", CONFIG)
        self.assertIn("#define RIGHT_ARM_CALIBRATION_SERVO_ID 2U", CONFIG)
        self.assertIn(
            "Calibration and normal motion authorization are mutually exclusive",
            CONFIG,
        )

    def test_probe_is_visible_but_still_bounded(self) -> None:
        delta = int(
            re.search(
                r"RIGHT_ARM_CALIBRATION_DELTA_RAW\s+(\d+)U", CONFIG
            ).group(1)
        )
        duration = int(
            re.search(
                r"RIGHT_ARM_CALIBRATION_DURATION_MS UINT32_C\((\d+)\)",
                CONFIG,
            ).group(1)
        )
        torque = int(
            re.search(
                r"RIGHT_ARM_CALIBRATION_TORQUE_LIMIT_RAW UINT16_C\((\d+)\)",
                CONFIG,
            ).group(1)
        )
        self.assertGreaterEqual(delta, 34)
        self.assertLessEqual(delta, 64)
        self.assertGreaterEqual(duration, 800)
        self.assertLessEqual(torque, 250)

    def test_only_selected_joint_can_be_armed_from_ascii(self) -> None:
        self.assertIn(
            "requested_id == RIGHT_ARM_CALIBRATION_SERVO_ID", APP
        )
        self.assertIn("RIGHT_ARM_CALIBRATION_P_GAIN", APP)
        self.assertIn("RIGHT_ARM_CALIBRATION_TORQUE_LIMIT_RAW", APP)
        self.assertIn("RIGHT_ARM_CALIBRATION_MAX_HOME_OFFSET_RAW", APP)

    def test_probe_and_return_are_relative_to_measured_start(self) -> None:
        self.assertIn(
            "(int32_t)start_position +\n"
            "\t                      (int32_t)active_test_delta",
            APP,
        )
        self.assertIn(
            "return_position =\n"
            "#if RIGHT_ARM_CALIBRATION_MODE\n"
            "\t                      start_position;",
            APP,
        )

    def test_multi_axis_and_unbounded_home_commands_are_locked(self) -> None:
        for command in ("J", "A", "B", "H"):
            self.assertIn(
                f'"{command}_LOCKED_CALIBRATION_',
                APP,
            )

    def test_physical_torque_disable_is_available_even_after_stop(self) -> None:
        disable_branch = APP.index("(rx_byte == 'D')")
        stop_latched_block = APP.index(
            "(BinaryControl_StopIsLatched() != 0U) &&"
        )
        self.assertLess(disable_branch, stop_latched_block)
        self.assertIn("Servo_DisableTorqueAll()", APP[disable_branch:])
        self.assertIn("TORQUE_DISABLED_ALL_VERIFIED", APP)
        self.assertIn("TORQUE_DISABLE_VERIFY_FAILED", APP)

    def test_binary_mode_is_unavailable_during_calibration(self) -> None:
        self.assertIn("BINARY_MODE_LOCKED_DURING_CALIBRATION", APP)


if __name__ == "__main__":
    unittest.main()
