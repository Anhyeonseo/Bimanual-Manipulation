# 단계 7 Elbow P28 bounded 후보

- 날짜: 2026-07-31
- 상태: 격리·loaded first-lift P28 gate 통과; 전체 자율 경로는 보류
- 작업 중 로봇 물리 상태: bridge/MoveIt 정지, 12 V OFF, 팔 안전

## 증거와 판단

firmware `0x00021500`, calibration `0x095CB9A5`, Shoulder P32, Elbow
P24에서 반복한 P32 단계 7 경로는 다음과 같은 정착 endpoint 패턴을
보였다.

| 구간 분류 | Shoulder 오차(raw) | Elbow 오차(raw) |
| --- | ---: | ---: |
| pregrasp 1~4 | 2~4 | 17~19 |
| grasp approach 1, 2 | 5 | 19 |
| 첫 20 mm lift 구간 | 26 | 16 |

Elbow 잔차는 항상 같은 방향을 유지했고 30-raw 완료 임계값 아래였지만,
접근 중 지속적으로 가장 큰 잔차였다. 전압은 12.4~12.5 V를 유지했고
서보 보호 fault는 보고되지 않았다. 따라서 다음 bounded 실험은 Elbow
비례 gain만 24에서 28로 바꾼다. torque limit, D/I gain, safety
watchdog, raw limit, 완료 허용치는 변경하지 않는다.

## 후보 identity

- Firmware version: `0x00021600`
- Calibration hash: `0x8AD27897`
- Base P16
- Shoulder P32
- Elbow P28
- Wrist Flex P16
- Wrist Roll P16
- Gripper P16
- Shoulder torque limit: 780 raw
- Elbow torque limit: 650 raw
- D gain: 전축 32
- I gain: 전축 0

## 로컬 검증

- 두 개의 정본 JSON 사본에서 독립적으로 계산한 calibration hash:
  `0x8AD27897`
- Host/firmware calibration table 동기화: PASS
- Firmware/host identity 계약: PASS
- 저장소 Python suite: 213/213 PASS
- `single_arm_bridge` package: 21 tests, 오류 0, 실패 0
- STM32 Cortex-M4 hard-float Release build: PASS
- Firmware 크기: text 30228, data 112, bss 4160 bytes
- HEX SHA-256:
  `f84dad6cd40533916e9687f7b07faf112f47dc549cf0e9bce2dd68a17ee88e41`
- HEX-to-binary round-trip과 linked build output 대조: PASS
- Host/firmware 배포 archive SHA-256:
  `cc9d90213b5141ae274942bf0e895366f8b576723e40dff5ec8917183944552b`
- Pi 배포와 `single_arm_bridge` rebuild: PASS
- Pre-flash 512 KiB rollback backup SHA-256:
  `0bdc5c2bbf9311612d28de28e1e53749f6368e900b1a1466d826914d646987a2`
- STM32 program/verify/reset: PASS
- Post-flash identity와 heartbeat gate: PASS

사전 CMake 설정 시도 2회는 firmware 산출물을 만들지 못했다. 하나는
외부 build 디렉터리에서 상대 toolchain 경로를 사용했고, 다른 하나는
설치되지 않은 Ninja를 요청했다. 최종 빌드는 절대 toolchain 경로를
사용하고 `/usr/bin/make`를 설치해 진행했다.

## 격리 물리 결과

무동작 readback으로 모든 축에서 의도한 실제 설정을 확인했다: Shoulder
P32/780, Elbow P28/650, D32, I0. 이전에 문제였던 ROS-positive/raw
감소 방향으로 Elbow를 2초간 승인된 1회 이동시켜 raw 1602에서 1550을
명령했다.

- Terminal status: 성공
- Firmware terminal detail: 13 raw
- Elbow goal/actual: 1550/1563 raw
- Elbow load/current: 100/5 raw
- Elbow voltage/temperature: 12.5 V/31 C
- 다른 축 endpoint 오차: 0~6 raw
- 사용자 물리 관측: 정상 동작, 이상 진동·소음 없음

기존 P24 접근 잔차는 16~19 raw였다. 따라서 P28 격리 결과는 안정성
저하 없이 3~6 raw 개선됐다. P28을 잠정 채택하며, loaded multiaxis
lift로 최종 확인이 필요하다.

## Loaded first-lift 결과

안정적인 접촉 grasp 이후, 새 host commanded-setpoint 보존을 사용한
약 20 mm lift를 1회 승인해 실행했고 Wrist Flex는 lower raw limit
근처에 고정 유지됐다. Firmware `0x00021600`이 2초 multiaxis 이동을
성공적으로 완료했다.

- Terminal status: 성공
- Firmware terminal detail: 26 raw
- Shoulder goal/actual: 3421/3447 raw (오차 26 raw)
- Elbow goal/actual: 1537/1553 raw (오차 16 raw)
- Wrist Flex goal/actual: 1204/1209 raw (오차 5 raw)
- Elbow load/current: 121/6 raw
- Elbow voltage/temperature: 12.5 V/34 C
- Gripper goal/actual: 1963/1984 raw
- Gripper load/current: 96/4 raw
- 사용자 물리 관측: 물체가 성공적으로 들어올려짐

Elbow P28 잔차는 loaded lift에서도 16 raw를 유지했고 보호 fault나
grasp 손실이 없었다. 이는 개선된 13-raw 격리 결과를 유지하면서
최고의 P24 lift 잔차와 일치한다. Elbow P28을 이후 bounded 단계 7
시험에 채택한다. 이 결과로는 Wrist Flex나 Wrist Roll gain 증가가
정당화되지 않는다.

## 필수 물리 gate

1. P24를 rollback 후보로 유지한다.
2. 자율 pick/place 전에, 기존의 1회성 물리 승인과 충돌 검사로 전체
   perception-to-place 경로를 반복한다.
3. 더 긴 loaded 경로 동안 Elbow load/current와 온도를 계속 관찰한다.
   축별 근거 없이 Wrist gain을 올리지 않는다.
