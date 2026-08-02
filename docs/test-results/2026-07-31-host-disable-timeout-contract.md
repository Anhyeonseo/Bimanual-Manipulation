# Host DISABLE timeout 계약

- 날짜: 2026-07-31

## 관측

STM32 firmware `0x00021700`을 플래시·검증한 뒤, 첫 READ_ONLY bridge
시작이 `DISABLE`의 `STATE_FEEDBACK`을 기다리다 실패했다.

```text
single_arm_bridge.transport.TransportError:
timeout waiting for STATE_FEEDBACK
```

진단을 계속하기 전 bridge를 정지하고 12 V를 끄고 팔을 물리적으로
지지했다.

## 근본 원인

host는 `DISABLE`에 0.5초만 허용했다. firmware는 의도적으로 6개 서보
전부의 torque-off write와 물리 torque-register readback을 시도할
때까지 이 요청을 확인 응답하지 않는다.

HAL timeout 범위:

- 6회 write: `6 * (100 ms 전송 + 2 ms 선택적 status 수신) = 612 ms`
- settling delay: `5 ms`
- 6회 readback: `6 * (100 ms 전송 + 100 ms 수신) = 1200 ms`
- 전체 firmware 범위: `1817 ms`

따라서 기존 500 ms host timeout은 firmware가 필수 물리 안전 확인을
수행하는 도중에 만료될 수 있었다.

## 수정

`DISABLE_RESPONSE_TIMEOUT_S`를 이제 2.5초로 설정한다. 이는 전용
timeout이며, heartbeat·position feedback·diagnostics는 기존의 더 짧은
한계를 그대로 유지한다.

firmware, protocol identity, calibration, HEX는 변경되지 않았다. 이번
host-only 수정에는 STM32 재플래시가 필요 없다.

## 계약 시험

physical-disable 계약은 이제 firmware source에서 실제 HAL timeout
값을 읽어와 6축 worst-case 범위를 계산하고, host timeout이 이보다
최소 500 ms 이상 커야 한다고 요구한다.

초기 시험 구현이 `Servo_WriteData`와 `Servo_ReadData`의 정의가 아니라
전방 선언(forward declaration)을 선택하는 오류가 있었다. 이 시험
parser 오류는 최종 함수 정의를 선택하도록 수정했으며, 제품 코드
결함을 나타내는 것은 아니었다.

## 검증

- 표적 physical-disable·transport suite: `24 passed`
- `single_arm_bridge` 로컬 rebuild: 통과
- 전체 Python/ROS suite: `323 passed`
- Pi 전송: 이번 변경 승인 범위에서는 미실행
- STM32 수정 또는 flash: 미실행
- 로봇 동작: 미실행
