# 단계 7 gripper hold setpoint 개선

- 날짜: 2026-07-31
- 상태: host 근본 수정을 첫 loaded lift로 물리 검증함

## 물리 관측

`0x00021500` P32 팔이 grasp pose에 도달해 물체를 성공적으로 잡았다.
close 직후:

- 요청한 gripper 위치: `0.13 rad`, raw Goal Position `1963`
- 측정된 접촉 위치: 약 `0.098 rad`, raw position `1984`
- load magnitude: `96`, current: `4`, 온도: `37 C`

첫 승인된 20 mm lift 구간이 firmware status 6, detail 26으로
완료됐고 사용자는 물체가 물리적으로 들어올려졌음을 확인했다. 하지만
lift 후 diagnostic은 다음을 보였다.

- gripper Goal Position이 `1963`에서 `1984`로 바뀜
- 측정 위치는 `1985`
- load와 current가 모두 0으로 떨어짐

이에 두 번째 lift 구간을 취소했다. 물체를 회수하고 두 ROS process를
정지하고 12 V를 제거해 팔을 안전 상태로 두었다.

## 근본 원인

두 ROS Action은 결국 하나의 6축 STM32 setpoint를 보낸다. arm
adapter는 5개 arm target을 받아들이고, 생략된 6번째 축(gripper)을
최신 **측정된** gripper feedback으로 채웠다.

```text
arm target + 실제 gripper 위치
```

접촉 grasp 중에는 측정 위치가 명령한 closing target과 의도적으로
다르다. 측정 위치를 재사용하면 lift 도중 접촉 잔차가 새로운 완화된
gripper goal로 바뀌어버린다. 물체는 오직 형상과 수동적 마찰로만
붙잡혀 있었으므로, 이 물리 lift는 유효한 active-hold lift gate로
인정할 수 없다.

같은 결함이 gripper adapter에도 대칭적으로 있었다: 마지막으로 성공
명령한 arm target 대신 측정된 arm 위치를 보존하고 있었다.

## Host-only 수정

이제 하나의 bridge instance가 소유하는 공유·thread-safe
`CommandedSetpointState`가 두 Action adapter 모두에 전달된다.

- 성공한 명령이 없을 때만, 생략된 축은 그 한 번의 goal에 한해 최신
  물리 feedback으로 대체한다.
- 전체 6축 target은 firmware가 성공적인 동작 완료를 보고한 뒤에만
  commit된다.
- 이후의 arm goal은 commit된 gripper target을 보존한다. 측정된
  gripper 위치와 다른 접촉 target도 포함한다.
- 이후의 gripper goal은 commit된 arm target을 보존한다.
- Abort, cancel, 연결 끊김, 명시적 fault 복구, transport fault,
  shutdown, adapter 해제는 저장된 target을 폐기한다.
- 거부되거나 유효하지 않은 feedback은 절대 명령 의도로 commit되지
  않는다.

이는 host-only 의미 수정이다. Firmware는 `0x00021500`, calibration은
`0x095CB9A5`로 유지되며 torque, gain, limit, tolerance, trajectory,
protocol, STM32 flash 변경이 필요 없다.

## 로컬 검증

- 표적 state/arm/gripper suite: 30/30 통과
- ROS-overlay 전체 저장소 suite: 212/212 통과
- `single_arm_bridge` build: 통과
- 설치된 module import: 통과
- `single_arm_bridge` ament 결과: 21 tests, 오류/실패 0

핵심 회귀 시험은 성공한 `0.10 rad` gripper 명령 이후 `0.07 rad`
접촉 feedback을 모사한다. 뒤이은 arm Action은 gripper 축에서 측정된
`0.07 rad`가 아니라 `0.10 rad`(`100000 urad`)를 전송해야 한다.

## 물리 회귀 결과

수정된 host를 Pi에 배포하고 firmware `0x00021600`, calibration
`0x8AD27897`, Shoulder P32, Elbow P28로 시험했다. Gripper가 명령된
Goal Position raw `1963`, 측정 접촉 위치 raw `1984`, load `96`,
current `4`로 물체를 잡았다.

승인된 약 20 mm arm lift 1회가 firmware status 6, detail 26으로
성공적으로 완료됐다. 직후 diagnostic이 다음을 확인했다.

- gripper Goal Position은 raw `1963` 유지
- 측정 접촉 위치는 raw `1984` 유지
- load는 `96` 유지
- current는 `4` 유지
- torque는 계속 활성화
- 사용자는 물체가 물리적으로 들어올려졌음을 확인

따라서 arm Action이 측정 feedback으로 대체하지 않고 활성 gripper
접촉 명령을 보존했다. 근본 수정은 첫 loaded lift에서 물리적으로
검증됐다.

## 남은 범위

이 결과는 무인 또는 자율 pick/place를 승인하지 않는다. 다음 gate는
기존 충돌·workspace·diagnostics·1회성 motion 승인을 갖춘 완전한
perception-to-place 실행이다. Abort/cancel과 fault 복구 동작은
자동 시험으로 계속 커버되며, 이 실행 동안에도 계속 관찰해야 한다.
