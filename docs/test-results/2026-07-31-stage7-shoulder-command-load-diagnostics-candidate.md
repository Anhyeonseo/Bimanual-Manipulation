# 단계 7 Shoulder command/load diagnostics 후보

- 날짜: 2026-07-31
- 상태: `0x00021500` P32 물리 gate 통과; 조건부 채택

## 문제 정의

반복된 Shoulder `-0.08 rad / 2 s` 시험이 최종 오차 42, 52 raw로
끝났다. 마지막으로 멈춘 snapshot은 `position_raw=3301`,
`load_magnitude_raw=220`, `current_raw=21`, `voltage=12.3 V`, `P=16`,
runtime torque limit 780을 보고했다. 이 값들만으로는 torque-limit
saturation을 증명할 수 없다. 이전 diagnostics는 servo Goal Position
register나 EEPROM protection 설정을 노출하지 않아서 명령 적용과
물리 load를 분리할 수 없었다.

## 공식 SO-101 기준값

- SO-101 follower: STS3215 모터 6개, 전부 1/345 기어비
- 12 V STS3215는 반드시 12 V 전원을 사용해야 한다. 7.4 V와 12 V
  버전은 호환되지 않는다.
- 현재 LeRobot follower 기본값은 P=16, I=0, D=32이며 sync write로
  `Goal_Position`을 명령한다.

참고:

- https://huggingface.co/docs/lerobot/en/so101
- https://huggingface.co/docs/lerobot/main/assemble_so101
- https://github.com/huggingface/lerobot/blob/main/src/lerobot/robots/so_follower/config_so_follower.py
- https://github.com/huggingface/lerobot/blob/main/src/lerobot/robots/so_follower/so_follower.py
- https://github.com/huggingface/lerobot/blob/main/src/lerobot/motors/feetech/tables.py

## 로컬 진단 전용 변경

수정된 후보 identity는 firmware `0x00021300`, capabilities
`0x000000FF`다. gain, torque cap, calibration limit, trajectory,
safety threshold는 아무것도 바꾸지 않았다.

관절별 diagnostic payload가 이제 다음을 노출한다.

- runtime Goal Position register 42..43
- model number와 servo firmware version
- EEPROM maximum torque limit
- minimum startup force와 CW/CCW dead zone
- protection current, operating mode, protective torque/time,
  overload torque

이로써 이후의 통제된 시험 1회로 다음을 구분할 수 있다.

1. Goal register 불일치: host/firmware/servo 명령 경로 결함
2. Goal은 일치하는데 load/current가 낮게 유지: servo 설정, 기어/전압
   변형, 또는 position-loop authority 문제
3. Goal은 일치하고 load/current가 limit 쪽으로 상승: 실제 하중/모멘트
   부족

## 로컬 검증

- ROS 소스 전체 Python suite: 290 통과
- Native actuator core: 빌드 PASS, CTest 1/1 PASS
- Cortex-M4 Release build: PASS
- `git diff --check`: PASS
- 거부된 배포 `0x00021200` HEX SHA-256:
  `305a04abc30afbc9991597a295b1459fa9809d7c102008bf3d52725cb47a234c`
- 수정된 로컬 `0x00021300` HEX SHA-256:
  `52f8b62426bab7742ab8a9250b60439633bb8c12f6b35ef97bb5dc77515aaa7b`

## 0x00021200 배포 결과

firmware `0x00021200`, calibration `0x4D62F8D5`, capabilities
`0x000000FF`로 host와 firmware identity gate를 통과했다. 이어진 첫
READ_ONLY diagnostic 요청은 로봇 동작 없이 fail-closed됐다.

```text
servo diagnostics rejected: diagnostic read failed:
joint_index=0 status=2 read_status=0x10
```

근본 원인은 firmware에서 결정론적이었다. `Servo_ReadData()`는 bus
transaction당 최대 16 byte만 허용하는데, 새 protection diagnostic이
EEPROM 주소 13..39를 한 번의 27-byte 요청으로 요구했다. 이 함수는
그 요청을 서보로 전송하지도 못한 채 로컬에서 `HAL_ERROR`를
반환했다.

`0x00021300` 수정은 같은 register map과 diagnostic payload를
유지하되, 이 블록을 두 번의 bounded transaction으로 나눠 읽는다:
주소 13..28(16 byte)과 29..39(11 byte). 계약 시험이 이제 이 초과
크기 읽기의 재도입을 거부한다.

