# STATE_FEEDBACK 명시적 state 필드 부재 — 계약 대조

## 목표

다음 안전 전이 시험을 준비하던 중 `STATE_FEEDBACK`(id 49)에 성공/실패
코드만 있고 `SAFE_DISABLED`/`ARMED`/`ACTIVE`/`HOLD` 중 어느 상태인지 담는
필드가 없다는 의문이 나왔다. 이대로면 host가 `ENABLE` 성공 뒤 실제
`ACTIVE`인지 직접 읽을 방법이 없어 보였다. 이것이 계약 누락인지, 아니면
다른 필드/메시지로 이미 보완돼 있는지 firmware와 host 소스를 직접 대조해
판단했다.

## 대조 대상

- `firmware/stm32_g474_single_arm/Core/Src/binary_control.c`
- `firmware/stm32_actuator/include/actuator_core/safety.h`
- `protocol/message_ids.json`
- `ros2_ws/src/single_arm_bridge/single_arm_bridge/protocol.py`
- `ros2_ws/src/single_arm_bridge/single_arm_bridge/transport.py`

## 결과

`STATE_FEEDBACK`은 `HEARTBEAT`/`GET_STATE`/`ENABLE`/`HOLD`/`SAFE_STOP`/
`DISABLE`/`CLEAR_FAULT`의 공통 응답이며(`Host_SendBinaryState`,
`binary_control.c:148-172`, 발신부 `:1061-1099`), 20-byte payload는
`payload[0]=stop_latched`, `payload[1]=status_code`,
`payload[2]=joint_count`, `payload[3]=protocol_version` 뒤에 heartbeat/
reject 카운터, calibration hash, 마지막 heartbeat 시각이 이어진다.
`actuator_state_t` 값을 담는 byte는 여기에 없다. Python `State`
dataclass(`protocol.py:94-114`, `STATE_BASE = "<BBBBIIII"`)도 동일하게
`stop_latched`/`status_code`만 있고 `state` 필드가 없다.

원시 `actuator_state_t` 값을 실제로 실어 보내는 메시지는 두 곳이다.

- `ARM_RESPONSE`(id 17, `binary_control.c:510-512`): `payload[1] =
  (uint8_t)host_binary_safety.state`. host는 `transport.py`에서
  `result != 0 or state != 2(ARMED)`이면 거절한다.
- `SETPOINT_STATUS`(id 33, `binary_control.c:534-536`): `payload[2] =
  (uint8_t)host_binary_safety.state`. Python에는
  `MotionResult.safety_state`로 노출된다(`protocol.py:118-133`).

`ENABLE`은 그 자체로 `ACTIVE` 진입을 반환하지 않으며, host는
`status_code == 0`이고 `stop_latched == false`일 때만 성공으로 판단한다.
`SAFE_STOP`/timeout으로 인한 `HOLD` 전이는 `stop_latched == true`로
확인한다. 이 세 가지 간접 확인 방식이 `transport.py`의 ARM/ENABLE/
SAFE_STOP 처리 전체와 일치한다.

## 판정

**계약 누락 아님, 설계 의도.** 원본 protocol이 `STATE_FEEDBACK`에
`state`를 직접 넣지 않고 `ARM_RESPONSE`·`SETPOINT_STATUS`의 원시 state와
`STATE_FEEDBACK`의 `status_code`/`stop_latched` 조합으로 상태를
추론하도록 처음부터 설계돼 있었다. firmware와 host 양쪽이 이 방식으로
서로 일치하므로 지금 20-byte 형식을 유지한다.

## 결정과 남은 항목

- 이번 안전 전이 시험은 기존 20-byte `STATE_FEEDBACK` + `ARM_RESPONSE`/
  `SETPOINT_STATUS`의 간접 state 확인 방식 그대로 진행한다.
- 모든 응답에 `state`를 직접 노출하는 개선(예: `STATE_FEEDBACK`에 1 byte
  추가)은 가능하지만 firmware payload 크기·offset과 Python/ROS Phase 2
  계약을 동시에 바꿔야 하므로 지금은 하지 않는다. 별도 backlog 항목으로
  남긴다.
- `protocol/README.md` 7절에 "Host가 상태를 확인하는 방법" 절을 추가해
  이 설계를 문서화했다.
