# 오른팔 ID 2~6 방향 관측

- 날짜: 2026-08-02 KST

## 범위

수리된 SO-ARM101 오른팔에 대한 방향 전용 시운전 관측이다. ID 2~6
5개 관절을 대상으로 하며(ID 1 방향은 이전에 별도로 확인됨), 잠정
운용 범위, 정상 동작 gain, 정상 torque limit, 전체 팔 동작
authorization을 승인하지 않는다.

공통 조건:

- 보드: NUCLEO-G474RE
- 서보 adapter: Waveshare Bus Servo Adapter, UART mode
- Firmware: `firmware/stm32_g474_right_arm`. 관절마다
  `RIGHT_ARM_CALIBRATION_SERVO_ID`를 해당 ID로 설정해 재빌드·재플래시
- ASCII console(관절 번호 선택, `M` 이동, `R` 복귀, `D` disable),
  정상 binary motion은 잠금
- 다른 모든 관절 선택과 다축 명령: 잠금

## ID 2 — SHOULDER

- 서보가 관측한 전원: 12.4 V
- Calibration P gain: 12, torque limit: 250 raw
- 동작: 1000 ms 동안 상대 raw 증가

첫 번째 bounded probe는 raw 1973에서 1985로 이동했고 shoulder가
앞으로 움직이는 것으로 관측됐다. 더 큰 확인 probe 결과:

| 측정 | Raw |
| --- | ---: |
| 시작 | 1979 |
| 목표 | 2036 |
| 종료 | 2030 |
| 목표 오차 | -6 |

물리 동작이 명확히 보였고 다시 팔이 앞으로 움직였다. 따라서 ID 2
raw 증가는 앞쪽으로 기록하며 기존 `positive_raw_direction = +1`
후보 값이 방향 검증됐다.

복귀 명령은 raw 2030에서 캡처된 시작 raw 1979로 이동해 raw 1981에서
종료했고 오차는 +2 raw였다. 이어진 전축 torque-disable 명령은
register 40을 6개 ID 전부 0으로 쓰고 모든 readback을 검증했다.

남은 gate: 최소/최대 raw 위치 미측정, 정상 P gain·torque limit
미승인.

## ID 3 — ELBOW

- 서보가 관측한 전원: 12.4~12.5 V
- 동작: 1000 ms 동안 상대 raw 증가

**첫 두 차례 시도는 판정 불가**였다(torque/gain 부족).

- 시도 1 (P gain 12, torque limit 250 — ID 2 시험과 같은 상수):
  `OUT_START POS=2174 TARGET=2231` → `OUT_END=2177 ERROR=-54`. 명령한
  57 raw 중 약 3 raw만 움직였고, 조작자의 육안 판단도 불확실했다
  ("아래로 가는거 같은데").
- 시도 2 (P gain 12, torque limit을 450으로 상향): `OUT_START
  POS=2175 TARGET=2232` → `OUT_END=2176 ERROR=-56`. torque limit만
  올려서는 차이가 없었다(움직임이 여전히 0에 가까움). 병목이 torque
  상한이 아니라 위치 오차 gain이었음을 보여준다.
- 결론: `RIGHT_ARM_CALIBRATION_P_GAIN=12`(ID 2, SHOULDER용으로 조정된
  값)는 ELBOW의 부하를 57 raw 전체 명령 step만큼 구동하기에 너무
  약하다. 이 두 차례 시도에서는 calibration 필드를 바꾸지 않았으며,
  증거로만 기록한다.

**확인 시도**: `RIGHT_ARM_CALIBRATION_P_GAIN`을 12에서 28로 올렸다
(ELBOW 자체의 잠정 운용 `p_gain`으로 `config/single_arm_calibration.json`에
이미 기록된 값). torque limit은 450으로 유지, firmware 재빌드·재플래시.

| 측정 | Raw |
| --- | ---: |
| 시작 | 2175 |
| 목표 | 2232 |
| 종료 | 2235 |
| 목표 오차 | +3 |

이번에는 물리 동작이 명확히 보였고 전완/손목 끝을 아래로 움직였다.
따라서 ID 3 raw 증가는 아래쪽으로 기록한다.

복귀 명령은 raw 2237에서 캡처된 시작 raw 2175로 이동해 raw 2211에서
종료했고 오차는 +36 raw였다(출발 방향보다 큼 — 복귀 방향이 부하를
거슬러 더 많은 힘이 필요하다는 것과 일치). 전축 torque-disable
명령(`D`)은 `TORQUE_DISABLED_ALL_VERIFIED`를 보고했다.

남은 gate: 최소/최대 raw 위치 미측정. 정상 P gain(28)과 torque
limit은 잠정값이며 미승인이다. +36 raw 복귀 오차는 복귀 방향이
전체 범위에서 더 많은 여유가 필요할 수 있음을 시사한다.

## ID 4 — WRIST_FLEX