## 0x00021300 READ_ONLY 배포 결과

수정된 firmware가 firmware `0x00021300`, calibration `0x4D62F8D5`,
capabilities `0x000000FF`로 host identity를 통과했다. 6축 READ_ONLY
diagnostic이 `success=True`를 반환했다. 동작이나 fault clear는
요청하지 않았다.

6개 서보 모두 model 777, firmware 3.10, EEPROM maximum torque 1000,
minimum startup force 16, CW/CCW dead zone 1/1, protection current
310, operating mode 0, protective torque 20, protection time 200,
overload torque 80을 보고했다. 정지 전압은 12.3~12.4 V였다. Shoulder
runtime 설정은 P/D/I 16/32/0, torque limit 780이었다.

READ_ONLY는 물리적으로 torque를 비활성화하므로 load와 current는
0이었고 Goal Position은 마지막 명령값을 유지했다. 따라서 큰 Elbow
position/goal 차이는 실패한 동작 명령이 아니라 지지된 수동
재배치를 기록한 것이다. 다음 구분 gate는 torque가 활성화된 상태에서
MOTION_ENABLED 명령 1회 직후 diagnostics를 실행하는 것이다.

## 0x00021300 단일 동작 진단 결과

승인된 Shoulder 명령 1회가 다른 arm joint를 유지한 채 ROS target을
2초에 걸쳐 1.8775924844에서 1.7975924844 rad로 이동시켰다. firmware는
최종 오차 37 raw로 soft abort를 반환했고 safety stop을 latch하지
않았다. 직후 diagnostics는 다음을 보였다.

- Shoulder Goal Position: 3220 raw(요청한 target과 정확히 일치)
- Shoulder 실제 위치: 3257 raw
- 측정된 잔차: 37 raw, terminal 결과와 일치
- load magnitude: 160 raw, current: 13 raw, voltage: 12.3 V
- runtime P/D/I: 16/32/0, torque limit: 780, maximum torque: 1000

이는 Goal Position 누락/오류, host/firmware 변환 결함, torque-limit
saturation, 정지 전압 강하를 모두 배제한다. 서보는 최종 goal을
받아들였지만 낮은 load/current 상태로 약 3.25도 벗어난 곳에서
정착했다. 남은 주된 원인은 중력 아래에서의 Shoulder position-loop
강성 부족과 gearbox backlash·구조적 유연성의 결합이다. 완료 허용치를
늘리면 오차를 감추는 것일 뿐 해법이 아니다. 다음 통제된 후보는
Shoulder P=24(Elbow가 이미 쓰는 것과 같은 값)이며 torque와 safety
limit은 변경하지 않는다.

## 0x00021400 Shoulder P=24 후보

증거 기반 수정은 Shoulder runtime P gain만 16에서 24로 바꿔 기존
Elbow gain과 맞춘다. Shoulder torque limit은 780을 유지하고;
load/current watchdog, raw range, trajectory timing, 최종 허용치는
변경하지 않는다. P gain은 hardware calibration identity의 일부이므로
host와 firmware calibration hash가 함께 `0x4D62F8D5`에서
`0xAFCC3512`로 바뀐다.

로컬 검증:

- ROS 소스 전체 Python suite: 290 통과
- native actuator core: CTest 1/1 통과
- Cortex-M4 Release build: 통과
- `git diff --check`: 통과
- `0x00021400` HEX SHA-256:
  `fb7613256ba6ab4f1e754fe97151223457a2a23d0b1f65b46b21cb7ddf2178fb`

이 로컬 후보 빌드에서는 Pi 배포, STM32 flash, fault clear, 로봇 동작
어느 것도 수행하지 않았다.

## 0x00021400 Shoulder P=24 동작 결과

승인된 P=24 비교 명령이 Shoulder를 2초에 걸쳐 -0.08 rad 이동시켰다.
Goal Position은 3537 raw로 올바르게 기록됐고, 실제 위치는 3568 raw로
정착해 safety latch 없이 31-raw soft abort가 발생했다. 직후
diagnostics는 load 196, current 19, voltage 12.2 V, P/D/I 24/32/0,
torque limit 780을 보고했다. Shoulder 진동이나 전압 강하는
보고되지 않았다.

