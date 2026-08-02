# STM32 물리 torque-disable firmware 빌드

- 날짜: 2026-07-28
- 보드 대상: NUCLEO-G474RE / STM32G474RETx
- Firmware identity: `0x00020800`
- Calibration identity: 변경 없음 `0x3DB42B48`
- 빌드/플래시 중 로봇 전원: OFF
- 플래시 수행: YES, OpenOCD program/verify PASS
- 최종 로봇 전원 상태: OFF

## 안전 변경

host `DISABLE` transaction은 이제 성공 상태 응답을 반환하기 전에
`Servo_DisableTorqueAll()`을 호출한다. firmware는 다음을 수행한다.

1. 6개 서보 ID 전부의 STS3215 Torque Enable register `40`에 `0`을 쓴다.
2. 하나가 실패해도 나머지 ID를 계속 진행한다.
3. 6개 서보 전부의 register `40`을 다시 읽는다(readback).
4. write/readback 중 하나라도 실패하면 safety fault를 보고하고 stop을
   latch한다.
5. 이전 서보 trajectory 설정을 무효화한다.

host identity gate는 firmware `0x00020800`을 요구하도록 바뀌어서 기존
`0x00020700` 이미지가 새 bridge와 조용히 함께 실행되지 않는다.

## 빌드 검증

Toolchain:

- `arm-none-eabi-gcc 13.2.1`
- `CMake 3.28.3`
- Cortex-M4 hard-float Release build

결과:

```text
text   data   bss    dec    hex
26116  112    3080   29308  727c
```

ELF는 `ELF32`, little-endian, ARM EABI5, hard-float이며 entry point는
`0x08003d81`이다. Vector table은 stack pointer `0x20020000`과 reset
handler `0x08003d81`로 시작한다.

`arm-none-eabi-nm`과 disassembly로 다음을 확인했다.

- `Servo_DisableTorqueAll`이 `0x080024dc`에 존재한다.
- binary request handler가 state response path 이전에 이를 호출한다.
- Torque Enable register 주소 `40`이 컴파일된 함수 안에 존재한다.

빌드 산출물은 로컬에서 생성·검증한 뒤, ELF/HEX/BIN/MAP이 재현 가능한
결과물이므로 의도적으로 Git에서 제외했다. 아래의 불변 hash와
source/build 계약이 영구 증거로 남는다.

SHA-256:

```text
7aea1a4b63d3c6778246e19e130c65060f6831bada68f343568a2624891f8561  ELF
000a4737ad94ad8a0453d682fdc7fb0326e7ad2009cf3aef9ffae20cc643122a  HEX
8303373c37274b491702d98805a760f44ef048a798fe75df15160f5df27f20d5  BIN
```

전체 512 KiB pre-flash rollback readback은 Pi에 다음 경로로 저장돼 있다.

```text
/home/pi/firmware_updates/backup/stm32_before_0x00020800.bin
SHA-256 021f386ae02889d4632baeac19e4bff81c7c1415d4e5eab7e0e39ad969beef76
```

시험:

- STM32 physical-disable/host identity 계약: PASS
- 관련 bridge/action 시험: 32 PASS
- 플랫폼 독립 actuator C core: 1/1 PASS
- 컴파일된 ELF의 physical-disable symbol/call 검사: PASS

## Flash와 물리 수락

OpenOCD가 ST-LINK V3와 STM32G47/G48 Cortex-M4 target을 `3.297 V`에서
식별했고, `512 KiB` dual-bank flash와 RDP level 0이었다. Programming과
verification 모두 완료됐다.

```text
Programming Finished
Verified OK
Resetting Target
```

이어서 read-only HELLO identity 확인 결과:

```text
protocol=1
joints=6
firmware=0x00020800
calibration=0x3DB42B48
capabilities=0x0000000F
stop_latched=0
HOST_IDENTITY_GATE=PASS
```

팔을 기계적으로 지지한 상태에서, trajectory나 setpoint 명령 없이 bridge를
한 번 시작했다. `MOTION_ENABLED`로 진입했다. Ctrl+C가 bridge를 깨끗이
종료했고 `DISABLE during shutdown failed` 메시지는 나타나지 않았으며,
조작자가 5개 arm 축과 gripper의 holding torque가 풀렸음을 물리적으로
확인했다. 이후 12 V 서보 전원을 껐다.

물리 결과:

- host/firmware identity gate: PASS
- setpoint 없는 bridge enable: PASS
- shutdown DISABLE 확인: PASS
- 실제 6개 서보 torque 해제: PASS
- process 정상 종료: PASS
- 의도하지 않은 명령 동작: 0
- 최종 12 V 상태: OFF

fail-closed readback-error 경로는 source/host 시험으로 계속 커버된다.
이번 수락 시험에서는 실제 servo bus에 의도적인 fault를 주입하지 않았다.
