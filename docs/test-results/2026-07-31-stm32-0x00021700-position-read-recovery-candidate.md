# STM32 0x00021700 position-read recovery 후보

- 날짜: 2026-07-31

## 사건

단계 7 pregrasp-to-grasp 전이 중, fresh `/joint_states`가 더 이상
들어오지 않아 executor가 동작 명령을 보내지 않았다. Bridge log는
다음을 보였다.

1. 백그라운드 `GET_STATE` 응답이 `status=2`
2. 8 ms 뒤 heartbeat 응답이 `status=0`, `latched=1`
3. 두 번째 `GET_STATE` 실패, 이후 host가 transport fault 상태로 진입

heartbeat는 늦지 않았다. 백그라운드 `GET_STATE` 경로의
`Servo_ReadAllPositions()` sweep이 소진되면서 이미 MCU stop을
latch한 상태였다. 실패한 서보 ID는 firmware 내부 변수에만
존재했고, host는 heartbeat와 feedback 트래픽 사이에서 하나의
연속-오류 counter를 공유하고 있었다.

## 후보 정책

- Firmware identity: `0x00021700`
- Capabilities: `0x000001FF`
- 새 capability bit: `0x00000100`, position-read failure diagnostics
- 백그라운드 position sweep은 기존 서보별 재시도를 그대로 사용한다.
- 백그라운드 sweep이 1회 소진되면 실패한 서보 ID, 실패 연속 횟수,
  설정된 실패 한계를 담은 24-byte `STATE_FEEDBACK`을 반환한다.
- 성공적인 전체 sweep은 백그라운드 실패 연속 횟수를 초기화한다.
- MCU는 연속 3회 백그라운드 sweep 소진 뒤에만 latch한다.
- 동작 시작과 동작 최종 검증의 읽기 실패는 계속 즉시 fail-closed
  조건이다.
- host는 heartbeat와 feedback 오류 counter를 독립적으로 유지한다.
- 보고된 MCU latch는 계속 즉시 host fault다.

설정된 5 Hz feedback rate에서 3회 실패한 백그라운드 주기는 첫 실패
응답부터 세 번째까지 약 0.4초에 걸쳐 있다. 각 sweep이 이미 실패한
서보를 3회 재시도하므로, latch는 이 세 주기에 걸쳐 서보별 9회
소진된 시도를 나타낸다.

## 변경된 영역

- STM32 version, capability, failure-limit 설정
- STM32 백그라운드 `GET_STATE` failure 상태 머신과 diagnostic 응답
- Host identity gate
- 20-, 24-, 32-byte state 응답에 대한 host와 tool의 protocol parsing
- 타입이 지정된 host position-read/stop-latch 오류
- 독립적인 host heartbeat와 feedback 복구 counter
- Protocol 문서와 회귀 계약

## 검증

- Python/ROS 회귀 suite: `322 passed`
- Native actuator C core: `1/1 passed`, warning을 오류로 처리
- `single_arm_bridge` 로컬 `colcon build`: 통과
- STM32 ARM Release build: compiler warning 없이 통과
- Firmware 크기: text 30516, data 112, bss 4160, 총 34788 bytes
- 표적 `git diff --check`: 통과

이전 전체 suite 실행 2회는 시험 환경 문제로만 실패했다.

1. 저장소 root가 `PYTHONPATH`에 없었음
2. 새로 빌드한 ROS overlay를 source하지 않았음

두 환경 설정을 바로잡은 뒤 전체 suite가 통과했다. 이후 transport
round-trip 시험 2개를 추가해 최종 합계가 322개 통과가 됐다.

## 로컬 빌드 산출물

- HEX: `/tmp/stm32_g474_single_arm_0x00021700.hex`
- HEX SHA-256:
  `0cd04c457780892a2dc07a288396a043c67375d0d8e2f2e3eec2f52ce709795a`
- ELF: `/tmp/stm32_g474_single_arm_0x00021700.elf`
- ELF SHA-256:
  `760cb1397d65dcfd208b3dc2f366a8385edcfb63be5984aabc7c5a5a34fffc1a`

## 범위 경계

이 후보는 로컬에서만 수정·시험·빌드했다. Pi로 전송한 파일은 없고
STM32는 flash나 reset을 하지 않았으며 로봇 동작도 수행하지 않았다.

## 필요한 물리 gate

동작 검증 전:

1. 검토한 host 파일과 검증된 HEX를 backup과 함께 Pi로 전송한다.
2. `single_arm_bridge`를 rebuild하고 설치된 module hash를 검증한다.
3. 12 V를 끄고 팔을 지지한 상태에서 현재 STM32 flash를 backup한다.
4. `program verify reset`을 별도로 승인받아 정확히 1회 수행한다.
5. firmware, calibration, capability, heartbeat identity gate를
   확인한다.
6. 먼저 READ_ONLY를 실행해 물리 torque disable을 확인한다.
7. 그 뒤에만 별도로 승인된 MOTION_ENABLED 무동작 시험과 통제된
   position-read fault 검증을 수행한다.