- `RIGHT_ARM_CALIBRATION_P_GAIN` 16, torque limit 450
- 이 관절에 한해 `RIGHT_ARM_CALIBRATION_DIRECTION`을 **-1**로
  설정했다. 잠정 후보 범위(`minimum_raw` 1194, `maximum_raw` 2048)는
  `maximum_raw`가 정확히 home 위치라서, firmware의 기존 고정 동작인
  `+57` probe를 그대로 쓰면 검증되지 않은 후보 범위를 즉시
  초과했을 것이다. 이 관절을 범위 안쪽인 낮은 raw 쪽으로 probe할 수
  있도록 calibration firmware(`single_arm_config.h` /
  `single_arm_app.c`)에 부호 있는 방향 옵션을 추가했다.
- 동작: 1000 ms 동안 -57 raw 상대 변화

| 측정 | Raw |
| --- | ---: |
| 시작 | 2265 |
| 목표 | 2208 |
| 종료 | 2220 |
| 목표 오차 | +12 |

물리 동작이 명확히 보였고 이 raw **감소**에서 손목이 **위로**
움직였다. 따라서 역으로, ID 4 raw **증가**는 손목을 **아래로**
굽히는 것으로 기록한다.

복귀 명령은 raw 2220에서 캡처된 시작 raw 2265로 이동해 raw 2263에서
종료했고 오차는 -2 raw였다. 전축 torque-disable 명령(`D`)은
`TORQUE_DISABLED_ALL_VERIFIED`를 보고했다.

남은 gate: 캡처된 시작 위치(raw 2265)는 이미 coded home(2048)에서
217 raw 떨어져 있었다(firmware 512-raw gate 안쪽이지만, 다른 관절만큼
home에 딱 맞지 않음). 최소/최대 raw 위치 미측정. 정상 P gain(16)과
torque limit은 잠정값. ID 6도 같은 이유(home이 maximum_raw)로
`RIGHT_ARM_CALIBRATION_DIRECTION = -1`이 필요하다.

## ID 5 — WRIST_ROLL

- `RIGHT_ARM_CALIBRATION_P_GAIN` 16(이 관절 자체의 잠정 운용 gain이며,
  ID 3에서 공용 기본값 12가 일부 관절에는 너무 약할 수 있다는 교훈을
  얻은 뒤 첫 시도부터 적용), torque limit 450
- 동작: 1000 ms 동안 상대 raw 증가

| 측정 | Raw |
| --- | ---: |
| 시작 | 2041 |
| 목표 | 2098 |
| 종료 | 2094 |
| 목표 오차 | -4 |

첫 시도에서 물리 동작이 명확히 보였다. Elbow 쪽에서 gripper 쪽을
바라볼 때 손목이 반시계 방향으로 회전했다. 따라서 ID 5 raw 증가는
반시계 방향(elbow→gripper 시점)으로 기록한다.

복귀 명령은 raw 2094에서 캡처된 시작 raw 2041로 이동해 raw 2045에서
종료했고 오차는 +4 raw였다. 전축 torque-disable 명령(`D`)은
`TORQUE_DISABLED_ALL_VERIFIED`를 보고했다.

남은 gate: 최소/최대 raw 위치 미측정, 정상 P gain(16)·torque limit
잠정값.

## ID 6 — GRIPPER

- `RIGHT_ARM_CALIBRATION_P_GAIN` 16, torque limit 450,
  `RIGHT_ARM_CALIBRATION_DIRECTION` -1(잠정 `maximum_raw`가 ID 4와
  같은 이유로 home에 있음)
- 동작: 1000 ms 동안 -57 raw 상대 변화
- gripper는 단일 actuator, 단일 이동 jaw 구조다(한쪽 jaw는 고정,
  ID 6이 다른 쪽 jaw를 구동). 조작자는 고정 쪽에 풀린 나사나 빠진
  링크가 없음을 확인한 뒤 이것이 고장이 아니라 기구의 정상 설계임을
  확인했다.

| 측정 | Raw |
| --- | ---: |
| 시작 | 2049 |
| 목표 | 1992 |
| 종료 | 1994 |
| 목표 오차 | +2 |

물리 동작이 명확히 보였다: 이 raw 감소에서 gripper의 이동 jaw가
닫혔다. 따라서 역으로, ID 6 raw 증가는 gripper를 여는 것으로
기록한다.

복귀 명령은 raw 1994에서 캡처된 시작 raw 2049로 이동해 raw 2045에서
종료했고 오차는 -4 raw였다. 전축 torque-disable 명령(`D`)은
`TORQUE_DISABLED_ALL_VERIFIED`를 보고했다.

남은 gate: 최소/최대 raw 위치 미측정, 정상 P gain(16)·torque limit
잠정값.

## 종합

이제 여섯 관절(ID1~ID6) 방향을 모두 관측했다. `motion_authorized`는
계속 false이며, 축별 최소/최대 raw 측정이 다음 gate다(`docs/RIGHT_ARM_PORT_STATUS.md`
참고).
