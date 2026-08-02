# 단계 7 감독형 실제 Pick and Place

- 날짜: 2026-07-31
- 결과: **PASS — 감독형 end-to-end 1 cycle**

## 범위

이 결과는 단계 7 시운전(commissioning) 실행을 마감하는 것이며, 50회
반복 신뢰성 benchmark는 아니다. 모든 물리 전이는 먼저 plan-only로
확인하고, hash로 고정하고, fresh-state를 확인하고, 명시적으로 1회
승인받은 뒤 실행했으며, 실패한 gate가 있으면 즉시 중단했다. 자동
재시도와 `CLEAR_FAULT`는 사용하지 않았다.

## 채택된 하드웨어·제어 identity

- firmware: `0x00021800`
- protocol: `1`
- calibration: `0x8AD27897`
- capabilities: `0x000003FF`
- Shoulder: P/D/I `32/32/0`, torque limit `780`
- Elbow: P/D/I `28/32/0`, torque limit `650`
- firmware HEX SHA-256:
  `4b9ca7c7b3927ce798048258fb1b3deecfb0718d660c6c1bd93308862ef3f317`
- guarded external executor SHA-256:
  `3d28e2f7856a91e792f98ec48fd87a5debda094a688001851e6c61bbd3d3ec7c`

Firmware `0x00021800`은 bounded 서보-response stream 재동기화, 전체
UART 복구, 분류된 복구 diagnostics를 추가한다. 작업 동작 전에
identity/capability, READ_ONLY 6축 물리 disable, MOTION_ENABLED
무동작 readback, 5분 heartbeat/feedback 실행, 주입된 single-sweep
failure, reset 없는 6축 복구를 통과했다.

## 실행 안전 계약

External executor는 매 arm 구간 전후로 다음을 강제했다.

- 정확한 protocol, calibration, 6개 서보 identity
- 6축 torque-enabled diagnostics
- Shoulder 온도가 `50 C` 미만으로 엄격히 유지
- Shoulder는 `0.055 rad`, 다른 arm 축은 `0.050 rad`의 start/
  post-settle 오차
- plan interpolation과 fresh 측정 실행 범위를 분리
- soft abort 후 재전송 없음; 2초 settle과 fresh state/diagnostic
  통과 후에만 계속

Bridge는 여전히 의도적으로 제한된 single-point Action 계약을
구현한다. 따라서 시운전을 위해 긴 경로는 각각 독립적으로 정착·진단된
여러 Action으로 실행했다. 이는 안전 검증 방법이지 의도한
생산-속도 trajectory 계약이 아니다.

## 물리 결과

최종 감독형 cycle이 완료한 내용:

1. fresh grasp correction
2. gripper close와 물체 hold 확인
3. 약 20 mm lift
4. Place pregrasp로 이동
5. Place 하강과 bounded 5 mm Z correction 2회
6. 물체 지지 확인과 gripper release
7. Place pregrasp로 retreat
8. 충돌 없는 q0 복귀
9. 경고 없는 Bridge shutdown, 12 V off, 팔 안전

채택된 Place와 복귀 구간에 사용한 최종 plan:

| Phase | Plan SHA-256 | 구간 수 |
|---|---|---:|
| Place pregrasp | `035a199a5bc74eee9d23bf6d33366c79ec6fe51b0a20a0d247466d3a4c1d0a9c` | 3 |
| Place grasp | `28140d35902df603c9ec8216ed276670df3e589af66bf651260ff1a046d94008` | 4 |
| 첫 -5 mm Z correction | `c2c327655d9294fd5dfc0331da4b1eb9e01987e50d49224c0f61e65f95518e99` | 2 |
| 두 번째 -5 mm Z correction | `f3706fa902ac9d27e3c6768c683828a496bfd194fa3a5a258b838ba3a1b9fbb7` | 1 |
| Place retreat | `0aa5c127f398b44c19d3da1917d204f026ccb05a2e093b0186b8c3179aaaeea0` | 5 |
| q0 복귀 | `2bb28414845dfaa7d09ef96163dfa59fcc935fa88d6d05c0c0447dd1f2657216` | 11 |

q0 복귀는 11/11 Action을 완료했다. 최종 arm 오차는 약 `+0.004602,
+0.004602, -0.004602, +0.001534, -0.007670 rad`였고 Shoulder 온도는
`36 C`였다. 사용자는 Pick, lift, Place, retreat, q0 자세와 최종
안전 shutdown을 육안으로 확인했다.

## 저장소 검증

- ROS 2 symlink-install 빌드: 8개 package 통과
- ROS-overlay Python 회귀: 336 통과
- `git diff --check`: 통과
- 생성된 plan, capture, 로컬 build/install 출력은 계속 무시(ignore)

## 수락 경계와 다음 작업

시운전 순서는 완료됐다. 로드맵이 요구하는 50회 시험 중 Pick/Place
각각 90% 이상 성공, 비명령 동작·충돌 0회 조건이 아직이므로 정식 단계
7 태스크 수락은 계속 `부분 통과`다. 이번 단일 감독 실행도
조작자가 확인한 정렬과 수동 Z correction 2회를 사용했으므로 무인
perception-to-task 자율성의 증거는 아니다.

50회 benchmark 전에:

1. 정지-후-정착(stop-and-settle) single-point 체인을 별도로 설계한
   multi-point/buffered trajectory 계약으로 교체한다. 하드웨어에
   사용하기 전에 timing, queue 용량, cancel/stop semantics, 연속
   diagnostics와 경로 추적을 정의해야 한다. 공개 `FollowJointTrajectory`
   interface는 변경하지 않는다.
2. nominal Place TCP offset `0.025 m`를 실측 TCP-to-contact 계약으로
   교체한다. 이번 승인된 실행은 bounded `-5 mm` correction을 2회
   추가로 필요로 했으며, 이는 초기 후보값이 `0.015 m` 근처임을
   시사한다. Pick과 Place offset은 분리하고, plan-only/충돌 검사와
   물리 검증 1회를 거친 뒤 값을 채택해야 한다.
