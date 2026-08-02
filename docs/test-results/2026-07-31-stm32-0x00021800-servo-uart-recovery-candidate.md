# STM32 0x00021800 servo UART recovery 후보

- 날짜: 2026-07-31

## 문제 정의

Firmware `0x00021700`은 백그라운드 position read를 3회-소진
fail-closed 정책으로 올바르게 바꾸고 첫 실패 서보 ID를 노출했다.
하지만 그 아래에 있는 UART 수신 상태 자체는 복구하지 않았다.
반복되는 현장 실패가 여전히 `servo_id=1`로 나타났는데, ID 1이 매
sweep에서 처음 조회되는 축이기 때문이며, STM32를 reset하면
일시적으로 통신이 복구됐다.

기존 bus 구현은 모든 status packet이 처음 수신한 byte에서
시작한다고 가정했다. 부분적이거나 늦게 온 WRITE 응답, 오래된 byte,
다른 ID를 위한 응답, 또는 UART ORE/FE/NE/PE/RTO 상태가 다음
고정-길이 READ를 오염시킬 수 있었다. failure 경로는 이 상태의
일부만 flush했고, framing loss와 연결 끊김·전기적 불안정 bus를
구분할 만큼 충분한 증거를 남기지 않았다.

## 후보 정책

- Firmware identity: `0x00021800`
- Capabilities: `0x000003FF`
- 새 capability bit: `0x00000200`, servo-bus recovery diagnostics
- 각 서보 READ는 bounded byte-stream parser를 사용한다: 50 ms,
  최대 64 byte.
- Parser는 `FF FF`를 찾아 반복된 sync byte를 허용하고, data를
  반환하기 전에 ID·길이·status·checksum을 검증한다.
- 오래된 접두 byte, 다른 ID의 늦은 packet, 잘못된 길이, 손상된
  checksum은 폐기되며 parser는 같은 transaction 안에서 기대하는
  packet을 계속 찾는다.
- 최종 실패 시 `HAL_UART_Abort`를 수행하고 ORE/NE/PE/FE/RTO를
  clear하고 RX data를 flush한 뒤 2 ms 대기하고 RX 상태를 다시
  clear한다.
- WRITE는 이제 선택적 status 응답 도중 timeout날 수 있는 고정
  6-byte 수신을 시작하지 않는다. 대신 그 응답이 2 ms 동안 정착하도록
  두고 atomic하게 flush한다. 안전에 중요한 write는 register
  readback을 계속 유지한다.
- 기존 3회-소진 백그라운드 정책과 즉시 fail-closed되는
  동작-경계(motion-boundary) 동작은 변경되지 않았다.

## Failure diagnostics

position-read failure `STATE_FEEDBACK`은 호환되는 24-byte
접두부에서 capability `0x00000200`이 있을 때 40 byte로 커진다.
다음이 추가된다.

- failure 원인(`TX`, `RX timeout`, `UART`, `header`, `ID`, `length`,
  servo `status`, `checksum`, `recovery`)
- HAL status와 servo status
- 누적 UART recovery 횟수와 폐기된 byte 수
- UART `ErrorCode`와 USART ISR snapshot

잘못된 형식의 frame 뒤에 침묵이 이어지면, 최종 byte timeout으로
덮어써지지 않고 구체적인 parser 거부 사유가 보존된다. 실제
무응답 상태는 계속 `RX timeout`으로, 활성 UART flag는 계속
`UART`로 남는다. Host와 독립 protocol tool은 기존 24-byte 응답과
새 40-byte 응답을 모두 parsing한다.

## Fault injection

Native C parser 시험은 다음을 주입하고 복구를 검증한다.

1. 임의의 오래된 접두 byte
2. 겹치거나 반복된 `FF FF FF` 동기화 경계
3. 잘못된 서보 ID의 완전한 늦은 응답 뒤 기대하는 응답
4. 잘못된 checksum 응답 뒤 기대하는 응답
5. 잘못된 길이 header 뒤 기대하는 응답
6. target 서보의 status 오류를 최종 분류된 failure로 처리

정적 firmware 계약은 추가로 전체 HAL abort/flag-clear/RX flush
복구 순서, bounded receive loop, 부분 WRITE 응답 drain 제거,
firmware identity, capability, 40-byte diagnostic payload를
검증한다.

## 검증

- Python/ROS 회귀 suite: `329 passed`
- Native parser fault-injection 시험: `-Wall -Wextra -Wpedantic
  -Werror`로 통과
- Native actuator C core: `1/1 passed`, warning을 오류로 처리
- `single_arm_bridge` 로컬 `colcon build --symlink-install`: 통과
- STM32 ARM Release build: compiler warning 없이 통과
- Firmware 크기: text 31560, data 112, bss 4176, 총 35848 bytes
- `git diff --check`: 통과

첫 전체 suite 실행은 ROS 2와 로컬 package overlay를 source하지
않아 collection 중 멈췄다. ROS 2 Jazzy를 source하고 로컬
package를 `PYTHONPATH`에 추가한 뒤 329개 시험 전부 통과했다.

## 로컬 빌드 산출물

- HEX: `/tmp/stm32_g474_single_arm_0x00021800.hex`
- HEX SHA-256:
  `4b9ca7c7b3927ce798048258fb1b3deecfb0718d660c6c1bd93308862ef3f317`
- ELF: `/tmp/stm32_g474_single_arm_0x00021800.elf`
- ELF SHA-256:
  `2fd820e03fb2624d5f77fdc43d2d40361058b849cc110dc53f123ecd3306d0ca`

## 범위 경계

이 후보는 로컬에서만 수정·fault-injection 시험·빌드했다. Pi로
전송한 파일은 없다. STM32는 flash나 reset을 하지 않았고
`CLEAR_FAULT`도 실행하지 않았으며 로봇 동작도 요청하지 않았다.

## 필요한 물리 gate

동작 검증 전:

1. 로컬 diff를 검토하고, 검토된 host 파일과 검증된 HEX만 backup과
   함께 Pi로 전송한다.
2. Pi에서 `single_arm_bridge`를 rebuild하고 source, 설치된 module,
   HEX hash를 검증한다.
3. 12 V를 끄고 팔을 지지한 상태에서 현재 STM32 flash를 backup한다.
4. 검증된 HEX SHA를 사용해 `program verify reset`을 별도로 승인받아
   정확히 1회 수행한다.
5. firmware `0x00021800`, calibration `0x8AD27897`, capabilities
   `0x000003FF`, latch-clear 상태, heartbeat identity를 확인한다.
6. 먼저 READ_ONLY를 실행해 6축 물리 torque disable을 확인한다.
7. 그 뒤에만 별도로 승인된 무동작·통제된 fault-injection 검증을
   수행한다.