P=16 잔차 37 raw와 비교해 P=24는 잔차를 31 raw로 줄이고 실제 이동
거리를 늘렸지만, 변하지 않은 30-raw 수락 한계를 1만큼 놓쳤다. Load와
current는 독립적인 800/320 watchdog보다 훨씬 낮게 유지됐다. 이는
마지막 bounded 비례 후보인 Shoulder P=32를 뒷받침하며, 정지 규칙은
다음과 같다: P=32로도 진동 없이 허용치를 만족하지 못하면 gain을 더
올리지 않고 중력 보상/평형추 또는 별도로 설계한 적분 제어기로
전환한다.

## 0x00021500 Shoulder P=32 최종 bounded 후보

마지막 비례 전용 후보는 Shoulder P gain을 24에서 32로 바꾼다.
Shoulder torque limit은 780, 최종 허용치는 30 raw를 유지하며 모든
load/current watchdog, trajectory timing, raw limit, shutdown 동작은
변경하지 않는다. 동기화된 host/firmware calibration identity는
`0x095CB9A5`다.

로컬 검증:

- ROS workspace: 8개 package 빌드
- ROS 소스 전체 Python suite: 290/290 통과
- `single_arm_bridge` ament 결과: 21 tests, 오류/실패 0
- native actuator core: CTest 1/1 통과
- Cortex-M4 clean Release: 통과, text 30228, data 112, bss 4160
- `git diff --check`: 통과
- `0x00021500` HEX SHA-256:
  `6a78cd9eaaadd284af2f35333c7f1317c7c4afe99b023cfcddfe8c98d9c62c23`

Pi 배포, STM32 flash, fault clear, 로봇 동작 어느 것도 수행하지
않았다. 이는 정지 규칙 후보다: 비교 동작 1회만 실행한다. 이번에도
30 raw를 놓치거나 진동이 발생하면 비례 gain을 다시 올리지 않는다.
다음 해법은 중력 보상/평형추 또는 자체 bounded safety 검증을 갖춘
별도 설계 적분 제어기여야 한다.

## 0x00021500 Shoulder P=32 물리 결과

승인된 단일 비교 명령이 Shoulder를 2초에 걸쳐 2.3009711818에서
2.2209711818 rad로 이동시켰다. firmware는 `state=succeeded`, status
6, detail 26을 보고했다: safety latch나 재시도 없이 변경되지 않은
30-raw 최종 허용치를 만족했다. 직후 diagnostics는 Goal Position
3496 raw, 실제 위치 3522 raw, load 216, current 21, voltage 12.2 V,
온도 35 C, P/D/I 32/32/0, runtime torque limit 780을 보였다.

사용자는 진동을 관측했지만, 새로운 P32 진동이 아니라 SO-ARM101
참고 영상에서 보이는 정상 동작과 동등한 수준으로 판단했다. Shutdown은
깨끗했고 12 V를 제거해 팔을 안전 상태로 두었다. 이로써 bounded
진행은 P16=37 raw, P24=31 raw, P32=26 raw로 마무리됐다. 단계 7에
P32를 조건부 채택하며, 추가 비례 증가나 이 진단 동작의 반복은
승인하지 않는다. 이후 trajectory 시험도 진동, 온도, 전압, load,
current를 계속 관찰해야 한다. 관측된 기준선을 넘어서는 증가가 있으면
이 gate를 다시 열고, P gain을 32 이상으로 올리는 대신 중력 보상/
평형추 또는 별도 설계 적분 제어가 필요하다.

## 필요한 다음 gate

1. 12 V OFF 상태에서 Shoulder ID 2가 1/345 기어비의 STS3215 12 V
   follower 모터인지 물리적으로 확인한다. Model register 777만으로는
   전압이나 기어 변형을 구분할 수 없다.
2. Host와 firmware는 명시적 승인 후에만 함께 배포한다.
3. 먼저 READ_ONLY diagnostics를 실행한다. 동작 없음.
4. 지지된 Shoulder 동작을 정확히 1회 실행하고, bridge를 정지하기
   전에 terminal 결과 직후 diagnostics를 캡처한다.
5. 위 경로가 입증된 뒤에만 gain을 조정하거나 기구를 재설계한다.
